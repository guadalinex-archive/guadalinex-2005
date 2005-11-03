
/*
 *
 * Copyright (c) 2004, Frederick Ros (sl33p3r@free.fr)
 * Forked from ADI Linux driver from Analog Devices Inc.,
 *										 
 * eu_boot_sm.c		 
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
 * $Id: eu_boot_sm.c,v 1.4 2004/10/27 17:16:27 sleeper Exp $
 */

#include "eu_boot_sm.h"
#include "Adiutil.h"
#include "Me.h"
#include "eu_types.h"
#include "eu_utils.h"
#include "debug.h"
#include "eu_sm.h"

#define kMAILBOX_EMPTY          0
#define kMAILBOX_FULL           1

static int reboot (eu_instance_t *ins);
static int stage_1 (eu_instance_t *ins);
static int stage_2 (eu_instance_t *ins);
static int stage_3 (eu_instance_t *ins);
int boot_the_modem (eu_instance_t *ins);
static void boot_sm_advance (eu_instance_t *ins);
static int eu_upload_idma_mainp (eu_instance_t *ins);
static int eu_idma_write (eu_instance_t *ins, void *data, uint32_t size);
static int upload_p ( eu_instance_t *ins ) ;
USB_COMPLETION_PROTO (eu_idma_complete,urb,regs);


static int (*state_funcs[STATE_MAX])(eu_instance_t *) = 
{
    &boot_the_modem,
    &stage_1,
    &stage_2,
    &stage_3,
    &reboot,
    &upload_p,
};

/*
 * Structure required by the IDMA boot engine to load pages
 */
typedef struct
{
    uint16_t  ADIHeader;
    uint16_t  Addr;
    uint16_t  Size;
    uint16_t  OvlOffset;
    uint16_t  Ovl;
    uint16_t  LastBlock;
} IDMALoadBlockInfo;


/**
 * Advance boot state machine ... This is indeed a front-end
 * to boot_sm_advance
 *
 */
  
void eu_boot_sm ( void *data )
{
    eu_instance_t *ins = (eu_instance_t *)data;
    
    eu_enters (DBG_BOOT);
    
    /*
     * Check we get instance ...
     */
    if ( !ins ) 
    {
        eu_err ("Invalid instance pointer !\n");
        return;
    }

    boot_sm_advance (ins);
    
    eu_leaves (DBG_BOOT);
    
}





/*
 * Advance to the next state of the boot FSM.
 */
static void boot_sm_advance (eu_instance_t *ins) 
{
    int retcode;
    
    eu_enters (DBG_BOOT);

    /*
     * ins already checked by caller ...
     */

    if ( ins->boot_state >= STATE_MAX ) 
    {
        eu_err ("Invalid boot state value: 0x%x\n",ins->boot_state);
        retcode = -EINVAL;
        goto byebye;
    }

    retcode = (*state_funcs[ins->boot_state])(ins);


  byebye:
    
    if ( retcode != 0 ) 
    {
        reboot (ins);
    }    
    eu_leaves (DBG_BOOT);
}


int boot_the_modem (eu_instance_t *ins)
{
    int stat;

    eu_enters (DBG_BOOT);

    /*
     * To kickoff the boot process, do a custom command, SET_MODE to
     * BOOTMODE_IDMA
     */
	stat = eu_cmd_to_modem(ins, EU_CMD_SET_MODE, MODE_BOOTMODE_IDMA, 0, NULL);
	if ( stat != 0 )
	{
	    eu_err( "eu_cmd_to_modem error %x on SET_MODE BOOTMODE_IDMA\n", stat);
	    goto end_boot;
	}
	/*
     * LOOPBACK_ON correlates to ENUM_FPGALBK in the NDIS code
     */
	stat = eu_cmd_to_modem(ins, EU_CMD_SET_MODE, MODE_LOOPBACK_ON, 0, NULL);
	if ( stat != 0 )
	{
	    eu_err("eu_cmd_to_modem error %x on SET_MODE LOOPBACK_ON\n", stat);
	    goto end_boot;
	}

    stat = reboot (ins);
    
  end_boot:
    eu_leaves (DBG_BOOT);
    return stat;
}


