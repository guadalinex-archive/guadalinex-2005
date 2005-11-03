/***************************************************************************************/
/* $Id: Sar.c,v 1.1 2004/02/06 22:01:34 sleeper Exp $   		               */
/*										       */
/* Copyright (c) 2001, Analog Devices Inc., All Rights Reserved			       */
/*										       */
/* Sar.c									       */
/*										       */
/* AAL5 Segmentation and Reassembly support routines				       */
/*										       */
/* This file is part of the "ADI USB ADSL Driver for Linux".			       */
/*										       */
/* "ADI USB ADSL Driver for Linux" is free software; you can redistribute it 	       */
/* and/or modify it under the terms of the GNU General Public License as 	       */
/* published by the Free Software Foundation; either version 2 of the License, 	       */
/* or (at your option) any later version.					       */
/*										       */
/* "ADI USB ADSL Driver for Linux" is distributed in the hope that it will be 	       */
/* useful, but WITHOUT ANY WARRANTY; without even the implied warranty of	       */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the		       */
/* GNU General Public License for more details.					       */
/*										       */
/* You should have received a copy of the GNU General Public License		       */
/* along with "ADI USB ADSL Driver for Linux"; if not, write to the Free Software      */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA	       */
/***************************************************************************************/

#include "Adiutil.h"
#include "Sar.h"
#include "eu_utils.h"
#include "debug.h"

#define CRC_INITIAL_VALUE  0xFFFFFFFF
#define ATM_AAL5_TRAILER_SIZE               8
#define ATM_AAL5_CRC_SIZE                   4


/***************************************************************************************/
/* Aal5InitSegmenter								       */
/*										       */
/* Initializes the specified Sar segmenting state machine, preparing for segmentation  */
/*										       */
/* Notes:									       */
/*       If the PDU is encapsulated, the total encapsulated length should be	       */
/*       passed in as the PduLength						       */
/***************************************************************************************/
/**
 * eu_aal5_seg_init - Initialize segmenter state machine
 */

void eu_aal5_seg_init (
                       eu_sar_s_t *psar, 
                       uint32_t    pdu_length, 
                       uint8_t   *out_buff, 
                       uint8_t    *cell_hdr
                      )
{
    eu_enters (DBG_SAR);
    

    psar->out_buff                = out_buff;
    psar->cell_hdr                = cell_hdr;
    psar->raw_pdu_length          = pdu_length;
    psar->padding_len             = 0;
    psar->bytes_left_in_curr_cell = 0;
    psar->cell_count              = 0;
    psar->running_crc             = CRC_INITIAL_VALUE;

    eu_leaves (DBG_SAR);
    
}

/**
 * eu_aal5_segment - Segment the input buffer
 *
 * Splits input data into ATM_CELL_PAYLOAD_SIZE sized packets, with an ATM cell
 * header at beginning of each packet, leading to an ATM_CELL_SIZE sized packet.
 *
 * @buff   -  Input buffer
 * @len    -   Length of input buffer
 *
 */
void eu_aal5_segment (
                      eu_sar_s_t *psar,
                      uint8_t  *buff,
                      uint32_t len
                     )
{
    uint16_t bytes_to_copy;

    eu_enters (DBG_SAR);
    

    /*
     * Lets start by first calculating the CRC for this input buffer
     */
    psar->running_crc = eu_crc_calculate ( buff, len, psar->running_crc );

    /*
     * Loop until we exhaust the bytes in the input buffer
     */
    while ( len )
    {
        if ( psar->bytes_left_in_curr_cell == 0 )
        {
          /*
           * This is the start of a new ATM cell, so copy the default cell header
           */
            eu_dbg (DBG_SAR,"src=%p, dst=%p, len=%x\n", psar->cell_hdr, 
                    psar->out_buff, ATM_CELL_HEADER_SIZE);
          
          memcpy ( psar->out_buff, psar->cell_hdr, ATM_CELL_HEADER_SIZE);
          
          psar->out_buff += ATM_CELL_HEADER_SIZE;
          
          /*
           * Now that we've actually started the cell, set the bytes left accordingly
           */
          psar->bytes_left_in_curr_cell = ATM_CELL_PAYLOAD_SIZE;
          psar->cell_count++;
       }

       /*
        * We have two possibilities, either the data left in the input buffer will 
        * be exhausted within the current cell, or it will require additional cells
        */
       bytes_to_copy = min ( psar->bytes_left_in_curr_cell, len );
       
       eu_dbg (DBG_SAR,"src=%p, dst=%p, len=%x\n", buff, 
                psar->out_buff, bytes_to_copy);
       
       memcpy ( psar->out_buff, buff, bytes_to_copy );
       
       psar->bytes_left_in_curr_cell -= bytes_to_copy;
       psar->out_buff                += bytes_to_copy;
       buff                          += bytes_to_copy;
       len                           -= bytes_to_copy;
   }

   eu_leaves (DBG_SAR);
   
}

