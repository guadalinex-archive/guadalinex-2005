
/*
 * Mpoa.c
 *
 * Copyright (c) 2001, Analog Devices Inc., All Rights Reserved
 *										 
 * RFC2684(1483) "Multi-Protocol Encapsulation Over ATM AAL5" support module
 *										 
 * Fix by Robert.Siemer-eagleusb@backsla.sh  22.8.2004:
 * Correction of mru in PPPoA cases.
 *
 * ----------------------------------------------------------------------------
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
 * $Id: Mpoa.c,v 1.5 2004/08/26 20:47:54 sleeper Exp $
 */

#include "Adiutil.h"
#include "Mpoa.h"
#include "Uni.h"
#include "debug.h"

/**********************************************************************************/
/* This module provides support similar to Rfc1483.c from the NDIS code.       */
/* Therefore, the following text from Rfc1483.c is provided, modified slightly,   */
/* as an explanation for how the various encapsulation types are handled. It      */
/* should be noted that some of the resonsibility for encapsulation is shared     */
/* by the more generic UniSendPdu routine. It checks to see if an encapsulation   */
/* header has been specified and includes it went sending the Pdu. This eliminates*/
/* an additional call for the most common operations.                  */
/*                                          */
/* The MPOA module supports four PDU options:                      */
/*      1. LLC Encapsulated Ethernet                          */
/*      2. VC Encapsulated Ethernet (without FCS)                  */
/*      3. LLC Encapsulated IP                               */
/*      4. VC Encapsulated IP                              */
/*                                            */
/*      The type of encapsulation is set during initialization, based on the      */
/*      MpoaMode configuration option. The default mode is LLC Encapsulated       */
/*      Ethernet. Each of the PDU options is described briefly below.            */
/*      See http://www.ietf.org/rfc/rfc2684.txt?number=2684 for details.      */
/*      (Note that OT = Open Transport.)                      */
/*                                          */
/*      1. LLC Encapsulated Ethernet                          */
/*                                          */
/*      Ethernet frames are accepted from OT and encapsulated as shown below.       */
/*      Note that the PID value of 0x00-07 is used, indicating that the LAN      */
/*      FCS (Frame Check Sequence) is not present.                  */
/*                                          */
/*               +-------------------------------+                  */
/*               |       LLC 0xAA-AA-03          |                  */
/*               +-------------------------------+                  */
/*               |       OUI 0x00-80-C2          |                  */
/*               +-------------------------------+                  */
/*               |       PID 0x00-07             |                  */
/*               +-------------------------------+                  */
/*               |       PAD 0x00-00             |                  */
/*               +-------------------------------+                  */
/*               |    MAC destination address    |                  */
/*               +-------------------------------+                  */
/*               |                               |                  */
/*               |   (remainder of MAC frame)    |                  */
/*               |                               |                  */
/*               +-------------------------------+                  */
/*                                          */
/*      Once encapsulated, the RFC2684 PDUs are sent using AAL5 on the PVC       */
/*      provisioned during initialization.                      */
/*                                          */
/*      Inbound LLC encapsulated 802.3 frames are accepted from the ATM UNI      */
/*      module.  Note that in the inbound direction, either PID value is       */
/*      allowed.  If the PID value is 0x00-01 (indicating LAN FCS is present),      */
/*      the LAN FCS is simply discarded.  The LLC encapsulation is removed and      */
/*      the Ethernet frame is indicated to OT.                      */
/*                                          */
/*      2. VC Encapsulated Ethernet                          */
/*                                           */
/*      Same as (1) above with the exception that the LLC header is ommitted      */
/*      (except for the PAD field).  Thus the PDUs transmitted are as shown      */
/*      below.                                      */
/*                                          */
/*               +-------------------------------+                  */
/*               |       PAD 0x00-00             |                  */
/*               +-------------------------------+                  */
/*               |    MAC destination address    |                  */
/*               +-------------------------------+                  */
/*               |                               |                  */
/*               |   (remainder of MAC frame)    |                  */
/*               |                               |                  */
/*               +-------------------------------+                  */
/*                                          */
/*      Note that the inclusion of the LAN FCS is not supported with VC          */
/*      Encapsulated Ethernet.                              */
/*                                          */
/*      3. LLC Encapsulated IP                                */
/*                                            */
/*      Ethernet frames are accepted from OT and the Ethernet header is          */
/*      discarded up to the EtherType.  If the EtherType is not IP, the              */
/*      frame is discarded.  Otherwise, the frame is encapsulated and sent      */
/*      as shown below.                                  */
/*                                          */
/*               +-------------------------------+                  */
/*               |       LLC 0xAA-AA-03          |                  */
/*               +-------------------------------+                  */
/*               |       OUI 0x00-00-00          |                  */
/*               +-------------------------------+                  */
/*               | EtherType 0x08-00 (IP)        |                  */
/*               +-------------------------------+                  */
/*               |                               |                  */
/*               |           IP PDU              |                  */
/*               |                               |                  */
/*               +-------------------------------+                  */
/*                                            */
/*      In addition, the MPOA module will generate responses to outbound      */
/*      Ethernet ARP requests in order to satisfy the IP stack.              */
/*      Note:                                          */
/*      I do not know if the above statement will be true on the Mac platform,    */
/*      real debugging will determine whether or not we need to do this.      */
/*                                              */
/*      4. VC Encapsulated IP                              */
/*                                          */
/*      Same as (3) above except that the LLC header and Ethertype are ommitted.  */
/*      Thus the PDUs transmitted are as shown below.                  */
/*                                          */
/*               +-------------------------------+                  */
/*               |                               |                  */
/*               |           IP PDU              |                  */
/*               |                               |                  */
/*               +-------------------------------+                  */
/*                                          */
/**********************************************************************************/

