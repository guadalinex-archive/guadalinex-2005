/*
 *
 * Copyright (c) 2004, Frederick Ros (sl33p3r@free.fr)
 *                        
 * Me.c
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
 * $Id: Me.c,v 1.3 2004/09/12 20:00:46 sleeper Exp $
 */

/*************************************************************************************/
/*  The initial concept for MODEM DEVICE MANAGEMENT had been built around            */
/*  timer callbacks after 100 msec (0.1 sec), which is far from elegant.             */
/*                                                                                   */
/*  MAC OS USL provides completion callback for each USB transaction,                */
/*  moreover, refernce says at the time of callback the structs are reusable.        */
/*                                                                                   */
/*  Hence, timer callback makes sense only as watchdog for modem gone AWOL,          */
/*  but not to receive modem responses - that task goes in InterruptHandler.         */
/*                                                                                   */
/*  Yet modem can go retraining all by itself, w/o host being notified.              */
/*  QUESTION: can we get retrain IRQ from modem, to avoid polling?                   */
/*  else, to discover such changes, periodic poll will be unavoidable.               */
/*                                                                                   */
/*  This implementation had to cope w/ variety of timing issues.                     */
/*  Quite often, the ADSL book would say one thing,                                  */
/*  and the prototype code would to rather different.                                */
/*                                                                                   */
/*  It is built around completion callbacks, that are implemented in MAC OS.         */
/*  The sequence of reading/writing USB addresses is *absolutely* important.         */
/*                                                                                   */
/*                                                                                   */
/*  Terminology:                                                                     */
/*  Request - a CMV sent out from this code to Modem                                 */
/*  Reply - a CMV response to the most recent Request.                               */
/*        Requests and replies are processed synchronously.                          */
/*                                                                                   */
/*  Alert - unsolicited message from Modem, usu. about specific event.               */
/*          The most important one is MODEM_READY.                                   */
/*                                                                                   */
/*                                                                                   */
/*                                                                                   */
/*  This MANAGEMENT ENTITY consists of following routines:                           */
/*                                                                                   */
/*                                                                                   */
/*  1. ProcessIncomingCmv( eu_instance_t* ins, UInt16* pMessage )                    */
/*                                                                                   */
/*     This fxn is called from the interrupt handler, whenever it                    */
/*     finds out that the IRQ message is a CMV (SWAPs are filtered)                  */
/*     Cleaning of TSX mailbox is done immediatelly, it is all-important!            */
/*                                                                                   */
/*     All input data are passed at once to State Machine for processing.            */
/*                                                                                   */
/*                                                                                   */
/*  2. WatchdogTimer callback (to be implemented)                                    */
/*     This routine is of the type <SecondaryInterruptHandlerProc2> callback.        */
/*     Its main purpose is to detect incomplete transactions                         */
/*     and to initiate transition to RESET upon such condition.                      */
/*     The timeout period is be set at 3 secs, to reduce overload.                   */
/*                                                                                   */
/*     Its secondary purpose is to ping the device periodically,                     */
/*     when there's no command activity going on. Host has no means to detect        */
/*     modem's self-initiated RERTRAIN transition, exept w/ polling.                 */
/*     Not that we care too much, successful retrain may go unnoticed,               */
/*     ditto successful autoreboot that will return modem in OPERATIONAL.            */
/*                                                                                   */
/*     If modem goes to FAIL state, nothing except host-initiated RESET              */
/*     will resurrect it; therefore, implementation of periodical polls is a must.   */
/*                                                                                   */
/*                                                                                   */
/*  3.  SetTimerInterval routine                                                     */
/*    A simple utility routine, used by the fxns herein to reset timer watchdog.     */
/*    There are two possible time periods to be set:                                 */
/*       - heartbeat timeout - every 3 secs to see if modem is alive.                */
/*       - special times set from inside ModemSM,                                    */
/*         for specific delays between series of CMVs, or                            */
/*         for specific timeots while waiting for rely.                              */
/*    Ultimately, <SetInterruptTimer> will be called w/ <WatchdogTimer> as callback. */
/*                                                                                   */
/*                                                                                   */
/*  4. <ModemSM> IS THE EXTERNAL MODULE w/ FXNS THAT SEND COMMANDS TO MODEM.         */
/*     It does the detailed processing of incoming CMV base on the current state.    */
/*                                                                                   */
/*     It is called either from USB receive interrupt, or from timer routine,        */
/*     but NEVER EVER by both in the same time - that would be a disaster!!!         */
/*                                                                                   */
/*     If necessary, <ModemSM> will send out a CMV to the modem and wait for Reply.  */
/*     The Reply is expected to arrive timely, if no Reply comes, watchdog kick in.  */
/*                                                                                   */
/*                                                                                   */
/*                                                                                   */
/*  One more task of ModemSM is to tell upper network services about state changes   */
/*  and connection availability/speed/business etc.                                  */
/*  Which fxn calls are req'd in MAC OS, is yet to be seen.                          */
/*                                                                                   */
/*                                                                                   */
/*  These notes are subject to revision w/ implementation's progress.                */
/*                                                                                   */
/*  Ilko Dossev,  Oct. 06, 2001, revision 3                                          */
/*                                                                                   */
/*************************************************************************************/