/***************************************************************************************/
/* Aal5WritePadAndTrailer							       */
/*										       */
/* Performs cell padding when necessary and writes the CPCS PDU trailer		       */
/*										       */
/* Returns the total count of ATM cells created for the Pdu			       */
/***************************************************************************************/
uint16_t eu_aal5_write_pad_and_trailer (
                                        eu_sar_s_t *psar, 
                                        uint8_t     cpcs_uu, 
                                        uint8_t     cpi
                                       )
{
    uint16_t padding_size;
    uint8_t *pcell;
    eu_aal5_trailer_t *ptrail;
    
    eu_enters (DBG_SAR);
    

    /*
     * If we don't have room for the trailer in the current packet, then   
     * we need to pad out the rest of the current packet and start a new one
     */
    if ( psar->bytes_left_in_curr_cell < ATM_AAL5_TRAILER_SIZE )
    {
       /*
        * We might have 0 bytes left in the current cell, which would mean we
        * still need to start a new cell for the trailer, but we don't need  
        * to pad the current cell
        */
        
       if ( psar->bytes_left_in_curr_cell )
       {
           memset ( psar->out_buff, 0, psar->bytes_left_in_curr_cell );
           
          /*
           * Account for the padding in the CRC
           */
          psar->running_crc     = eu_crc_calculate ( psar->out_buff, 
                                                 psar->bytes_left_in_curr_cell, 
                                                 psar->running_crc );
          
          psar->out_buff += psar->bytes_left_in_curr_cell;
       }

       /*
        * Start the new cell that will contain padding + trailer
        */
       eu_dbg ( DBG_SAR,"src=%p, dst=%p, len=%x\n", psar->cell_hdr, 
                psar->out_buff, ATM_CELL_HEADER_SIZE);
       
       memcpy ( psar->out_buff, psar->cell_hdr, ATM_CELL_HEADER_SIZE );
       
       psar->out_buff                += ATM_CELL_HEADER_SIZE;
       psar->bytes_left_in_curr_cell  = ATM_CELL_PAYLOAD_SIZE;
       psar->cell_count ++;
       
    }
    

    /*
     * We're now ready with the cell that will contain trailer and possible padding
     */
    
    padding_size = psar->bytes_left_in_curr_cell - ATM_AAL5_TRAILER_SIZE;
    
    if ( padding_size )
    {
       memset ( psar->out_buff, 0, padding_size );
       
       /*
        * Account for the padding in the CRC
        */
       psar->running_crc              = eu_crc_calculate ( psar->out_buff, 
                                                       padding_size, 
                                                       psar->running_crc);
       psar->bytes_left_in_curr_cell -= padding_size;
       psar->out_buff                += padding_size;
       
    }

    /*
     * Set the "AAL_Indicate" bit in the Payload Type field of the current ATM cell
     */
    pcell = psar->out_buff - ( ATM_CELL_SIZE - psar->bytes_left_in_curr_cell );
    pcell[ATM_CELL_HEADER_PT_OFFSET] |= ATM_PT_AAL_INDICATE;

    /*
     * Finally, write out the CPCS PDU trailer
     */
    ptrail          = (eu_aal5_trailer_t *)(psar->out_buff);
    ptrail->cpcs_uu = cpcs_uu;
    ptrail->cpi     = cpi;
    ptrail->pdu_len = cpu_to_be16 ( psar->raw_pdu_length );

    /*
     * Finish the CRC by calculating the CRC for the trailer
     */
    psar->running_crc              = eu_crc_calculate ( psar->out_buff, 
                                                    ATM_AAL5_TRAILER_SIZE - ATM_AAL5_CRC_SIZE, 
                                                    psar->running_crc );
    ptrail->crc                    = cpu_to_be32 ( ~psar->running_crc );
    psar->out_buff                += ATM_AAL5_TRAILER_SIZE;
    psar->bytes_left_in_curr_cell -= ATM_AAL5_TRAILER_SIZE;
    
    /*
     * Actually, BytesLeftInCurrentCell had better equal 0!
     */
    if ( psar->bytes_left_in_curr_cell ) 
    {
        eu_dbg (DBG_SAR,"eu_aal5_write_pad_and_trailer finished with BytesLeftInCurrentCell != 0!\n");
    }
    
    eu_leaves (DBG_SAR);
    

    return psar->cell_count;
}

