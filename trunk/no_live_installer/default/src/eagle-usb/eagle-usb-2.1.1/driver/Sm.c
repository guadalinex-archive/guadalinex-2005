/*
 *
 * Copyright (c) 2004, Frederick Ros (sl33p3r@free.fr)
 *										    
 * Sm.c (State Machine)
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
 * $Id: Sm.c,v 1.6 2004/09/27 21:59:07 mcoolive Exp $
 */

#include "Adiutil.h"
#include "eu_sm.h"
#include "eu_eth.h"
#include "Cmv.h"
#include "macros.h"
#include "eu_utils.h"
#include "eu_msg.h"
#include "Me.h"
#include "debug.h"
#include "eu_boot_sm.h"
#ifdef LINUX_2_4
#include <linux/tqueue.h>
#elif defined(LINUX_2_6)
#include <linux/workqueue.h>
#else
#warning "Unsupported version"
#endif

#define RESET_TIMER(_TIMEOUT_) pAdiSM->ModemReplyExpected = MP_FUNCTION_TYPE_WAITING_4_REPLY; \
                               SetTimerInterval(ins,_TIMEOUT_); 

#define ENDCASE SetTimerInterval(ins,TRANS_TIME_50MSEC_PAUSE); break;

#define OPER_CHECK_FREQ           3
#define OPER_CHECK_FREQ_DETAIL  0xF

#define LOS_DEFECT_RETRAIN_THRESHOLD    2   /* about  1  */
#define CRC_RETRAIN_THRESHOLD           4   /* about 10 secs of continuous CRC to cause retrain */


#define RETRY_LIMIT             10
#define MAX_IDLE_COUNT          100
#define RETRAIN_ATTEMPTS        160


/* quasi-functions to force beginning of SM reset   */
/* actual processing would happen when timer expires*/
#define TimerResetModemSM SetTimerInterval(ins,TRANS_TIME_2_BEGINRESET); \
                          ResetModemSM(pAdiSM);

#ifdef LINUX_2_4
static struct tq_struct eth_create_cb = {
	routine: eu_eth_create,
};
#endif


