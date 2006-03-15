/*************************************************************************************/
/*										     */
/* Uni.h									     */
/*										     */
/* Helper functions for User Network Interface.				     	     */
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

#ifndef __ADIUNI_H__
#define __ADIUNI_H__

#include "eagle-usb.h"

extern void eu_uni_establish_vc (
                                 eu_instance_t *ins,
                                 uint8_t *encap_hdr,
                                 uint32_t encap_size,
                                 uint32_t vpi,
                                 uint32_t vci
                                );
extern int eu_uni_process_out_pdu ( eu_instance_t *ins, uint8_t *packet, uint32_t size );
extern int eu_uni_process_in_data ( eu_instance_t *ins, uint8_t *data, uint32_t len );


#endif
