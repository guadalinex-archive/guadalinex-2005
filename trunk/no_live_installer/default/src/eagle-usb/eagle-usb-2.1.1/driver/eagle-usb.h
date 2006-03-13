/*
 *
 * Copyright (c) 2003, Frederick Ros (sl33p3r@free.fr)
 *										    
 * eagle-usb.h
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
 * $Id: eagle-usb.h,v 1.2 2004/10/09 11:59:56 sleeper Exp $
 */

#ifndef __EAGLE_USB_H__
#define __EAGLE_USB_H__ 

#include "eu_types.h"

/*
 * Sagem USB IDs
 */
#define EAGLE_VID              0x1110
#define EAGLE_I_PID_PREFIRM    0x9010  /* Eagle I */
#define EAGLE_I_PID_PSTFIRM    0x900F  /* Eagle I */

#define EAGLE_IIC_PID_PREFIRM  0x9024  /* Eagle IIC */
#define EAGLE_IIC_PID_PSTFIRM  0x9023  /* Eagle IIC */

#define EAGLE_II_PID_PREFIRM   0x9022  /* Eagle II */
#define EAGLE_II_PID_PSTFIRM   0x9021  /* Eagle II */


/*
 *  Eagle III Pid
 */
#define EAGLE_III_PID_PREFIRM   0x9032 /* Eagle III */
#define EAGLE_III_PID_PSTFIRM   0x9031 /* Eagle III */

/*
 * USR USB IDs
 */
#define USR_VID		           0x0BAF
#define MILLER_A_PID_PREFIRM   0x00F2
#define MILLER_A_PID_PSTFIRM   0x00F1
#define MILLER_B_PID_PREFIRM   0x00FA
#define MILLER_B_PID_PSTFIRM   0x00F9
#define HEINEKEN_A_PID_PREFIRM 0x00F6
#define HEINEKEN_A_PID_PSTFIRM 0x00F5
#define HEINEKEN_B_PID_PREFIRM 0x00F8
#define HEINEKEN_B_PID_PSTFIRM 0x00F7

#define IS_EAGLE_I(c) ( EAGLE_I_PID_PREFIRM == (c)                                   || \
                        MILLER_A_PID_PREFIRM   == (c) || MILLER_B_PID_PREFIRM == (c) || \
                        HEINEKEN_A_PID_PREFIRM == (c) || HEINEKEN_B_PID_PREFIRM == (c)  \
                      )

#define IS_EAGLE_III(c) ( EAGLE_III_PID_PREFIRM == (c) ) 


#ifdef __KERNEL__



void eu_msg_initialize ( eu_instance_t *ins, const eu_options_t opt );

void eu_send_msg ( eu_instance_t *ins, uint16_t *packet );


#endif /* __KERNEL__ */

#endif /* __EAGLE-USB_H__ */
