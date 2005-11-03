/*
 *
 * Copyright (c) 2003, Frederick Ros (sl33p3r@free.fr)
 *										    
 * eu_sm.h  - State Machine handling functions/structures
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
 * $Id: eu_sm.h,v 1.1 2004/02/06 22:01:34 sleeper Exp $
 */

#ifndef __EU_SM_H__
#define __EU_SM_H__

#include "eu_types.h"

/*
 *  Events accepted by modem state machine - bitmask type
 */

#define EVENT_NONE              0
#define EVENT_TIMER_TICK        0x0001
#define EVENT_RX_SYNC           0x0002
#define EVENT_RX_SYNC_ERROR     0x0003
#define EVENT_RX_ASYNC          0x0004
#define EVENT_RX_ASYNC_ERROR    0x0005
#define EVENT_HARD_RESET        0x0008

#define EVENT_SOFT_RESET        0x000F

/*
 *  Modem state machine possible states for <DMT_***_state> members 
 */

#define STATE_UNDEFINED             0x0000
#define STATE_BOOT_STAGE_1          0x0001
#define STATE_BOOT_STAGE_2          0x0002
#define STATE_BOOT_STAGE_3          0x0003
#define STATE_STALLED_FOREVER       0xFFFF

/* ---- Startup/Reboot related states ----*/

#define STATE_HARD_RESET_INITIATE   0x8000 /* reset modem */
#define STATE_HARD_RESET_END        0x8001
#define STATE_BOOT_WAIT             0x8002
#define STATE_JUST_PLUGGED_IN       0x8003

#define STATE_KERNEL                0x4000 /* user controls the CMVs */
#define STATE_KERNEL_TX             0x4001
#define STATE_KERNEL_RX             0x4002

#define STATE_UNTRAIN               0x2000 /* sends CMVs to train */
#define STATE_UNTRAIN_TX            0x2001
#define STATE_UNTRAIN_RX            0x2002

#define STATE_TEST                  0x1000 /* send out test tones */
#define STATE_TEST_TX               0x1001
#define STATE_TEST_RX               0x1002

/* ---- User interface related states ----*/

#define STATE_USER_CONTROL          0x0800 /* user controls all */
#define STATE_USER_CONTROL_TX       0x0801
#define STATE_USER_CONTROL_RX       0x0802

/* #define   STATE_IDMABOOT              0x0400 */

#define STATE_EN_FASTTRAIN          0x0100 /* send fast retrain enable */
#define STATE_EN_FASTTRAIN_TX       0x0101
#define STATE_EN_FASTTRAIN_RX       0x0102

/* ---- Functional states ----*/

#define STATE_INITIALIZING          0x0080 /* read STAT CMV to find out if we are trained or not */
#define STATE_INITIALIZING_TX       0x0081
#define STATE_INITIALIZING_RX       0x0082

#define STATE_OPERATIONAL           0x0040 /* read STAT, DIAG, ...etc to see if we stay in sync or not */
#define STATE_OPERATIONAL_TX        0x0041
#define STATE_OPERATIONAL_RX        0x0042

#define STATE_FAST_RETRAIN          0x0020 /* read STAT only */
#define STATE_FAST_RETRAIN_TX       0x0021
#define STATE_FAST_RETRAIN_RX       0x0022

/*
 *  Times for transition from state to state - for watchdog timer (msec)
*/

#define TRANS_TIME_2_BEGINRESET         50  /* time to begin reset */
#define TRANS_TIME_4_RESETINIT        5000  /* time for BootMachine completion */
#define TRANS_TIME_4_BOOTWAIT         8000  /* time to wait for MODEM_READY */
#define TRANS_TIME_BOOTWAIT_2_UNTRAIN   10  /* pause before UNTRAIN starts */

#define TRANS_TIME_2_BOOT_STAGE_1      200  /* From beginning to Boot to Stage 1 */
#define TRANS_TIME_2_BOOT_STAGE_2      200  /* Stage 1 to Stage 2              */
#define TRANS_TIME_2_BOOT_STAGE_3     1000  /* Stage 2 to Stage 3              */

#define TRANS_TIME_4_UNTRAIN           200  /* time for UNTRAIN CMV completion */
#define TRANS_TIME_B4_UNTRAIN          400

#define TRANS_TIME_4_INITIALIZE      60000  /* time for STAT CMV completion */
#define TRANS_TIME_B4_INITIALIZE      5000  /* time to wait before INIT retry */

#define TRANS_TIME_4_ENAFAST            50  /* time for ENAFAST completion */
#define TRANS_TIME_B4_ENAFAST          500

#define TRANS_TIME_4_FASTRETRAIN      5000  /* time to wait for FASTRETRAIN */
#define TRANS_TIME_B4_FASTRETRAIN      500  /* pause before FASTRETRAIN starts */

#define TRANS_TIME_4_OPERATIONAL        50  /* time for oper CMV completion */
#define TRANS_TIME_B4_OPERATIONAL      100  /* time before sending out 1st CMV */

#define TRANS_TIME_4_OPSTAT             80  /* time for each next completion */
#define TRANS_TIME_B4_OPSTAT          2000  /* time to wait before 1st completes */

#define TRANS_TIME_5MSEC_PAUSE           5
#define TRANS_TIME_50MSEC_PAUSE         50

#define TRANS_TIME_HEARTBEAT          3000  /* timer heartbeat is 3 secs */

#define TRANS_TIME_AFTER_PAGE_SWAP     750  /* requested pause after time swap */



extern void ModemSM(UInt16 uEventCode, eu_instance_t *ins, void *pData);


#endif /* __EU_SM_H__ */
