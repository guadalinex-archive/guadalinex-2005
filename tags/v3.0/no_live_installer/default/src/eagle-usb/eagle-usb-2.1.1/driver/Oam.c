
/* $Id: Oam.c,v 1.4 2004/09/12 20:00:46 sleeper Exp $ */

//---------------------------------------------------------------------------
//
// Copyright (c) 2002, Analog Devices Inc., All Rights Reserved
//
// Oam.c
//
// Operations And Maintenance ATM support module
//
// Some abbreviations ...
//
//  CC  = Continuity Check
// AIS  = Alarm Indication Signal
// RDI  = Remote Defect Indication 
//
//---------------------------------------------------------------------------

#include "Adiutil.h"
#include "eagle-usb.h"
#include "Oam.h"
#include "Sar.h"
#include "macros.h"
#include "eu_sm.h"
#include "debug.h"

//---------------------------------------------------------------------------
// Some sample OAM cells from Dennis Chan ...
//---------------------------------------------------------------------------

// OAM F5 E2E cell:
// 00 00 03 2A 00 18 01 00 00 00 01 FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF 6A 6A 6A 6A 6A 6A 6A 6A 03 93
//
// OAM F5 Segment cell:
// 00 00 03 28 00 18 01 00 00 00 02 FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF 6A 6A 6A 6A 6A 6A 6A 6A 01 23
//
// OAM F4 E2E cell:
// 00 00 00 40 00 18 01 00 00 00 03 FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF 6A 6A 6A 6A 6A 6A 6A 6A 00 B3
//
// OAM F4 Segment cell:
// 00 00 00 30 00 18 01 00 00 00 04 FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF 6A 6A 6A 6A 6A 6A 6A 6A 02 70
//
// UInt8 OAM01[ATM_CELL_SIZE] = {
//    0x00, 0x00, 0x03, 0x2A, 0x00,
//    0x18, 0x01, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF,
//    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
//    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
//    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
//    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x6A, 0x6A,
//    0x6A, 0x6A, 0x6A, 0x6A, 0x6A, 0x6A, 0x00, 0x00 };

//---------------------------------------------------------------------------
// Local function prototypes
//---------------------------------------------------------------------------

Boolean CheckForAndProcessOAMCell(eu_instance_t *ins, UInt8 *pCell);
static Boolean CheckForAndProcessOAMCCChange(eu_instance_t *ins, UInt8 *pCell);
static Boolean CheckForAndProcessOAMResponse(eu_instance_t *ins, UInt8 *pCell, UInt32 CellType);
//static Boolean CheckForAndProcessOAMCC(eu_instance_t *ins, UInt8 *pCell);
static UInt16 ByteCalcCRC10(UInt8 *pData, UInt32 Size);
static void USBSendAmoeba(eu_instance_t *ins, UInt8 *pCell, Boolean MoreThanOneCell);
#ifdef LINUX_2_4
static void AmoebaCompletion(struct urb *urb);

#else
static void AmoebaCompletion(struct urb *urb, struct pt_regs *regs );    
#endif    
static void ArmOAMTimer(eu_instance_t *ins);
int SetOAMTimerInterval(eu_instance_t* ins, int reqDelay);
void StopOAMTimer(eu_instance_t *ins);

// Add queuing feature for USBBulkWrite - Shahriar
// Tag : QUEUE
void WriteToUSB (void);

//---------------------------------------------------------------------------
// CheckForAndProcessOAMCell
//
// If the provided data is an OAM cell, process the cell appropriately and
// returns TRUE. Otherwise, returns FALSE.
//
// Much of the logic is modeled after the Windows driver OAM.c code.
//---------------------------------------------------------------------------

