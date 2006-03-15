/*********************************************************************************//*
 *
 * Copyright (c) 2003, Frederick Ros (sl33p3r@free.fr)
 * Forked from ADI Linux driver from Analog Devices Inc.,
 *										 
 * Pipes.c
 *										 
 * --------------------------------------------------------------------------------
 *
 * This file is part of the eagle-usb driver package.
 *
 * "eagle-usb driver package" is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * "eagle-usb driver package" is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with "eagle-usb driver package"; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * $Id: Pipes.c,v 1.8 2005/01/17 20:54:42 sleeper Exp $
 */

#include "Adiutil.h"
#include "macros.h"
#include "Pipes.h"
#include "Mpoa.h"
#include "Uni.h"
#include "debug.h"
#include "eu_sm.h"

/**
 * --| Reception Mechanism:
 *
 *
 * Data reception is done by sending read requests via URBs + callback. USB
 * sub-system then calls the callback, with the associated request URB as
 * parameter when data is available.
 *
 * Each URB is associated to a Read Buffer (eu_rb_t), which hold
 * meta-information (that will be used later on) and received data.  Callbacks
 * are executed in interruption mode: thus they should be as quick as
 * possible. To achieve this, we use tasklets, to re-assemble ATM frames.
 *
 * Callbacks are simple: they just "de-associate" received buffer from the URB,
 * put it in a queue, schedule the "receive complete tasklet, get a new read
 * buffer, associates it with the URB, and re-submit the URB.
 *
 * Consummer thread, just wakes up, look if there's some data to process, and
 * processes it.
 *
 * Read Buffer structures are different whether driver is compiled to use Bulk
 * mode (USEBULK), or ISO mode.
 *
 * In Bulk mode, eu_rb_t is defined to be adi_bulk_rb_t:
 *
 *        +--------------------------+
 *        |    next                  |  to link read buffers in completed queue
 *        +--------------------------+
 *        |    length                |  length of received data
 *        +--------------------------+
 *        |                          |  Received Data (max=INCOMING_DATA_SIZE)
 *        |                          |
 *         ...........................
 *        |                          |
 *        |                          |
 *        +--------------------------+
 *
 * It is linked in the following way with the URB :
 *
 *
 *                                       +----------------+
 *          URB                          |    next        |  
 *    +-----------------------+          +----------------+   
 *    |                       |          |    length      |
 *    |    transfer_buffer    +--------> +----------------+ 
 *    |                       |          |                |   
 *     .......................            ................    
 *    |                       |          |                |  
 *    +-----------------------+          +----------------+  
 *                                                            
 * In ISO mode, eu_rb_t is defined to be adi_iso_rb_t
 *
 *    +------------------------+
 *    |         next           |       To link read buffers in completed queue
 *    +------------------------+ ---+  
 *    |  eu_iso_frame_t        |     \
 *    +------------------------+      \
 *    |  eu_iso_frame_t        |       \ 
 *    +------------------------+          FRAMES_PER_ISO_URB
 *     ........................        /
 *    +------------------------+      /
 *    |  eu_iso_frame_t        |     /
 *    +------------------------+ ---+
 *
 *   An eu_iso_frame_t describes a frame, as well as it's status after
 *   reception and it's length
 *
 *    +------------------------+
 *    |       status           |  Status of the reception for this frame
 *    +------------------------+ 
 *    |       length           |  Received Data Length
 *    +------------------------+
 *    |                        |  Received Data (max=FASTEST_ISO_RATE bytes)
 *     ........................
 *    |                        |
 *    +------------------------+
 *
 *  It is linked with the URB in the followin way:
 *
 *                               +--->+-----------------+
 *    +------------------------+ |    |     next        |
 *    |                        | |    +-----------------+
 *    +------------------------+ |    |     status      |
 *    |     transfer_buffer ---+-+    +-----------------+
 *    +------------------------+      |     length      |
 *    |   offset --------------+----->+-----------------+
 *    |   length               |      |                 |
 *    +------------------------+       .................
 *    |   offset -----------------+   |                 |
 *    |   length               |  |   +-----------------+
 *    +------------------------+  |   |     status      |
 *     ........................   |   +-----------------+
 *    |   offset               |  |   |     length      |
 *    |   length               |  +-->+-----------------+
 *    +------------------------+      |                 |
 *                                     .................
 *                                    |                 |
 *                                     .................
 *                                    |                 |
 *                                    +-----------------+
 *
 *   Associated callbacks, copies each status/length from the URB to the
 *   concerned ISO frame in the read buffer. Thus the kernel thread can process
 *   the received buffer, without having to refer to an URB.
 *
 *   As we're going to allocate a lot of read buffers, they are allocated from a
 *   lookaside cache (slab).
 *
 * --| Reassembly:
 *
 *  To speed up reassembly, we'll be using directly an skbuff, to hold the
 *  de-assembled cells.
 *
 */
 