static int stage_1 (eu_instance_t *ins) 
{
    int stat = 0;
    
    eu_enters (DBG_BOOT);
    
    stat = eu_cmd_to_modem(ins, EU_CMD_SET_MODE, MODE_START_RESET, 0, NULL);
    if ( stat != 0 )
    {
        eu_err ("eu_cmd_to_modem error %x on SET_MODE START_RESET\n", stat);
        goto end_boot;
    }

    /*
     * Advance to stage 2 and wait for 200 ms
     */
    ins->AdiModemSm.CurrentAdiState = STATE_BOOT_STAGE_2;
    ins->boot_state = STAGE_2;
    SetTimerInterval(ins, TRANS_TIME_2_BOOT_STAGE_2);

  end_boot:
    eu_leaves (DBG_BOOT);
    return stat;
}

static int stage_2 (eu_instance_t *ins) 
{
    int stat = 0;
    uint16_t word;

    eu_enters (DBG_BOOT);

    stat = eu_cmd_to_modem(ins, EU_CMD_SET_MODE, MODE_END_RESET, 0, NULL);
    if ( stat != 0 )
    {
        eu_err ("eu_cmd_to_modem error %x on SET_MODE END_RESET\n", stat);
        goto end_boot;
    }

    /*
     * Clear all of the CMV mailboxes - this is a MUST!!!  [IIDos-Oct.18,01]
     */
    word = cpu_to_le16(kMAILBOX_EMPTY);
    stat  = eu_cmd_to_modem(ins, EU_CMD_SET_2183_DATA, (DSP_MP_TX_MAILBOX | 0x4000),
                            2, (UInt8 *)&word);
    stat  = eu_cmd_to_modem(ins, EU_CMD_SET_2183_DATA, (DSP_MP_RX_MAILBOX | 0x4000),
                            2, (UInt8 *)&word);
    stat  = eu_cmd_to_modem(ins, EU_CMD_SET_2183_DATA, (DSP_SWAP_MAILBOX  | 0x4000),
                            2, (UInt8 *)&word);

    ins->AdiModemSm.CurrentAdiState = STATE_BOOT_STAGE_3;
    ins->boot_state = STAGE_2;
    SetTimerInterval(ins, TRANS_TIME_2_BOOT_STAGE_3);

  end_boot:
    eu_leaves (DBG_BOOT);
    return stat;
}


static int stage_3 (eu_instance_t *ins) 
{
    int stat = 0;

    eu_enters (DBG_BOOT);
    
    /*
     * Finally, we get to do something that seems substantial!
     */
    stat = eu_upload_idma_mainp (ins);
    
    if ( stat == -ENODEV ) 
    {
        /*
         * Wow .. no device ... Do not continue ...
         */
        ins->AdiModemSm.CurrentAdiState = STATE_STALLED_FOREVER;

        /*
         * If we do not set stat to 0, reboot will be called, and rebooting a
         * non present modem is stupid ;)
         */
        stat = 0;
        goto byebye;
    }
    

    /*
     * This is the last operation in our sequence of tasks for device
     * initialization. Everything else will be driven by the hardware.
     * Once the main page is loaded, the hardware will start requesting
     * swap pages.
     * Put our modem in the BOOT_WAIT state and start our watchdog!
     */
    ins->AdiModemSm.CurrentAdiState = STATE_BOOT_WAIT;
    ins->boot_state = PRE_BOOT;
    SetTimerInterval(ins, TRANS_TIME_4_BOOTWAIT);

  byebye:
    eu_leaves (DBG_BOOT);
    
    return stat;
}