Boolean CheckForAndProcessOAMCell(eu_instance_t *ins, UInt8 *pCell)
{
    UInt32 OAMType = OAM_NONE;
    UInt32 PTI, VPI, VCI, VpiVci,CellHeader;

    eu_enters (DBG_OAM);

	CellHeader = be32_to_cpu(*((UInt32 *)pCell)); // FFD102902
	VpiVci     = (CellHeader >> 4) & ATM_CELL_REMOVE_GFC_MASK; // FFD102902

    PTI = (pCell[ATM_CELL_HEADER_PT_OFFSET] & 0x0E) >> 1;
    VPI = (VpiVci & 0x00FF0000) >> 16;
    VCI = (VpiVci & 0x0000FFFF);

    eu_dbg (DBG_OAM,"OAM cell: VPI=%#X, VCI=%#X, PTI=%#X\n", VPI, VCI, PTI);
    

// was : FFD102902
    // Check for F5 segment cell
    if ( (PTI == 0x04) &&
         (VCI != 0x00) && (VCI != 0x03) &&
         (VCI != 0x04) && (VCI != 0x06) && (VCI != 0x07) )
    {
        eu_dbg (DBG_OAM,"OAM F5 segment cell received.\n");
        
        OAMType = OAM_F5_SEGMENT;
	/*************************/
	// this is the place I test the timer FFDFFD 
 	// ArmOAMTimer(ins); 
	/**************************/
    }

    // Check for F5 end-to-end cell
    if ( (PTI == 0x05) &&
         (VCI != 0x00) && (VCI != 0x03) &&
         (VCI != 0x04) && (VCI != 0x06) && (VCI != 0x07) )
    {
        eu_dbg (DBG_OAM,"OAM F5 end-to-end cell received.\n");
        
        OAMType = OAM_F5_ENDTOEND;
    }

    // Check for F4 segment cell
    if (VCI == 0x03)
    {
        eu_dbg (DBG_OAM,"OAM F4 segment cell received.\n");
        
        OAMType = OAM_F4_SEGMENT;
    }

    // And finally, check for F4 end-to-end cell
    if (VCI == 0x04)
    {
        eu_dbg (DBG_OAM,"OAM F4 end-to-end cell received.\n");
        
        OAMType = OAM_F4_ENDTOEND;
    }

// END FFD102902

    // If this is not an OAM cell, we're done
    if (OAMType == OAM_NONE)
        goto OAM_Done;

    eu_dbg (DBG_OAM,"OAM cell: VPI=%#X, VCI=%#X, PTI=%#X\n", VPI, VCI, PTI);
    

    // Check for and process CC activate and deactivate
    if (!CheckForAndProcessOAMCCChange(ins, pCell))
    {
        // This is not a CC activate or deactivate cell, see if this is 
        // an OAM cell that needs a response (loopback, AIS, RDI)
        if (!CheckForAndProcessOAMResponse(ins, pCell, OAMType))
        {
			/* FFD : Shahriar suggestion: removed .....
            // This is not a cell that needed a response, so its either
            // a CC cell or a response to a loopback request that originated
            // with us (which currently is not possible).
            if (!CheckForAndProcessOAMCC(ins, pCell))
            {
                // Must be a loopback response ... should never get them
                // because we currently provide no mechanism for requesting!
                ZAP(DEBUG_OAM, DEBUG_ATTN, (kTAB"OAM loopback indicator = 0!\n"));
            }
			*/
        }
    }

OAM_Done:
    eu_leaves (DBG_OAM);
    memcpy(ins->pOAMCell,pCell,ATM_CELL_SIZE);    // ffd nov26 DC : now we use pOAMCell
    memcpy(ins->OAMCellHeader,pCell,ATM_CELL_HEADER_SIZE);

    if (OAMType == OAM_NONE)
        return FALSE;
    else
        return TRUE;
}


//---------------------------------------------------------------------------
// CheckForAndProcessOAMCCChange
//---------------------------------------------------------------------------

