/*
 *
 * Copyright (c) 2003, Frederick Ros (sl33p3r@free.fr)
 *										    
 * eu_msg.h  - Message protocol subsystem functions
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
 * $Id: eu_msg.h,v 1.1 2004/02/06 22:01:34 sleeper Exp $
 */


#ifndef __EU_MSG_H__
#define __EU_MSG_H__

#include "eagle-usb.h"

/* ---------------------------- Exported Macros ---------------------------- */

/*
 * Message preamble
 */
#define MSG_PREAMBLE              0x535C

/*
 * Bit pattern to access various function subfields
 */
#define MP_FUNCTION_TYPE_MASK     0xf000
#define MP_FUNCTION_SUBTYPE_MASK  0x0f00
#define MP_FUNCTION_SENDERID_MASK 0x00f0
#define MP_FUNCTION_RECEIVER_MASK 0x000f

#define MP_FUNCTION_TYPE_LSB      0xc
#define MP_FUNCTION_SUBTYPE_LSB   0x8
#define MP_FUNCTION_SENDERID_LSB  0x4
#define MP_FUNCTION_RECEIVER_LSB  0x0

/*
 * Function types
 */
#define MP_FUNCTION_TYPE_WAITING_4_REPLY              0
#define MP_FUNCTION_TYPE_MEMACCESS                  0x1
#define MP_FUNCTION_TYPE_MSGDECERR                  0x2
#define MP_FUNCTION_TYPE_MSGACCERR                  0x3
#define MP_FUNCTION_TYPE_FLASHACC                   0x4
#define MP_FUNCTION_TYPE_FLASHACCERR                0x5
#define MP_FUNCTION_TYPE_MEDIRECTIVE                0x6
#define MP_FUNCTION_TYPE_ADSLDIRECTIVE              0x7

/*
 * Functions sub-types
 */
#define SUBTYPE_MEMACCESS_REQREAD                   0x0
#define SUBTYPE_MEMACCESS_REQWRITE                  0x1
#define SUBTYPE_MEMACCESS_REPLYREAD                 0x2
#define SUBTYPE_MEMACCESS_REPLYWRITE                0x3

#define SUBTYPE_MEMACCESSERROR_UNKNOWNADDR          0x1
#define SUBTYPE_MEMACCESSERROR_DATANOTRDY           0x2
#define SUBTYPE_MEMACCESSERROR_ILLEGALREAD          0x3
#define SUBTYPE_MEMACCESSERROR_ILLEGALWRITE         0x4
#define SUBTYPE_MEMACCESSERROR_DATANOTAVAIL         0x5

#define SUBTYPE_MSGDECERR_LENGTHERROR               0x1
#define SUBTYPE_MSGDECERR_PREAMBLEERROR             0x2
#define SUBTYPE_MSGDECERR_UNKNOWNTYPE               0x3
#define SUBTYPE_MSGDECERR_CRCERROR                  0x4

#define SUBTYPE_FLASHACC_ENKERNELREBOOTREQ          0x0
#define SUBTYPE_FLASHACC_ENMODEMREBOOTREQ           0x1
#define SUBTYPE_FLASHACC_DECOMPMODEMAPPLREQ         0x2
#define SUBTYPE_FLASHACC_STREAMEDDATAWRREQ          0x3
#define SUBTYPE_FLASHACC_STREAMEDDATAWRBLOCK        0x4
#define SUBTYPE_FLASHACC_READREQ                    0x5
#define SUBTYPE_FLASHACC_CALCMODEMCRCREQ            0x7
#define SUBTYPE_FLASHACC_ENKERNELREBOOTREPLY        0x8
#define SUBTYPE_FLASHACC_ENMODEMREBOOTREPLY         0x9
#define SUBTYPE_FLASHACC_DECOMPMODEMAPPLREPLY       0xa
#define SUBTYPE_FLASHACC_STREAMEDDATAWRREPLY        0xb
#define SUBTYPE_FLASHACC_STREAMEDDATAWRBLOCKREPLY   0xc
#define SUBTYPE_FLASHACC_READREPLY                  0xd
#define SUBTYPE_FLASHACC_CALCMODEMCRCREPLY          0xf

#define SUBTYPE_FLASHACCERR_READCOMPERR             0x0
#define SUBTYPE_FLASHACCERR_DECOMPRESSERR           0x1

#define SUBTYPE_MEDIRECTIVE_REBOOT                  0x0

#define SUBTYPE_ADSLDIRECTIVE_KERNELREADY           0x0
#define SUBTYPE_ADSLDIRECTIVE_MODEMREADY            0x1
#define SUBTYPE_ADSLDIRECTIVE_MODEMCRCERROR         0x2

/*
 * Sender / Receiver ID
 */
#define SUBTYPE_ID_ADSL                             0x0
#define SUBTYPE_ID_ME                               0x1


/*
 * eu_msg_t decode function return values
 */

#define MP_DECODE_OK                                0x00000000
#define MP_DECODE_ERROR                             0x00000001
#define MP_DECODE_PREAMBLE_ERROR                    0x00000002
#define MP_DECODE_TYPE_ERROR                        0x00000004
#define MP_DECODE_SUBTYPE_ERROR                     0x00000008
#define MP_DECODE_SENDERID_ERROR                    0x00000010
#define MP_DECODE_RECEIVERID_ERROR                  0x00000020
#define MP_DECODE_SYMB_ADDR                         0x00000040




/* -------------------------- Exported Functions --------------------------- */


uint32_t eu_decode_msg ( eu_msg_t *msg, uint16_t raw_msg[8] );



#endif /* __EU_MSG_H__ */