#include "Adiutil.h"
#include <linux/sched.h>
#include "Me.h"
#include "macros.h"
#include "eu_sm.h"
#include "eu_msg.h"
#include "debug.h"

/************************************************************************/
/*  ProcessIncomingCmv                                                  */
/*                                                                      */
/*  Invoked on secondary System Task Level thread, this routine grabs   */
/*  new data from modem and clears TX mailbox status early on.          */
/*  Input data are passed immediatelly to State Machine for processing. */
/*  Messages are discriminated to Replies and Alerts according to       */
/*  the type in the header, Replies are always synchronous messages.    */
/*                                                                      */
/*  We expect response to arrive from modem within few hundred msecs.   */
/*  It gets all messages - either Replies or unsolicited Alerts.        */
/************************************************************************/
void ProcessIncomingCmv(eu_instance_t *ins, UInt16 *pMsg)
{
    UInt32 rv;
    eu_msg_t Msg; /*decoded message struct*/
    Boolean* pFlag; /*which flag to clear at exit*/

    eu_enters (DBG_MSG);

    rv = eu_decode_msg ( &Msg, pMsg );

    if ( Msg.type == MP_FUNCTION_TYPE_ADSLDIRECTIVE )
    {
        /*Alerts from modem, arrive ASYNCHRONOUSLY*/
        ins->AdiModemSm.InboundAsyncPending = TRUE;
        pFlag = &ins->AdiModemSm.InboundAsyncPending;
        if ( rv )
        {
            eu_dbg (DBG_MSG,"ProcessIncomingCmv: EVENT_RX_ASYNC\n");
            rv = EVENT_RX_ASYNC;
        }
        else
        {
            eu_dbg (DBG_MSG,"ProcessIncomingCmv: EVENT_RX_ASYNC_ERROR\n");
            
            rv = EVENT_RX_ASYNC_ERROR;
        }
    }
    else
    {
        /*Replies from modem, arrive SYNCHRONOUSLY*/
        ins->AdiModemSm.InboundSyncPending = TRUE;
        pFlag = &ins->AdiModemSm.InboundSyncPending;
        if ( rv )
        {
            eu_dbg (DBG_MSG,"ProcessIncomingCmv: EVENT_RX_SYNC\n");
            rv = EVENT_RX_SYNC;
        }
        else
        {
            eu_dbg (DBG_MSG,"ProcessIncomingCmv: EVENT_RX_SYNC_ERROR\n");
            rv = EVENT_RX_SYNC_ERROR;
        }
        /* when SYNC reply arrives, we can clear outbound CMV flag*/
        /* it is set in <MsgSend> fxn, that may be called from <ModemSM>*/
        ins->AdiModemSm.OutboundPending = FALSE;
    }
    
    /* process the message and reflect new SM state*/
    ModemSM((UInt16)rv, ins, &Msg);
    *pFlag = FALSE;

    eu_leaves (DBG_MSG);
}