static Boolean CheckForAndProcessOAMCCChange(eu_instance_t *ins, UInt8 *pCell)
{
    Boolean Retval = FALSE;
    eu_enters (DBG_OAM);

    if (pCell[OAM_TYPE_AND_FN] == OAM_ACTIVATION_CC)
    {
        Retval = TRUE;

        switch (pCell[OAM_MSGID_AND_DIR] & OAM_MASK_MSGID)
        {
            // As far as I know, we should only receive requests. The
            // confirm and denied messages only go to those endpoints
            // who actually originate CC activation changes - we don't.
            case OAM_ACTIVATE_REQUEST:
            {
                UInt8 OAM_Dir;

                eu_dbg (DBG_OAM,"OAM CC activate request\n");
                

                OAM_Dir = pCell[OAM_MSGID_AND_DIR] & OAM_MASK_DIR;
                if (OAM_Dir & OAM_MASK_DIR_AB)
                {
                    eu_dbg (DBG_OAM,"AB direction.\n");
                    
                    ins->OAMState_SendingCC = TRUE;

                }

                if (OAM_Dir & OAM_MASK_DIR_BA)
                {
                    eu_dbg (DBG_OAM,"BA direction.\n");
                    
                    ins->OAMState_ReceiveCC = TRUE;
                }

                // If OAM_Dir is non-zero, then it is valid
                if (OAM_Dir)
                {
                    eu_dbg (DBG_OAM,"Preparing and sending OAM CC activate confirm.\n");
                    
                    // We have no real use for this cell after we're done
                    // here, so we'll just modify it in place and use it to send
                    // back the activate confirmation. Much of the cell format is
                    // the same as the request cell, so we don't have to change much.
                    pCell[OAM_MSGID_AND_DIR] = OAM_Dir | OAM_ACTIVATE_CONFIRMED;
                    
                    // Store a copy of the cell's header for later use
                    memcpy( ins->OAMCellHeader,pCell, ATM_CELL_HEADER_SIZE);
                    
                    CALC_AND_STORE_CRC10(pCell);

					USBSendAmoeba(ins, pCell, FALSE);

                    // We need to be timing the time between receives or sends
                    // of CC cells, so start the timer if its not already ticking.
                    if (ins->OAMState_TimerOn == FALSE)
                    {
                        ArmOAMTimer(ins);
                    }
                }
                else
                {
                    eu_err ( "CC activation with no direction.\n" );
                    
                }
            } break;

            case OAM_DEACTIVATE_REQUEST:
            {
                UInt8 OAM_Dir;

                eu_dbg (DBG_OAM,"OAM CC deactivate request.\n");
                

                OAM_Dir = pCell[OAM_MSGID_AND_DIR] & OAM_MASK_DIR;
                if (OAM_Dir & OAM_MASK_DIR_AB)
                {
                    eu_dbg (DBG_OAM,"AB direction.\n");
                    
                    ins->OAMState_SendingCC = FALSE;
                }

                if (OAM_Dir & OAM_MASK_DIR_BA)
                {
                    eu_dbg (DBG_OAM,"BA direction.\n");
                    
                    ins->OAMState_ReceiveCC = FALSE;
                    // When BA direction is deactivated, AIS state needs cleared
                    ins->OAMState_AIS       = FALSE;
                    // Reset our received CC cell counter
                    ins->OAMState_CCSink    = 0;
                }

                if (OAM_Dir)
                {
                    eu_dbg (DBG_OAM,"Preparing and sending OAM CC deactivate confirm.\n");
                    

                    // If sending and receiving are now deactivated and the timer
                    // is currently running, shut it off.
                    if ((ins->OAMState_TimerOn   == TRUE) &&
                        (ins->OAMState_SendingCC == FALSE) &&
                        (ins->OAMState_ReceiveCC == FALSE))
                    {
                        StopOAMTimer(ins);
                    }

                    // We have no real use for this cell after we're done
                    // here, so we'll just modify it in place and use it to send
                    // back the activate confirmation. Much of the cell format is
                    // the same as the request cell, so we don't have to change much.
                    pCell[OAM_MSGID_AND_DIR] = OAM_Dir | OAM_DEACTIVATE_CONFIRMED;
                    
                    CALC_AND_STORE_CRC10(pCell);

					USBSendAmoeba(ins, pCell, FALSE);
                }
                else
                {
                    eu_err ("CC deactivation with no direction.\n");
                    
                }

            } break;

            case OAM_ACTIVATE_CONFIRMED:
            case OAM_ACTIVATE_REQUEST_DENIED:
            case OAM_DEACTIVATE_CONFIRMED:
            case OAM_DEACTIVATE_REQUEST_DENIED:
            {
                eu_dbg (DBG_OAM,"Warning - OAM activation change response received.\n");
                
                goto CCChange_Done;
            } break;
        }
    }

CCChange_Done: 	
    eu_leaves (DBG_OAM);
    return Retval;
}


