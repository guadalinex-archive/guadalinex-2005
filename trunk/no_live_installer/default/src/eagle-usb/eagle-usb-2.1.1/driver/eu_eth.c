/*
 * eu_eth - Ethernet handling functions.
 * Copyright (C) 2004  Frederick Ros (sl33p3r@free.fr)
 *
 * Fix by Robert.Siemer-eagleusb@backsla.sh  22.8.2004:
 * Lift MTU limit in PPPoA case so the kernel will not return
 * -EMSGSIZE on send()-alike syscalls before asking us...
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Id: eu_eth.c,v 1.8 2005/01/17 20:54:42 sleeper Exp $ 
 */

#include "Adiutil.h"
#include <linux/module.h>
#include <linux/ctype.h>   /* For isxdigit */
#include "Pipes.h"
#include "macros.h"
#include "Uni.h"
#include "Mpoa.h"
#include "debug.h"
#include "eu_eth.h"
#include "eu_sm.h"



/* --------------------- Private Functions Declaration --------------------- */
static int eu_eth_open ( struct net_device *dev );
static int eu_eth_close ( struct net_device *dev );
static int eu_eth_ioctl ( struct net_device *dev, struct ifreq *rq, int cmd );
static struct net_device_stats *eu_eth_stats ( struct net_device *dev );
static int eu_eth_start_xmit ( struct sk_buff *skb, struct net_device *dev );
static void eu_eth_set_multicast ( struct net_device *dev );
static void eu_eth_tx_timeout ( struct net_device *dev );


/* -------------------------- Exported Functions --------------------------- */

/**
 * eu_eth_create - Creates our ethernet device
 *
 */
void eu_eth_create ( void *data )
{
    eu_instance_t *ins = (eu_instance_t *)data;
    
    struct net_device *ether = NULL;
    int result = 0;
    
    /*
     * First allocate an ethernet device  structure
     */
    
    ether = alloc_etherdev (0);

    if ( !ether )
    {
        eu_err ("eu_eth_create: Can't allocate etherdev.\n");
        return ;
    }
    
    /* Initialize the Ethernet device: */
    ether->priv = ins;
    
    if ( ins->if_name[0] != '\0' ) 
    {
        /* User supplied a name */
        strncpy ( ether->name, ins->if_name, IFNAMSIZ );
        ether->name[IFNAMSIZ-1] = '\0';
    }

    ether->init = eu_eth_init;

    /*
     * Register device to network subsystem: this will call in return eu_eth_init
     */
    result = register_netdev ( ether );

    if ( result != 0 ) 
    {
        EU_FREE_NETDEV (ether);
        eu_err ("eu_create_etherdev: Cannot register to network (res=%d)\n",result);
        
    }
    else 
    {
        eu_report (" Ethernet device %s created.\n",
                   ether->name);
    }
    
    ins->eth = ether;    

    EU_SET_FLAG (ins, EU_ETH_REGISTERED);
    EU_CLEAR_FLAG (ins,EU_ETH_REGISTERING);
    
}


/**
 * eu_eth_init - Initialize ethernet device structure
 *
 */
int eu_eth_init ( struct net_device *dev ) 
{
    eu_instance_t *ins = (eu_instance_t *)dev->priv;
    
    if ( ins == NULL ) 
    {
        eu_err ("eu_eth_init: No instance !\n");
        return (-EINVAL);
    }
    
    if (ins->MpoaMode == MPOA_MODE_PPPOA_VC ||
        ins->MpoaMode == MPOA_MODE_PPPOA_LLC) {
	    dev->mtu = ins->mru;	/* Who wonders: quite symetric here */
	    dev->change_mtu = NULL;
    }
   
    dev->open = eu_eth_open;
    dev->stop = eu_eth_close;
    dev->do_ioctl = eu_eth_ioctl;
    dev->get_stats = eu_eth_stats;
    dev->hard_start_xmit = eu_eth_start_xmit;

    dev->set_multicast_list = eu_eth_set_multicast;
    dev->tx_timeout         = eu_eth_tx_timeout;
    dev->watchdog_timeo     = TRANSMIT_TIMEOUT;
        
    /*
     * MAC address has already been retrieved from device at init time
     */
    memcpy ( dev->dev_addr, ins->mac, ETH_ALEN );
    
    SET_MODULE_OWNER(dev);

    eu_dbg (DBG_ENET,"Ethernet device initialized.\n");
    
    return 0;
}

/* --------------------------- Private Functions --------------------------- */

/**
 * eu_eth_open - Someone opens the ethernet interface
 *
 */
