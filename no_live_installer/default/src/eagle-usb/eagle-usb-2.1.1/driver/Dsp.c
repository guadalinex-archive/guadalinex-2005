/*
 *
 * Copyright (c) 2004, Frederick Ros (sl33p3r@free.fr)
 *										    
 * Dsp.c
 *
 *
 * This file is part of the eagle-usb driver package.
 *
 * eagle-usb driver package is free software; you can redistribute it
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
 * along with "ADI USB ADSL Driver for Linux"; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: Dsp.c,v 1.2 2004/06/04 18:56:19 sleeper Exp $
 */


#include "Adiutil.h"
#include "Dsp.h"
#include "macros.h"
#include "debug.h"

/****************************************************************************/
/* itoi - straight from NDIS code					    */
/*      - does nibble swapping for endian conversions			    */
/****************************************************************************/
static UInt32 itoi(UInt8 *h, UInt32 l)
{
    UInt32 num, i;
    for (num = 0, i=0; i<l ; i++)
    {
	num<<=8;
	num+=h[i];
    }
    return num;
}


/***********************************************************************************/
/* ProcessDSPCode								   */
/*										   */
/* The DSP code is in motorolla S-record format. We need to process it into	   */
/* blocks and pages so we can intelligently send it to the DSP.			   */
/***********************************************************************************/