/***************************************************************************************/
/* Aal5InitReassembler								       */
/*										       */
/* Initializes the specified Sar reassembly state machine, preparing for reassembly    */
/***************************************************************************************/
void Aal5InitReassembler ( eu_sar_r_t *psar )
{
    eu_enters (DBG_SAR);
    eu_dbg (DBG_SAR,"Aal5InitReassembler: psar=%p \n", psar);
    
/*     pSar->pQueueEntry           = pBuf; */
/*     pSar->pReassemblyBuffer     = pBuf->GB.pData; */
/*     pSar->BytesLeftInReassembly = pSar->ReassemblyBufferSize = pBuf->GB.AllocedSize; */
    psar->pdu_len_from_trailer = 0;
    psar->running_crc          = CRC_INITIAL_VALUE;
    psar->cell_count           = 0;
    
    eu_leaves (DBG_SAR);
    
}

/***************************************************************************************/
/* Aal5ReassembleNonFinalCell							       */
/*										       */
/* Simply copies the payload of the cell into the reassembly buffer. Since we	       */
/* don't do anything with the cell header, the function expects the pointer	       */
/* to be pointing to the payload of the cell, not the beginning of the cell	       */
/*										       */
/* Notes:									       */
/*       Assumes there is ample space in the reassembly buffer for the cell	       */
/***************************************************************************************/
void Aal5ReassembleNonFinalCell(
                                eu_sar_r_t *psar, 
                                uint8_t     *pCellPayload
                               )
{
    eu_enters (DBG_SAR);
    

    /*
     * First, lets calculate the crc for the cell
     */
    psar->running_crc = eu_crc_calculate (
                                      pCellPayload, 
                                      ATM_CELL_PAYLOAD_SIZE, 
                                      psar->running_crc
                                     );

    /*
     * Copy the cell data into the reassembly buffer
     */
    
    memcpy(skb_put (psar->skb, ATM_CELL_PAYLOAD_SIZE),
           pCellPayload, ATM_CELL_PAYLOAD_SIZE);
    

    eu_leaves (DBG_SAR);
    
}