static int eu_eth_open ( struct net_device *dev )
{
    eu_instance_t *ins = (eu_instance_t *)dev->priv;
    int retcode = 0;

    if ( !ins ) 
    {
        eu_err ("eu_eth_open: No instance !\n");
        return (-EINVAL);
    }
    
    MODULE_USER_GET;
    eu_enters (DBG_ENET);
    

    if ( !(ins->AdiModemSm.CurrentAdiState & STATE_OPERATIONAL ) )
    {
        eu_err ("eu_eth_open: Can't open ethernet device until modem is synchronized.\n");
        retcode = -EINVAL;
        goto byebye;
    }    

    EU_SET_FLAG (ins,EU_OPEN);

    /*
     * Create incoming data URBs
     */
    if ( eu_start_read_pipe (ins) != 0 ) 
    {
        eu_err ("Error while trying to start comm\n");
        EU_CLEAR_FLAG (ins, EU_OPEN);
        retcode = -ERESTARTSYS;
        goto byebye;
    }
    

    /*
     * Tell the kernel we're ready for network traffic flow
     */
    EU_CLEAR_FLAG (ins, EU_WRITING);
    netif_start_queue (dev);

  byebye:

    if ( retcode != 0 ) 
    {
        MODULE_USER_RELEASE;
    }
    
    eu_leaves (DBG_ENET);
    
    return (retcode);
}

/**
 * eu_eth_close - Shutdown of our ethernet interface
 *
 */
static int eu_eth_close ( struct net_device *dev )
{
    eu_instance_t *ins = (eu_instance_t *)dev->priv;
    
    if ( !ins ) 
    {
        eu_err ("eu_eth_close: No instance !\n");
        return (-EINVAL);
    }

    eu_enters (DBG_ENET);    
    
    /*
     * Tell the kernel that we do not want any more network traffic
     */
    netif_stop_queue (dev);

    /*
     * Stop our outgoing data urbs
     */
    if ( EU_TEST_FLAG (ins, EU_WRITING ) )
    {
        USB_KILL_URB(ins->urb_write);        
    }
    
    /*
     * Stop our incoming data urbs
     */
    eu_stop_read_pipe (ins);

    
    /*
     * Tell ourselves that we're closed for business!
     */
    EU_CLEAR_FLAG (ins,EU_OPEN);

    eu_leaves (DBG_ENET);
    
    MODULE_USER_RELEASE;
    return 0;
}

/**
 * eu_eth_ioctl - Handles ioctl on our ethernet if. Currently no ioctl are supported.
 *
 */
static int eu_eth_ioctl ( struct net_device *dev, struct ifreq *rq, int cmd )
{
    eu_instance_t *ins = (eu_instance_t *)dev->priv;

    if ( !ins ) 
    {
        eu_err ("eu_eth_ioctl: No instance !\n");
        return (-EINVAL);
    }

    eu_enters (DBG_ENET);
    
    
    eu_leaves (DBG_ENET);
    
    return -ENOTTY;
}

/** 
 * eu_eth_stats - return ethernet device stats
 *
 */
static struct net_device_stats *eu_eth_stats ( struct net_device *dev )
{
    eu_instance_t *ins = (eu_instance_t *)dev->priv;

    if ( !ins ) 
    {
        eu_err ("eu_eth_stats: No instance !\n");
        return (NULL);
    }

    eu_enters (DBG_ENET);
    

    eu_leaves (DBG_ENET);
    
    return &ins->LinuxStats;
}


/** 
 * eu_eth_start_xmit - Sends a packet on the network.
 *
 */