//---------------------------------------------------------------------------
// CheckForAndProcessOAMResponse
//
// Loopback, AIS, and RDI OAM F5 cells need to have a response sent back.
//---------------------------------------------------------------------------

static Boolean CheckForAndProcessOAMResponse(eu_instance_t *ins, UInt8 *pCell, UInt32 CellType)
{
    Boolean Retval = FALSE;
    eu_enters (DBG_OAM);

    // First, make sure the cell type is F5 (segment or end-to-end), as thats
    // a requirement for all of these OAM types.
// To enable F4 reply - Shahriar
// Tag : ECIOAMCC
/*
    if ((CellType != OAM_F5_SEGMENT) &&
        (CellType != OAM_F5_ENDTOEND))
    {
        ZAP(DEBUG_OAM, DEBUG_INFO, (kTAB"Not an F5 cell.\n"));
        goto Response_Done;
    }
*/

    switch (pCell[OAM_TYPE_AND_FN])
    {
        case OAM_LOOPBACK:
        {
            if (pCell[OAM_LOOPBACK_INDICATOR] & OAM_LOOPBACK_REPLY_MASK)
            {
                eu_dbg (DBG_OAM,"Loopback reply request received.\n");
                
                
                // Mark this cell as a loopback response
                pCell[OAM_LOOPBACK_INDICATOR] = 0;

                // Tell lower code to send response cell
                Retval = TRUE;
            }
            else
            {
                // We should never get here, 'cause we have no current mechanism
                // for originating a loopback reply request.
                eu_dbg (DBG_OAM,"Loopback reply received.\n");
                
            }
        } break;

        case OAM_AIS:
        {
            eu_dbg (DBG_OAM,"AIS received.\n");
            

            // Per ADI Windows code, ASI gets an RDI response
            pCell[OAM_TYPE_AND_FN] = OAM_RDI;

            // Tell lower code to send response cell
            Retval = TRUE;
        } break;
        
        case OAM_RDI:
        {
            // Per ADI Windows code, RDI gets a CC response
            pCell[OAM_TYPE_AND_FN] = OAM_CC;
        
            // Tell lower code to send response cell
            Retval = TRUE;
        } break;
        
		// To enable CC reply - Shahriar
		// Tag : ECIOAMCC
		//////////////////
        case OAM_CC:
        {
            eu_dbg (DBG_OAM,"CC received.\n");
            
			// Improve OAM CC - Shahriar
			// Tag: OAMCC 
			////////////////
			if ( ins->OAMState_ReceiveCC == TRUE ) 
			{
			            ins->OAMState_CCSink = 0;
			}
			else
			{
			            // Per ADI Windows code, CC gets an CC response
			            pCell[OAM_TYPE_AND_FN] = OAM_CC;

			            // Tell lower code to send response cell
			            Retval = TRUE;
			}
			////////////////
        } break;

        default:
        {
        } break;
		//////////////////
    }

    // This will be TRUE if a cell needs to be sent
    if (Retval == TRUE)
    {
        CALC_AND_STORE_CRC10(pCell);
		USBSendAmoeba(ins, pCell, FALSE);
		
		//pCell[OAM_LOOPBACK_INDICATOR] = 1;
		//USBSendAmoeba(ins, pCell, FALSE);   // extra ffdffdffd=========================================

    }
    
// Response_Done:
    eu_leaves (DBG_OAM);
    return Retval;
}


