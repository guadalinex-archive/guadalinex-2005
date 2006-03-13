/*************************************************************************************/
/*										     */
/* Dsp.h									     */
/*										     */
/* Functions to initialize DSP data before sending them to device.		     */
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

#ifndef __ADIDSP_H__
#define __ADIDSP_H__

#include "eagle-usb.h"

extern int ProcessDSPCode(eu_instance_t *ins, UInt8 *pDSP, UInt32 IDMAStart, UInt32 DSPSize,
    IDMAPage *pMainPage, IDMAPage **ppSwapPages, UInt32 *pSwapPageCount);
extern void FreeDspData(IDMAPage *pMainPage,
    IDMAPage **ppSwapPages, UInt32 *pSwapPageCount);

#endif