/*
 * Local prototypes
 */

#ifdef USEBULK
static USB_COMPLETION_PROTO (eu_bulk_read_completion,urb,regs);
#else
static USB_COMPLETION_PROTO (eu_iso_read_completion,urb,regs);
#endif

static USB_COMPLETION_PROTO (eu_write_completion,urb,regs);



/**
 * eu_start_read_pipe - prepare receiving subsystem
 *
 * @ins   Concerned instance
 * @return 0 on success, error code otherwise.
 *
 * Prepare the receiving subsystem by queueing BULK or ISO IN urbs.
 *
 */
int eu_start_read_pipe (eu_instance_t *ins)
{
    int result;
    int retcode = 1;
    int i;
#ifndef USEBULK
    int j;
    unsigned iso_off;    
#endif    
    struct urb *purb;
    eu_rb_t  *rb;
    
    
    if (ins->AdiModemSm.CurrentAdiState & STATE_OPERATIONAL )
    {
        eu_enters (DBG_SAR);

        for ( i=0; i<INCOMING_Q_SIZE; i++ ) 
        {
            
            purb = ins->read_urb[i];            

            rb = (eu_rb_t *)kmem_cache_alloc ( ins->rb_cache , GFP_KERNEL );

            if ( rb == NULL ) 
            {
                goto byebye;
            }
            
            
#ifdef USEBULK
            
            usb_fill_bulk_urb ( ins->read_urb[i],
                                ins->usbdev,
                                ins->pipe_bulk_data_in, 
                                &rb->data[0],
                                INCOMING_DATA_SIZE,
                                eu_bulk_read_completion,
                                ins);
            
            ins->read_urb[i]->transfer_flags |= USB_QUEUE_BULK;            
#else
            /*
             * Init the URB fields per the spec
             */
#ifdef LINUX_2_4            
            purb->next     = ins->read_urb[(i+1) % INCOMING_Q_SIZE];
#endif            
            purb->dev      = ins->usbdev;
            purb->pipe     = ins->pipe_iso_data_in;
            purb->context  = ins;
            purb->complete = eu_iso_read_completion;
#ifdef LINUX_2_6            
            purb->interval = 1;
#endif            
             /*
             * Get a receive buffer from lookaside cache
             */
            purb->transfer_buffer        = rb;
            purb->transfer_buffer_length = sizeof ( eu_rb_t);
            purb->number_of_packets      = FRAMES_PER_ISO_URB;
            purb->transfer_flags         = URB_ISO_ASAP;            
            
            /*
             * Setup the frame descriptors that will receive the data
             */
            iso_off = offsetof (eu_iso_rb_t, frames[0]);
            
            for ( j=0;  j < purb->number_of_packets; ++j )
            {
                
                purb->iso_frame_desc[j].offset = iso_off +
                                                 offsetof (eu_iso_frame_t,data[0]);
                purb->iso_frame_desc[j].length = FASTEST_ISO_RATE;

                eu_dbg ( DBG_INIT," Set frame %d of URB %d to offset: %u\n",
                          j,i, purb->iso_frame_desc[j].offset);
                
                iso_off += sizeof (eu_iso_frame_t);
            }
                    
#endif /* USEBULK */
        }

        for ( i=0; i<INCOMING_Q_SIZE; i++ )
        {
            /*
             * Ok, data is initialized, fire off the ISO URB ring
             */
            if ( ( result = USB_SUBMIT_URB (ins->read_urb[i], GFP_KERNEL) ) )
            {
                eu_dbg (DBG_READ,"Error %d on read URB submission.\n", result);
            }   
        }
        
        retcode = 0;
        
      byebye:
        if ( retcode != 0 ) 
        {
            /*
             * Free previously allocated data
             */
            for ( i=0; i<INCOMING_Q_SIZE; i++ ) 
            {
                if ( ins->read_urb[i]->transfer_buffer ) 
                {
                    kmem_cache_free ( ins->rb_cache,
                                      GET_RBUF (ins->read_urb[i] ));
                    ins->read_urb[i]->transfer_buffer = NULL;
                }
            }            
        }
        
        
        eu_leaves (DBG_READ);
    }
    
    return (retcode);
}