static int eu_eth_start_xmit ( struct sk_buff *skb, struct net_device *dev )
{
    eu_instance_t *ins = (eu_instance_t *)dev->priv;
    struct ethhdr *phdr = (struct ethhdr *) skb->data;
    int result=0, packet_sent=0;
    
    if ( !ins ) 
    {
        eu_err ("eu_eth_start_xmit: No instance !\n");
        return (-EINVAL);
    }

    eu_enters (DBG_ENET);
    
    
    /*
     * Tell the kernel to hold off until we're done sending this packet
     */
    netif_stop_queue (dev);

    /*
     * Send the packet through the SAR to get ATM cells
     */
    switch ( ins->MpoaMode )
    {
        case MPOA_MODE_BRIDGED_ETH_LLC :
        case MPOA_MODE_BRIDGED_ETH_VC  :
            result = eu_uni_process_out_pdu ( ins, skb->data, skb->len );
            break;
            
        case MPOA_MODE_ROUTED_IP_LLC   :
        case MPOA_MODE_ROUTED_IP_VC    :
            
            if ( ( phdr->h_proto != ETHERTYPE_IP ) &&
                 ( phdr->h_proto != ETHERTYPE_IPV6 ) )
            {
                if (phdr->h_proto == ETHERTYPE_ARP)
                {
                    EthernetArpReply (ins, skb->data, skb->len);
                    
                    result = eu_uni_process_out_pdu ( ins, skb->data, skb->len );
                }
                else
                {
                    /*
                     * If the packet is not an ARP or IP, then its invalid, but
                     * tell Linux that all is well
                     */
                    eu_err ("Invalid ethernet in packet %x.\n",phdr->h_proto);
                    goto xmit_exit;
                } 
            }
            else
            {
                /*
                 * Adjust the packet pointer and size so we
                 * skip the ethernet header
                 */
                skb->data   += sizeof (struct ethhdr);
                skb->len -= sizeof (struct ethhdr);
                result = eu_uni_process_out_pdu ( ins, skb->data, skb->len );
            }
            
            break;
            
        case MPOA_MODE_PPPOA_LLC       :
        case MPOA_MODE_PPPOA_VC        :
            result = eu_uni_process_out_pdu ( ins, skb->data+16, skb->len-16 );
            break;
    }
    
    /*
     * If all is OK, send the data:
     */
    if ( !result && !packet_sent)
    {
        /*
         * Fix the timestamp of transaction for timeout detection:
         */
        dev->trans_start = jiffies;
        
        /*
         * Store packet information for transmit statistics:
         */
        ins->out_pkt_size = skb->len;
        
        /*
         * Uni says all is ok to proceed with sending the data:
         */
        result = eu_write_atm_data (ins);
    }
    /*
     * We've copied the data into our own internal buffers, so we can 
     * go ahead and free the kernel's buffer for this packet.
     */
    
  xmit_exit:
    dev_kfree_skb(skb);
    
    if ( 0 != result )
    {
	
        /*
         * Update statistics: count only real errors (not "not sent" that occured
         * because the modem is (re)booting
         */
        if ( result != 1 ) 
        {
            eu_warn ("Packet transmission error: result=%d\n", result);
            
            ++ins->LinuxStats.tx_errors;
            ++ins->LinuxStats.tx_carrier_errors;
        }
        
        /*
         * There will be no URB completion, so we must wake up the network layer
         * ourself:
         */
        EU_CLEAR_FLAG (ins, EU_WRITING);
        netif_wake_queue (dev);
    }
    else
    {
        /*******************************************************************************/
        /* You'll notice that, in this case, we do not wake up the netif queue	       */
        /* .... that's because we're only using one write URB, so we have	       */
        /* to wait until the completion handler for the write is called		       */
        /* or a transmit timeout occur before we can tell kernel that we can handle    */
        /* more network data.							       */
        /* A potential performance improvement would be to have an internal	       */
        /* URB queue for outgoing data, so we could (theoretically) start	       */
        /* the software-processing of a packet while another one is being sent.	       */
        /*******************************************************************************/
    }
    
    eu_leaves (DBG_ENET);
    
    return 0;
}

/**
 * eu_eth_set_multicast - Set multicast for our ethernet device
 *
 */
static void eu_eth_set_multicast ( struct net_device *dev )
{
    eu_enters (DBG_ENET);
    

    /*******************************************************************************/
    /* Our ethernet side is really all software, so anything that		   */
    /* a normal ethernet card would do in hardware, we do here in the driver.	   */
    /* On a normal ethernet card, you would upload the filter address list	   */
    /* to the card - but we just loop through a fast software compare. 		   */
    /* Since we will just always check whats in the net_device *, we don't 	   */
    /* have to do anything here!						   */
    /*******************************************************************************/

    eu_leaves (DBG_ENET);
    
}

/**
 * eu_eth_tx_timeout  -   Transmission timeout
 *
 */
static void eu_eth_tx_timeout ( struct net_device *dev )
{
    eu_instance_t *ins = (eu_instance_t *)dev->priv;
    
    if ( !ins ) 
    {
        eu_err ("eu_eth_tx_timeout: No instance !\n");
        return;
    }

    eu_warn ("Transmission timed out!\n");
    
    /*
     * Other linux drivers don't seem to recover and resend
     * the socket buffer Linux gives them. However, O'Reilly's
     * Linux Device Driver says we must not lose the buffer.
     * But, as packets may anyway be lost by media in general,
     * it's quite certain the protocol will resend it sooner or
     * later, or consider packet lost as normal. Therefore, this
     * driver do not attempt anything more than removing the broken
     * URB. This will call WriteCompletion, which will call
     * netif_wakequeue:
    */
    
    ins->urb_write->transfer_flags |= URB_ASYNC_UNLINK;
    usb_unlink_urb(ins->urb_write);

    ins->urb_oam_write->transfer_flags |= URB_ASYNC_UNLINK;
    usb_unlink_urb(ins->urb_oam_write);
    
    /* We must reset the transaction time to keep the watchdog quiet: */
    dev->trans_start = jiffies;

    /*
     * restart queue if needed
     */
    netif_wake_queue (dev);

    eu_leaves (DBG_ENET);
    
}