//---------------------------------------------------------------------------
// CheckForAndProcessOAMCC
//---------------------------------------------------------------------------
/*
static Boolean CheckForAndProcessOAMCC(eu_instance_t *ins, UInt8 *pCell)
{
    Boolean Retval = FALSE;
    eu_enters (DBG_OAM);

    if (pCell[OAM_TYPE_AND_FN] == OAM_CC)
    {
        ZAP(DEBUG_OAM, DEBUG_INFO, (kTAB"CC received.\n"));

        // Reset CC receive timeout counter (counts seconds between CC cells)
        ins->OAMState_CCSink = 0;

        // Clear AIS state
        ins->OAMState_AIS = FALSE;

        Retval = TRUE;
    }

    eu_leaves (DBG_OAM);
    return Retval;
}
*/
//---------------------------------------------------------------------------
// ByteCalcCRC10
//
// Taken pretty much verbatim from ADI Windows driver source
//---------------------------------------------------------------------------

#define POLYNOMIAL 0x633

static UInt16 ByteCalcCRC10(UInt8 *pData, UInt32 Size)
{
    UInt32 i;
    UInt16 crc10_accum = 0;
    static Boolean AlreadyDone = FALSE;
    static UInt16 byte_crc10_table[256];

    // Generate byte values for use, needs to be done once
    if (!AlreadyDone) 
    {
        UInt32 k, j;
        UInt16 Gen_crc10_accum;

        for (k = 0;  k < 256;  k++)
        {
            Gen_crc10_accum = ((UInt16) k << 2);
            for (j = 0;  j < 8;  j++)
            {
                if ((Gen_crc10_accum <<= 1) & 0x400) 
                    Gen_crc10_accum ^= POLYNOMIAL;
            }
            byte_crc10_table[k] = Gen_crc10_accum;
        }   

        AlreadyDone = TRUE;   
    }   
    
    for (i = 0;  i < Size;  i++)
    {
        crc10_accum = ((crc10_accum << 8) & 0x3ff) ^ byte_crc10_table[( crc10_accum >> 2) & 0xff]
                                                   ^ *pData++;
    }

    return crc10_accum;
}

//---------------------------------------------------------------------------
// USBSendAmoeba
//
// An amoeba is a single-celled organism. We're sending a single ATM cell :)
// One potential problem is the fact that we're using a single PB for all
// OAM related sends. If there is ever a time where we need to send a new
// cell and the previous write has not completed, I don't know what will happen.
// However, it seems that the likelihood of that happening are virtually nil.
//---------------------------------------------------------------------------

// FFDTEST		TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT
static void USBSendAmoeba(eu_instance_t *ins, UInt8 *pCell, Boolean MoreThanOneCell)
{

    int result;
    int cntr;	
    UInt32         uiTotalBytes = 0;
    UInt8 MyCell[ATM_CELL_SIZE*2];

    eu_enters (DBG_OAM);
    eu_dbg (DBG_OAM,"******* FFD >>>> ins = %#lx\n", (unsigned long)ins);

   if ( MoreThanOneCell == TRUE )
	uiTotalBytes = ATM_CELL_SIZE*2;
   else
	uiTotalBytes = ATM_CELL_SIZE;

    memcpy(MyCell,pCell,uiTotalBytes);	

    for (cntr=0;cntr<uiTotalBytes;cntr++)
        eu_dbg (DBG_OAM," %#x ", MyCell[cntr]);

        eu_dbg (DBG_OAM,"\n *** OAM **** Writing %#x bytes to USB\n", uiTotalBytes);


    FILL_BULK_URB ( ins->urb_oam_write, ins->usbdev,
                    ins->pipe_bulk_data_out,
                    pCell,uiTotalBytes , AmoebaCompletion, ins);

    result = USB_SUBMIT_URB ( ins->urb_oam_write , GFP_ATOMIC );
    
    eu_dbg (DBG_OAM,"usb_submit_urb result=%d \n",result);
    eu_leaves (DBG_OAM);


}