/***************************************************************/
/*										                       */
/* ModemSM.C									               */
/*										                       */
/* Contains functions to maintain State Machine for ADI modem. */
/*										                       */
/*   Rewrite for MAC OS, 09/2001						       */
/*										                       */
/***************************************************************/
void ModemSM(UInt16 uEventCode, eu_instance_t *ins, void *pData)
{
    UInt32 DspStatus;
    eu_msg_t *pMsg = (eu_msg_t *) pData;
    AdiMSM* pAdiSM   = &(ins->AdiModemSm);
    UInt16  tmpState = pAdiSM->CurrentAdiState;

    dbg (DBG_SM,"Entering SM : current state is 0x%x\n"
         "                  new event is 0x%x\n",
         tmpState,uEventCode);
    

    /* first of all, see if we are dead forever*/
    if ( tmpState == STATE_STALLED_FOREVER )
    {
        dbg (DBG_SM, "SM: STALLED FOREVER\n");
	CancelTimerSM(ins);
	goto ExitSM;
    }

    /* check for bad conditions (should not happen, though)*/
    if (pData == NULL && 
	( uEventCode != EVENT_HARD_RESET && uEventCode != EVENT_TIMER_TICK ) )
    {
        eu_err ("**** NULL data ptr in SM!!! ****\n");
        
	goto ExitSM;
    }

    /* keep current event code*/
    pAdiSM->ModemReplyExpected = uEventCode;

    /* cancel the timer*/
    CancelTimerSM(ins);

    /* in *all* cases EVENT_HARD_RESET is procesed the same way*/
    if (uEventCode == EVENT_HARD_RESET )
    {
        eu_dbg (DBG_SM,"SM: HARD RESET requested\n");
        
	tmpState = STATE_UNDEFINED; /* will fall thru default case */
    }

    switch( tmpState )
    {
        case STATE_HARD_RESET_INITIATE:
            if (uEventCode == EVENT_TIMER_TICK )
            {
                eu_dbg (DBG_SM,"SM: HARD RESET INITIATED!\n");
                
                /* clear all mailbox flags*/
                pAdiSM->OutboundPending     = FALSE;
                pAdiSM->InboundAsyncPending = FALSE;
                pAdiSM->InboundSyncPending  = FALSE;
	    
                /* CLEAN UP some vars (will review later - IIDos)*/
                pAdiSM->LOS_count = 0;
                pAdiSM->CRC_count = 0;
                pAdiSM->XferRate0 = 0;
                pAdiSM->FwRxTimeout = 0;
	    
                pAdiSM->stats_Ne_Fast_Lod_Failure = FALSE;
                pAdiSM->stats_Ne_Fast_Hec_Failure = FALSE;
	    
                /* set timer watchdog to quite a few seconds for that*/
                SetTimerInterval(ins,TRANS_TIME_4_RESETINIT);
                /* clear control var to allow timer reset*/
                pAdiSM->ModemReplyExpected = MP_FUNCTION_TYPE_WAITING_4_REPLY;
	    
                /* execute machine boot in rebooting mode*/
                ins->boot_state = REBOOT;
                EU_SCHEDULE ( &ins->boot_sm);
	
                /* the last step of BootMachine will:		 */
                /* load main page;					 */
                /* set state to STATE_BOOT_WAIT;			 */
                /* set timer interval to TRANS_TIME_4_BOOTWAIT secs.*/
            }
            else
            {
                eu_dbg (DBG_SM,"SM: HARD RESET REQUEST at int time, re-posting the request.\n");
                
                /* set timer watchdog to complete RESET after a few msecs*/
                SetTimerInterval(ins,TRANS_TIME_HEARTBEAT);
            }
            break;
        case STATE_HARD_RESET_END:
            /* this step will never actually happens,*/
            /* it is hidden in BootMachine - step 5  */
            break;
        
        case STATE_BOOT_STAGE_1:
            /*
              If we're here from a timer tick, we can call
              the required function
            */      
            if (  uEventCode == EVENT_TIMER_TICK ) 
            {
                ins->boot_state = STAGE_1;
                EU_SCHEDULE ( &ins->boot_sm );                
            }
            break;

        case STATE_BOOT_STAGE_2:
            /*
              If we're here from a timer tick, we can call
              the required function
            */
            if (  uEventCode == EVENT_TIMER_TICK ) 
            {
                ins->boot_state = STAGE_2;
                EU_SCHEDULE ( &ins->boot_sm );                
            }
            
            break;
            
        case STATE_BOOT_STAGE_3:
            /*
              If we're here from a timer tick, we can call
              the required function
            */
            if (  uEventCode == EVENT_TIMER_TICK ) 
            {
                ins->boot_state = STAGE_3;
                EU_SCHEDULE ( &ins->boot_sm );                
            }
            break;

            
        case STATE_BOOT_WAIT:
            switch( uEventCode )
            {
                default:
                case EVENT_RX_SYNC_ERROR:
                    eu_dbg (DBG_SM, "*********** FFD >>>> EVENT_RX_SYNC_ERROR(3) => RESTE.\n");
                    
                    /// FFDTEST : FFD020303     
                    TimerResetModemSM;				
                    break;
                case EVENT_RX_ASYNC_ERROR:
                    // By Mani ///////////////////////////////////////
                    //TimerResetModemSM;				//
                    //////////////////////////////////////////////////
                    break;
                case EVENT_RX_ASYNC:
                    /* Original code does not check for MODEM_READY*/
                    if (pMsg->type == MP_FUNCTION_TYPE_ADSLDIRECTIVE )
                    {
                        /* FROM ORIGINAL ADI CODE --				      */
                        /* we need to check if the modem is in modem or kernel mode*/ 
                        /* sometimes the modem wakes up in kernel mode (why?).     */
                        /* when this happens, we request modem mode & reboot.      */ 
                        if (pMsg->subtype == SUBTYPE_ADSLDIRECTIVE_MODEMREADY )
                        {
                            eu_dbg (DBG_SM,"MODEM READY alert received, starting State Machine.\n");
                            
                            /* set small delay before it */
                            SetTimerInterval(ins,TRANS_TIME_BOOTWAIT_2_UNTRAIN);
                            /* advance to next state*/
                            pAdiSM->CurrentAdiState = STATE_UNTRAIN;
		    
                            /* clear heartbeat counter before proceeding*/
                            pAdiSM->HeartbeatCounter = 0;
		    
                            break;
                        }
                        else
                        {
                            if (pMsg->subtype == SUBTYPE_ADSLDIRECTIVE_KERNELREADY )
                            {
                                eu_dbg (DBG_SM,"KERNEL READY alert received unexpectedly!\n");
                                
                                TimerResetModemSM;
                                break;
                            }
                        }
                    }
                case EVENT_TIMER_TICK:
                    eu_dbg (DBG_SM,"BOOT_WAIT going to HARD_RESET\n");
                    
                    TimerResetModemSM;
                    break;
            }
            break;
        case STATE_UNTRAIN:
            pAdiSM->RetryCount = 0;
            pAdiSM->MsgStage   = 0;
        case STATE_UNTRAIN_TX:
            /* this stage shall execute only on timer tick trigger*/
            if (uEventCode == EVENT_TIMER_TICK )
            {
                eu_dbg (DBG_SM,"UNTRAIN\n");
                
	    
                /* send out the beginning of the sequence from stage 0*/
                eu_send_msg(ins, pAdiSM->MsgSeq_Retrainer.MsgMax.RawCmd[pAdiSM->MsgStage]);
	    
                /* prepare watchdog timer*/            
                RESET_TIMER(TRANS_TIME_4_UNTRAIN);
	    
                /* next progress will depend on timely replies from modem*/
                pAdiSM->CurrentAdiState = STATE_UNTRAIN_RX;
            }
            break;
        case STATE_UNTRAIN_RX:
            /* continuation of UNTRAIN, goes when modem confirms CMV*/
            if ( uEventCode == EVENT_RX_SYNC )
            {
                eu_dbg (DBG_SM,"UNTRAIN_RX stage %i complete\n",pAdiSM->MsgStage);
                
                /* send next command to untrain*/
                eu_send_msg(ins, pAdiSM->MsgSeq_Retrainer.MsgMax.RawCmd[++pAdiSM->MsgStage]);
	    
                /* prepare watchdog timer*/            
                RESET_TIMER(TRANS_TIME_4_UNTRAIN);
	    
                /* if it is the last, make transition*/
                if (pAdiSM->MsgStage+1 >= pAdiSM->MsgSeq_Retrainer.MsgCount )
                {
                    pAdiSM->CurrentAdiState = STATE_INITIALIZING;
                }
            }
            else
            {
                if (uEventCode == EVENT_RX_SYNC_ERROR )
                {
                    eu_dbg (DBG_SM,"RxSyncError - retrying UNTRAIN\n");
                    
                    /* set wait time for the retry*/
                    SetTimerInterval(ins,TRANS_TIME_B4_UNTRAIN);
                    /* Idle state - no CO detected, retry again*/
                    pAdiSM->CurrentAdiState = STATE_UNTRAIN_TX;
                    /* do not retry infinitely*/
                    if ( pAdiSM->RetryCount++ > RETRY_LIMIT )
                        ResetModemSM(pAdiSM);
                }
            }
            break;
        case STATE_INITIALIZING:
            pAdiSM->RetryCount = 0;
            pAdiSM->MsgStage   = 0;
            pAdiSM->ReTrain    = 0;
        case STATE_INITIALIZING_TX:
            /* if it is smooth continuation from UNTRAIN, it is on SYNC*/
            /* otherwise, may be transition from mid-sequence on timer */
            if (pAdiSM->MsgStage < MAX_IDLE_COUNT &&
                (uEventCode == EVENT_RX_SYNC || uEventCode == EVENT_TIMER_TICK ))
            {
                eu_dbg (DBG_SM,"INITIALIZING\n");                
	    
                /* send the only one CMV to initialize*/
                eu_send_msg(ins, pAdiSM->MsgSeq_Stat.MsgMax.RawCmd[0]);
	    
                /* prepare watchdog timer*/            
                RESET_TIMER(TRANS_TIME_4_INITIALIZE);
	    
                /* we know there's only one command in STAT*/
                pAdiSM->CurrentAdiState = STATE_INITIALIZING_RX;
            }
            else
            {
                TimerResetModemSM;
            }
            break;
        case STATE_INITIALIZING_RX:
            if (uEventCode == EVENT_RX_SYNC)
            {
                /* WE DO NOT CHECK FOR <NULL> POINTER HERE - */
                /* WE TRUST OUR CODE WILL NEVER DO BAD THING!*/
                DspStatus = (pMsg->data&STAT_SW_MASK)>>STAT_SW_LSB;
	    
                switch( DspStatus )
                {
                    /* -----------------  SUCCESS! */
                    case STAT_SW_OPERATIONAL:
                        eu_dbg (DBG_SM,"INIT DspStatus = STAT_SW_OPERATIONAL\n");                        
		
                        /* Need to reenable ADSL traffic in the FPGA*/
                        if (pAdiSM->HeartbeatCounter == 0 )
                            eu_cmd_to_modem(ins, EU_CMD_SET_MODE, MODE_LOOPBACK_OFF, 0, NULL);
		
                        /* set wait time for transition*/
                        SetTimerInterval(ins,TRANS_TIME_B4_OPERATIONAL);
                        /* this is success!*/
                        pAdiSM->CurrentAdiState = STATE_OPERATIONAL;
                        eu_report ( "Modem operational !!\n");
                        break;

                        /* ----------------- fast retrain for G.Lite modes*/
                    case STAT_SW_FASTRETRAIN:
                        eu_dbg (DBG_SM,"INIT DspStatus = STAT_SW_FASTRETRAIN\n");
                        
                        /* set wait time for the retry*/
                        SetTimerInterval(ins,TRANS_TIME_B4_FASTRETRAIN);
                        /* Fast retrain is enabled for this modem*/
                        pAdiSM->CurrentAdiState = STATE_FAST_RETRAIN;
                        break;
		
                        /* ----------------- can stay forever in this state, if line not connected*/
                    case STAT_SW_R_ACT_REQ:
                        eu_dbg (DBG_SM,"INIT DspStatus = STAT_SW_R_ACT_REQ\n");
                        
                        /* set wait time for the retry*/
                        SetTimerInterval(ins,TRANS_TIME_B4_INITIALIZE);
                        /* Idle state - no CO detected, retry again*/
                        pAdiSM->CurrentAdiState = STATE_INITIALIZING_TX;
                        /* keep track of retry attempts*/
                        pAdiSM->MsgStage++;
                        break;
	    
                        /* ----------------- retry INIT (for how long?)*/
                    case STAT_SW_INITIALIZATION:
                        eu_dbg (DBG_SM,"INIT DspStatus = STAT_SW_INITIALIZATION\n");
                        
                        /* if training had taken too long, fall thru RESET*/
                        if ( ++pAdiSM->ReTrain < RETRAIN_ATTEMPTS )
                        {
                            /* set wait time for the retry*/
                            SetTimerInterval(ins,TRANS_TIME_B4_INITIALIZE);
                            /* keep on initializing for a while*/
                            pAdiSM->CurrentAdiState = STATE_INITIALIZING_TX;
                            break;
                        }

                        /* -----------------  invoke RESET*/
                    case STAT_SW_FAIL:
                    case STAT_SW_TESTSIGNAL:
                    case STAT_SW_TESTHW:
                    default:
                        eu_dbg (DBG_SM,"SM: INIT DspStatus = %x to RESET\n", DspStatus);
                        
                        TimerResetModemSM;
                }
            }
            else
            {
                if (uEventCode == EVENT_RX_SYNC_ERROR )
                {
                    eu_dbg (DBG_SM,"RxSyncError - retrying INIT\n");                    
		
                    /* set wait time for the retry*/
                    SetTimerInterval(ins,TRANS_TIME_B4_INITIALIZE);
                    /* Idle state - no CO detected, retry again*/
                    pAdiSM->CurrentAdiState = STATE_INITIALIZING_TX;
                    /* do not retry infinitely*/
                    if (pAdiSM->RetryCount++ > RETRY_LIMIT )
                        ResetModemSM(pAdiSM);
                }
            } 
            break;
        case STATE_EN_FASTTRAIN:
            pAdiSM->RetryCount = 0;
            pAdiSM->MsgStage   = 0;
            pAdiSM->ReTrain    = 0;
        case STATE_EN_FASTTRAIN_TX:
            if ( uEventCode == EVENT_TIMER_TICK )
            {
                eu_dbg (DBG_SM,"EN_FAST_RETRAIN - Warning!!! ****\n");
                
	    
                /* send the only one CMV to initialize*/
                eu_send_msg(ins, pAdiSM->MsgSeq_EnaFR.MsgMax.RawCmd[0]);
	    
                /* prepare watchdog timer*/            
                RESET_TIMER(TRANS_TIME_4_ENAFAST);
	    
                /* we know there's only one command in STAT*/
                pAdiSM->CurrentAdiState = STATE_EN_FASTTRAIN_RX;
            }
            break;
        case STATE_EN_FASTTRAIN_RX:
            if (uEventCode == EVENT_RX_SYNC)
            {
                eu_dbg (DBG_SM,"EN_FAST_RETRAIN accepted\n");
	    
                /* set wait time for the retry*/
                SetTimerInterval(ins,TRANS_TIME_B4_FASTRETRAIN);
                /* then fast retrain may begin*/
                pAdiSM->CurrentAdiState = STATE_FAST_RETRAIN;
            }
            else
            {
                if ( uEventCode == EVENT_RX_SYNC_ERROR )
                {
                    eu_dbg (DBG_SM,"RxSyncError - retrying EN_FASTTRAIN\n");
                    
                    /* set wait time for the retry*/
                    SetTimerInterval(ins,TRANS_TIME_B4_ENAFAST);
                    /* Idle state - no CO detected, retry again*/
                    pAdiSM->CurrentAdiState = STATE_EN_FASTTRAIN_TX;
                    /* do not retry infinitely*/
                    if (pAdiSM->RetryCount++ > RETRY_LIMIT)
                        ResetModemSM(pAdiSM);
                }
            }
            break;
        case STATE_FAST_RETRAIN:
            pAdiSM->RetryCount = 0;
            pAdiSM->MsgStage   = 0;
            pAdiSM->ReTrain    = 0;
        case STATE_FAST_RETRAIN_TX:
            /* this stage shall execute only on timer tick trigger, identical to INIT*/
            if (uEventCode == EVENT_TIMER_TICK)
            {
                eu_dbg (DBG_SM,"FAST RETRAIN BEGINS\n");
	    
                /* send the only one CMV to check status*/
                eu_send_msg(ins, pAdiSM->MsgSeq_Stat.MsgMax.RawCmd[0]);
	    
                /* prepare watchdog timer*/            
                RESET_TIMER(TRANS_TIME_4_FASTRETRAIN);
	    
                /* we know there's only one command in STAT*/
                pAdiSM->CurrentAdiState = STATE_FAST_RETRAIN_RX;
            }
            else
            { 
                TimerResetModemSM;
            }
            break;
        case STATE_FAST_RETRAIN_RX:
            if (uEventCode == EVENT_RX_SYNC)
            {
                /* WE DO NOT CHECK FOR <NULL> POINTER HERE - */
                /* WE TRUST OUR CODE WILL NEVER DO BAD THING!*/
                DspStatus = (pMsg->data&STAT_SW_MASK)>>STAT_SW_LSB;
	    
                switch( DspStatus )
                {
                    /* -----------------  SUCCESS!*/
                    case STAT_SW_OPERATIONAL:
                        eu_dbg (DBG_SM,"FAST DspStatus = STAT_SW_OPERATIONAL\n");
                        
                        /* set wait time for transition*/
                        SetTimerInterval(ins,TRANS_TIME_B4_OPERATIONAL);
                        /* this is success!*/
                        pAdiSM->CurrentAdiState = STATE_OPERATIONAL;
                        eu_err ("Modem operational\n");
                        break;

                        /* ----------------- retry INIT*/
                    case STAT_SW_INITIALIZATION:
                        eu_dbg (DBG_SM,"FAST DspStatus = STAT_SW_INITIALIZATION\n");
                        
                        /* set wait time for the retry*/
                        SetTimerInterval(ins,TRANS_TIME_B4_INITIALIZE);
                        /* Idle state - no CO detected, retry again*/
                        pAdiSM->CurrentAdiState = STATE_INITIALIZING;
                        break;
		
                        /* ----------------- keep on retraining (for how long?)*/
                    case STAT_SW_FASTRETRAIN:
                        eu_dbg (DBG_SM,"FAST DspStatus = STAT_SW_FASTRETRAIN\n");
                        
                        /* if training had taken too long, fall thru RESET*/
                        if (++pAdiSM->ReTrain < RETRAIN_ATTEMPTS)
                        {
                            /* set wait time for the retry*/
                            SetTimerInterval(ins,TRANS_TIME_B4_FASTRETRAIN);
                            /* Idle state - no CO detected, retry again*/
                            pAdiSM->CurrentAdiState = STATE_FAST_RETRAIN_TX;
                            break;
                        }
		
                        /* -----------------  invoke RESET*/
                    case STAT_SW_R_ACT_REQ:
                    case STAT_SW_FAIL:
                    case STAT_SW_TESTSIGNAL:
                    case STAT_SW_TESTHW:
                    default:
                        eu_dbg (DBG_SM,"FAST DspStatus = %x to RESET\n", DspStatus);
                        
                        TimerResetModemSM;
                }
            }
            else
            {
                if (uEventCode == EVENT_RX_SYNC_ERROR)
                {
                    eu_dbg (DBG_SM,"RxSyncError - retrying FAST\n");
		
                    /* set wait time for the retry*/
                    SetTimerInterval(ins,TRANS_TIME_B4_FASTRETRAIN);
                    /* Idle state - no CO detected, retry again*/
                    pAdiSM->CurrentAdiState = STATE_FAST_RETRAIN_RX;
                    /* do not retry infinitely*/
                    if (pAdiSM->RetryCount++ > RETRY_LIMIT)
                        ResetModemSM(pAdiSM);
                }
            }
            break;
        case STATE_KERNEL:
        case STATE_KERNEL_TX:
        case STATE_KERNEL_RX:
            eu_dbg (DBG_SM,"KERNEL state\n");
            
            TimerResetModemSM;
            break;
        case STATE_TEST:
        case STATE_TEST_TX:
        case STATE_TEST_RX:
            eu_dbg (DBG_SM,"TEST state\n");
            
            TimerResetModemSM;
            break;
        case STATE_OPERATIONAL:
            /* check if we're getting to OPER from some other state*/
            if ( (pAdiSM->PrevAdiState&0xFFF0) != STATE_OPERATIONAL )
            {
                /* prepare this special start value!*/
                pAdiSM->MsgStage = pAdiSM->MsgSeq_OpStat.MsgCount;
	    
                pAdiSM->LOS_count = 0;
                pAdiSM->CRC_count = 0;
                pAdiSM->Block_CRC90 = 0;
                pAdiSM->Block_CRC97 = 0;
                pAdiSM->stats_Rx_Blks = 0;
                pAdiSM->stats_Corr_Blks = 0;
                pAdiSM->stats_Uncorr_Blks = 0;
                pAdiSM->stats_Uncorr_Blks_Delta = 0;
                pAdiSM->watchBadBlocks = FALSE;
                eu_dbg (DBG_SM,"*** Transition to OPER state successful! ***\n");
                
                wake_up_interruptible(&ins->sync_q);
            }

            /* This case is built around timer tick.		    */
            /* From time to time, heavy OpSTAT sequence is sent out.   */
            /* more often, it goes to light Stat to recheck the status.*/
            if (uEventCode == EVENT_TIMER_TICK)
            {
                /* no command will be sent if anything is under way*/
                if ( pAdiSM->MsgStage >= pAdiSM->MsgSeq_OpStat.MsgCount )
                {
                    eu_dbg (DBG_SM,"Starting OPER stat sequence\n");
                    
                    /* set the stage for the Stat sequence*/
                    pAdiSM->MsgStage = 0;
                    /* begin transmission*/ 
                    eu_send_msg(ins, pAdiSM->MsgSeq_OpStat.MsgMax.RawCmd[pAdiSM->MsgStage]);
                    /* clear retry counter*/
                    pAdiSM->RetryCount = 0;
                    /* prepare watchdog timer*/
                    RESET_TIMER(TRANS_TIME_B4_OPSTAT);
                    /* continue w/ reading the first stat*/
                    pAdiSM->CurrentAdiState = STATE_OPERATIONAL_RX;
                }
                /* increment the global counter*/
                pAdiSM->HeartbeatCounter++;
            }
            break;
        case STATE_OPERATIONAL_TX:
            if (uEventCode == EVENT_TIMER_TICK)
            {
                eu_dbg (DBG_SM,"OPER_TX stage %i\n",pAdiSM->MsgStage+1);
	    
                /* send out CMV for the next stage*/
                eu_send_msg (ins, pAdiSM->MsgSeq_OpStat.MsgMax.RawCmd[++pAdiSM->MsgStage]);
                /* prepare watchdog timer*/
                if (pAdiSM->MsgStage != 0 &&
                    pAdiSM->MsgStage < pAdiSM->MsgSeq_OpStat.MsgCount-1)
                {
                    RESET_TIMER(TRANS_TIME_4_OPSTAT);
                }
                else
                {
                    RESET_TIMER(TRANS_TIME_B4_OPSTAT);
                }
                /* set the new state*/
                pAdiSM->CurrentAdiState = STATE_OPERATIONAL_RX;
            }
            break;
        case STATE_OPERATIONAL_RX:
            if (uEventCode == EVENT_RX_SYNC)
            {
                /* clear retry counter*/
                pAdiSM->RetryCount = 0;
                /* get ready for the next transmission*/
                pAdiSM->CurrentAdiState = STATE_OPERATIONAL_TX;
	    
                eu_dbg (DBG_SM,"OPER_RX processing stage %i\n",pAdiSM->MsgStage);
                
	    
                switch( pAdiSM->MsgStage )
                {
                    /* Actual examination of the returned values must be done here*/
                    case 0:  /* STAT.status*/
                        pAdiSM->sw_status  = (pMsg->data&STAT_SW_MASK)>>STAT_SW_LSB;
                        pAdiSM->crc_status = (pMsg->data&STAT_CRC_MASK)>>STAT_CRC_LSB;
                        ENDCASE;
                    case 1:  /* DIAG.flags   (DIAG.1)*/
                        pAdiSM->flags = pMsg->data;
                        
                        switch( pAdiSM->sw_status )
                        {
                            /* -----------------  invoke RESET for all other values*/
                            case STAT_SW_FAIL:
                            case STAT_SW_TESTSIGNAL:
                            case STAT_SW_TESTHW:
                            default:
                                eu_dbg (DBG_SM,"OPSTAT step1 to RESET\n");
                                
                                TimerResetModemSM;
                                break;
                                /*  -----------------  go to INIT, nevermind it's not in the diagram*/
                            case STAT_SW_R_ACT_REQ:
                            case STAT_SW_INITIALIZATION:
                                eu_dbg (DBG_SM,"OPSTAT step1 to INIT\n");
                                
                                /* set wait time for the retry*/
                                SetTimerInterval(ins,TRANS_TIME_B4_INITIALIZE);
                                /* Idle state - no CO detected, retry again*/
                                pAdiSM->CurrentAdiState = STATE_INITIALIZING_TX;
                                break;
                                /*  -----------------  go to FAST RETRAIN for G.Lite modes*/
                            case STAT_SW_FASTRETRAIN:
                                eu_dbg (DBG_SM,"SM: OPSTAT step1 to FASTRETRAIN\n");
                                
                                /* set wait time for the retry*/
                                SetTimerInterval(ins,TRANS_TIME_B4_FASTRETRAIN);
                                /* Idle state - no CO detected, retry again*/
                                pAdiSM->CurrentAdiState = STATE_FAST_RETRAIN;
                                break;
                            case STAT_SW_OPERATIONAL:
                                eu_dbg (DBG_SM,"OPSTAT step1 is OPERATIONAL\n");
                                
                                /* check CRC detect flag*/
                                if ((pAdiSM->flags &
                                     (DIAG_FLAGS_NEFASTCRC_MASK|DIAG_FLAGS_NEINTLCRC_MASK) ) != 0)
                                {

                                    if (++pAdiSM->CRC_count >= CRC_RETRAIN_THRESHOLD)
                                    {
                                        eu_dbg (DBG_SM,"OPSTAT step1 CRC thresh RESET\n");
                                        
                                        eu_err("CRC count threshold reached. Rebooting\n");
                                        ResetModemSM(pAdiSM);
                                    }
                                }
                                else
                                {
                                    /* crc status back to normal, reset the count */
                                    pAdiSM->CRC_count = 0;
                                }
                                /* check for LOS_defect flag */
                                if ((pAdiSM->flags & DIAG_FLAGS_NELOSDEFECT_MASK) != 0)
                                {
                                    eu_err (" SM: LOS Defect\n");
                                    if (++pAdiSM->LOS_count >= LOS_DEFECT_RETRAIN_THRESHOLD)
                                    {
                                        eu_dbg (DBG_SM,"OPSTAT step1 LOS thresh RESET\n");
                                        
                                        eu_report ("LOS Defect threshold reached. Rebooting\n");
                                        ResetModemSM(pAdiSM);
                                        
                                    }
                                }
                                else
                                {
                                    /* reset the LOS count as the line is ok */
                                    pAdiSM->LOS_count = 0;
                                }
                                /* added check for ratio of bad/good blocks*/
                                if ( pAdiSM->watchBadBlocks )
                                {
                                    if ( pAdiSM->stats_Rx_Blks_Delta != 0 )
                                    {
                                        /* no need to go digital on ratio calcs, we can use hex*/
                                        int BadGoodRatio = (pAdiSM->stats_Uncorr_Blks_Delta<<8) /
                                            pAdiSM->stats_Rx_Blks_Delta;
			
                                        eu_dbg (DBG_SM,"  Cycle %i, bad/good ratio is %i,  cnt90/97=(%i,%i)\n",
                                                pAdiSM->HeartbeatCounter,BadGoodRatio,
                                                pAdiSM->Block_CRC90,pAdiSM->Block_CRC97);
                                        
                                        /* if bad/good ratio is 98% or more*/
                                        if (BadGoodRatio > 250)
                                        {
                                            /* if this is second consequtive time (6 secs)*/
                                            if (++pAdiSM->Block_CRC97 >= 1)
                                            {
                                                
                                                eu_report("97th Rx err threshold reached. Rebooting\n");
                                                
                                                ResetModemSM(pAdiSM); 
                                                
                                            }
                                        }
                                        else
                                        {
                                            /* clear ultra hi-err counter*/
                                            pAdiSM->Block_CRC97 = 0;
				
                                            /* check for error ratio of 90% or more*/
                                            if (BadGoodRatio > 230)
                                            {
                                                /* for 20 secs we're getting 90% bad, go to reset*/
                                                if (++pAdiSM->Block_CRC90 > 6)
                                                {
                                                    eu_report ("90th Rx err threshold reached. Rebooting\n");
                                                    
                                                    ResetModemSM(pAdiSM);
                                                }
                                            }
                                            else
                                            {
                                                /* clear hi-err counter*/
                                                pAdiSM->Block_CRC90 = 0;
                                                /* clear error watch flag*/
                                                pAdiSM->watchBadBlocks = FALSE;
                                            }
                                        }
                                    }
                                }

                                /* check & set for ATM Sync/Hunt status*/
                                pAdiSM->stats_Ne_Fast_Lod_Failure = 
                                    pAdiSM->flags & DIAG_FLAGS_NEFASTLODFAIL_MASK;
		    
                                /* check for HEC errors*/
                                pAdiSM->stats_Ne_Fast_Hec_Failure = 
                                    pAdiSM->flags & DIAG_FLAGS_NEFASTHECFAIL_MASK;
		    
                                SetTimerInterval(ins,TRANS_TIME_5MSEC_PAUSE);
                        }
                        break;
                    case 2:  /* DIAG.near_end_total_ES_count  (DIAG.22)*/
                        pAdiSM->stats_ES_count = pMsg->data;
                        
                        if (pAdiSM->stats_ES_count > 20)
                        {
                            eu_dbg (DBG_SM,"OPSTAT step2 total ES count RESET\n");
                            
                            /* if negative, certain error*/
                            
                        }
                        ENDCASE;
                    case 3:  /* DIAG.near_end_Current_Attenuation  (DIAG.23)*/
                        pAdiSM->stats_Cur_Atten = pMsg->data;
                        ENDCASE;
                    case 4:  /* DIAG.near_end_Current_SNR_Margin  (DIAG.25)*/
                        pAdiSM->stats_Cur_SNR = pMsg->data;
                        if ((int)(pAdiSM->stats_Cur_SNR) < 0)
                        {
                            
                            eu_report ("SNR margin < 0 . Rebooting\n");
                            /* if negative, certain error*/
                            ResetModemSM(pAdiSM); 
                        }
                        else
                        {
                            /*
                              if (pAdiSM->watchBadBlocks == FALSE &&
                              (pAdiSM->HeartbeatCounter & OPER_CHECK_FREQ) != 0)
                              {
                              // rarified polls only if ADI monitor is NOT running
                              if (!ins->MonitoringApp)
                              {
                              // fast forward to last STAT
                              pAdiSM->MsgStage = pAdiSM->MsgSeq_OpStat.MsgCount-2;
                              }
                              }
                            */
                        }
                        ENDCASE;
                    case 5:  /* DIAG.near_end_Current_Rcvd_Blocks   (DIAG.51)*/
                        pAdiSM->stats_Rx_Blks_Delta = pMsg->data - pAdiSM->stats_Rx_Blks; 
                        pAdiSM->stats_Rx_Blks = pMsg->data;
                        ENDCASE;
                    case 6:  /* DIAG.near_end_Current_Xmitted_Blocks   (DIAG.52)*/
                        pAdiSM->stats_Tx_Blks = pMsg->data;
                        ENDCASE;
                    case 7:  /* DIAG>near_end_Current_Corrected_Blocks   (DIAG.53)*/
                        pAdiSM->stats_Corr_Blks_Delta = pMsg->data - pAdiSM->stats_Corr_Blks; 
                        pAdiSM->stats_Corr_Blks = pMsg->data;
                        ENDCASE;
                    case 8:  /* DIAG.near_end_Current_Uncorr_Blocks   (DIAG.54)*/
                        /* if for two consequtive checks there's delta in err count, set watch on*/
                        pAdiSM->watchBadBlocks |= (pAdiSM->stats_Uncorr_Blks_Delta!=0);
                        pAdiSM->stats_Uncorr_Blks_Delta = pMsg->data - pAdiSM->stats_Uncorr_Blks;
                        if (pAdiSM->stats_Uncorr_Blks_Delta == 0)
                        {
                            pAdiSM->Block_CRC90 =
                                pAdiSM->Block_CRC97 = 0;
                            pAdiSM->watchBadBlocks = FALSE;
                        }
                        pAdiSM->stats_Uncorr_Blks = pMsg->data;
                        ENDCASE;
                    case 9:  /* RATE.actual*/
                        pAdiSM->XferRate0 = pMsg->data;
                        pAdiSM->DownRate = (pMsg->data>>16) *32*1024;
                        pAdiSM->UpRate = (pMsg->data&0xFFFF) *32*1024;
                        
                        if ( !EU_TEST_FLAG(ins,EU_LOW_RATE) )
                        {
                            if (pAdiSM->DownRate < (1024*1024))
                            {
                                eu_err (" **** LOW LINE RATE (0x%x) ****\n",pAdiSM->DownRate);
                                eu_cmd_to_modem(ins, EU_CMD_SET_TIMEOUT, 0, 0, NULL);
                            }
                            else						 
                            {
                                eu_err ("***** HIGH LINE RATE (0x%x) *****\n", pAdiSM->DownRate);
                                
                                eu_cmd_to_modem(ins, EU_CMD_SET_TIMEOUT, 0xFF, 0, NULL);
                            }
                        }
                        
                        EU_SET_FLAG (ins, EU_LOW_RATE);

                        if (pAdiSM->FwRxTimeout != pAdiSM->XferRate0)
                        {
                            UInt32 threshold;
                            if (pAdiSM->DownRate <= 0x80 *32*1024)
                            {
                                threshold = 0xB000;
                            }
                            else
                            {
                                threshold = 0xF000;
                            }
                            if (pAdiSM->FwRxTimeout != threshold)
                            {
                                pAdiSM->FwRxTimeout = threshold;
                                /**********************************************************************/
                                /* We're getting an error -6912 back from the device		      */
                                /* when we send this command. Waiting to hear back		      */
                                /* from ADI about this, 'cause the NDIS code doesn't		      */
                                /* actually send it.						      */
                                /* eu_cmd_to_modemNoPB( ins, ADI_CMD_SET_TIMEOUT, threshold, 0, NULL );*/
                                /* ADI says not to worry about this - hardware does		      */
                                /* fine without having the driver set this value		      */
                                /**********************************************************************/
                            }
                        }
                        ENDCASE;
                    case 10:   /* DIAG.3*/
                        pAdiSM->DIAG03 = pMsg->data;
/*                         ins->Statistics[STAT_ATMHEC] += pMsg->data; */
                        
                        ENDCASE;
                    case 11:   /* DIAG.47*/
                        pAdiSM->DIAG47 = pMsg->data;
                        ENDCASE;
                    case 12:   /* DIAG.49*/
                        pAdiSM->DIAG49 = pMsg->data;
                        ENDCASE;
                    case 13:   /* INFO.10*/
                        pAdiSM->INFO10 = pMsg->data;
                        ENDCASE;
                    case 14:   /* PSDM.1*/
                        pAdiSM->PSDM01 = pMsg->data;
                        ENDCASE;
                    case 15:   /* INFO.8*/
                        pAdiSM->INFO08 = pMsg->data;
                        ENDCASE;
                    case 16:   /* INFO.14*/
                        pAdiSM->INFO14 = pMsg->data;
                        ENDCASE;
                        /* this is the quick stat poll - always the last in the sequence*/
                    default:
                        DspStatus = (pMsg->data&STAT_SW_MASK)>>STAT_SW_LSB;

                        switch (DspStatus)
                        {
                            /*  -----------------  OPERATIONAL it remains!*/
                            case STAT_SW_OPERATIONAL:
                                eu_dbg (DBG_SM,"OPER DspStatus = STAT_SW_OPERATIONAL\n");
                                
                                /* set wait time for transition*/
/*                                 SetTimerInterval(ins,TRANS_TIME_HEARTBEAT); */
                                SetTimerInterval(ins,1000);
                                /* this is success!*/
                                pAdiSM->CurrentAdiState = STATE_OPERATIONAL;
                                break;
                                /*  -----------------  go to FAST RETRAIN for G.Lite modes*/
                            case STAT_SW_FASTRETRAIN:
                                eu_dbg (DBG_SM,"OPER DspStatus = STAT_SW_FASTRETRAIN\n");
                                
                                /* set wait time for the retry*/
                                SetTimerInterval(ins,TRANS_TIME_B4_FASTRETRAIN);
                                /* Idle state - no CO detected, retry again*/
                                pAdiSM->CurrentAdiState = STATE_FAST_RETRAIN;
                                break;
                                /*  -----------------  go to INIT, nevermind it's not in the diagram*/
                            case STAT_SW_R_ACT_REQ:
                            case STAT_SW_INITIALIZATION:
                                eu_dbg (DBG_SM,"OPER DspStatus = INIT/R_ACT_REQ\n");
                                
                                /* set wait time for the retry*/
                                SetTimerInterval(ins,TRANS_TIME_B4_INITIALIZE);
                                /* Idle state - no CO detected, retry again*/
                                pAdiSM->CurrentAdiState = STATE_INITIALIZING_TX;
                                break;
                                /* -----------------  invoke RESET for all other values*/
                            case STAT_SW_FAIL:
                            case STAT_SW_TESTSIGNAL:
                            case STAT_SW_TESTHW:
                            default:
                                eu_dbg (DBG_SM,"OPER DspStatus = %x to RESET\n", DspStatus);
                                TimerResetModemSM;
                                break;
                        }
                        pAdiSM->MsgStage = pAdiSM->MsgSeq_OpStat.MsgCount;
                        break;
                }
            }
            else
            {
                if (uEventCode == EVENT_RX_SYNC_ERROR)
                {
                    /* repeat the last stage - TX will auto-increment*/
                    pAdiSM->MsgStage--;
                    /* set wait time for the retry*/
                    SetTimerInterval(ins,TRANS_TIME_B4_OPERATIONAL);
                    /* retry the transmission*/
                    pAdiSM->CurrentAdiState = STATE_OPERATIONAL_TX;
                    /* do not retry infinitely*/
                    if (pAdiSM->RetryCount++ > RETRY_LIMIT)
                        ResetModemSM(pAdiSM);
                }
            }
            break;
        default:  /* STATE_UNDEFINED et all*/
            eu_dbg (DBG_SM,"Error - undefined state!\n");
            TimerResetModemSM;
            break;
    }

    /*
     * Create ethernet device if not present
     */
    if ( ( ins->eth == NULL ) &&
         ( ! EU_TEST_FLAG (ins, EU_ETH_REGISTERING ) ) &&
         ( (pAdiSM->CurrentAdiState & 0xFFF0) == STATE_OPERATIONAL ) )
    {
        EU_SET_FLAG (ins,EU_ETH_REGISTERING);
        
#ifdef LINUX_2_4
        INIT_TQUEUE (&eth_create_cb, eu_eth_create, ins);        
        
        if ( !schedule_task (  &eth_create_cb ) ) 
#elif defined(LINUX_2_6)
        /*
         * Task queue have been removed in 2.6 in favor of workqueue
         */
        if ( !schedule_work( &ins->create_eth) )
#else
        #warning "Unsupported version"
#endif
        {
            eu_err ("Sm: Scheduled net dev. creation dropped.\n");
            EU_CLEAR_FLAG (ins,EU_ETH_REGISTERING);            
        }
        else
        {
            eu_dbg (DBG_SM,"Ethernet dev. creation scheduled.\n");
        }        

    }
    
    
    /*
     * Add carrier sense information
     */

    if (ins->eth) 
    {
        
        if ( ( (pAdiSM->CurrentAdiState & 0xFFF0) == STATE_OPERATIONAL ) &&
             ( !netif_carrier_ok(ins->eth) ) 
           ) 
        {
            eu_dbg (DBG_SM,"** Carrier ON ** \n");
            netif_carrier_on(ins->eth);
        }
        else if ( ( (tmpState & 0xFFF0) == STATE_OPERATIONAL ) &&
                  ( ( pAdiSM->CurrentAdiState & 0xFFF0) != STATE_OPERATIONAL )
                ) 
        {
            eu_dbg (DBG_SM,"** Carrier OFF **\n");
            
            netif_carrier_off(ins->eth);
        }
    }
    
    
    /* refresh previous state to be last known current one*/
    pAdiSM->PrevAdiState = tmpState;

  ExitSM:
    eu_dbg (DBG_SM," New State is 0x%x\n",pAdiSM->CurrentAdiState);
    eu_leaves (DBG_SM);
}