#ifdef USEBULK
/**
 * eu_bulk_read_completion - BULK mode read completion function
 *
 * @urb     Completed URB 
 * @regs
 *
 * Called when inbound BULK urb is ready to be processed. We check some
 * conditions, add it to the received urb queue, and schedule the tasklet to
 * process it.
 * We can then resend a read URB and quit.
 */
static USB_COMPLETION_PROTO (eu_bulk_read_completion,urb,regs)
{
    eu_instance_t *ins = urb->context;
    struct net_device *ether;
    int result;
    eu_bulk_rb_t *rb;
    unsigned long flags;
    
    
    eu_enters (DBG_READ);
    

    /*This should never happen - but in a kernel driver, better safe than sorry!*/
    if ( !ins )
    {
        eu_dbg (DBG_READ,"NULL ins from urb->context!\n");
        
        goto Read_exit;
    }

    /* If we're not open for business, ignore this callback*/
    if ( !EU_TEST_FLAG(ins, EU_OPEN) )
    {
        eu_dbg (DBG_READ,"Read callback, but Open = FALSE!\n");
        goto Read_exit;
    }

    ether = ins->eth;
    if ( !netif_device_present(ether) )
    {
        eu_dbg (DBG_READ,"Somebody has killed our network interface.\n");
        
        goto Read_exit;
    }

    if (
        ( urb->status == -ENOENT ) ||
        (urb->status == -ECONNRESET ) ||
        ( urb->status == -ESHUTDOWN )
       )
    {
        
        eu_dbg (DBG_READ,"BULK URB unlinked.\n");
        if ( urb->transfer_buffer) 
        {
            kmem_cache_free (ins->rb_cache, urb->transfer_buffer);
            urb->transfer_buffer = NULL;
            urb->transfer_buffer_length = 0;
        }
        goto Read_exit;
    }

#ifdef SYNC_READS
    /***************************************************************************/
    /* We only allow one read to happen at a time. Since we only queue up one  */
    /* BULK IN urb, I don't know how we could get called again. But, we do need*/
    /* to make sure that we're not being reentered, just in case.	       */
    /***************************************************************************/
    if ( EU_TEST_FLAG ( ins, EU_READING ) )
    {
        ins->LinuxStats.rx_errors++;
        eu_dbg (DBG_READ,"Read callback, but Reading = TRUE!\n");
        
        goto Read_exit;
    }

    EU_SET_FLAG (ins, EU_READING);
    
#endif

    if ( urb->status != 0)
    {
        /*
         * These errors seem to be what we get when the device has been unplugged, so
         * lets stop a bunch of these from happening by unlinking the urb now
         */
        if ( (urb->status != -EILSEQ) &&
             (urb->status != -ETIMEDOUT) )
        {
            eu_dbg (DBG_READ,"Error status = %d in eu_bulk_read_completion - requeuing\n",
                    urb->status);
            
            goto ReadResubmit;        
        }
        else
        {
            eu_dbg (DBG_READ,"Error status = %d in eu_bulk_read_completion - not requeuing\n",
                    urb->status);
            
            goto Read_exit;        
        }
    }

    /*
     * If this was a zero-length read, we're done
     */
    if ( !urb->actual_length )
    {
        goto ReadResubmit;
    }

    eu_dbg ( DBG_READ,"Read data length is %#x bytes.\n", urb->actual_length );

    rb = GET_RBUF (urb);
    rb->length = urb->actual_length;

    /*
     * Enqueue received buffer completed read list
     */
    spin_lock_irqsave (&ins->comp_read_q_lock, flags);
    list_add_tail ( &rb->next, &ins->comp_read_q);
    spin_unlock_irqrestore (&ins->comp_read_q_lock, flags);


    /*
     * Schedule tasklet
     */
    tasklet_schedule ( &ins->rcv_complete_tasklet );
    
    
    /*
     * Get a new bunch of memory (we're in interrupt mode)
     */
    rb = (eu_bulk_rb_t *)kmem_cache_alloc ( ins->rb_cache, GFP_ATOMIC );

    if ( !rb ) 
    {
        eu_err ("Not enough mem to allocate read buffer.\n");
        
        goto Read_exit;        
    }
    
    /*
     * And skip a list head
     */
    urb->transfer_buffer = &rb->data[0];
    
  ReadResubmit:
    
    if ( ins->AdiModemSm.CurrentAdiState & STATE_OPERATIONAL )
    {
        usb_fill_bulk_urb(urb, ins->usbdev, ins->pipe_bulk_data_in, 
                          urb->transfer_buffer, INCOMING_DATA_SIZE,
                          eu_bulk_read_completion, ins);
        if ( ( result = USB_SUBMIT_URB ( urb, GFP_ATOMIC ) ) ) 
        {
            eu_dbg (DBG_READ,"Error %d on read URB submit.\n", result);
        }
        
    }
    
#ifdef SYNC_READS
    EU_CLEAR_FLAG (ins, EU_READING);
#endif

/*We will only jump to here on conditions where we don't want to queue another read*/
  Read_exit:
    eu_leaves (DBG_READ);
    
    return;
}

