/*************************************************************************************/
/*										     */
/* Sar.h									     */
/*										     */
/* Helper functions for AAL5 segmentation and reassembly.		     	     */
/*										     */
/* This file is part of the "ADI USB ADSL Driver for Linux".			     */
/*										     */
/* "ADI USB ADSL Driver for Linux" is free software; you can redistribute it 	     */
/* and/or modify it under the terms of the GNU General Public License as 	     */
/* published by the Free Software Foundation; either version 2 of the License,       */
/* or (at your option) any later version.					     */
/*										     */
/* "ADI USB ADSL Driver for Linux" is distributed in the hope that it will be 	     */
/* useful, but WITHOUT ANY WARRANTY; without even the implied warranty of	     */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the		     */
/* GNU General Public License for more details.					     */
/*										     */
/* You should have received a copy of the GNU General Public License		     */
/* along with "ADI USB ADSL Driver for Linux"; if not, write to the Free Software    */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA	     */
/*************************************************************************************/

#ifndef __ADISAR_H__
#define __ADISAR_H__

#include "eagle-usb.h"

#define ATM_CELL_PAYLOAD_SIZE               48
#define ATM_PT_AAL_INDICATE                 2
#define ATM_CELL_REMOVE_GFC_MASK         	0x00FFFFFF
#define ATM_CELL_HEADER_PT_OFFSET           3

#define CRC_RESIDUE        0xC704DD7B

extern void eu_aal5_seg_init (
                              eu_sar_s_t *psar, 
                              uint32_t    pdu_length, 
                              uint8_t    *out_buff, 
                              uint8_t    *cell_hdr
                             );

extern void eu_aal5_segment (
                             eu_sar_s_t *psar,
                             uint8_t  *buff,
                             uint32_t len
                            );

extern void Aal5InitReassembler ( eu_sar_r_t *psar );
extern uint16_t eu_aal5_write_pad_and_trailer (
                                        eu_sar_s_t *psar, 
                                        uint8_t     cpcs_uu, 
                                        uint8_t         cpi
                                       );

extern void Aal5ReassembleFinalCell(
                                    eu_sar_r_t *psar, 
                                    uint8_t     *pCellPayload
                                   );
extern void Aal5ReassembleNonFinalCell(
                                       eu_sar_r_t *psar, 
                                       uint8_t     *pCellPayload
                                      );
#endif