/**********************************************************************************/
/* Local prototypes                                  */
/**********************************************************************************/
static Boolean MpoaFilterReceivedPacket(eu_instance_t *ins, struct ethhdr *pMac);


/*
 * For processing network data
 */
#define MPOA_MAX_ENCAP_SIZE                 10
#define MPOA_LLC_ENCAP_BRIDGED_ETH_SIZE     10
#define MPOA_LLC_ENCAP_ROUTED_IP_SIZE        8
#define MPOA_VC_ENCAP_BRIDGED_ETH_SIZE       2
#define MPOA_LLC_ENCAP_PPPOA_SIZE            4

/**********************************************************************************/
/* These static data structures are the encapsulation headers we need for Mpoa    */
/**********************************************************************************/

#define IS_LLCBRIDGEDETHNOFCS(p) (\
    (cpu_to_le32(*((UInt32 *)p))     == 0x0003AAAA) && \
    (cpu_to_le32(*((UInt32 *)(p+4))) == 0x0700C280) && \
    (cpu_to_le32(*((UInt32 *)(p+6))) == 0x00000700))

typedef struct LLC_HEADER 
{
    uint8_t Llc[3];
    uint8_t Oui[3];
    uint8_t Pid[2];
    uint8_t Pad[2];
} LLC_HEADER;


static const LLC_HEADER MpoaHdr_LlcEncapBridgedEthNoFcs =
{ {0xAA,0xAA,0x03},
  {0x00,0x80,0xC2},
  {0x00,0x07},
  {0x00,0x00}
};

#define IS_LLCBRIDGEDETHFCS(p) (\
    (cpu_to_le32(*((UInt32 *)p))     == 0x0003AAAA) && \
    (cpu_to_le32(*((UInt32 *)(p+4))) == 0x0100C280) && \
    (cpu_to_le32(*((UInt32 *)(p+6))) == 0x00000100))

#ifdef LATER_LATER_LATER /* this isn't used right now */
static const LLC_HEADER MpoaHdr_LlcEncapBridgedEthFcs =
{ {0xAA,0xAA,0x03},
  {0x00,0x80,0xC2},
  {0x00,0x01},
  {0x00,0x00}
};
#endif

#define IS_LLCROUTEDIP(p) (\
    (cpu_to_le32(*((UInt32 *)p))     == 0x0003AAAA) && \
    (cpu_to_le32(*((UInt32 *)(p+4))) == 0x00080000))

