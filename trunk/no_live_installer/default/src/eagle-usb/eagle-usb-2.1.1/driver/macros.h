/*
 *
 * Copyright (c) 2004, Frederick Ros (sl33p3r@free.fr)
 *										    
 * macros.h
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
 * $Id: macros.h,v 1.6 2005/01/17 20:54:42 sleeper Exp $
 */


#ifndef MACROS_H
#define MACROS_H

/*
 * Compatibility macros (USB changed in kernel 2.4.20)
 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,19)

typedef struct usb_ctrlrequest devrequest;
#define FILL_USB_CTRL_REQUEST(p, t, r, v, i, l)         \
	{                                                   \
            p->bRequestType = t;                        \
            p->bRequest     = r;                        \
            p->wValue       = cpu_to_le16 ((UInt16) v); \
            p->wIndex       = cpu_to_le16 ((UInt16) i); \
            p->wLength      = cpu_to_le16 ((UInt16) l); \
	}

#else
#define FILL_USB_CTRL_REQUEST(p, t, r, v, i, l)         \
	{                                                   \
            p->requesttype = t;                         \
            p->request     = r;                         \
            p->value       = cpu_to_le16 ((UInt16)v);   \
            p->index       = cpu_to_le16 ((UInt16)i);   \
            p->length      = cpu_to_le16 ((UInt16)l);   \
	}

#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
/*
 * Linux 2.6 introduces kmalloc flags
 */
#define USB_SUBMIT_URB(u,f)    usb_submit_urb((u),(f))
#define USB_ALLOC_URB(u,f)     usb_alloc_urb((u),(f))

#define GET_INTF_PTR(u,n)   ((u)->actconfig->interface[(n)])

#define USB_QUEUE_BULK 0


#define FILL_BULK_URB(u,d,p,tb,bl,c,ctx) \
        usb_fill_bulk_urb ((u),(d),(p),(tb),(bl),(c),(ctx))
/*
 * Module reference is now obsolete
 */
#define MODULE_USER_GET
#define MODULE_USER_RELEASE

/*
 * Get Configuration value for config #n
 */
#define GET_CONFIG_VALUE(d,n)  (d)->config[n].desc.bConfigurationValue


#define USB_COMPLETION_PROTO(f,u,r)   void f ( struct urb *u, struct pt_regs *r )

#define USB_DRIVER_CLAIM_INTERFACE(d,i,p)   \
    usb_driver_claim_interface((d), (i), (p))

#define EU_SCHEDULE(w)    schedule_work ( w )
#define EU_FLUSH(w)       flush_scheduled_work()

#define EU_FREE_NETDEV(n)     free_netdev(n)

#else

/*
 *   K E R N E L   2.4
 *
 */
 

#define USB_SUBMIT_URB(u,f)    usb_submit_urb(u)
#define USB_ALLOC_URB(u,f)     usb_alloc_urb((u))

#define GET_INTF_PTR(u,n)   (&((u)->actconfig->interface[(n)]))

#define URB_ASYNC_UNLINK       USB_ASYNC_UNLINK
#define URB_ISO_ASAP       USB_ISO_ASAP

#define MODULE_USER_GET     MOD_INC_USE_COUNT
#define MODULE_USER_RELEASE MOD_DEC_USE_COUNT

/*
 * Get Configuration value for config #n
 */
#define GET_CONFIG_VALUE(d,n)  (d)->config[n].bConfigurationValue

#define USB_COMPLETION_PROTO(f,u,r)   void f ( struct urb *u )

#define USB_DRIVER_CLAIM_INTERFACE(d,i,p)   \
   ( usb_driver_claim_interface((d), (i), (p)) , 0)

#define EU_SCHEDULE(w)    schedule_task ( w )
#define EU_FLUSH(w)       flush_scheduled_tasks()

#define EU_FREE_NETDEV(n)     kfree(n)

#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,6)
/*
 * wait_ms disappear from 2.6.7 in favor of msleep
 */
static __inline__ void wait_ms(unsigned int ms)
{
        if(!in_interrupt()) {
            msleep (ms);
        }
        else
        {
            mdelay (ms);
        }
}

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)

#define USB_KILL_URB(urb)    usb_kill_urb(urb)

#else

#define USB_KILL_URB(urb)                       \
  do {                                          \
    (urb)->transfer_flags &= ~URB_ASYNC_UNLINK; \
    usb_unlink_urb(urb);                        \
  } while(0)

#endif

#define GET_KBUFFER(_memsize_) kmalloc((_memsize_), in_interrupt() ? GFP_ATOMIC : GFP_KERNEL)
#define FREE_KBUFFER(_ptr_)                     \
    if (_ptr_)                                  \
    {                                           \
        kfree(_ptr_);                           \
        (_ptr_) = NULL;                         \
    }

#define GET_VBUFFER(_memsize_) vmalloc(_memsize_)
#define FREE_VBUFFER(_ptr_)                     \
    if (_ptr_)                                  \
    {                                           \
        vfree(_ptr_);                           \
        (_ptr_) = NULL;                         \
    }

/*
 * Quasi-function to obtain current status of SM
 */
#define GetOpStatusSM(_ADAPTER_) ((_ADAPTER_->CurrentAdiState&STATE_OPERATIONAL)!=0)

/*
 * Macro for cancelling the StateMachine timer
 */
#define CancelTimerSM(_ADAPTER_) SetTimerInterval(_ADAPTER_,0)

/*
 * Quasi-function to signal beginning of SM reset
 * actual processing would happen later, on next refresh
 */
#define ResetModemSM(_ADAPTER_) if(_ADAPTER_->CurrentAdiState!=STATE_STALLED_FOREVER) \
                                { (_ADAPTER_->CurrentAdiState=STATE_HARD_RESET_INITIATE); }

#define MSEC_TO_JIFFIES(_msec_) ((HZ*(unsigned long)_msec_)/1000)

#endif /* MACROS_H */

