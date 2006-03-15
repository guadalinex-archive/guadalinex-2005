/***********************************************************************************/
/* $Id: Uni.c,v 1.2 2004/06/30 20:32:02 sleeper Exp $ 			           */
/*										   */
/* Copyright (c) 2001, Analog Devices Inc., All Rights Reserved			   */
/*										   */
/* Uni.c									   */
/*										   */
/* UNI (User Network Interface) support module (similar to AtmUni.c from NDIS)     */
/*										   */
/* This file is part of the "ADI USB ADSL Driver for Linux".			   */
/*										   */
/* "ADI USB ADSL Driver for Linux" is free software; you can redistribute it       */
/* and/or modify it under the terms of the GNU General Public License as           */
/* published by the Free Software Foundation; either version 2 of the License,     */
/* or (at your option) any later version.					   */
/*										   */
/* "ADI USB ADSL Driver for Linux" is distributed in the hope that it will be      */
/* useful, but WITHOUT ANY WARRANTY; without even the implied warranty of	   */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the		   */
/* GNU General Public License for more details.					   */
/*										   */
/* You should have received a copy of the GNU General Public License		   */
/* along with "ADI USB ADSL Driver for Linux"; if not, write to the Free Software  */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA       */
/***********************************************************************************/

#include "Adiutil.h"
#include "Uni.h"
#include "Pipes.h"
#include "Sar.h"
#include "debug.h"

extern Boolean CheckForAndProcessOAMCell(eu_instance_t *ins, UInt8 *pCell);

/**********************************************************************************/
/* IsPTICell									  */
/**********************************************************************************/
#define ATM_PTI_OAM_CELL	0x04
/* static int IsPTICell(UInt32 header)  */
/* { */
/*     return (((header >> 1) & ATM_PTI_OAM_CELL) == ATM_PTI_OAM_CELL); */
/* } */
#define IS_PTI_CELL(h)  ((((h) >> 1) & ATM_PTI_OAM_CELL) == ATM_PTI_OAM_CELL)

/*********************************************************************************/
/* This will hold network packet data that is waiting to be sent back to         */
/* the higher-level stack. Two queues of these will be maintained - a free       */
/* queue and a waiting queue							 */
/*********************************************************************************/
#define ETHERNET_PACKET_Q_SIZE    64
#define ETHERNET_PACKET_DATA_SIZE 1536


/***********************************************************************************/
/* UniEstablishVc								   */
/*										   */
/* Allocates memory for and initializes our Vc (virtual channel, virtual circuit)  */
/* for this session. Real ATM allows multiple Vc's per Uni, but it seems that      */
/* there are no circumstances at this point that will require more than one        */
/* Vc in this driver. So, we don't do all the extra logic of managing multiple     */
/* Vc's.									   */
/***********************************************************************************/
void eu_uni_establish_vc (
                          eu_instance_t *ins,
                          uint8_t *encap_hdr,
                          uint32_t encap_size,
                          uint32_t vpi,
                          uint32_t vci
                         )
{
    eu_enters (DBG_UNI);
    
    
    /*
     * Initialize the various fields of the VC
     */
    ins->Vc.encaps_hdr     = encap_hdr;
    ins->Vc.encaps_size    = encap_size;
    ins->Vc.vpi            = vpi;
    ins->Vc.vci            = vci;
    ins->Vc.vpi_vci        = (ins->Vc.vpi<<16) | ins->Vc.vci;
    ins->Vc.max_sdu_size   = (OUTGOING_DATA_SIZE / ATM_CELL_SIZE) * ATM_CELL_PAYLOAD_SIZE;
    ins->Vc.reassembly_ipg = FALSE;

    /*
     * NDIS4 code always sets these to 0
     */
    ins->Vc.cpcs_uu = 0;
    ins->Vc.cpi     = 0;

    /*
     * This came straight from the NDIS4 code. Build Cell Header
     */

    ins->Vc.cell_hdr[0] = (uint8_t)( ( 0 << 4) | ((ins->Vc.vpi & 0xF0) >> 4));
    ins->Vc.cell_hdr[1] = (uint8_t)( (ins->Vc.vpi << 4) | ((ins->Vc.vci & 0xF000) >> 12));
    ins->Vc.cell_hdr[2] = (uint8_t)( (ins->Vc.vci & 0x0FF0) >> 4);
    ins->Vc.cell_hdr[3] = (uint8_t)( (ins->Vc.vci & 0x000F) << 4);
    ins->Vc.cell_hdr[4] = 0;
    
    eu_leaves (DBG_UNI);
    
    return;
}