static const LLC_HEADER MpoaHdr_LlcEncapRoutedIp =
{ {0xAA,0xAA,0x03},
  {0x00,0x00,0x00},
  {0x08,0x00}
};

#define IS_VCBRIDGEDETH(p) (\
    (*((UInt16 *)p)     == 0x0000))

static const UInt8 MpoaHdr_VcEncapBridgedEth[ MPOA_VC_ENCAP_BRIDGED_ETH_SIZE] =
{ 0x00,0x00 };

/**********************************************************************************/
/* Added on April 30,2002 by Anoosh to support PPPoA                                        */
static const UInt8 MpoaHdr_LlcEncapPPPoA[ MPOA_LLC_ENCAP_PPPOA_SIZE ] = { 0xFE, 0xFE, 0x03, 0xCF };

#define IS_LLCPPPOA(p) (\
    (cpu_to_le32(*((UInt32 *)p)) == 0xCF03FEFE))
/**********************************************************************************/

static const UInt8 MacBroadcast[ ETH_ALEN ] =
{ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };

#define EQUAL_MACHDR(a1,a2) (\
    (*((UInt32 *)a1) == *((UInt32 *)a2)) && \
    (*((UInt32 *)(a1+2)) == *((UInt32 *)(a2+2))))

#define MAC_SIZE    6
#define IP_SIZE     4
#define ARP_OP_REQUEST  0x0100
#define ARP_OP_REPLY    0x0200

// Field names taken from RFC826
typedef struct ARP_PACKET
{
    uint16_t  Hrd;
    uint16_t  Pro;
    uint8_t   Hln;
    uint8_t   Pln;
    uint16_t  Op;
    uint8_t   Sha[MAC_SIZE];
    uint8_t   Spa[IP_SIZE];
    uint8_t   Tha[MAC_SIZE];
    uint8_t   Tpa[IP_SIZE];
} ARP_PACKET;