#else /* USEBULK */

/**
 * eu_iso_read_completion - ISO mode read completion function
 *
 * @urb     Completed URB 
 * @regs
 *
 * Called when inbound ISO urb is ready to be processed. We check some
 * conditions, add it to the received urb queue, and schedule the tasklet to
 * process it.
 * We then resubmit a read URB ( on 2.6 kernels only ) and quit.
 */

static USB_COMPLETION_PROTO (eu_iso_read_completion,urb,regs)
{
    eu_instance_t *ins = urb->context;
    struct net_device *ether;
    int i;
    eu_iso_rb_t *rb;
    unsigned long  flags;
    
    
    eu_enters (DBG_READ);
    

    /*
     * This should never happen - but in a kernel driver, better safe than sorry!
     */
    if ( !ins )
    {
        eu_err ("NULL ins from urb->context!\n");

        goto byebye;
        
    }

    /*
     * If we're not open for business, ignore this callback
     */
    if ( !EU_TEST_FLAG(ins, EU_OPEN) )
    {
        eu_err ("Read callback, but Open = FALSE!\n");
        
        if ( urb->transfer_buffer) 
        {
            kmem_cache_free (ins->rb_cache, urb->transfer_buffer);
            urb->transfer_buffer = NULL;
            urb->transfer_buffer_length = 0;
        }
        
	goto get_new_urb_buffer;
    }

    ether = ins->eth;
    if ( !netif_device_present(ether) )
    {
        eu_err ("Somebody has killed our network interface.\n");
        
	goto byebye;
    }

    if (
        ( urb->status == -ENOENT )     ||
        ( urb->status == -ECONNRESET ) ||
        ( urb->status == -ENOENT )     ||
        ( urb->status == -ENODEV )
       )
    {

        eu_dbg (DBG_READ,"ISO URB unlinked.\n");
        if ( urb->transfer_buffer) 
        {
            kmem_cache_free (ins->rb_cache, urb->transfer_buffer);
            urb->transfer_buffer = NULL;
            urb->transfer_buffer_length = 0;
        }
        goto byebye;
    }

    /*
     * If this was a zero-length read, we're done
     */
    if ( !urb->actual_length ) 
    {
	goto clear_status;
    }
    
    /*
     * We can now process incoming data. We must go through each frame, copy the
     * status/length returned to in relevant fields in the eu_iso_rb_t structure,
     * enqueue the buffer in the list of completed read, and wake-up the consumer
     * thread, and get a new bunch of memory.
     */
    urb->status = 0;
    rb = GET_RBUF (urb);
    for ( i=0; i<urb->number_of_packets; i++ ) 
    {
        rb->frames[i].status = urb->iso_frame_desc[i].status;
        rb->frames[i].length = urb->iso_frame_desc[i].actual_length;
        
        urb->iso_frame_desc[i].status = 0;
    }
    
    /*
     * Enqueue in completed read list
     */
    spin_lock_irqsave (&ins->comp_read_q_lock, flags);
    list_add_tail ( &rb->next, &ins->comp_read_q );
    spin_unlock_irqrestore (&ins->comp_read_q_lock, flags);

    
    /*
     * Schedule tasklet
     */
    tasklet_schedule ( &ins->rcv_complete_tasklet );

  get_new_urb_buffer:
    /*
     * Get a new bunch of memory (we're in interrupt mode)
     */
    rb = (eu_iso_rb_t *) kmem_cache_alloc ( ins->rb_cache, GFP_ATOMIC );
    
    if ( !rb ) 
    {
        eu_err ("Not enough mem to allocate read buffer.\n");
        urb->transfer_buffer = NULL;
        urb->transfer_buffer_length = 0;
	goto byebye;        
    }

    urb->transfer_buffer = rb;    
    urb->transfer_buffer_length = sizeof ( eu_rb_t);
    
  clear_status:
    
        for (i=0; i<urb->number_of_packets; i++)
        {
            urb->iso_frame_desc[i].status = 0;
            urb->iso_frame_desc[i].actual_length = 0;
        }
        urb->status = 0;
        
    
#ifdef LINUX_2_6
        {
            int ret;

            urb->dev = ins->usbdev;
           
            ret = USB_SUBMIT_URB (urb, GFP_ATOMIC);
            
            if ( ret ) 
            {
                eu_err ("eu_iso_read_completion: Can't submit urb (%d)\n",ret);
            }
        }
#endif
        
  byebye:
    eu_leaves (DBG_READ);
    
    return;
}