static int reboot (eu_instance_t *ins) 
{
    int stat = 0;

    eu_enters (DBG_BOOT);

    /*
     * Original code comments states:
     *
     *    The steps in the boot process are not documented, they were "gleaned"
     *    from the NDIS code. A different, simpler process might be possible,
     *    but we have no way to prove it at this point.
     *
     */

    
    stat = eu_cmd_to_modem(ins, EU_CMD_SET_MODE, MODE_BOOTMODE_IDMA, 0, NULL);
    if ( stat != 0 )
    {
        eu_err("eu_cmd_to_modem error %x on SET_MODE BOOTMODE_IDMA\n", stat);
        goto end_boot;
    }
    
    ins->AdiModemSm.CurrentAdiState = STATE_BOOT_STAGE_1;
    ins->boot_state = STAGE_1;
    ins->AdiModemSm.ModemReplyExpected = EVENT_TIMER_TICK;
    SetTimerInterval(ins, TRANS_TIME_2_BOOT_STAGE_1);
    
  end_boot:
    eu_leaves (DBG_BOOT);
    return stat;
       
}

static int upload_p ( eu_instance_t *ins ) 
{
    int retcode = 0;
    
    eu_enters (DBG_BOOT);

    if ( EU_TEST_FLAG( ins, EU_DSP_IPG ) ) 
    {
        eu_report ("DSP is being uploaded. I give up.");
        retcode = -1;
        goto byebye;
    }
    
    eu_upload_idma_swapp ( ins, ins->swap_data );
    
  byebye:    
    eu_leaves (DBG_BOOT);
    return (retcode);
}

/**
 * eu_upload_idma_mainp
 *
 * Comments from orignla function: "
 * Send the main page block of the DSP code through the IDMA interface. Note
 * that the bulk of this logic is straight from legacy Buffer.c in NDIS code,
 * there was virtually no doc on how this is supposed to work. "
 *
 * return: 0 if everything is OK, negated error code otherwise...
 */