/**********************************************************************************/
/* MpoaInitialize                                  */
/* Prepares for MPOA processing by setting up the proper encapsulation          */
/**********************************************************************************/
void MpoaInitialize( eu_instance_t *ins, const eu_options_t opt)
{
    UInt16 HdrSize=0;
    UInt8 *pHdr=NULL;
    UInt32 MpoaMode = opt[CFG_ENCAPS].value;

    eu_enters (DBG_MPOA);
    

    /*Setup the encapsulation header and function pointer based on the mode specified*/
    switch (MpoaMode)
    {
        case MPOA_MODE_BRIDGED_ETH_LLC:
            eu_dbg(DBG_MPOA,"MpoaInitialize - MPOA mode set at: MPOA_MODE_BRIDGED_ETH_LLC\n");
    
            pHdr    = (UInt8 *)&MpoaHdr_LlcEncapBridgedEthNoFcs;
            HdrSize = MPOA_LLC_ENCAP_BRIDGED_ETH_SIZE;
            break;
        case MPOA_MODE_BRIDGED_ETH_VC:
            eu_dbg (DBG_MPOA,"MpoaInitialize - MPOA mode set at: MPOA_MODE_BRIDGED_ETH_VC\n");
          
            pHdr    = (UInt8 *)&MpoaHdr_VcEncapBridgedEth;
            HdrSize = MPOA_VC_ENCAP_BRIDGED_ETH_SIZE;
            break;
        case MPOA_MODE_ROUTED_IP_LLC:
            eu_dbg (DBG_MPOA,"MpoaInitialize - MPOA mode set at: MPOA_MODE_ROUTED_IP_LLC\n");
          
            pHdr    = (UInt8 *)&MpoaHdr_LlcEncapRoutedIp;
            HdrSize = MPOA_LLC_ENCAP_ROUTED_IP_SIZE;
            break;
        case MPOA_MODE_ROUTED_IP_VC:
            eu_dbg (DBG_MPOA,"MpoaInitialize - MPOA mode set at: MPOA_MODE_ROUTED_IP_VC\n");
          
            /* Since there is no ethernet header and the encapsulation is already*/
            /* a part of the VC by definition, no additional header info needs   */
            /* stuffed into packets when in this mode                   */
            pHdr    = NULL;
            HdrSize = 0;
            break;
            /**************************************************************************/
            /*The following two cases added on April30,2002 by Anoosh to support PPPoA*/
        case MPOA_MODE_PPPOA_LLC:
            eu_dbg (DBG_MPOA,"MpoaInitialize - MPOA mode set at: MPOA_MODE_PPPOA_LLC\n");
          
            pHdr    = (UInt8 *)&MpoaHdr_LlcEncapPPPoA;
            HdrSize = MPOA_LLC_ENCAP_PPPOA_SIZE;
            break;
        case MPOA_MODE_PPPOA_VC:
            eu_dbg (DBG_MPOA,"MpoaInitialize - MPOA mode set at: MPOA_MODE_PPPOA_VC\n");
          
            pHdr    = NULL;
            HdrSize = 0;
            break;
            /**********************************************************************/
    }

    /*
     * The NDIS code has a distinction between "creating" and "activating" the
     * virtual channel. That code is only necessary when dealing with
     * higher-level software that understands and manages virtual channels (like
     * NDIS 5).  Since this is the Mac ethernet driver, we know that nobody above
     * us is going to talk ATM, so we combined both functions into one,
     * UniEstablishVc.
     */
    eu_uni_establish_vc ( ins,
                          pHdr,
                          HdrSize,
                          opt[CFG_VPI].value,
                          opt[CFG_VCI].value );

    ins->MpoaMode = MpoaMode;

    /*
     * We also need to set the reassembly size that will be used. By default it
     * is set to the size of the pReassemblyBuffer, which is 1536, but as this
     * size is used to control that the received PDU is not to big, we should
     * set it up accordingly to the encapsulation type
     */
    switch (MpoaMode)
    {
        case MPOA_MODE_BRIDGED_ETH_LLC:
        case MPOA_MODE_BRIDGED_ETH_VC:
            ins->mru = 1524;
            ins->eth_hdr = 0;
            break;
            
        case MPOA_MODE_ROUTED_IP_LLC:
        case MPOA_MODE_ROUTED_IP_VC:
            /* Do not change it ... set to 1536 */
            ins->mru = 1536;
            ins->eth_hdr = 14;
            break;
            
        case MPOA_MODE_PPPOA_LLC:
            /*
             *Like VC, but LLC occupies 4 Bytes AAL5 space
             */
            ins->mru= 65535-4;	
            ins->eth_hdr = 16;
            break;

        case MPOA_MODE_PPPOA_VC:
            /*
             * PPPoA packet should hold in an Ethernet frame. Thus it is limited
             * to 1500 bytes. As we'll add 2 bytes for packet size following
             * ethernet header, the payload size is 1498.
             * 
             * What is a PPPoA packet? PPPoA is a method.
             * Okay, let's say it's a PPP packet going to be
             * transmitted or was received over an ATM link.
             * So, a PPPoA packet should hold a PPP packet, which can be 
             * 65537 Bytes. Unfortunately AAL5 can carry only 65535 Bytes.
             *     Robert
             */
            ins->mru = 65535;
            ins->eth_hdr = 16;
            break;
            
        default:
            eu_warn( "Unknown encapsulation type 0x%x\n",MpoaMode);
            break;
    }            
    
    eu_leaves (DBG_MPOA);
    
}

