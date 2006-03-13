/*
 * eu_eth - Ethernet handling functions header
 * Copyright (C) 2003  Frederick Ros (sl33p3r@free.fr)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Id: eu_eth.h,v 1.1 2004/02/06 22:01:34 sleeper Exp $
 */

#ifndef __EU_ETH_H__
#define __EU_ETH_H__

extern int eu_eth_init ( struct net_device *dev );
extern void eu_eth_create ( void *data);

#endif /* __EU_ETH_H__ */