/******************************************************************************/
/*                                                                            */
/*  WatchdogTimer                                                             */
/*                                                                            */
/*  This routine is of the type <SecondaryInterruptHandler2> for timer,       */
/*  guaranteed to be called at <kSecondaryInterruptLevel>, never <kTaskLevel> */
/*                                                                            */
/*  Its main purpose is to detect modem gone AWOL, and to kick-off RESET.     */
/*  If some USB command transaction did not complete, that's failure.         */
/*  If there is no USB command sequence going on at the moment,               */
/*  the function will initiate "Poll Status" for modem periodically.          */
/*                                                                            */
/*  To avoid too frequent polling, "at ease" interval is set to 1 sec or so.  */
/*                                                                            */
/*  When this callback is invoked, pGlobals->timerSM is already released.     */
/*  No need to do anything w/ it, only set to NULL just in case.              */
/******************************************************************************/
void WatchdogTimer(unsigned long ptr)
{
    eu_instance_t* ins = (eu_instance_t *)ptr;
    AdiMSM*   pAdiSm = &(ins->AdiModemSm);

    eu_enters (DBG_MSG);

    if (pAdiSm->ModemReplyExpected == MP_FUNCTION_TYPE_WAITING_4_REPLY )
    {
        eu_dbg (DBG_MSG,"Watchdog timer found modem error - starting RESET!\n");
        
        /*modem AWOL, initiate reset and terminate Modem SM*/
        ResetModemSM(pAdiSm);
    }

    /*call State Machine to process the TIMER_TICK event*/
    if (ins->AdiModemSm.InboundSyncPending  == FALSE &&
        ins->AdiModemSm.InboundAsyncPending == FALSE )
    {
        eu_dbg (DBG_MSG,"Timer tick going to SM\n");
        
        ModemSM(EVENT_TIMER_TICK, ins, NULL);
    }

    /*if no timer is set inside State Machine, set default timeout*/
    if ( !timer_pending(&pAdiSm->timerSM) )
    {
        eu_dbg (DBG_MSG,"Default timeout value set (0x%x)\n", TRANS_TIME_HEARTBEAT);
        
        SetTimerInterval(ins, TRANS_TIME_HEARTBEAT);
    }

    eu_leaves (DBG_MSG);
}

/***************************************************************************/
/*                                                                         */
/*  SetTimerInterval                                                       */
/*                                                                         */
/*  This utility sets next timer interval, as requested by caller.         */
/*  Caller provides delay in millisecond units, conversion done inside.    */
/*  If current timer hasn't expire yet, it is cancelled at once.           */
/*  New timer is created *automatically* and its Id is placed in pGlobals. */
/*                                                                         */
/*  WARNING!                                                               */
/*  There are no provisions to cope w/ <SetInterruptTimer> failure.        */
/*  MAC OS manuals say, it will always succeed, so in them we trust!       */
/*                                                                         */
/*  COMPATIBILITY                                                          */
/*  This call is "Carbon"-incompatible, good for MAC OS 8 and 9 only.      */
/*  For MAC OS X, rewrite will be an absolute must.                        */
/***************************************************************************/
int SetTimerInterval(eu_instance_t* ins, int reqDelay)
{
    AdiMSM   *pAdiSm = &(ins->AdiModemSm);
    unsigned long curtime = jiffies;
    long delay   = MSEC_TO_JIFFIES(reqDelay);
    unsigned long exptime;

    eu_enters (DBG_MSG);
    eu_dbg (DBG_MSG,"Requested delay = %d\n",reqDelay);
    
    /*If the timer is currently running, delete it*/
    if ( timer_pending(&pAdiSm->timerSM) )
        del_timer(&pAdiSm->timerSM);

    pAdiSm->CurrentExpirationTime = curtime;
    if ( reqDelay == 0 )
        goto SetTimer_exit;

    /*calculate the minimal delay requested*/
    if ( curtime < pAdiSm->SwapPageRequiredTime )
    {
        long newDelay;
        newDelay = pAdiSm->SwapPageRequiredTime - curtime;
        if ( delay < newDelay )
            delay = newDelay;
    }

    exptime = curtime + delay;
    pAdiSm->timerSM.expires = exptime;
    pAdiSm->CurrentExpirationTime = exptime;
    
    /*we expect this system call to succeed always*/
    add_timer(&pAdiSm->timerSM);

  SetTimer_exit:    
    eu_leaves (DBG_MSG);
    return 0;
}