/**********************************************************************************/
/* MpoaProcessReceivedPdu                              */
/*                                          */
/* This routine will be called by the receive data routine by the "user"      */
/* of this driver. I thought it was best to delay as much processing so          */
/* we could exit the data receiver handler as soon as possible              */
/* Return TRUE if the packet should be dropped                                    */
/**********************************************************************************/
unsigned int MpoaProcessReceivedPdu (
                                     eu_instance_t *ins,
                                     struct sk_buff *skb,
                                     uint32_t *encaps_skip_size
                                    )
{
    unsigned int drop_packet = FALSE;            /* Do not drop this packet */
    
    struct ethhdr *phdr;
    
    eu_dbg (DBG_MPOA,"MpoaProcessReceivedPdu\n");
    
    
    switch ( ins->MpoaMode )
    {
        case MPOA_MODE_BRIDGED_ETH_LLC:
            if ( IS_LLCBRIDGEDETHNOFCS(skb->data) )
            {
                /* We've got an LLC encapsulated ethernet packet without FCS,*/
                /* since our "user" is ethernet, skip over the LLC header    */
                *encaps_skip_size = MPOA_LLC_ENCAP_BRIDGED_ETH_SIZE;
            }
            else
            {
                if ( IS_LLCBRIDGEDETHFCS(skb->data) )
                {
                    /* We've got an LLC encapsulated ethernet packet with FCS,*/
                    /* since our "user" is ethernet, skip over the LLC header */
                    *encaps_skip_size = MPOA_LLC_ENCAP_BRIDGED_ETH_SIZE;
                }
                else
                {
                    eu_dbg (DBG_MPOA,"MpoaProcessReceivedPdu: invalid LLC header\n");                
                    drop_packet  = TRUE; /* Drop it */
                }
            } 
            break;
        case MPOA_MODE_BRIDGED_ETH_VC:
            if ( IS_VCBRIDGEDETH (skb->data) )
            {
                /* We've got a VC encapsulated ethernet packet,         */    
                /* since our "user" is ethernet, skip over the VC header*/
                *encaps_skip_size = MPOA_VC_ENCAP_BRIDGED_ETH_SIZE;
            }
            else
            {
                /* The header for this Pdu did not match our stored VC header,*/
                /* so we cannot with good faith accept this Pdu           */
                eu_dbg (DBG_MPOA,"MpoaProcessReceivedPdu: invalid VC header\n");
            
                drop_packet = TRUE; /* Drop it */
            }
            break;
        case MPOA_MODE_ROUTED_IP_LLC:
            if( IS_LLCROUTEDIP (skb->data) )
            { 
                *encaps_skip_size =  MPOA_LLC_ENCAP_ROUTED_IP_SIZE ;
            }
            else
            {
                eu_dbg (DBG_MPOA,"MpoaProcessReceivedPdu: invalid LLC header\n");
            
                drop_packet = TRUE; /* Drop it */
            }
            break;
            
        case MPOA_MODE_ROUTED_IP_VC:
            *encaps_skip_size =  0;
            break;
            
        case MPOA_MODE_PPPOA_LLC:
            if ( IS_LLCPPPOA (skb->data) )
            {
                /* We've got an LLC encapsulated PPPoA packet,      */
                /* since our "user" is PPP, skip over the LLC header*/
                *encaps_skip_size = MPOA_LLC_ENCAP_PPPOA_SIZE;
            }
            else
            {
                /* We're in LLC PPPOA mode, but this packet does not have an LLC*/
                /* header, so we cannot with good faith accept this Pdu         */
                eu_dbg (DBG_MPOA,"MpoaProcessReceivedPdu: invalid LLC header\n");
            
                drop_packet = TRUE; /* Drop it */
            }
            break;
            
        case MPOA_MODE_PPPOA_VC:
            /* In PPPOA_VC mode, there is no MPOA header, so we don't do skip anything*/
            *encaps_skip_size = 0;
            break;
            
            /****************************************************************************/
        default:
            eu_err ("MpoaProcessReceivedPdu: unsupported MpoaMode\n");        
            drop_packet = TRUE; /* Drop it */
            break;
    }
    
    /**************************************************************************/
    /* This comment has been added on April 30,2002 by Anoosh to support PPPoA*/
    /* We do not do packet filtering in any of the PPP modes, therefore I need*/
    /* to add a flag to see if this isPPP then do not do packet filtering like*/
    /* MAC driver in Mpoa.c line 364                                          */
    /**************************************************************************/
    
    /****************************************************************************/
    /* We've determined any header size that we need to skip and verified that  */
    /* we have a valid header. If we determined that this Pdu should be dropped,*/
    /* we shouldn't do any more processing                    */
    /****************************************************************************/
    if ( !drop_packet )
    {
        phdr = (struct ethhdr *)(skb->data + *encaps_skip_size);
    
        /**************************************************************/
        /* Comment and added by Anoosh on April30,2002 to support     */
        /* PPPoA we do not do packet filtering in any of the PPP modes*/
        switch ( ins->MpoaMode )
        {
            case MPOA_MODE_ROUTED_IP_LLC: 
            case MPOA_MODE_ROUTED_IP_VC:
            case MPOA_MODE_PPPOA_LLC:
            case MPOA_MODE_PPPOA_VC:
                /*****************************************************************************/
                /*Comment by Anoosh on April30,2002 to support PPPoA                         */
                /*This is when we are in PPPoA mode and we do not have any packet filtering  */
                /*We do not do anything here. MAC driver do some stuff for PPPoE to make     */
                /*sure about the type of ethernet packet and session ID and if that is       */
                /*correct then it take out the extra header for PPPoE and passes to PPP stack*/
                /*****************************************************************************/
                break;
            default:
                /************************************************************************/
                /*Comment by Anoosh on April30,2002 to support PPPoA                    */
                /*This is the normal situation for LAN driver that we filter that packet*/
                /************************************************************************/
                drop_packet = MpoaFilterReceivedPacket (ins, phdr);
                if ( drop_packet )
                {
                    ins->Statistics[STAT_PAKTS_FILTERED]++;
                }
                
                break;
        }
        /**************************************************************/
    }
    
    eu_leaves (DBG_MPOA);

    return (drop_packet);
    
}