int ProcessDSPCode(eu_instance_t *ins, UInt8 *pDSP, UInt32 IDMAStart, UInt32 DSPSize,
    IDMAPage *pMainPage, IDMAPage **ppSwapPages, UInt32 *pSwapPageCount)
{
    int retval = 0;
    UInt32 i, DSPIndex;
    UInt32 MainPageFileOffset;
    UInt32 SwapPageCount, MainBlockCount;
    IDMAPage *pPages = NULL;
    IDMABlock *pBlocks = NULL;

    eu_enters (DBG_DSP);
    
    /* First, initialize out params just in case an error occurs: */
    pMainPage->BlockCount = 0;
    pMainPage->Blocks = NULL;
    *ppSwapPages = NULL;
    *pSwapPageCount = 0;

    /* Get main page offset and swap page count from DSP header: */
    if (IDMAStart + 5 >= DSPSize)
    {
	retval = -EINVAL;
	goto end_process_dspcode;
    }
    DSPIndex = IDMAStart;
    MainPageFileOffset = itoi(pDSP + DSPIndex, 4); DSPIndex+=4;
    SwapPageCount      = itoi(pDSP + DSPIndex, 1); DSPIndex+=1;
    
    /* Page block count is stored on a single byte, and
       each page offset is stored on 4 bytes (see below): */
    if (MainPageFileOffset + 1 >= DSPSize
	||
	DSPIndex + 4 * SwapPageCount >= DSPSize)
    {
	retval = -EINVAL;
	goto end_process_dspcode;
    }
    
    /* Allocate space for our swap page array: */
    pPages = (IDMAPage *) GET_VBUFFER(sizeof(IDMAPage) * SwapPageCount);
    if (NULL == pPages)
    {
	retval = -ENOMEM;
	goto end_process_dspcode;
    }
    *ppSwapPages = pPages;
    *pSwapPageCount = SwapPageCount;

    /* Allocate memory for swap page blocks and get their file offsets: */
    for (i=0; i<SwapPageCount; i++)
    {
	pPages[i].PageOffset = itoi(pDSP + DSPIndex, 4); DSPIndex+=4;
	pPages[i].BlockCount = 0;
	pPages[i].Blocks = NULL;
	/* No file offset implies the page does not really exist: */
	if (pPages[i].PageOffset != 0)
	{
	    /* Page block count is stored on a single byte (see below): */
	    if (pPages[i].PageOffset + 1 >= DSPSize)
	    {
		retval = -EINVAL;
		continue;
	    }
	    /* Swap pages have two blocks at most: */
	    pPages[i].Blocks = (IDMABlock *) GET_VBUFFER(sizeof(IDMABlock)*2);
	    if (NULL == pPages[i].Blocks)
		retval = -ENOMEM;
	}
    }
    if (retval != 0)
	goto end_process_dspcode;

    /* Read swap pages: */
    for (i=0; i<SwapPageCount; i++)
    {
	UInt32 BlockCount;
	UInt32 j;
	/* Make sure this page actually exists before processing: */
        if (NULL == pPages[i].Blocks)
	    continue;
	/* Get the page block count: */
	DSPIndex = pPages[i].PageOffset;
	BlockCount = itoi(pDSP + DSPIndex, 1); DSPIndex+=1;
	pPages[i].BlockCount = BlockCount;
	/* Swap pages can have 2 blocs max: */
	if (0 == BlockCount || BlockCount > 2)
	{
	    pPages[i].BlockCount = 0;
	    retval = -EINVAL;
	    continue;
	}
	/* Read page blocks: */
	for (j=0; j<BlockCount; ++j)
	{
	    UInt32 Size, DataBlock;
	    pPages[i].Blocks[j].MemOffset = NULL;
	    if (DSPIndex + 6 >= DSPSize)
	    {
		retval = -EINVAL;
		continue;
	    }
	    pPages[i].Blocks[j].DSPAddr         = itoi(pDSP + DSPIndex, 2); DSPIndex+=2;
	    pPages[i].Blocks[j].DSPSize         = itoi(pDSP + DSPIndex, 2); DSPIndex+=2;
	    pPages[i].Blocks[j].DSPExtendedSize = itoi(pDSP + DSPIndex, 2); DSPIndex+=2;
	    Size = pPages[i].Blocks[j].DSPExtendedSize;
	    if (DSPIndex + Size >= DSPSize)
	    {
		retval = -EINVAL;
		continue;
	    }
	    DataBlock = pPages[i].Blocks[j].DSPAddr & 0x4000;
	    /* If this is data memory, we can leave it alone: */
	    if (DataBlock)
	    {
		pPages[i].Blocks[j].MemOffset = (UInt8 *) GET_VBUFFER(Size);
		if (pPages[i].Blocks[j].MemOffset != NULL)
		    memcpy(pPages[i].Blocks[j].MemOffset, pDSP + DSPIndex, Size);
		else
		    retval = -ENOMEM;
	    }
	    else
	    /**********************************************************************/
	    /* However, if it is program memory, we have to stuff pad bytes. Since*/
	    /* each PM word is 3 bytes, 21 "words" = 63 bytes. USB transfers are  */
	    /* broken into 64 byte chunks by the low level drivers/hardware. In   */
	    /* order to keep from sending partial words, the firmware expects each*/
	    /* 64 byte chunk to end in a pad byte - he throws it away. So, we have*/
	    /* to make a new PM buffer that has these pad bytes		      */
	    /**********************************************************************/
	    {
		/* Allocate the same number of 64byte chunks as 63byte chunks we already have: */
		UInt8 *pNewPM = (UInt8 *) GET_VBUFFER(((Size/63)+1)*64);
		pPages[i].Blocks[j].MemOffset = pNewPM;
		if (pNewPM != NULL)
		{
		    /* a = source index, b = dest index: */
		    UInt32 a, b;
		    UInt8 *pBlockCode = pDSP + DSPIndex;
		    for (a=0, b=0; a < Size; a+=3, b+=3)
		    {
			/* Are we at byte 63? (which would really be byte 64): */
			if (b && ((a%63)==0))
			{
			    pNewPM[b++] = 0xFF; /* Stuff in an extra byte */
			    pPages[i].Blocks[j].DSPExtendedSize++; /* Add it to the total length */
			}
			pNewPM[b + 0] = pBlockCode[a + 0];
			pNewPM[b + 1] = pBlockCode[a + 1];
			pNewPM[b + 2] = pBlockCode[a + 2];
		    }
		}
		else
		    retval = -ENOMEM;
	    }
	    /* Go to the next block: */
	    DSPIndex += Size;
	}
    }
    if (retval != 0)
	goto end_process_dspcode;
    
    /* Get space for main page blocks: */
    MainBlockCount = itoi(pDSP + MainPageFileOffset, 1);
    pBlocks = (IDMABlock *) GET_VBUFFER(sizeof(IDMABlock)*MainBlockCount);
    if (NULL == pBlocks)
    {
	retval = -ENOMEM;
	goto end_process_dspcode;
    }
    pMainPage->BlockCount = MainBlockCount;
    pMainPage->Blocks     = pBlocks;

    /* Read main page blocks: */
    for (i=0; i<MainBlockCount; i++)
    {
	UInt32 Size, DataBlock;
	/* Each main page block seems to be 0xC00 byte long: */
	DSPIndex = MainPageFileOffset + 1 + i*0xC00;
	if (DSPIndex + 6 >= DSPSize)
	{
	    retval = -EINVAL;
	    continue;
	}
        pBlocks[i].DSPAddr         = itoi(pDSP + DSPIndex, 2); DSPIndex+=2;
        DataBlock = pBlocks[i].DSPAddr & 0x4000;
        pBlocks[i].DSPSize         = (itoi(pDSP + DSPIndex, 2))*((DataBlock)?2:3); DSPIndex+=2;
        pBlocks[i].DSPExtendedSize = itoi(pDSP + DSPIndex, 2); DSPIndex+=2;
	Size = pBlocks[i].DSPSize;
	if (DSPIndex + Size >= DSPSize)
	{
	    retval = -EINVAL;
	    continue;
	}
        /* If this is data memory, we can leave it alone: */
        if (DataBlock)
        {
	    pBlocks[i].MemOffset = (UInt8 *) GET_VBUFFER(Size);
	    if (pBlocks[i].MemOffset != NULL)
		memcpy(pBlocks[i].MemOffset, pDSP + DSPIndex, Size);
	    else
		retval = -ENOMEM;
        }
        else
	/* Otherwise we must do some padding: */
        {
	    UInt8 *pNewPM = (UInt8 *) GET_VBUFFER(((Size/63)+1)*64);
	    pBlocks[i].MemOffset = pNewPM;
	    if (pNewPM != NULL)
	    {
		UInt32 a, b;
		UInt8 *pBlockCode = pDSP + DSPIndex;
		/***********************************************************************/
		/* If this is the "first" block (which will be the last one sent),     */
		/* we need to offset our padding by 3 bytes, 'cause we don't include   */
		/* the first 3 bytes in the full block send. The DSP starts up when    */
		/* offset 0 is written, so we save those 3 bytes until last.	   */
		/* (Yes, it really does make sense.)				   */
		/***********************************************************************/
		if (pBlocks[i].DSPAddr == 0)
		{
		    pNewPM[0] = pBlockCode[0];
		    pNewPM[1] = pBlockCode[1];
		    pNewPM[2] = pBlockCode[2];
		    pNewPM     += 3;
		    pBlockCode += 3;
		}
		/* a = source index, b = dest index: */
		for (a=0, b=0; a<Size; a+=3, b+=3)
		{
		    /* Are we at byte 63? (which would really be byte 64): */
		    if (b && ((a%63)==0))
		    {
			pNewPM[b++] = 0xFF;     /* Stuff in an extra byte */
			pBlocks[i].DSPSize++;   /* Add it to the total length */
		    }
		    pNewPM[b + 0] = pBlockCode[a + 0];
		    pNewPM[b + 1] = pBlockCode[a + 1];
		    pNewPM[b + 2] = pBlockCode[a + 2];
		}
	    }
	    else
		retval = -ENOMEM;
        }
    }

end_process_dspcode:
    eu_leaves (DBG_DSP);
    return retval;
}