/***********************************************************************************/
/* eu_uni_process_out_pdu							   */
/*										   */
/* Takes an ethernet or raw IP packet and turns it into ATM data. Modeled after    */
/* AtmUniSend in the NDIS code. Where we differ from the NDIS code, comments       */
/* explain why. One major difference is that we will get an entire packet in one   */
/* buffer. The NDIS code gets multiple buffers that compose the entire packet.     */
/* Also, this routine doesn't actually send the data - it just processes the	   */
/* input data into and output buffer.						   */
/*										   */
/* This routine assumes that all parameters have been verified (it doesn't	   */
/* check for NULL pointers or zero lengths).					   */
/*										   */
/* Assumes the following fields in the Vc are initialized:			   */
/*      EncapSize, pEncapHdr							   */
/*      MaxSduSize								   */
/*      CellHeader								   */
/*      CpcsUu, Cpi								   */
/***********************************************************************************/
int eu_uni_process_out_pdu ( eu_instance_t *ins, uint8_t *packet, uint32_t size )
{
    uint32_t cells_in_output;

    eu_enters (DBG_UNI);
    

    /* The NDIS code checks to make sure the VC is ACTIVE. However, since*/
    /* we don't deal with ACTIVE or INACTIVE VC's, we don't need to worry*/
    /* about that check							 */

    /* Make sure that the packet + encapsulation is within the max SDU size*/
    if ((size + ins->Vc.encaps_size) > ins->Vc.max_sdu_size)
    {
        eu_dbg (DBG_UNI,"eu_uni_process_out_pdu: Packet+Encap size > MaxSduSize\n");
        
	ins->Statistics[STAT_PAKTS_LOST_OSIZE]++;
	return -EMSGSIZE;
    }

    /* Prepare our segmentation state machine for segmentation by first grabbing*/
    /* a segmentation buffer, then initializing the state machine	        */

    eu_aal5_seg_init ( &ins->Vc.segmenter,
                       size + ins->Vc.encaps_size,
                       ins->segmentation_buffer,
                       &ins->Vc.cell_hdr[0] );

    /*
     * If there is some RFC2684 encapsulation enabled, put that in the output first
     */
    if ( ins->Vc.encaps_size ) 
    {
        eu_aal5_segment ( &ins->Vc.segmenter,
                          ins->Vc.encaps_hdr,
                          ins->Vc.encaps_size
                        );
    }

    /*
     * Now segment the real network packet into ATM cells
     */
    eu_aal5_segment ( &ins->Vc.segmenter, packet, size );

    /*
     * Finish the CPCS PDU processing by writing the trailer and any padding
     */
    cells_in_output = eu_aal5_write_pad_and_trailer ( &ins->Vc.segmenter,
                                                      ins->Vc.cpcs_uu,
                                                      ins->Vc.cpi );
    ins->Statistics[STAT_CELLS_TX] += cells_in_output;
    ins->Statistics[STAT_PAKTS_TX] ++;
    
    /*
     * Size of output buffer is ATM cell count * size of ATM cell
     */
    ins->Vc.segmenter.size = cells_in_output * ATM_CELL_SIZE;
    
    eu_dbg ( DBG_UNI,
             "eu_uni_process_out_pdu: packet processed into ATM cells "
             "- ready to be sent!\n" );
    

    eu_leaves (DBG_UNI);
    
    return 0; /* success in Linux*/
}


