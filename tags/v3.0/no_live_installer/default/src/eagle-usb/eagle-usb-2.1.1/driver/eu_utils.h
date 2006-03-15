/*
 *
 * Copyright (c) 2003, Frederick Ros (sl33p3r@free.fr)
 *										    
 * eu_utils.h  - Various utils functions
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
 * $Id: eu_utils.h,v 1.3 2004/11/07 09:06:55 sleeper Exp $
 */


#ifndef __EU_UTIL_H__
#define __EU_UTIL_H__
#include "macros.h"

int eu_load_firmware ( eu_instance_t *ins, uint32_t pid, unsigned int *ver );

int eu_cmd_to_modem (
                     eu_instance_t *ins,
                     uint32_t cmd,
                     uint32_t idx,
                     uint32_t count,
                     uint8_t *data
                    );
int alloc_queued_urb_ctrl ( eu_instance_t *ins, unsigned int count );
void free_queued_urb_ctrl ( struct list_head *q );
void ctrl_urb_q_watcher ( unsigned long ptr );
void unlink_ipg_ctrl_urb ( eu_instance_t *ins );
USB_COMPLETION_PROTO (ctrl_urb_completion,urb,regs);
void ctrl_urb_retry_send ( unsigned long ptr );
void queue_ctrl_urb(eu_instance_t *ins, struct urb *urb);
void eu_crc_generate ( void );
uint32_t eu_crc_calculate ( uint8_t *cell, uint32_t len, uint32_t crc );
void eu_get_mac_addr ( eu_instance_t *ins );


#endif /* __EU_UTIL_H__ */
