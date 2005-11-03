/***********************************************************************************/
/* $Id: Adiutil.h,v 1.1 2004/02/06 22:01:34 sleeper Exp $                          */
/*										   */
/* Adiutil.h									   */
/*										   */
/* Includes all headers necessary for using Adiutil				   */
/*										   */
/* This file is part of the "ADI USB ADSL Driver for Linux".			   */
/*										   */
/* "ADI USB ADSL Driver for Linux" is free software; you can redistribute it       */
/* and/or modify it under the terms of the GNU General Public License as           */
/* published by the Free Software Foundation; either version 2 of the License,     */
/* or (at your option) any later version.					   */
/*										   */
/* "ADI USB ADSL Driver for Linux" is distributed in the hope that it will be      */
/* useful, but WITHOUT ANY WARRANTY; without even the implied warranty of	   */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the		   */
/* GNU General Public License for more details.					   */
/*										   */
/* You should have received a copy of the GNU General Public License		   */
/* along with "ADI USB ADSL Driver for Linux"; if not, write to the Free Software  */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA       */
/***********************************************************************************/

#ifndef __ADIHEADERS_H__
#define __ADIHEADERS_H__

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#define LINUX_2_4
#else
#define LINUX_2_6
#endif

/*Include OS includes here ...*/
#include <linux/config.h>
#ifdef LINUX_2_4
#if defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS)
#define MODVERSIONS
#endif
#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif
#endif /* LINUX_2_4 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/usb.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/param.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/spinlock.h>
#include <linux/stddef.h>
#include <asm/semaphore.h>
#include <linux/ctype.h>
#include <asm/byteorder.h>

#ifdef LINUX_2_6
#define EXPORT_NO_SYMBOLS 
#endif

/* Types definition */
typedef uint8_t		UInt8;
typedef uint16_t	UInt16;
typedef uint32_t	UInt32;
typedef int32_t		SInt32;
typedef unsigned int	Boolean;
#define FALSE                       0
#define TRUE                        1

typedef struct list_head QHdr;




#ifdef USEBULK

/*
 * Get Read Buffer from urb
 */
#define GET_RBUF(u)   (void *)((unsigned long)(u)->transfer_buffer -           \
                               (unsigned long)(&((eu_bulk_rb_t *)0)->data))

#else

#define GET_RBUF(u)   (u)->transfer_buffer

#endif /* USEBULK */


#endif /*ADIUTIL_H*/