/***************************************************************************************/
/* Aal5ReassembleFinalCell							       */
/*										       */
/* If this final cell contains real Pdu data, copies that data into the 	       */
/* reassembly buffer. If not, determines if we copied some padding from the	       */
/* previous cell and backs up our reassembly pointer appropriately		       */
/*										       */
/* Notes:									       */
/*       Assumes there is ample space in the reassembly buffer for the cell	       */
/***************************************************************************************/
void Aal5ReassembleFinalCell(
                             eu_sar_r_t *psar, 
                             uint8_t     *pCellPayload
                            )
{
    eu_aal5_trailer_t *ptrail;
    uint16_t real_pdu_data_left, padding_size;

    eu_enters (DBG_SAR);
    

    ptrail = (eu_aal5_trailer_t *)(pCellPayload +
                                    (ATM_CELL_PAYLOAD_SIZE-ATM_AAL5_TRAILER_SIZE));
    
    /*******************************************************************************/
    /* First, lets calculate the crc for the cell. Since this is the last cell,    */
    /* it will have the stored CRC as the last dword. Including the stored CRC     */
    /* in the CRC calculation should result in the final CRC (stored in RunningCRC)*/
    /* being a known value (CRC_RESIDUE from Crc.h).				   */
    /*******************************************************************************/
    psar->running_crc = eu_crc_calculate (
                                      pCellPayload, 
                                      ATM_CELL_PAYLOAD_SIZE, 
                                      psar->running_crc
                                     );

    /*******************************************************************************/
    /* Before we do any actual data copying, we have to figure out how much of     */
    /* this cell is actual Pdu data. It will be one of the following formats:      */
    /*      data+trailer						           */
    /*      data+padding+trailer						   */
    /*      padding+trailer							   */
    /*******************************************************************************/
    psar->pdu_len_from_trailer = be16_to_cpu(ptrail->pdu_len);
    real_pdu_data_left         = psar->pdu_len_from_trailer % ATM_CELL_PAYLOAD_SIZE;

    /*******************************************************************************/
    /* Note that RealPduDataLeft will actually be the amount of real Pdu data      */
    /* in the previous cell if there wasn't enough space left in that cell for     */
    /* the trailer. That would mean this packet contains padding+trailer	   */
    /*******************************************************************************/

    if ( real_pdu_data_left > (ATM_CELL_PAYLOAD_SIZE-ATM_AAL5_TRAILER_SIZE))
    {
       /* Yep, this cell is just padding+trailer, so last cell was last real data*/
       /* We need to backup our reassembly info to account for padding we copied */
       
/*        pSar->pReassemblyBuffer     -= padding_size; */
/*        pSar->BytesLeftInReassembly += padding_size; */

        padding_size                 = ATM_CELL_PAYLOAD_SIZE - real_pdu_data_left;
        skb_trim ( psar->skb , psar->skb->len - padding_size );
    }
    else
    {
       /* if RealPduDataLeft == 0, then the last cell contained 100% Pdu data, and*/
       /* it finished out the Pdu, so we don't have to do any backtracking or     */
       /* additional copying							  */
       if ( real_pdu_data_left != 0 )
       {
          /* This cell contains some valid Pdu data to copy into the reassembly buffer*/
/*           eu_dbg (DBG_SAR,"src=%p, dst=%p, len=%x\n", pCellPayload,  */
/*                    pSar->pReassemblyBuffer, RealPduDataLeft); */
          
/*           memcpy(pSar->pReassemblyBuffer, pCellPayload, RealPduDataLeft); */
/*           pSar->BytesLeftInReassembly -= RealPduDataLeft; */
/*           pSar->pReassemblyBuffer     += RealPduDataLeft; */

          memcpy ( skb_put ( psar->skb, real_pdu_data_left ) ,
                   pCellPayload, real_pdu_data_left );          
          
       }
    }

    eu_leaves (DBG_SAR);
    
}