/********************************************************************
$Log: Me.c,v $
Revision 1.3  2004/09/12 20:00:46  sleeper
Fix jiffies computation plus error/dbg string for AMD64 portation

Revision 1.2  2004/06/04 18:56:19  sleeper
Cosmetics

Revision 1.1  2004/02/06 22:01:34  sleeper
Initial creation

Revision 1.3  2003/10/04 14:25:47  sleeper
2.6 portage

Revision 1.2  2003/09/14 22:09:00  sleeper
Various changes ( I know this is not a good reason :)

Revision 1.2  2003/06/03 22:42:29  sleeper
Changed ZAPS to eu_dbg/err macros

Revision 1.1.1.1  2003/02/10 23:29:49  sleeper
Imported sources

Revision 1.50  2002/05/24 21:59:33  Anoosh Naderi
Clean up the code

Revision 1.4  2002/01/14 21:59:33  chris.edgington
Added GPL header.

Revision 1.3  2001/12/28 19:36:17  chris.edgington
Removed setting of ModemReplyExpected in WatchdogTimer.
Added comments from Ilko to AdjustTimerInterval.

Revision 1.2  2001/12/26 22:48:02  chris.edgington
Added kernel timer support.

Revision 1.1  2001/12/22 21:38:04  chris.edgington
Initial versions - from MacOS9 project tree.

---------------------------------------------------------------------------
 Log from MacOS9 project
---------------------------------------------------------------------------

Revision 1.29  2001/12/14 06:41:17  ilko.dossev
Trace messages added for special timer cases.
<AdjustTimerInterval> checks for invalid max delay (zero is now prohibited).

Revision 1.28  2001/12/05 22:16:00  chris.edgington
Added Analog Devices copyright notice.

Revision 1.27  2001/11/19 18:12:37  ilko.dossev
OS 9 specific code moved to <UtilOS9.C> module.
Obsolete special debug traces removed.

Revision 1.26  2001/11/13 20:40:50  ilko.dossev
<AdjustTimerInterval> stub function defined, to be completed later.
Pls, see comments before function definition.

Revision 1.25  2001/11/09 03:21:04  ilko.dossev
Removed quick return from ME if message fails to decode.

Revision 1.24  2001/10/18 22:43:39  martyn.deobald
Added quick cutout in ProcessIncomingCMV if the message doesn't decode OK

Revision 1.23  2001/10/18 21:42:51  ilko.dossev
Writing MAILBOX_EMPTY not req'd any more w/ latest ADI firmware.

Revision 1.22  2001/10/11 17:01:16  ilko.dossev
Var names changed to reflect their actual usage in AdiMSM structure

Revision 1.21  2001/10/07 03:32:03  ilko.dossev
Processing simplified for incoming messages, immediate call to Statte Machine from USB interrupt callback.
Timer procedure simplified, special care taken that:
  - no timer will be initialized twice by accident;
  - no call to ModemSM will occur while processing USB interrupt.
Discarded obsolete <HandlePendingRequests> procedure.

Revision 1.20  2001/10/05 23:35:00  ilko.dossev
Changed ModemSM calls - to include as parameter a pointer to the decoded message structure.

Revision 1.19  2001/10/04 15:44:06  chris.edgington
Added code to disable DCON output while we're in timer handler.

Revision 1.18  2001/10/03 21:30:49  chris.edgington
Major rework and simplification in order to get CMV upload and training sequence working.

Revision 1.17  2001/10/03 18:33:21  ilko.dossev
Comments' sectoin revised, comments brought up-to date, worths reading them.
Parsing error is passed for processing to StateMachine, as it is in the original code.
Special comment about secondary RX mailbox cleanup put in the code.

Revision 1.16  2001/10/03 16:32:54  ilko.dossev
The process for receiving CMV from modem changed to three-step.
First, RX and TX mailboxes are cleared.
Next, after completion of these writes, SM is called.
Finally, outbound CMV is placed in the queue.

Revision 1.15  2001/10/02 16:14:55  ilko.dossev
Sending message to modem cleared from this module - moved to ModemSM fxn.

Revision 1.14  2001/10/02 01:40:29  ilko.dossev
Delayed initialization sequence implemented

Revision 1.13  2001/10/01 21:47:10  ilko.dossev
Some extra debug output added.
Fixed HARD_RESET setting for state machine.

Revision 1.12  2001/09/29 04:31:16  ilko.dossev
Timer loop for State Macine refined, now exits when driver unloaded.
Some extra trace output added for message decoding.

Revision 1.11  2001/09/28 20:50:58  ilko.dossev
Cleaned up ModemME, w/ only ProcessIncomingCmv and Timer Watchdog.

Revision 1.9  2001/09/27 23:22:13  ilko.dossev
ZAPs added in ProcessIncomingCmv

Revision 1.8  2001/09/27 22:07:18  ilko.dossev
ProcessIncomingCmv function added, message processing architecture to be revised.

Revision 1.7  2001/09/26 22:33:23  ilko.dossev
Functions modified following changes in MsgProtocol.C
Important functions ZAP-ed (debug output).

Revision 1.6  2001/09/26 15:43:16  chris.edgington
Moved all the boot code to Boot.c.

Revision 1.5  2001/09/25 22:30:06  chris.edgington
Added BootModem, StateMachine, and StateMachineCompletionHandler.

Revision 1.4  2001/09/25 18:42:46  ilko.dossev
$ID$ and $LOG$ sections added.

Revision 1.3 2001/09/24 20:59:52 ilko.dossev
**********************************************************************************/