static int eu_upload_idma_mainp (eu_instance_t *ins)
{
    int stat;
    uint32_t i, myBlockSize;
    uint8_t *pLastBits = NULL;
    uint8_t *pBlockData;
    IDMALoadBlockInfo myBLI;

    eu_enters (DBG_BOOT);

    eu_dbg (DBG_BOOT,"Loading 0x%#x blocks from main page.\n",
            ins->MainPage.BlockCount);
    

    for (i=0; i<ins->MainPage.BlockCount; i++)
    {
        /*
         * Init data that is not block-location-specific
         */
        myBLI.ADIHeader = cpu_to_le16(0xABCD);
        myBLI.Ovl       = 0;
        myBLI.OvlOffset = cpu_to_le16(0x8000);
        myBLI.LastBlock = 0;

        /*
         * Is this NOT the first block?
         */
        if (ins->MainPage.Blocks[i].DSPAddr != 0)
        {
            myBLI.Size  = cpu_to_le16((UInt16)(ins->MainPage.Blocks[i].DSPSize));
            myBLI.Addr  = cpu_to_le16(ins->MainPage.Blocks[i].DSPAddr);
            myBlockSize = (UInt16)(ins->MainPage.Blocks[i].DSPSize);
            pBlockData  = ins->MainPage.Blocks[i].MemOffset;
        }
        /*
         * This is block 0 - we want to send all but offset 0 - so we start at
         * offset 1
         */
        
        /*
         * We should only get here once!
         */
        else
        {
            myBLI.Size  = cpu_to_le16((UInt16)(ins->MainPage.Blocks[i].DSPSize - 3));
            myBLI.Addr  = cpu_to_le16(1);
            myBlockSize = (UInt16)(ins->MainPage.Blocks[i].DSPSize - 3);
            pBlockData  = ins->MainPage.Blocks[i].MemOffset+3;
            pLastBits   = ins->MainPage.Blocks[i].MemOffset;
        }

        /*
         * Write the structure telling the DSP what to expect in the next block
         */
        stat = eu_idma_write(ins, (void *)&myBLI, sizeof(IDMALoadBlockInfo));
        
        if ( stat != 0 ) 
        {
            eu_err ("Error %#X on IDMA write of BlockLoadInfo\n", stat);
            if (stat == -ENODEV) 
            {
                eu_err ("Wow .. this is a \"no device\" one. I give up.\n");
                return -ENODEV;
            }
        }
        
        /*
         * Occassionally the modem will freak out during idma boot and get in
         * a weird mode where it requests wrong swap pages, etc.
         * Comparing the usb traces of linux and Windows reveals that linux
         * does things MUCH faster, so my theory is that we're overwhelming
         * the hardware. So, I'll inject a tiny delay here to keep things
         * under control
         */
        
        wait_ms(1); 

        /* Now write the actual block data
         */
        stat = eu_idma_write(ins, (void *)pBlockData, myBlockSize);

        if ( stat != 0 ) 
        {
            eu_err ("Error %#X on IDMA write of block data\n", stat);
            if (stat == -ENODEV) 
            {
                eu_err ("Wow .. this is a \"no device\" one. I give up.\n");
                return -ENODEV;
            }
            
        }
        

        wait_ms(1);
    }

    /*
     * All blocks are loaded, now we can write out offset 0 of the first block,
     * which will tell the hardware to start - resulting in requests for swap
     * pages
     */
    myBLI.ADIHeader = cpu_to_le16(0xABCD);
    myBLI.Ovl       = 0;
    myBLI.OvlOffset = cpu_to_le16(0x8000);
    myBLI.LastBlock = cpu_to_le16(1);
    myBLI.Size      = cpu_to_le16(3);
    myBLI.Addr      = 0;
    myBlockSize     = 3;
    pBlockData      = pLastBits;
    eu_dbg (DBG_BOOT,"sending last entry from main page, DSP will start soon!.\n");
    

    /*
     * Write the structure telling the DSP what to expect in the next block
     */
    stat = eu_idma_write(ins, (void *)&myBLI, sizeof(IDMALoadBlockInfo));
    
    if ( stat != 0 )
    {
        eu_err ("Error %#X on IDMA write of BlockLoadInfo\n", stat);
        if (stat == -ENODEV) 
        {
            eu_err ("Wow .. this is a \"no device\" one. I give up.\n");
            return -ENODEV;
        }
    }
    
    
    /*
     * Now write the actual block data
     */
    stat = eu_idma_write(ins, (void *)pBlockData, myBlockSize);
    
    if ( stat != 0 ) 
    {
        eu_err ("Error %#X on IDMA write of block data\n", stat);
        if (stat == -ENODEV) 
        {
            eu_err ("Wow .. this is a \"no device\" one. I give up.\n");
            return -ENODEV;
        }

    }
    eu_leaves (DBG_BOOT);

    return(0);
}


/**
 * eu_upload_idma_swapp
 *
 * Sends the blocks of the requested swap page through the IDMA interface.  Note
 * that the bulk of this logic is straight from Buffer.c in the NDIS code, there
 * was virtually no documentation on how this is supposed to work.
 *
 */