/****************************************************************************************
$Log: Sar.c,v $
Revision 1.1  2004/02/06 22:01:34  sleeper
Initial creation

Revision 1.2  2003/09/14 21:54:12  sleeper
Assembler/Segmenter rewrote

Revision 1.3  2003/06/03 22:42:54  sleeper
Changed ZAPS to eu_dbg/err macros

Revision 1.2  2003/04/23 22:27:03  sleeper
PPC support

Revision 1.1.1.1  2003/02/10 23:29:49  sleeper
Imported sources

Revision 1.30  2002/05/27 21:59:34  Anoosh Naderi
Clean up the code

Revision 1.2  2002/01/14 21:59:34  chris.edgington
Added GPL header.

Revision 1.1  2002/01/02 21:56:31  chris.edgington
First version - from MacOS9 project (with Linux mods to get to compile).


------------------------------------------------------------------------------
 Log from MacOS9 project
------------------------------------------------------------------------------

Revision 1.26  2001/12/05 22:09:55  chris.edgington
Added Analog Devices copyright notice.

Revision 1.25  2001/11/12 14:23:25  chris.edgington
Added some comments and did some simple reformatting in preparation for code review.

Revision 1.24  2001/10/25 19:08:23  chris.edgington
Reset cell counter when initializing reassembler.

Revision 1.23  2001/10/15 17:00:46  chris.edgington
Fixed CRC problem on outgoing packets - CRC stored in trailer had to be negated before storing!

Revision 1.22  2001/10/12 22:38:04  chris.edgington
Added more debug output.

Revision 1.21  2001/10/09 15:12:41  chris.edgington
Updated to reflect new buffer field names.

Revision 1.20  2001/10/05 16:07:28  chris.edgington
Added more debug output.
Removed endian translations on packet fields - we're already big-endian and that's what network-byte-order is.

Revision 1.19  2001/09/24 18:05:44  chris.edgington
Removed include of multiple files, added single include of Adiutil.h.

Revision 1.18  2001/09/20 21:14:17  chris.edgington
Fixed typo in one of the ZAPs.

Revision 1.17  2001/09/10 18:17:03  chris.edgington
Changed UInt16 size parameters to UInt32.

Revision 1.16  2001/09/07 22:22:48  chris.edgington
Aal5InitReassembler now gets a pointer to the full ATMUSB_BUFFER instead of just a pointer to the data buffer.

Revision 1.15  2001/09/07 19:33:46  chris.edgington
Added more fields to keep better track of reassembly process.

Revision 1.14  2001/09/06 21:23:37  chris.edgington
Made Aal5WritePadAndTrailer return the total cell count of the PDU, so the size can be calculated by the caller.

Revision 1.13  2001/09/05 20:43:35  chris.edgington
Fixed Aal5ReassembleFinalCell to include the last DWORD in the CRC calculation, to match the NDIS code, which should result in a final calculated CRC of CRC_RESIDUE.

Revision 1.12  2001/09/04 22:30:30  chris.edgington
A few cosmetic fixes.

Revision 1.11  2001/09/04 14:48:42  chris.edgington
Added Aal5InitReassembler

Revision 1.10  2001/08/31 19:34:18  chris.edgington
Fixed a few typos, file now compiles properly.

Revision 1.9  2001/08/31 15:08:39  chris.edgington
Moved data definitions, constants, macros, and prototypes to header files.

Revision 1.8  2001/08/31 14:57:18  chris.edgington
Changed memory manipulation calls to macros to make code more portable.

Revision 1.7  2001/08/30 22:37:51  chris.edgington
Changed Aal5WritePadAndTrailer to use Aal5Trailer structure instead of raw buffer manipulation.
Wrote Aal5ReassembleNonFinalCell and Aal5ReassembleFinalCell.

Revision 1.6  2001/08/30 20:27:35  chris.edgington
Changed ZAPs to fit modified API.

Revision 1.5  2001/08/30 20:02:28  chris.edgington
Renamed SarState to SarSegmenter.
Added CellCount field and code for keeping it up to date.
Some other minor reformatting and restructuring.

Revision 1.4  2001/08/30 19:18:42  chris.edgington
Small optimization to Aal5Segment. Complete implementation of Aal5WritePadAndTrailer.

Revision 1.3  2001/08/30 16:01:31  chris.edgington
Finished Aal5Segment. Added RunningCRC to SarSegmenter struct.

Revision 1.2  2001/08/29 22:16:21  chris.edgington
Wrote Aal5InitSar and part of Aal5Segment.

Revision 1.1  2001/08/29 21:23:58  chris.edgington
Initial version.
******************************************************************************************/