/***********************************************************************************/
/* eu_uni_process_in_data							   */
/*										   */
/* Processes the ATM data, reassembling into a CPCS PDU (which is really just	   */
/* an ethernet packet with the CPCS trailer). Once an entire Pdu has been 	   */
/* received, adds the completed ethernet packet buffer to the ReadWaitingQ 	   */
/* so the caller (the upper-level network stack components) can get the packet.    */
/***********************************************************************************/
int eu_uni_process_in_data ( eu_instance_t *ins, uint8_t *data, uint32_t len ) 
{
    
    uint32_t cell_hdr, vpi_vci;
    uint32_t is_last_cell;
    eu_atm_vc_t *pvc;
    eu_sar_r_t *psar;

    eu_enters (DBG_UNI);
    eu_dbg (DBG_UNI,"eu_uni_process_in_data: data=%p, len=%x\n",data, len);
    

    /*
     * Lets first make sure there is an integral number of ATM cells in the buffer
     */
    if ( (len % ATM_CELL_SIZE) != 0 )
    {
        eu_err ("eu_uni_process_in_data: non-integral number of cells in incoming data.\n");
        eu_err ("eu_uni_process_in_data: incoming data length = %x.\n", len);
        eu_err ("eu_uni_process_in_data: ATM_CELL_SIZE = %x.\n", ATM_CELL_SIZE);
        
	return -EMSGSIZE;
    }

    /*
     * Process each ATM cell in the input buffer
     */
    while (len)
    {
	/*
         * The first DWORD of the ATM cell contains all but the HEC byte of the
         *  cell's ATM header - but we don't look at the HEC byte anyway
         */
	cell_hdr = be32_to_cpu(*((uint32_t *)data));
	is_last_cell = (uint32_t)(cell_hdr & ATM_PT_AAL_INDICATE);
	vpi_vci     = (cell_hdr >> 4) & ATM_CELL_REMOVE_GFC_MASK;

        if ( ins->Vc.vpi_vci == vpi_vci ) 
        {
            pvc = &ins->Vc;
        }
        else
        {
            /*
             * No need to warn user : just emit a dbg statement
             */
            eu_dbg (DBG_UNI,"Received VPI/VCI (0x%x) does not match ours (0x%x)\n",
                     ins->Vc.vpi_vci, vpi_vci );
            pvc = NULL;
        }
        
	
        if ( CheckForAndProcessOAMCell (ins, data) )
        {
            /*
             * This cell was an OAM cell, so we don't want to include it in 
             * the reassembly. Therefore, skip the cell and continue on ...
             */
            eu_dbg (DBG_UNI,"OAM cell received and processed.\n");
            
            len  -= ATM_CELL_SIZE;
            data += ATM_CELL_SIZE;
            ins->Statistics[STAT_CELLS_OAM_RCVD]++;
            continue;
        }

	/*
         * If we cannot find an open VC for this cell, drop the cell
         */
	if ( NULL == pvc )
	{
            eu_dbg (DBG_UNI,"eu_uni_process_in_data: no VC for cell, VpiVci=%x\n", vpi_vci);
            
	    ins->Statistics[STAT_CELLS_LOST_VPIVCI]++;

            /*
             * Next cell
             */
	    len  -= ATM_CELL_SIZE;
	    data += ATM_CELL_SIZE;
	    continue; 
	}

	if ( IS_PTI_CELL (cell_hdr) )
	{
            eu_dbg (DBG_UNI,"eu_uni_process_in_data: PTI cell received\n");

            /*
             * Next cell
             */
	    len  -= ATM_CELL_SIZE;
	    data += ATM_CELL_SIZE;
	    continue; 
	}

	/*
         * NDIS code now checks to make sure the VC is in the Active state .
	 * Our VC is a PVC that is always active, so we don't do this right now
         * Go ahead and skip over the header for this cell
         */
	len  -= ATM_CELL_HEADER_SIZE;
	data += ATM_CELL_HEADER_SIZE;
	
        psar = &pvc->reassembler;
         

        /*
         * If there is not already a reassembly happening on this VC, get the   
         * reassembly structure initialized so we can get started with this cell
         */
	if ( !pvc->reassembly_ipg )
	{
            
            /*
             * Check if a previously allocated skb is here. If so reset it to
             * correct values, otherwise get a new one
             */
            if ( psar->skb ) 
            {
                psar->skb->data = psar->skb->head;                
                psar->skb->tail = psar->skb->head;
                psar->skb->len = 0;

                /*
                 * Reserve space for an Ethernet header
                 */
                skb_reserve ( psar->skb, ins->eth_hdr );
                
            }
            else
            {
                
                /*
                 * Allocate a new sk_buff
                 */
                psar->skb = dev_alloc_skb ( ETHERNET_PACKET_DATA_SIZE +
                                           ins->eth_hdr);
                if ( !psar->skb ) 
                {
                    eu_err ("eu_uni_process_in_data: Not enough memory.\n");
                    
                    /*
                     * Next Cell
                     */
                    len  -= ATM_CELL_PAYLOAD_SIZE;
                    data += ATM_CELL_PAYLOAD_SIZE;
                    continue;
                }

                /*
                 * Associate to ethernet device
                 */
                psar->skb->dev = ins->eth;

                /*
                 * Reserve space for an Ethernet header
                 */
                skb_reserve ( psar->skb, ins->eth_hdr );
                
            }
            
            Aal5InitReassembler (psar);
                
            pvc->reassembly_ipg = TRUE;
	}

        /*
         * If we do not have sufficient space in reassembly buffer to hold another
         * cell paylod, drop the ENTIRE pdu being reassembled.
         * (This can happen if the last cell of a PDU is lost, causing 2 PDUs to get
         * concatenated )
         */
        if ( skb_tailroom (psar->skb) < ATM_CELL_PAYLOAD_SIZE ) 
        {
            eu_dbg (DBG_UNI,
                    "eu_uni_process_in_data: no space left in buffer for cell, dropping PDU,\n");
            
	    pvc->reassembly_ipg = FALSE;
	    ins->Statistics[STAT_CELLS_LOST_OTHER] += psar->cell_count;

            /*
             * Next cell
             */
	    len  -= ATM_CELL_PAYLOAD_SIZE;
	    data += ATM_CELL_PAYLOAD_SIZE;
	    continue;
	}

        /*
         * We can now saefly re-assemble :)
         */
        
	if ( !is_last_cell )
	{
	    Aal5ReassembleNonFinalCell(psar, data);
	}
	else
	{
	    Aal5ReassembleFinalCell(psar, data);
	}
	
	psar->cell_count++;

	/*
         * Next cell
         */
	len  -= ATM_CELL_PAYLOAD_SIZE;
	data += ATM_CELL_PAYLOAD_SIZE;

	/* If this cell was the last cell in the PDU, do PDU processing*/
	if ( is_last_cell )
	{
            

            /*
             * We can now check whether or not the message is too big
             */
            if ( psar->skb->len > ins->mru ) 
            {
                /* Discard message */
                eu_warn("Discarding message (pdu %u > mru %u)\n",
                         psar->skb->len,ins->mru);
                
                ins->Statistics[STAT_CELLS_LOST_OTHER] += psar->cell_count;
                pvc->reassembly_ipg = FALSE;
                continue;
            }
            

            /**************************************************************************/
	    /* The way the CRC is calculated should result in the final calculated CRC*/
	    /* of the incoming PDU being a known value (because the CRC stored in the */
	    /* trailer of the PDU is used in the calculation), so we can verify the   */
	    /* CRC by just comparing the end result with the fixed value (CRC_RESIDUE)*/
	    /**************************************************************************/
	    
	    if ( psar->running_crc != CRC_RESIDUE )
	    {
                eu_dbg ( DBG_UNI, "eu_uni_process_in_data: CRC error, dropping PDU,\n");
                
		pvc->reassembly_ipg = FALSE;
		ins->Statistics[STAT_CELLS_LOST_CRC]++;
	    }
	    else
	    {
		/*
                 * CRC is ok, lets check the packet length
                 */
		if ( psar->skb->len != psar->pdu_len_from_trailer)
		{
                    eu_dbg (DBG_UNI,"eu_uni_process_in_data: PDU length error, "
                             "dropping PDU, Calculated = %x, stored = %x\n",
                             psar->skb->len,
                             psar->pdu_len_from_trailer);
                    
                        
		    pvc->reassembly_ipg = FALSE;
		    ins->Statistics[STAT_PAKTS_LOST_OSIZE]++;
		}

		/* If we get here and ReassemblyInProgress is still true, then we can*/
		/* go ahead and mark this buffer as being ready to receive.          */
		if ( pvc->reassembly_ipg )
		{
		    /***********************************************************/
		    /* There might be some MPOA (RFC1483/2684) processing that */
		    /* needs to be done, but we can wait until the higher level*/
		    /* code actually asks us for this packet. Reset skip size  */
		    /* for now to make sure its at a known value	       */
		    /***********************************************************/
		    
		    ins->Statistics[STAT_CELLS_RX] += psar->cell_count;
                    ins->Statistics[STAT_PAKTS_RX] ++;
                    eu_dbg (DBG_UNI,"eu_uni_process_in_data: Pdu processed successfully,"
                             " calling eu_read_to_uplayers,\n");

		    pvc->reassembly_ipg = FALSE;
                    
		    eu_read_to_uplayers ( ins, psar->skb );
                    
                    psar->skb = NULL;
                    
		}
	    }
	}
    }

    eu_leaves (DBG_UNI);
    
    return 0; /* linux success*/
}

