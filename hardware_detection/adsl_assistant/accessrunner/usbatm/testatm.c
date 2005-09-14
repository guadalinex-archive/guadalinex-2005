/******************************************************************************
 *  testatm.c  -  driver for testing the usbatm core
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

#include <linux/delay.h>
#include <linux/module.h>

#include "usbatm.h"

uint	driver;
module_param(driver, uint, 0444);

uint	idVendor;
module_param(idVendor, uint, 0444);

uint	idProduct;
module_param(idProduct, uint, 0444);

static int accept_bind(struct usbatm_data *usbatm, struct usb_interface *intf, const struct usb_device_id *id, int *need_heavy_init)
{
	dbg("accept_bind");

	return 0;
}

static int refuse_bind(struct usbatm_data *usbatm, struct usb_interface *intf, const struct usb_device_id *id, int *need_heavy_init)
{
	dbg("refuse_bind");

	return -1;
}

static int claim_bind(struct usbatm_data *usbatm, struct usb_interface *intf, const struct usb_device_id *id, int *need_heavy_init);

void testatm_unbind(struct usbatm_data *usbatm, struct usb_interface *intf)
{
	dbg("testatm_unbind");
}

void claim_unbind(struct usbatm_data *usbatm, struct usb_interface *intf);

static int accept_atm(struct usbatm_data *usbatm, struct atm_dev *dev)
{
	dbg("accept_atm");

	return 0;
}

static int refuse_atm(struct usbatm_data *usbatm, struct atm_dev *dev)
{
	dbg("refuse_atm");

	return -1;
}

void testatm_atm_stop(struct usbatm_data *usbatm, struct atm_dev *dev)
{
	dbg("testatm_atm_stop");
}

static int trivial_accept_heavy_init(struct usbatm_data *usbatm, struct usb_interface *intf)
{
	dbg("trivial_accept_heavy_init");

	return 0;
}

static int trivial_refuse_heavy_init(struct usbatm_data *usbatm, struct usb_interface *intf)
{
	dbg("trivial_refuse_heavy_init");

	return -1;
}

static int slow_accept_heavy_init(struct usbatm_data *usbatm, struct usb_interface *intf)
{
	unsigned long ret;

	dbg("slow_accept_heavy_init");

	ret = msleep_interruptible (30 * 60 * 1000);

	dbg("slow_accept_heavy_init done (%lu)", ret);

	return 0;
}

static int slow_refuse_heavy_init(struct usbatm_data *usbatm, struct usb_interface *intf)
{
	unsigned long ret;

	dbg("slow_refuse_heavy_init");

	ret = msleep_interruptible (30 * 60 * 1000);

	dbg("slow_refuse_heavy_init done (%lu)", ret);

	return -1;
}

static struct usbatm_driver testatm_drivers[] = {
	{
		.owner          = THIS_MODULE,
	        .driver_name    = "testatm_0"
	},
	{
		.owner          = THIS_MODULE,
	        .driver_name    = "testatm_1",

		.bind		= refuse_bind
	},
	{
		.owner          = THIS_MODULE,
	        .driver_name    = "testatm_2",

		.bind		= accept_bind,

		.atm_start	= refuse_atm
	},
	{
		.owner          = THIS_MODULE,
	        .driver_name    = "testatm_3",

		.bind		= accept_bind,
		.unbind		= testatm_unbind,

		.atm_start	= accept_atm,
		.atm_stop	= testatm_atm_stop
	},
	{
		.owner          = THIS_MODULE,
	        .driver_name    = "testatm_4",

		.bind		= accept_bind,
		.unbind		= testatm_unbind,

		.heavy_init	= trivial_refuse_heavy_init,

		.atm_start	= refuse_atm
	},
	{
		.owner          = THIS_MODULE,
	        .driver_name    = "testatm_5",

		.bind		= accept_bind,
		.unbind		= testatm_unbind,

		.heavy_init	= trivial_accept_heavy_init,

		.atm_start	= refuse_atm
	},
	{
		.owner          = THIS_MODULE,
	        .driver_name    = "testatm_6",

		.bind		= accept_bind,
		.unbind		= testatm_unbind,

		.heavy_init	= slow_refuse_heavy_init,

		.atm_start	= accept_atm,
		.atm_stop	= testatm_atm_stop
	},
	{
		.owner          = THIS_MODULE,
	        .driver_name    = "testatm_7",

		.bind		= accept_bind,
		.unbind		= testatm_unbind,

		.heavy_init	= slow_accept_heavy_init,

		.atm_start	= accept_atm,
		.atm_stop	= testatm_atm_stop
	},
	{
		.owner          = THIS_MODULE,
	        .driver_name    = "testatm_8",

		.bind		= claim_bind,
		.unbind		= claim_unbind,
	}
};

static int testatm_usb_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usbatm_driver *test_driver = &testatm_drivers[driver];

        return usbatm_usb_probe(intf, id, test_driver);
}

static struct usb_device_id testatm_usb_ids[] = {
	{}, {}
};

static struct usb_driver testatm_usb_driver = {
	.owner		= THIS_MODULE,
	.name		= "testatm",
	.probe		= testatm_usb_probe,
	.disconnect	= usbatm_usb_disconnect,
	.id_table	= testatm_usb_ids
};

static void release_interfaces(struct usb_device *usb_dev, int num_interfaces) {
	struct usb_interface *cur_intf;
	int i;

	for(i = 0; i < num_interfaces; i++)
		if ((cur_intf = usb_ifnum_to_if(usb_dev, i))) {
			usb_set_intfdata(cur_intf, NULL);
			usb_driver_release_interface(&testatm_usb_driver, cur_intf);
		}
}

static int claim_bind(struct usbatm_data *usbatm, struct usb_interface *intf, const struct usb_device_id *id, int *need_heavy_init)
{
	struct usb_device *usb_dev = interface_to_usbdev(intf);
	struct usb_interface *cur_intf;
	int ifnum = intf->altsetting->desc.bInterfaceNumber;
	int num_interfaces = usb_dev->actconfig->desc.bNumInterfaces;
	int i, ret;

	dbg("claim_bind");

	for (i=0; i < num_interfaces; i++) {
		cur_intf = usb_ifnum_to_if(usb_dev, i);

		if ((i != ifnum) && cur_intf) {
			ret = usb_driver_claim_interface(&testatm_usb_driver, cur_intf, usbatm);

			if (ret < 0) {
				usb_dbg(usbatm, "%s: failed to claim interface %d (%d)\n", __func__, i, ret);
				release_interfaces(usb_dev, i);
				return ret;
			}
		}
	}

	return 0;
}

void claim_unbind(struct usbatm_data *usbatm, struct usb_interface *intf)
{
	struct usb_device *usb_dev = interface_to_usbdev(intf);
	int num_interfaces = usb_dev->actconfig->desc.bNumInterfaces;

	dbg("claim_unbind");

	release_interfaces(usb_dev, num_interfaces);
}

static int __init testatm_init(void)
{
	struct usbatm_driver *test_driver = &testatm_drivers[driver];

	dbg("testatm_init");

	if (((char *)test_driver - (char *)testatm_drivers) >= sizeof(testatm_drivers))
		return -EINVAL;

	printk("idVendor: %04x\n", idVendor);
	printk("idProduct: %04x\n", idProduct);

	testatm_usb_ids[0].match_flags	= USB_DEVICE_ID_MATCH_DEVICE;
	testatm_usb_ids[0].idVendor	= idVendor;
	testatm_usb_ids[0].idProduct	= idProduct;

	return usb_register(&testatm_usb_driver);
}
module_init(testatm_init);

static void __exit testatm_exit(void)
{
	dbg("testatm_exit entered");

	usb_deregister(&testatm_usb_driver);
}
module_exit(testatm_exit);

MODULE_LICENSE("GPL");