/**********************************************************************************/
/* MpoaFilterReceivedPacket                              */
/*                                          */
/* Checks the MAC header relative to our current filter mode. Returns          */
/* TRUE if this packet should be filtered (not processed).              */
/**********************************************************************************/
static Boolean MpoaFilterReceivedPacket(eu_instance_t *ins, struct ethhdr *phdr)
{
    struct net_device *ether = ins->eth;
    Boolean FilterIt = FALSE;
    
    eu_enters (DBG_MPOA);
    
    

    /* If we're in promiscuous mode - then all packets go through!*/
    if (ether->flags & IFF_PROMISC)
    {
        eu_dbg (DBG_MPOA,"IFF_PROMISC set.\n");
        
        FilterIt = FALSE;
    }
    else
    {
        /* Lets determine if this is a Directed, Broadcast, or Multicast packet*/
        if ( phdr->h_dest[0] & 0x01 )
        {
            /* This is either a Broadcast or Multicast packet*/
            if ( EQUAL_MACHDR(phdr->h_dest, MacBroadcast) )
            {
                /* This is a broadcast packet*/
                eu_dbg (DBG_MPOA,"Broadcast packet.\n");                
                FilterIt = FALSE;
            }
            else
            {
                /* This is a multicast packet*/
                if ( ether->flags & IFF_ALLMULTI )
                {
                    eu_dbg (DBG_MPOA,"IFF_ALLMULTI set.\n");
                    
                    FilterIt = FALSE;
                }
                else
                {
                    eu_dbg (DBG_MPOA,"Multicast packet.\n");
                    
                    /* Now we have to compare the DstAddress with our list*/
                    /* of acceptable multicast addresses                  */
                    if (ether->mc_count > 0)
                    {
                        int i;
                        struct dev_mc_list *mc;
            
                        /* Start out by assuming that we need to filter this*/
                        /* packet, since we will encounter a match only when*/
                        /* this is a valid packet to go through (not filter)*/
                        FilterIt = TRUE;
            
                        /* This loop works because z will evaluate to 0 or NULL*/
                        /* when z->next is NULL, which will stop the loop      */
                        for (i = 0, mc = ether->mc_list;
                             mc && i < ether->mc_count;
                             i++, mc = mc->next) 
                        {
                            if ( EQUAL_MACHDR(phdr->h_dest, mc->dmi_addr) )
                            {
                                FilterIt = FALSE;
                                break; /* We found it - get out of the loop*/
                            }
                        }
                    }
                    else
                    {
                        FilterIt = TRUE;
                    }
                }
            }
        }
        else
        {
            /* This is a directed packet*/
            if ( EQUAL_MACHDR(phdr->h_dest, ins->mac) )
            {
                eu_dbg (DBG_MPOA,"Directed packet match.\n");
                
                /* This packet is directed to us*/
                FilterIt = FALSE;
            }
            else
            {
                eu_dbg (DBG_MPOA,"Directed packet not a match.\n");
                
                /* This packet is not directed to us */
                FilterIt = TRUE;
            }
        }
    }

    eu_leaves (DBG_MPOA);
    
    return FilterIt;
}