/******************************************************************************************************
$Log: Uni.c,v $
Revision 1.2  2004/06/30 20:32:02  sleeper
Implemented new boot state machine

Revision 1.1  2004/02/06 22:01:34  sleeper
Initial creation

Revision 1.4  2004/01/09 20:32:32  sleeper
Re-wrote integrality test in bulk mode

Revision 1.3  2003/12/01 22:07:45  sleeper
Fix a bug that will overwrite end of skb when dropping cells

Revision 1.2  2003/09/14 22:09:00  sleeper
Various changes ( I know this is not a good reason :)

Revision 1.7  2003/06/03 22:51:55  sleeper
Changed ZAPS to eu_dbg/err macros

Revision 1.6  2003/04/08 23:10:53  sleeper
Add mru

Revision 1.5  2003/04/23 22:28:20  sleeper
Add stats

Revision 1.4  2003/03/31 22:03:37  sleeper
Fix a comment

Revision 1.3  2003/03/21 00:22:41  sleeper
Msg Initialization modifs from 2.0.1

Revision 1.2  2003/03/11 00:38:03  sleeper
Add OAM modifs as present in Sagemn 2.0.1 driver

Revision 1.1.1.1  2003/02/10 23:29:49  sleeper
Imported sources

Revision 1.60  2002/05/27 18:04:42  Anoosh Naderi
Clean up the code

Revision 1.5  2002/01/18 18:04:42  chris.edgington
Pull VPI and VCI from driver options if they exist.

Revision 1.4  2002/01/14 21:59:34  chris.edgington
Added GPL header.

Revision 1.3  2002/01/08 16:02:43  chris.edgington
Added some necessary comments.

Revision 1.2  2002/01/04 20:59:02  chris.edgington
Changed data processing routines to use static buffers from eu_instance_t struct
instead of pulling buffers from queues (since we're not using queues on Linux).
Added call to ReadSendItUp when read to send data up the network stack.

Revision 1.1  2002/01/02 21:56:31  chris.edgington
First version - from MacOS9 project (with Linux mods to get to compile).


---------------------------------------------------------------------------
 Log from MacOS9 project
---------------------------------------------------------------------------

Revision 1.23 2002/07/24 11:32:39  Chinda Keodouangsy
Added support to handle PTI/OAM cells

Revision 1.22  2001/12/05 22:09:55  chris.edgington
Added Analog Devices copyright notice.

Revision 1.21  2001/11/12 14:23:25  chris.edgington
Added some comments and did some simple reformatting in preparation for code review.

Revision 1.20  2001/10/30 19:39:24  chris.edgington
Added error messages to USBExpertStatusLevel in appropriate places.

Revision 1.19  2001/10/29 23:53:14  martyn.deobald
Modified UniEstablishVC to use values read from config file

Revision 1.18  2001/10/26 18:22:45  chris.edgington
Changed CRC loss counter to packets instead of cells.

Revision 1.17  2001/10/25 19:09:33  chris.edgington
Added incs of packet and cell counters.

Revision 1.16  2001/10/17 21:08:46  chris.edgington
Removed ZAP that was reporting incorrect message.

Revision 1.15  2001/10/12 22:35:13  chris.edgington
Re-enabled read CRC checking.

Revision 1.14  2001/10/11 19:49:44  chris.edgington
Removed a DANGER that is no longer necessary.

Revision 1.13  2001/10/09 15:17:25  chris.edgington
Removed UniCreate, UniCreateQueues, UniDestroyQueues, UniEmptyQueues - no longer needed.
Updated to reflect new buffer field names.
Uni code now pulls buffers from buffer queues in eu_instance_t.

Revision 1.12  2001/10/05 16:09:37  chris.edgington
Temporarily disabled CRC verification of incoming packets.
Changed nils to NULLs.
Added more debug output.
Removed some endian translations - we're already big-endian and that's what the network expects.

Revision 1.11  2001/10/04 15:54:21  chris.edgington
Added call to notify TCPIP stack that there is network data available. 
(Currently HACKed - find better fix when network traffic is working.)

Revision 1.10  2001/10/01 18:41:30  chris.edgington
Save the size of the reassembled PDU in the PDU holder buffer's RealDataSize field.

Revision 1.9  2001/09/28 15:22:07  chris.edgington
Since ATMUNI Uni is part of eu_instance_t, we don't have to allocate space for it in UniCreate.
We should probably change the name of the function to UniInitialize.

Revision 1.8  2001/09/24 18:05:51  chris.edgington
Removed include of multiple files, added single include of Adiutil.h.

Revision 1.7  2001/09/11 15:35:53  chris.edgington
Added separate free queues for send and receive.
Added missing initialization to UniCreate and UniEstablishVc.

Revision 1.6  2001/09/10 18:16:00  chris.edgington
Moved header stuff into header files.
Fixed a few problems found in compile.

Revision 1.5  2001/09/07 22:25:36  chris.edgington
Wrote queue GET and PUT macros to make the code more portable.
Fixed a few problems found during code reading (typos, etc.).

Revision 1.4  2001/09/07 19:39:52  chris.edgington
Wrote UniCreate, UniCreateQueues, UniEmptyQueue, UniDestroyQueues.
Finished to 95% UniProcessInboundData.
Added ATMUNI and ATMUSB_BUFFER structs.
Not compilable - work in progress.

Revision 1.3  2001/09/06 22:30:42  chris.edgington
Changed UniProcessPdu to UniProcessOutboundPdu.
Started UniProcessInboundData.

Revision 1.2  2001/09/06 21:25:09  chris.edgington
Wrote UniProcessPdu.

Revision 1.1  2001/09/05 22:26:21  chris.edgington
Initial version.
*************************************************************************************************/