//---------------------------------------------------------------------------
// AmoebaCompletion
//---------------------------------------------------------------------------
#ifdef LINUX_2_4
static void AmoebaCompletion(struct urb *urb)
#else
static void AmoebaCompletion(struct urb *urb, struct pt_regs *regs )
#endif    
{

    eu_instance_t *ins = (eu_instance_t *) urb->context;
//    struct net_device *ether;


    eu_enters (DBG_OAM);

    if ((ins == NULL) || ( !EU_TEST_FLAG(ins, EU_OPEN ) ) )
    {
        eu_err("NULL urb->context or Open=FALSE in AmoebaCompletion!\n");
       goto WriteCompletion_exit;
    }
/***************************************************************************************
    ether = ins->eth;

    if (!netif_device_present(ether))
    {
       ZAP(DEBUG_OAM, DEBUG_ERRORS, (kTAB"Network interface no longer present!\n"));
       goto WriteCompletion_exit;
    }

    if (urb->status)
       ZAP(DEBUG_OAM, DEBUG_ERRORS, (kTAB"urb has status of %x\n", urb->status));

    ether->trans_start = jiffies;
    netif_wake_queue(ether);
***************************************************************************************/

WriteCompletion_exit:
    eu_leaves (DBG_OAM);

}
// END FFDTEST	------------------------------------------------------------------------



//---------------------------------------------------------------------------
// ArmOAMTimer
//---------------------------------------------------------------------------

static void ArmOAMTimer(eu_instance_t *ins)
{
   eu_enters (DBG_OAM);
   ins->OAMState_TimerOn = TRUE;
   SetOAMTimerInterval(ins,OAM_TIMER_VALUE);
   eu_leaves (DBG_OAM);
}

//---------------------------------------------------------------------------
// StopOAMTimer
//---------------------------------------------------------------------------

void StopOAMTimer(eu_instance_t *ins)
{

    eu_enters (DBG_OAM);

    ins->OAMState_TimerOn = FALSE;

    del_timer(&ins->OAMTimer);

    eu_leaves (DBG_OAM);
}

//---------------------------------------------------------------------------
// OAMTimer
//
// Called every 1 second.
//---------------------------------------------------------------------------