void eu_upload_idma_swapp(eu_instance_t *ins, uint16_t swap_info)
{
    int stat;
    uint32_t i, myBlockSize;
    uint16_t PageIndex, Ovl;
    IDMALoadBlockInfo myBLI;

    eu_enters (DBG_BOOT);

    PageIndex = (swap_info & 0x00FF) - 1;
    Ovl       = ((swap_info & 0xF000) >> 12) | ((swap_info & 0x0F00) >> 4);

    eu_dbg (DBG_BOOT,"Loading 0x%x blocks from swap page 0x%x, ovl=0x%x.\n", 
            ins->pSwapPages[PageIndex].BlockCount, PageIndex, Ovl);
    
    if (PageIndex > ins->SwapPageCount)
    {
        eu_err ("Request for swap page 0x%x when only 0x%x pages exist!\n",
                PageIndex, ins->SwapPageCount);       
        return;
    }
    
    /*
     * For swap pages, we actually know we won't have more than 2 blocks
     */
    for (i=0; i<ins->pSwapPages[PageIndex].BlockCount; i++)
    {
        /*
         * Init data that is not block-location-specific
         */
        myBLI.ADIHeader = cpu_to_le16(0xABCD);
        myBLI.Ovl       = cpu_to_le16(Ovl);
        myBLI.OvlOffset = cpu_to_le16(0x8000 | Ovl);
        myBlockSize     = ins->pSwapPages[PageIndex].Blocks[i].DSPExtendedSize;
        myBLI.Size      = cpu_to_le16((UInt16)myBlockSize);
        myBLI.Addr      = cpu_to_le16(ins->pSwapPages[PageIndex].Blocks[i].DSPAddr);

        if ((ins->pSwapPages[PageIndex].BlockCount-1) == i)
        {
            myBLI.LastBlock = cpu_to_le16(1);
        }
        else
        {
            myBLI.LastBlock = cpu_to_le16(0);
        }
        

        /*
         * Write the structure telling the DSP what to expect in the next block
         */
        stat = eu_idma_write(ins, (void *)&myBLI, sizeof(IDMALoadBlockInfo));
        if ( stat != 0 )
        {
            eu_err ("Error %#X on IDMA write of BlockLoadInfo\n", stat);
            if (stat == -ENODEV) 
            {
                eu_err ("Wow .. this is a \"no device\" one. I give up.\n");
                return;
            }            
        }
        

        /*
         * Occassionally the modem will freak out during idma boot and get in a
         * weird mode where it requests wrong swap pages, etc.  Comparing the
         * usb traces of linux and Windows reveals that linux does things MUCH
         * faster, so my theory is that we're overwhelming the hardware. So,
         * I'll inject a tiny delay here to keep things under control
         */
        
        wait_ms(1); 

        /*
         * Now write the actual block data
         */
        stat = eu_idma_write(ins, (void *)(ins->pSwapPages[PageIndex].Blocks[i].MemOffset), myBlockSize);
        if ( stat != 0 )
        {
            eu_err ("Error %#X on IDMA write of block data\n", stat);
            if (stat == -ENODEV) 
            {
                eu_err ("Wow .. this is a \"no device\" one. I give up.\n");
                return;
            }
        }
        
        wait_ms(1);
    }

    eu_leaves (DBG_BOOT);
}


/**
 * eu_idma_write
 *
 * Responsible for sending data to the IDMA interface via the BULK IDMA OUT
 * pipe.
 *
 */
 static int eu_idma_write (eu_instance_t *ins, void *data, uint32_t size)
{
    int ret = 0;
    struct urb *urb;
    uint8_t *xfer_buf = GET_KBUFFER(size);

    eu_enters (DBG_BOOT);

    if (!xfer_buf)
    {
       eu_err ("kmalloc in eu_idma_write for %#X bytes failed\n", size);
       
       ret = -ENOMEM;
       goto IDMA_exit;
    }

    memcpy(xfer_buf, data, size);

    /*
     * Get an URB and prepare it for submission
     */
    urb = USB_ALLOC_URB ( 0 , GFP_ATOMIC );

    usb_fill_bulk_urb ( urb, ins->usbdev, ins->pipe_bulk_idma_out,
                        xfer_buf, size, eu_idma_complete, ins );
    
    urb->transfer_flags |= USB_QUEUE_BULK;

    eu_dbg (DBG_BOOT,"Submitting BULK URB %p\n", urb);
    
    ret = USB_SUBMIT_URB ( urb, GFP_ATOMIC );

IDMA_exit:
    eu_leaves (DBG_BOOT);
    return ret;
}

/**
 * eu_idma_complete
 *
 * Called when an eu_idma_write urb has completed. All we have to do is free
 * the memory allocated in eu_idma_write.
 *
 */

USB_COMPLETION_PROTO (eu_idma_complete,urb,regs)
{
    eu_enters (DBG_BOOT);

    /*
     * All we really need to do is free the memory we alloced and free the urb
     */
    FREE_KBUFFER(urb->transfer_buffer);
    usb_free_urb(urb);

    eu_dbg (DBG_BOOT,"Freed BULK URB %p\n", urb);
    
    eu_leaves (DBG_BOOT);
}
