
/*
 *
 * Copyright (c) 2004, Frederick Ros (sl33p3r@free.fr)
 *										 
 * eu_boot_sm.h
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
 * $Id: eu_boot_sm.h,v 1.1 2004/06/30 20:32:02 sleeper Exp $
 */
#ifndef __BOOT_SM_H__
#define __BOOT_SM_H__

#include "Adiutil.h"
#include "eu_types.h"

/**
 * Advance boot state machine ... This is indeed a front-end
 * to boot_sm_advance
 *
 */
  
void eu_boot_sm ( void *data );

void eu_upload_idma_swapp(eu_instance_t *ins, uint16_t swap_info);

int boot_the_modem (eu_instance_t *ins);

#endif /* __BOOT_SM_H__ */