/****************************************************************************/
/* FreeDspData								    */
/*									    */
/* Release memory allocated for storage of DSP code			    */
/****************************************************************************/
void FreeDspData(IDMAPage *pMainPage, IDMAPage **ppSwapPages, UInt32 *pSwapPageCount)
{
    UInt32 i;
    eu_enters (DBG_DSP);

    /* Free the main block first: */
    for (i=0; i<pMainPage->BlockCount; ++i)
	FREE_VBUFFER(pMainPage->Blocks[i].MemOffset);
    FREE_VBUFFER(pMainPage->Blocks);
    pMainPage->BlockCount = 0;
    
    /* Then free swap pages: */
    for (i=0; i<*pSwapPageCount; ++i)
    {
	UInt32 j;
	for (j=0; j<(*ppSwapPages)[i].BlockCount; ++j)
	    FREE_VBUFFER((*ppSwapPages)[i].Blocks[j].MemOffset);
	FREE_VBUFFER((*ppSwapPages)[i].Blocks);
    }
    FREE_VBUFFER(*ppSwapPages);
    *pSwapPageCount = 0;
    
    eu_leaves (DBG_DSP);
}

/***************************************************************************
$Log: Dsp.c,v $
Revision 1.2  2004/06/04 18:56:19  sleeper
Cosmetics

Revision 1.1  2004/02/06 22:01:34  sleeper
Initial creation

Revision 1.2  2003/09/14 22:09:00  sleeper
Various changes ( I know this is not a good reason :)

Revision 1.2  2003/06/03 22:51:55  sleeper
Changed ZAPS to eu_dbg/err macros

Revision 1.1.1.1  2003/02/10 23:29:49  sleeper
Imported sources

Revision 1.70  2002/05/24 22:59:33  Anoosh Naderi
Clean up the code

Revision 1.6  2002/01/16 22:59:33  steve.urquhart
Moved portion of DspFoodProcessor and invocation of BootTheModem to
eu_user ioctl handler.  Removed LoadAndPrepareDsp.  Cleaned up the ioctl
interface.  Decreased initial DSP vmalloc to less than .5MB.

Revision 1.5  2002/01/14 21:59:30  chris.edgington
Added GPL header.

Revision 1.4  2001/12/22 19:45:55  chris.edgington
Actually use the return value from DspFoodProcessor.

Revision 1.3  2001/12/20 22:35:17  chris.edgington
Replaced tabs(\t) in ZAPs with kTAB - Linux doesn't like tabs.
Replaced direct pointer access of DSP code with itoi function for endian
conversion.

Revision 1.2  2001/12/17 22:06:28  chris.edgington
Redid ZAPs to comply with Linux peculararities.

Revision 1.1.1.1  2001/12/14 21:27:11  chris.edgington
Initial import
***************************************************************************/