// FFD120602 : (BUG?) Added for routed mode. It seems that there is a problem with 
// indicating the linux system that the send is complete. I had to do some calls after
// ARP call to make it working for now. Should be inverstigated later.
//---------------------------------------------------------------------------
// EthernetArpReply
//
// Simulates an ARP reply, used when in routed IP mode. Note, the logic
// for this code was taken from the Windows NDIS code.
//---------------------------------------------------------------------------
#if 0
void EthernetArpReply(eu_instance_t *ins, UInt8 *pBuf, UInt32 PacketSize)
{
    struct ethhdr *req_hdr;
    struct ethhdr *reply_hdr;
    ARP_PACKET *pArp;
    uint32_t ip_src, ip_dst;
    struct sk_buff *skb;

    eu_enters (DBG_ROUTEIP);
    

    req_hdr = (struct ethhdr *)pBuf;
    reply_hdr   = (struct ethhdr *)ins->pPacket;

    // First, copy the original ARP request into our reply packet
    memcpy ( ins->pPacket, pBuf, PacketSize );
    
    pArp  = (ARP_PACKET *)(ins->pPacket + sizeof(struct ethhdr));
    ip_src = *(uint32_t *)(pArp->Spa);
    ip_dst = *(uint32_t *)(pArp->Tpa);

    eu_dbg (DBG_ROUTEIP,"ARP Src IP: ");
    eu_dbg (DBG_ROUTEIP,"%d.%d.%d.%d", pArp->Spa[0], pArp->Spa[1],  pArp->Spa[2],  pArp->Spa[3]);
    eu_dbg (DBG_ROUTEIP," Dst IP: ");
    eu_dbg (DBG_ROUTEIP,"%d.%d.%d.%d", pArp->Tpa[0], pArp->Tpa[1],  pArp->Tpa[2],  pArp->Tpa[3]);
    eu_dbg (DBG_ROUTEIP,"\nARP IPs, UInt32 versions - Src: %x, Dst: %x\n", ip_src, ip_dst);
    
    /*
     * Gratuitous RAP request : do not reply
     */
    if ( ( ip_src == ip_dst ) || ( ip_src == 0x0000 ) )
    {
        eu_dbg (DBG_ROUTEIP,"Gratuitous ARP, no reply generated.\n");
        
        goto EthArpReplyExit;
    }
    
    /*
     * Build our ARP reply
     */
    pArp->Op = ARP_OP_REPLY;
    
    
    /*
     * Swap source and dest IP addresses
     */
    *(uint32_t *)pArp->Spa = ip_dst;
    *(uint32_t *)pArp->Tpa = ip_src;
    
    /*
     * Set the dest hardware address to the original source address
     */
    memcpy ( pArp->Tha, pArp->Sha, MAC_SIZE );
    
    /*
     *  Set source hardware address to the original dest IP address
     */
    
    *(uint32_t *)(pArp->Sha+2) = ip_dst;
    
    /*
     * Do the same in the ethernet header
     */
    memcpy ( reply_hdr->h_dest, reply_hdr->h_source, ETH_ALEN );
    
    *(uint32_t *) (reply_hdr->h_source+2) = ip_dst;
    
    /*
     * The packet is ready ... set the flag and tell OT
     */
    ins->FakePacketSize  = PacketSize;
    ins->FakePacketReady = TRUE;


    if ( !(skb = dev_alloc_skb(ETH_DATA_LEN+14+2)) )
    {
        eu_dbg (DBG_ROUTEIP,"Error allocating skb!\n");
        
        goto EthArpReplyExit;
    }
    skb->dev = ins->eth;
    eu_dbg (DBG_ROUTEIP,"EthernetArpReply: ROUTED : CALLING eth_copy_and_sum\n");
    
    
    eth_copy_and_sum(skb, ins->pPacket, PacketSize + 2 , 0);
    
    skb_put(skb, PacketSize + 2);
    skb->protocol = eth_type_trans(skb, ins->eth);
    netif_rx(skb);


  EthArpReplyExit:
    eu_leaves (DBG_ROUTEIP);
    
}
#endif /* 0 */