#endif /* USEBULK */

/**********************************************************************************/
/* eu_stop_read_pipe   								  */
/*										  */
/* Simple, really, but maybe we'll eventually have to do more.			  */
/**********************************************************************************/
void eu_stop_read_pipe ( eu_instance_t *ins )
{
    int i;

    eu_enters (DBG_READ);
    
    for (i=0; i<INCOMING_Q_SIZE; i++)
    {


        if ( ins->read_urb[i]) 
        {
            eu_dbg (DBG_READ,"About to unlink read urb %u\n",i);
            USB_KILL_URB( ins->read_urb[i] );
            
            eu_dbg (DBG_READ,"Read urb %u unlinked.\n",i);
        }
        else 
        {
            eu_dbg (DBG_READ,"Read urb %u not present\n",i);
            continue;
        }
        
        

        /*
         * And free read buffers
         */
        if ( ins->read_urb[i]->transfer_buffer ) 
        {
            eu_dbg (DBG_READ,"About to free rb (0x%p)from urb %u\n",
                    ins->read_urb[i]->transfer_buffer,i);
            kmem_cache_free ( ins->rb_cache, GET_RBUF (ins->read_urb[i]));
            ins->read_urb[i]->transfer_buffer = NULL;
            ins->read_urb[i]->transfer_buffer_length = 0;
            eu_dbg (DBG_READ,"Read urb %u rb freed.\n",i);
        }
    }

    eu_leaves (DBG_READ);
}