void OAMTimerFunction(unsigned long ptr)
{
    eu_instance_t *ins = (eu_instance_t *)ptr;

    UInt8 *pCell;
    
	eu_enters (DBG_OAM);


    if (!(ins->AdiModemSm.CurrentAdiState & STATE_OPERATIONAL))
    {
        eu_dbg (DBG_OAM," ***FFD*** Called while not at OPER state.\n");
        
        goto OAMTimer_Done;
    }

    //pCell = (UInt8 *)&ins->OAMCell;
   pCell = (UInt8 *)ins->pOAMCell;


// TESTTESETESTTESTTESTESTTESTTESETESTTESTTESTESTTESTTESETESTTESTTESTESTTESTTESETESTTESTTESTEST
/***************************** I send a dummy OAM here ..... 
    if (true)
    {
        ZAP(DEBUG_OAM, DEBUG_INFO, (kTAB"====>>>>>FFD>>>>>>>>>>>>>>>> Sending an OAM CELL TEST TEST...\n"));
        //COPY_MEM(ins->OAMCell, pCell, ATM_CELL_SIZE);
        //pCell[OAM_TYPE_AND_FN] = OAM_CC;
	pCell[OAM_LOOPBACK_INDICATOR]=1;
        CALC_AND_STORE_CRC10(pCell);
	//	if (ins->OAMState_ReceiveCC == FALSE)
			USBSendAmoeba(ins, pCell, FALSE);
    }
*****************************************************************************************/
// TESTTESETESTTESTTESTESTTESTTESETESTTESTTESTESTTESTTESETESTTESTTESTESTTESTTESETESTTESTTESTEST

    // If CC sending is enabled, then we need to send a CC
    if (ins->OAMState_SendingCC == TRUE)
    {
        eu_dbg (DBG_OAM,"====>>>>> Sending CC...\n");
        

	// FFD - DC bug with a ?
        memcpy( pCell, ins->OAMCellHeader, ATM_CELL_HEADER_SIZE);

        pCell[OAM_TYPE_AND_FN] = OAM_CC;

        CALC_AND_STORE_CRC10(pCell);

		// Improve OAM CC - Shahriar
		// Tag: OAMCC 
		if (ins->OAMState_ReceiveCC == FALSE)
			USBSendAmoeba(ins, pCell, FALSE);
    }

    // If CC receive is enabled, inc counter and send RDI if necessary
    if (ins->OAMState_ReceiveCC == TRUE)
    {
        if (++ins->OAMState_CCSink > 4)
        {
            eu_dbg (DBG_OAM,"Sending RDI...\n");
            
            // Turn on AIS state
            ins->OAMState_AIS = TRUE;
            
            // Keep our sink timer counter from rolling over
            ins->OAMState_CCSink = 5;
    
			// Improve OAM CC - Shahriar ////////////////////////////////////
			// Tag: OAMCC 
			// was :
            // COPY_MEM(ins->OAMCellHeader, pCell, ATM_CELL_HEADER_SIZE);
    
            // pCell[OAM_TYPE_AND_FN] = OAM_RDI;
    
            // CALC_AND_STORE_CRC10(pCell);

			// USBSendAmoeba(ins, pCell);
			//
			//////////////////////////////////////////////////////////////////
			if (ins->OAMState_SendingCC == TRUE)
			{				            
			            memcpy( (pCell + ATM_CELL_SIZE),ins->OAMCellHeader, ATM_CELL_HEADER_SIZE);
			    
			            pCell [ATM_CELL_SIZE + OAM_TYPE_AND_FN] = OAM_RDI;
			    
			            CALC_AND_STORE_CRC10((pCell + ATM_CELL_SIZE));
						USBSendAmoeba(ins, pCell, TRUE);

			}
			else
			{
			            memcpy( pCell,ins->OAMCellHeader, ATM_CELL_HEADER_SIZE);
			    
			            pCell[OAM_TYPE_AND_FN] = OAM_RDI;
			    
			            CALC_AND_STORE_CRC10(pCell);

						USBSendAmoeba(ins, pCell, FALSE);
			}
			//////////////////
        }
		// Improve OAM CC - Shahriar
		// Tag: OAMCC 
		////////////////
        else
        {
			if (ins->OAMState_SendingCC == TRUE)
			{
				USBSendAmoeba(ins, pCell, FALSE);
			}
        }
		////////////////
    }
	else
	{
		// Improve OAM CC - Shahriar
		// Tag: OAMCC 
//	// Request from ECI for CC - Shahriar
//	// Tag : ECICC
//		if (++ins->OAMState_CCSink > 4)
//		{
//			ins->OAMState_ReceiveCC = TRUE;
//		}
	}
	
OAMTimer_Done:
    // Need to reschedule the timer to go again
    ArmOAMTimer(ins);

    eu_leaves (DBG_OAM);
}


int SetOAMTimerInterval(eu_instance_t* ins, int reqDelay)
{

    eu_enters (DBG_OAM);
    eu_dbg (DBG_OAM,"Requested delay = %d\n",reqDelay);

    /*If the timer is currently running, delete it*/
    if ( timer_pending(&ins->OAMTimer) )
       del_timer(&ins->OAMTimer);

	ins->OAMTimer.expires = jiffies + MSEC_TO_JIFFIES(reqDelay); // ffdfix: Nov25, 2002


    add_timer(&ins->OAMTimer);

    eu_leaves (DBG_OAM);
    return 0;
}
