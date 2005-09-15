/******************************************************************************
 *  usbatm.h - Generic USB xDSL driver core
 *
 *  Copyright (C) 2001, Alcatel
 *  Copyright (C) 2003, Duncan Sands, SolNegro, Josep Comas
 *  Copyright (C) 2004, David Woodhouse
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program; if not, write to the Free Software Foundation, Inc., 59
 *  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 ******************************************************************************/

#ifndef	_USBATM_H_
#define	_USBATM_H_

#include <asm/semaphore.h>
#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/completion.h>
#include <linux/config.h>
#include <linux/kref.h>
#include <linux/list.h>

/*
#define DEBUG
#define VERBOSE_DEBUG
*/

#if !defined (DEBUG) && defined (CONFIG_USB_DEBUG)
#	define DEBUG
#endif

#include <linux/usb.h>

#ifdef DEBUG
#define UDSL_ASSERT(x)	BUG_ON(!(x))
#else
#define UDSL_ASSERT(x)	do { if (!(x)) warn("failed assertion '" #x "' at line %d", __LINE__); } while(0)
#endif


/* mini driver */

struct usbatm_data;

/*
*  Assuming all methods exist and succeed, they are called like this:
*
*  	bind, heavy_init, atm_start, atm_stop, unbind
*/

struct usbatm_driver {
	struct module *owner;

	const char *driver_name;

	/*
	*  init device ... can sleep, or cause probe() failure.  Drivers with a heavy_init
	*  method can avoid the call to heavy_init by setting need_heavy_init to zero.
	*/
        int (*bind) (struct usbatm_data *, struct usb_interface *, int *need_heavy_init);

	/* additional device initialization that is too slow to be done in probe() */
        int (*heavy_init) (struct usbatm_data *, struct usb_interface *);

	/* cleanup device ... can sleep, but can't fail */
        void (*unbind) (struct usbatm_data *, struct usb_interface *);

	/* init ATM device ... can sleep, or cause ATM initialization failure */
	int (*atm_start) (struct usbatm_data *, struct atm_dev *);

	/* cleanup ATM device ... can sleep, but can't fail */
	void (*atm_stop) (struct usbatm_data *, struct atm_dev *);

        int in;		/* rx endpoint */
        int out;	/* tx endpoint */

	unsigned rx_padding;
	unsigned tx_padding;
};

extern int usbatm_usb_probe(struct usb_interface *intf, const struct usb_device_id *id,
		struct usbatm_driver *driver);
extern void usbatm_usb_disconnect(struct usb_interface *intf);


/* usbatm */

#define UDSL_MAX_RCV_URBS		4
#define UDSL_MAX_SND_URBS		4
#define UDSL_MAX_RCV_BUFS		8
#define UDSL_MAX_SND_BUFS		8
#define UDSL_MAX_RCV_BUF_SIZE		1024	/* ATM cells */
#define UDSL_MAX_SND_BUF_SIZE		1024	/* ATM cells */
#define UDSL_DEFAULT_RCV_URBS		2
#define UDSL_DEFAULT_SND_URBS		2
#define UDSL_DEFAULT_RCV_BUFS		4
#define UDSL_DEFAULT_SND_BUFS		4
#define UDSL_DEFAULT_RCV_BUF_SIZE	64	/* ATM cells */
#define UDSL_DEFAULT_SND_BUF_SIZE	64	/* ATM cells */


/* receive */

struct usbatm_receive_buffer {
	struct list_head list;
	unsigned char *base;
	unsigned int filled_cells;
};

struct usbatm_receiver {
	struct list_head list;
	struct usbatm_receive_buffer *buffer;
	struct urb *urb;
	struct usbatm_data *instance;
};


/* send */

struct usbatm_send_buffer {
	struct list_head list;
	unsigned char *base;
	unsigned char *free_start;
	unsigned int free_cells;
};

struct usbatm_sender {
	struct list_head list;
	struct usbatm_send_buffer *buffer;
	struct urb *urb;
	struct usbatm_data *instance;
};


/* main driver data */

struct usbatm_data {
	/******************
	*  public fields  *
        ******************/

	/* mini driver */
	struct usbatm_driver *driver;
	void *driver_data;
	char driver_name[16];

	/* USB device */
	struct usb_device *usb_dev;
	struct usb_interface *usb_intf;
	char description[64];
	int tx_endpoint;
	int rx_endpoint;
	int tx_padding;
	int rx_padding;

	/* ATM device */
	struct atm_dev *atm_dev;

	/********************************
	*  private fields - do not use  *
        ********************************/

	struct kref refcount;
	struct semaphore serialize;

	/* heavy init */
	int thread_pid;
	struct completion thread_started;
	struct completion thread_exited;

	/* ATM device */
	struct list_head vcc_list;

	/* receive */
	struct usbatm_receiver receivers[UDSL_MAX_RCV_URBS];
	struct usbatm_receive_buffer receive_buffers[UDSL_MAX_RCV_BUFS];

	spinlock_t receive_lock;
	struct list_head spare_receivers;
	struct list_head filled_receive_buffers;

	struct tasklet_struct receive_tasklet;
	struct list_head spare_receive_buffers;

	/* send */
	struct usbatm_sender senders[UDSL_MAX_SND_URBS];
	struct usbatm_send_buffer send_buffers[UDSL_MAX_SND_BUFS];

	struct sk_buff_head sndqueue;

	spinlock_t send_lock;
	struct list_head spare_senders;
	struct list_head spare_send_buffers;

	struct tasklet_struct send_tasklet;
	struct sk_buff *current_skb;			/* being emptied */
	struct usbatm_send_buffer *current_buffer;	/* being filled */
	struct list_head filled_send_buffers;
};

#endif	/* _USBATM_H_ */