/**********************************************************************************/
/* eu_read_to_uplayers	       							  */
/*										  */
/* This is called by the Uni code once it has determined that a valid		  */
/* ethernet packet has been reassembled and is ready for processing		  */
/* by higher level network code. First we'll do things that a normal		  */
/* ethernet card would do in hardware (packet filtering, etc.), then		  */
/* we'll follow the standard procedure for sending data back up the stack.	  */
/* Comment by Anoosh on Apri30,2002, In MAC driver in ReadWaitingData function    */
/* in read.c we check if we are using PPPoA then use RealSize for the size of     */
/* data that we send up and for ethernet we pad packets smaller than 64 bytes     */
/* and add space for Ethernet FCS. But here in linux it looks that we just use    */
/* RealSize for the size of data without adding anything therefore we just        */
/* ignore adding anything here to match with MAC driver. Look into read.c in      */
/* MAC driver line 168                                                            */
/**********************************************************************************/
void eu_read_to_uplayers ( eu_instance_t *ins, struct sk_buff *skb )
{
    uint32_t packet_size = 0;
    uint32_t  encaps_skip_size = 0;
    static const uint8_t PPP_ETH_ADDR_DEST[ETH_ALEN] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    static const uint8_t PPP_ETH_ADDR_SRC[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    
    eu_enters (DBG_READ);
    
    
    if ( MpoaProcessReceivedPdu (ins, skb, &encaps_skip_size) ) 
    {
        /*
         * Drop it
         */
	ins->LinuxStats.rx_errors++;
        dev_kfree_skb( skb );
        goto byebye;
    }


    /*
     * Drop encapsulation if needed
     */
    skb_pull (skb, encaps_skip_size );            

    packet_size = skb->len;

    
    switch ( ins->MpoaMode )
    {
	case MPOA_MODE_BRIDGED_ETH_LLC:
	case MPOA_MODE_BRIDGED_ETH_VC :
        {
            /*
             * Everything is already done
             */
        }
        break;
        
	case MPOA_MODE_ROUTED_IP_LLC  :
	case MPOA_MODE_ROUTED_IP_VC   :
        {
            /*
             * Push an ethernet frame at beginning
             */
            skb_push ( skb, 14 );
            
            /*
             * Copy ethernet header info
             */
            memcpy ( skb->data, ins->eth->dev_addr, ETH_ALEN);
            memcpy ( skb->data + 6, ins->eth->dev_addr, ETH_ALEN);
            skb->data[11] ^=0x1;		/* Try a fake address */
            skb->data[12] = (uint8_t) 0x08;	/* ETH_P_IP */
            skb->data[13] = (uint8_t) 0x00;            
        }
        break;

        
	case MPOA_MODE_PPPOA_LLC      :
	case MPOA_MODE_PPPOA_VC       :
        {

            
            /*
             * Push an (ethernet frame + packet size) at beginning
             */
            skb_push ( skb, 16 );

            /*
             * Copy fake source and destination
             */
            memcpy(skb->data, PPP_ETH_ADDR_DEST, ETH_ALEN);
            memcpy(skb->data+ETH_ALEN, PPP_ETH_ADDR_SRC, ETH_ALEN);

            /*
             * Set the ethernet proto filed ( 0x0003 is for every packet
             */
            skb->data[12] = (uint8_t) 0x00;
            skb->data[13] = (uint8_t) 0x03;
            
            /*
             * Set the packet size field:
             */
            skb->data[14] = (uint8_t) (packet_size >> 8);
            skb->data[15] = (uint8_t) (packet_size);
            
        }
        break;
    }

    
    /*
     * Set up packet type:
     */
    skb->protocol = eth_type_trans (skb, ins->eth);
    
    /*
     * Update statistics:
     */
    ins->eth->last_rx = jiffies;    
    ++ins->LinuxStats.rx_packets;
    ins->LinuxStats.rx_bytes += packet_size;
    
    /*
     * Send the packet to upper layer:
     */
    netif_rx (skb);
    
  byebye:
    eu_leaves (DBG_READ);        
    return;
}


/**
 * eu_write_atm_data - Just send ATM cells to modem
 */

int eu_write_atm_data ( eu_instance_t *ins )
{
    int result;

    if ( ins->AdiModemSm.CurrentAdiState & STATE_OPERATIONAL )
    {
        eu_dbg (DBG_READ,"*** Writing %#x bytes to USB\n", ins->Vc.segmenter.size);

        /*
         * Prepare the URB
         */
        
#ifdef LINUX_2_6 
        usb_init_urb (ins->urb_write);
#endif
        ins->urb_write->status = 0;
        
        usb_fill_bulk_urb ( ins->urb_write,
                            ins->usbdev,
                            ins->pipe_bulk_data_out,
                            ins->segmentation_buffer,
                            ins->Vc.segmenter.size, 
                            eu_write_completion,
                            ins );
        /*
         * Marks the writing URB as pending
         */
        EU_SET_FLAG (ins,EU_WRITING);
        ins->urb_write->transfer_flags |= USB_QUEUE_BULK;
        
        /*
         * Send it:
         */
        result = USB_SUBMIT_URB ( ins->urb_write , GFP_ATOMIC );
    }
    else
    {
        /*I want to make sure that we return error if modem is*/
        /*not in OPERATIONAL mode because otherwise we screw  */
        /*USB pipes again. We do not want to send anything to */
        /*USB if modem is not in OPERATIONAL mode             */ 
        result = 1;
    }
    
    return result;
}


/**
 * eu_write_completion - Called when the write of outgoing ATM cells is complete.
 */
static USB_COMPLETION_PROTO (eu_write_completion,urb,regs)
{
    eu_instance_t *ins = urb->context;
    struct net_device *ether;

    eu_enters (DBG_READ);
    

    if ( (ins == NULL) || (!EU_TEST_FLAG(ins, EU_OPEN)) )
    {
       eu_dbg (DBG_READ,"NULL urb->context or Open=FALSE in WriteCompletion!\n");
       
       goto byebye;
    }

    ether = ins->eth;

    if ( !netif_device_present(ether) )
    {
       eu_dbg (DBG_READ,"Network interface no longer present!\n");
       
       goto byebye;
    }

    /* Update transmission statistics: */
    if ( 0 == urb->status )
    {
	++ins->LinuxStats.tx_packets;
	ins->LinuxStats.tx_bytes += ins->out_pkt_size;
    }
    else
    {
	if (
        ( urb->status == -ENOENT ) ||
        ( urb->status == -ECONNRESET ) ||
        ( urb->status == -ENOENT )
       )
	{
	    eu_warn ("transmit URB %p cancelled\n", urb);
            
	    ++ins->LinuxStats.tx_aborted_errors;
            /*
             *  Clear status in order to be able to re-use this URB : otherwise
             * host (UHCI/OHCI), will see status as -ECONNRESET when doing the
             *  next usb_submit_urb, and will refuse to send it, returning
             *  -EINVAL.
            */
            urb->status = 0;
            urb->transfer_flags &= ~URB_ASYNC_UNLINK;
	}
	else
	{
	    eu_warn ("transmit error with URB status %d\n", urb->status);
        
	    ++ins->LinuxStats.tx_carrier_errors;
	}
        
	++ins->LinuxStats.tx_errors;
	ins->out_pkt_size = 0;
    }

    /*
     * Ask for another outgoing socket buffer
     */
    EU_CLEAR_FLAG (ins,EU_WRITING);
    
    netif_wake_queue(ether);

byebye:
    eu_leaves (DBG_READ);
    
}