void EthernetArpReply(eu_instance_t *ins, UInt8 *pBuf, UInt32 PacketSize)
{
    struct ethhdr *req_hdr;
    struct ethhdr *reply_hdr;
    ARP_PACKET *pArp;
    uint32_t ip_src, ip_dst;
    struct sk_buff *skb;

    eu_enters (DBG_ROUTEIP);
    

    req_hdr = (struct ethhdr *)pBuf;

    
    pArp  = (ARP_PACKET *)(pBuf + sizeof(struct ethhdr));
    ip_src = *(uint32_t *)(pArp->Spa);
    ip_dst = *(uint32_t *)(pArp->Tpa);

    eu_dbg (DBG_ROUTEIP,"ARP Src IP: ");
    eu_dbg (DBG_ROUTEIP,"%d.%d.%d.%d", pArp->Spa[0], pArp->Spa[1],  pArp->Spa[2],  pArp->Spa[3]);
    eu_dbg (DBG_ROUTEIP," Dst IP: ");
    eu_dbg (DBG_ROUTEIP,"%d.%d.%d.%d", pArp->Tpa[0], pArp->Tpa[1],  pArp->Tpa[2],  pArp->Tpa[3]);
    eu_dbg (DBG_ROUTEIP,"\nARP IPs, UInt32 versions - Src: %x, Dst: %x\n", ip_src, ip_dst);
    
    /*
     * Gratuitous RAP request : do not reply
     */
    if ( ( ip_src == ip_dst ) || ( ip_src == 0x0000 ) )
    {
        eu_dbg (DBG_ROUTEIP,"Gratuitous ARP, no reply generated.\n");
        
        goto EthArpReplyExit;
    }

    if ( !(skb = dev_alloc_skb(ETH_DATA_LEN+14+2)) )
    {
        eu_dbg (DBG_ROUTEIP,"Error allocating skb!\n");
        
        goto EthArpReplyExit;
    }
    skb->dev = ins->eth;

    pArp  = (ARP_PACKET *)(skb->data + sizeof(struct ethhdr));

    memcpy ( skb->data, pBuf, PacketSize );
    reply_hdr   = (struct ethhdr *)skb->data ;

    /*
     * Build our ARP reply
     */
    pArp->Op = ARP_OP_REPLY;
    
    
    /*
     * Swap source and dest IP addresses
     */
    *(uint32_t *)pArp->Spa = ip_dst;
    *(uint32_t *)pArp->Tpa = ip_src;
    
    /*
     * Set the dest hardware address to the original source address
     */
    memcpy ( pArp->Tha, pArp->Sha, MAC_SIZE );
    
    /*
     *  Set source hardware address to the original dest IP address
     */
    
    *(uint32_t *)(pArp->Sha+2) = ip_dst;
    
    /*
     * Do the same in the ethernet header
     */
    memcpy ( reply_hdr->h_dest, reply_hdr->h_source, ETH_ALEN );
    
    *(uint32_t *) (reply_hdr->h_source+2) = ip_dst;

    eu_dbg (DBG_ROUTEIP,"EthernetArpReply: ROUTED : CALLING eth_copy_and_sum\n");    
    
    skb_put(skb, PacketSize + 2);
    skb->protocol = eth_type_trans(skb, ins->eth);
    netif_rx(skb);


  EthArpReplyExit:
    eu_leaves (DBG_ROUTEIP);
    
}
