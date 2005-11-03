
/*
 *
 * Copyright (c) 2003, Frederick Ros (sl33p3r@free.fr)
 * Forked from ADI Linux driver from Analog Devices Inc.,
 * User space interface rewritten by C.Casteyde (casteyde.christian@free.fr)
 * Multi-modem support added by Renaud Guerin (rguerin@freebox.fr)
 * Other stuff : Frederick Ros (sl33p3r@free.fr)
 *										 
 * eagle-usb.c				 
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
 * $Id: eu_main.c,v 1.25 2005/01/17 20:54:42 sleeper Exp $
 */

#include "Adiutil.h"
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/if_arp.h>
#include "eagle-usb.h"
#include "macros.h"
#include "Dsp.h"
#include "eu_msg.h"
#include "Pipes.h"
#include "eu_utils.h"
#include "eu_sm.h"
#include "Oam.h"
#include "Mpoa.h"
#include "Me.h"
#include "Uni.h"
#include "debug.h"
#include "eu_eth.h"
#include "eu_boot_sm.h"

/* ------------------------------- Private Macros ------------------------------- */

#define CASE_PREFIRM	EAGLE_I_PID_PREFIRM:    \
    case MILLER_A_PID_PREFIRM:                  \
    case MILLER_B_PID_PREFIRM:                  \
    case HEINEKEN_A_PID_PREFIRM:                \
    case HEINEKEN_B_PID_PREFIRM:                \
    case EAGLE_IIC_PID_PREFIRM:                 \
    case EAGLE_II_PID_PREFIRM:                  \
    case EAGLE_III_PID_PREFIRM


#define CASE_POSTFIRM	EAGLE_I_PID_PSTFIRM:    \
    case MILLER_A_PID_PSTFIRM:                  \
    case MILLER_B_PID_PSTFIRM:                  \
    case HEINEKEN_A_PID_PSTFIRM:                \
    case HEINEKEN_B_PID_PSTFIRM:                \
    case EAGLE_II_PID_PSTFIRM:                  \
    case EAGLE_IIC_PID_PSTFIRM:                 \
    case EAGLE_III_PID_PSTFIRM

#define ISPREFIRM(c) ( EAGLE_I_PID_PREFIRM == (c)                                     ||        \
                       MILLER_A_PID_PREFIRM   == (c) || MILLER_B_PID_PREFIRM == (c)   ||        \
                       HEINEKEN_A_PID_PREFIRM == (c) || HEINEKEN_B_PID_PREFIRM == (c) ||        \
                       EAGLE_II_PID_PREFIRM   == (c) || EAGLE_IIC_PID_PREFIRM == (c)  ||        \
                       EAGLE_III_PID_PREFIRM  == (c)                                            \
                     )

#define ISPOSTFIRM(c) ( EAGLE_I_PID_PSTFIRM == (c)                                     ||       \
                        MILLER_A_PID_PSTFIRM   == (c) || MILLER_B_PID_PSTFIRM == (c)   ||       \
                        HEINEKEN_A_PID_PSTFIRM == (c) || HEINEKEN_B_PID_PSTFIRM == (c) ||       \
                        EAGLE_II_PID_PSTFIRM   == (c) || EAGLE_IIC_PID_PSTFIRM == (c)  ||       \
                        EAGLE_III_PID_PSTFIRM  == (c)                                           \
                      )


/*
 * Hardcoded endpoint addresses
 */
#define EP_BULK_IDMA_OUT       0x04
#define EP_BULK_DATA_OUT       0x02
#define EP_BULK_DATA_IN        0x82
#define EP_ISOC_DATA_IN        0x88
#define EP_INT_IN              0x84

#define USB_INTF_IN            0x02
#define FASTEST_ISO_INTF       0x08

/*
 * Interrupts
 */
#define EU_INT_LOADSWAPPAGE   0x01
#define EU_INT_INCOMINGCMV    0x02


/* ----------------------- Private Function Declarations ------------------------ */

/*
 * USB related
 */
#ifdef LINUX_2_4
static void *eu_probe(struct usb_device *usb, unsigned int ifnum, const struct usb_device_id *id);
static void eu_disconnect(struct usb_device *usb, void *ptr);
static int eu_user(struct usb_device *dev, unsigned int code, void *buf);
#elif defined (LINUX_2_6)
static int eu_probe ( struct usb_interface *intf, const struct usb_device_id *id);
static void eu_disconnect(struct usb_interface *intf);
static int eu_user(struct usb_interface *intf, unsigned int code, void *buf);
#endif

static USB_COMPLETION_PROTO (eu_irq,urb,regs);

static int eu_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data);

/*
 * Ethernet device related
 */
static eu_instance_t *eu_init_prefirm ( struct usb_device *usb );
#ifdef LINUX_2_6
static eu_instance_t * eu_init_postfirm ( struct usb_device *usb, struct usb_interface *intf );
#else
static eu_instance_t * eu_init_postfirm ( struct usb_device *usb );
#endif
static void eu_disconnect_postfirm ( eu_instance_t *ins , struct usb_device *usb );
static void eu_process_rcv ( unsigned long data );
static int eu_check_options ( const eu_options_t opt );

/* ----------------------------- Private Variables ------------------------------ */


/*
 * List of supported VID/PID
 */
static const struct usb_device_id eu_ids[] =
{
    {  USB_DEVICE (EAGLE_VID, EAGLE_I_PID_PREFIRM) },
    {  USB_DEVICE (EAGLE_VID, EAGLE_I_PID_PSTFIRM) }, 
    {  USB_DEVICE (EAGLE_VID, EAGLE_II_PID_PREFIRM) },
    {  USB_DEVICE (EAGLE_VID, EAGLE_II_PID_PSTFIRM) },
    {  USB_DEVICE (EAGLE_VID, EAGLE_IIC_PID_PREFIRM) },
    {  USB_DEVICE (EAGLE_VID, EAGLE_IIC_PID_PSTFIRM) },
    {  USB_DEVICE (EAGLE_VID, EAGLE_III_PID_PREFIRM) },
    {  USB_DEVICE (EAGLE_VID, EAGLE_III_PID_PSTFIRM) },
    {  USB_DEVICE (USR_VID, MILLER_A_PID_PREFIRM) },
    {  USB_DEVICE (USR_VID, MILLER_A_PID_PSTFIRM) },
    {  USB_DEVICE (USR_VID, MILLER_B_PID_PREFIRM) },
    {  USB_DEVICE (USR_VID, MILLER_B_PID_PSTFIRM) },
    {  USB_DEVICE (USR_VID, HEINEKEN_A_PID_PREFIRM) },
    {  USB_DEVICE (USR_VID, HEINEKEN_A_PID_PSTFIRM) },
    {  USB_DEVICE (USR_VID, HEINEKEN_B_PID_PREFIRM) },
    {  USB_DEVICE (USR_VID, HEINEKEN_B_PID_PSTFIRM) },
    { }
};

/*
 * USB driver descriptor
 */
static struct usb_driver eu_driver =
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,19)
    .owner      = THIS_MODULE,
#endif    
    .name       = "eagle-usb",
    .id_table   = eu_ids,
    .probe      = eu_probe,
    .disconnect = eu_disconnect,
    .ioctl      = eu_user
};

/*
 * Linked list of modem eu_instance_t structs
 */
LIST_HEAD(modem_list);


/*
 * Our /proc dir entry
 */
struct proc_dir_entry* eu_procdir;


#define DEFAULT_OPTN0    0x80020066
#define DEFAULT_OPTN2    0x23700000
#define DEFAULT_OPTN3    0x00000000
#define DEFAULT_OPTN4    0x00000000
#define DEFAULT_OPTN5    0x00000000
#define DEFAULT_OPTN6    0x00000000
#define DEFAULT_OPTN7    0x02CD8044
#define DEFAULT_OPTN15   0x00000000
#define DEFAULT_VPI      0x00000008
#define DEFAULT_VCI      0x00000023
#define DEFAULT_ENCAPS   MPOA_MODE_PPPOA_VC
#define DEFAULT_LINETYPE 0x00000001
#define DEFAULT_POLLFREQ 0x0000000A

/*
 * Default driver options
 */
static const eu_options_t default_options =
{
    { "OPTN0", DEFAULT_OPTN0 },
    { "OPTN2", DEFAULT_OPTN2 },
    { "OPTN3", DEFAULT_OPTN3 },
    { "OPTN4", DEFAULT_OPTN4 },
    { "OPTN5", DEFAULT_OPTN5 },
    { "OPTN6", DEFAULT_OPTN6 },
    { "OPTN7", DEFAULT_OPTN7 },
    { "OPTN15", DEFAULT_OPTN15 },
    { "VPI", DEFAULT_VPI },
    { "VCI", DEFAULT_VCI },
    { "Encapsulation", DEFAULT_ENCAPS },
    { "Linetype", DEFAULT_LINETYPE },
    { "RatePollFreq", DEFAULT_POLLFREQ },
};

/*
 * User supplied name for ethernet interface
 */
static char *if_name = NULL;

/*
 * User supplied debug level
 */
unsigned int module_dbg_mask = 0x0;

/*
 * Chipset version
 */
static unsigned int chipset_version = 0;


/* -------------------------------- Module Stuff -------------------------------- */

MODULE_AUTHOR ("Anoosh Naderi <anoosh.naderi@analog.com>/Frederick Ros (sl33p3r@free.fr)");
MODULE_DESCRIPTION ("Eagle USB ADSL Modem driver");
MODULE_DEVICE_TABLE (usb, eu_ids);
MODULE_LICENSE("GPL");
MODULE_PARM (if_name,"s");
MODULE_PARM_DESC (if_name,"Exported ethernet interface name");
MODULE_PARM (module_dbg_mask,"i");
MODULE_PARM_DESC (module_dbg_mask,"Module Debug mask");

EXPORT_NO_SYMBOLS;




/* -----------------------  INITIALIZATION / DESTRUCTION ------------------------ */



/**
 * eu_init - Initialize the module.
 *      Generates CRC table
 *      Creates /proc/driver/eagle-usb directory
 *      Register to USB subsystem
 *
 */
static int __init eu_init (void)
{
    int result = 0;
    
    eu_enters (DBG_INIT);
    
    
    eu_report ("driver V"EAGLEUSBVERSION" loaded\n");
    
    eu_crc_generate();
    
    eu_procdir = proc_mkdir("driver/eagle-usb",NULL);
    
    if ( eu_procdir )
    {
       eu_procdir->owner = THIS_MODULE;
    }
    else
    {
        eu_report ("could not create /proc/driver/eagle-usb/\n");
        result = -ENOMEM;
    }


    usb_register(&eu_driver);
    
    
    eu_leaves (DBG_INIT);
    
    return 0;
}

module_init (eu_init);


/**
 * eu_exit  -  Finalize module
 *
 *    Deregister with USB subsystem
 *    Remove /proc/drivers/eagle-usb directory
 *
 */
static void __exit eu_exit (void)
{
    eu_enters (DBG_INIT);
    
    /*
     * This calls automatically the eu_disconnect method if necessary:
     */
    usb_deregister (&eu_driver);
    
    eu_report ("driver unloaded\n");
    remove_proc_entry("driver/eagle-usb",NULL);
    
    eu_leaves (DBG_INIT);
    
}

module_exit (eu_exit);


/**
 * eu_probe  -  Ask driver to probe this device
 *
 */
#ifdef LINUX_2_4
static void *eu_probe (
                       struct usb_device *usb,
                       unsigned int ifnum,
                       const struct usb_device_id *id
                      )
#elif defined(LINUX_2_6)
static int eu_probe (
                     struct usb_interface *intf,
                     const struct usb_device_id *id
                    )    
#endif
    
{
    void     *ins = NULL;
#ifdef LINUX_2_6
    struct usb_device *usb = interface_to_usbdev (intf);
#endif
    uint32_t  pid  = usb->descriptor.idProduct;
    
    
    eu_enters (DBG_INIT);
    
    eu_dbg (DBG_INIT,"vid (%#X) pid (%#X) \n",
             usb->descriptor.idVendor, usb->descriptor.idProduct);

    /*
     * This driver knows only pre and postfirmware devices.
     */
    if ( !ISPREFIRM(pid) && !ISPOSTFIRM(pid) )
    {
        eu_dbg (DBG_INIT," Not a supported modem\n");
        goto byebye;
    }    

    switch ( pid )
    {
        case CASE_PREFIRM:
            
            ins = eu_init_prefirm ( usb );
            break;
            
        case CASE_POSTFIRM:
#ifdef LINUX_2_6            
            ins = eu_init_postfirm ( usb, intf );
#else
            ins = eu_init_postfirm ( usb );
#endif
            
            break;
    }
    
    eu_leaves (DBG_INIT);

  byebye:
#ifdef LINUX_2_6
    if (ins) 
    {

        usb_set_intfdata (intf, ins);

        return (0);
    }
    else
    {
        return -ENODEV;
    }
#else   
    return (ins);
#endif 
}



/*
 * eu_init_prefirm - Initialize pre-firmware device
 *
 * @usb  -  USB device to init.
 */
static eu_instance_t *eu_init_prefirm ( struct usb_device *usb ) 
{
    eu_instance_t *ins    = NULL;
    int            ret;
    uint32_t       pid    = usb->descriptor.idProduct;

    eu_enters (DBG_INIT);
    
    /*
     * Create a new pre-firmware device:
     */
    ins = GET_KBUFFER (sizeof(eu_instance_t));
    if ( !ins )
    {
        eu_err ("Not enough memory to get new driver structure..\n");
        goto byebye;
    }

    eu_report ("New pre-firmware modem detected\n");
    
    /*
     * Reinitialize the modem structure:
     */
    memset (ins, 0, sizeof(eu_instance_t));
    ins->usbdev = usb;
    
    EU_CLEAR_FLAG (ins, EU_MSG_INITIALIZED);
    EU_CLEAR_FLAG (ins, EU_UNPLUG);
            
            
    eu_report ("Uploading firmware..\n");
    
    ret = eu_load_firmware ( ins, pid, &chipset_version );
    if ( ret < 0 ) 
    {
        eu_err ("Can't upload firmware to modem...\n");
        FREE_KBUFFER (ins);
        ins = NULL;
        goto byebye;
    }
    
    eu_report ("Binding eu_instance_t to USB %03d/%03d\n",
               usb->bus->busnum, usb->devnum);
    
    list_add (&ins->list,&modem_list);

  byebye:
    eu_leaves ( DBG_INIT);
    
    return(ins);
            
}

/**
 * eu_init_postfirm  - Initialize post firmware device
 *
 * @usb  -  USB device to init
 */
#ifdef LINUX_2_6
static eu_instance_t * eu_init_postfirm ( struct usb_device *usb, struct usb_interface *intf )
#else    
static eu_instance_t * eu_init_postfirm ( struct usb_device *usb )
#endif
{
    int                             i;
    eu_instance_t                  *ins = NULL;
    char                            path[32];
    struct proc_dir_entry*          procfile;
    int                             ret = 0;
    unsigned int                    nb_frames;
    int tmp;
    
    
    eu_enters (DBG_INIT);

    /*
     * Create a new post-firmware device:
     */
    ins = GET_KBUFFER (sizeof (eu_instance_t));
    
    if ( !ins )
    {
        eu_err ("eu_init_postfirm: Not enough memory !\n");
        goto byebye;
    }
            
    /*
     * Initialize our instance
     */
    memset ( ins, 0, sizeof(eu_instance_t) );
    ins->usbdev = usb;
    
    EU_CLEAR_FLAG ( ins, EU_MSG_INITIALIZED);
    EU_CLEAR_FLAG ( ins, EU_UNPLUG);    

    init_waitqueue_head (&ins->sync_q);
    ins->lock = SPIN_LOCK_UNLOCKED;

    /*
     * Add our new device's list hook to the modem_list
     */
    list_add ( &ins->list,&modem_list );
    
    eu_report ("New USB ADSL device detected, waiting for DSP code...\n");
#ifdef LINUX_2_6
    eu_report ("Interface %d accepted.\n",intf->altsetting[0].desc.bInterfaceNumber);
#endif    

#ifdef USEBULK
    EU_CLEAR_FLAG (ins, EU_LOW_RATE);
#else
    EU_SET_FLAG (ins, EU_LOW_RATE);
#endif

    /*
     * Use constants for endpoint addresses - It saves us from parsing through
     * descriptors ..
     */
    ins->pipe_bulk_idma_out = usb_sndbulkpipe(usb, EP_BULK_IDMA_OUT);
    ins->pipe_bulk_data_out = usb_sndbulkpipe(usb, EP_BULK_DATA_OUT);
    ins->pipe_bulk_data_in  = usb_rcvbulkpipe(usb, EP_BULK_DATA_IN);
    ins->pipe_iso_data_in  = usb_rcvisocpipe(usb, EP_ISOC_DATA_IN);
    ins->pipe_int_in     = usb_rcvintpipe(usb, EP_INT_IN);
    
    /*
     * Make sure we can get all the memory we need
     */
    ins->intr_data = GET_KBUFFER (sizeof (eu_cdc_t));

    if ( !ins->intr_data ) 
    {
        eu_err ("Can't allocate interrupt buffer\n");
        goto free_instance;
    }
    
    ins->segmentation_buffer  = GET_KBUFFER(OUTGOING_DATA_SIZE);

    if ( !ins->segmentation_buffer ) 
    {
        eu_err ("Can't allocate segmentation buffer\n");
        goto free_int_buff;
    }
            
    INIT_LIST_HEAD ( &ins->comp_read_q );
    ins->comp_read_q_lock = SPIN_LOCK_UNLOCKED;

#ifdef USEBULK
    nb_frames     = 0;
#else
    nb_frames     =  FRAMES_PER_ISO_URB;
#endif /* USEBULK */

    /*
     * Get a lookaside cache
     */
    snprintf ( path,32,"eagle-usb-%03d-%03d",
               ins->usbdev->bus->busnum,
               ins->usbdev->devnum);
    
    ins->rb_cache = kmem_cache_create ( path, sizeof(eu_rb_t),0,0, NULL,NULL);

    if ( !ins->rb_cache ) 
    {                
        eu_err ("eu_init_postfirm : Can't get lookaside cache.\n");
        goto free_seg_buff;
    }

    for ( i=0; i < INCOMING_Q_SIZE; i++ ) 
    {
        ins->read_urb[i]   = USB_ALLOC_URB ( nb_frames, GFP_ATOMIC );

        if ( !ins->read_urb[i] ) 
        {
            eu_err ("eu_init_postfirm : Can't allocate URB \n");
            goto free_urbs;
        }

        ins->read_urb[i]->transfer_buffer = NULL;
        
    }
            

    ins->pOAMCell = GET_KBUFFER(128);
    if (ins->pOAMCell == 0 )
    {
        eu_err ("eu_init_postfirm : Can't allocate memory\n");
        goto free_urbs;
    }
            
    ins->mru = 0;
            
    /*
     * Initialize the CTRL URB queue
     */
    ret = alloc_queued_urb_ctrl ( ins, CTRL_URB_Q_SIZE );
            
    if (ret != 0)
    {
        eu_err ("eu_init_postfirm : alloc_queued_urb_ctrl out of memory\n");
        goto free_oam_cell;
    }

    INIT_LIST_HEAD(&ins->ctrl_urb_ipg_q);
    ins->ctrl_q_lock = SPIN_LOCK_UNLOCKED;
    ins->ctrl_urb_failed= FALSE;

    /*
     * Initialize needed kernel timers.
     * WatchdogTimer is the "management entity", responsible for initiating a regular
     * status check of the modem
     */
    init_timer(&ins->AdiModemSm.timerSM);
    ins->AdiModemSm.timerSM.function = WatchdogTimer;
    ins->AdiModemSm.timerSM.data     = (unsigned long) ins;

    /*
     * As UHCI does not allow to queue control urbs, we make our own queueing
     */
    init_timer(&ins->ctrl_urb_q_timer);
    ins->ctrl_urb_q_timer.function  = ctrl_urb_q_watcher;
    ins->ctrl_urb_q_timer.data      = (unsigned long) ins; 

    init_timer (&ins->ctrl_urb_retry);
    ins->ctrl_urb_retry.function = ctrl_urb_retry_send;
    ins->ctrl_urb_retry.data = (unsigned long) ins;
    ins->ctrl_urb_failed = NULL;
            

    /*
     * Get interrupt urb
     */
    ins->urb_int   = USB_ALLOC_URB ( 0, GFP_ATOMIC );
    
    if ( !ins->urb_int ) 
    {
        eu_err ("eu_init_postfirm : Can't allocate INT URB \n");
        goto free_oam_cell;
    }


    /*
     * Get write urb
     */
    ins->urb_write   = USB_ALLOC_URB ( 0, GFP_ATOMIC );
    
    if ( !ins->urb_write ) 
    {
        eu_err ("eu_init_postfirm : Can't allocate WRITE URB \n");
        goto free_urb_int;
    }

    /*
     * Get oam_write urb
     */
    ins->urb_oam_write   = USB_ALLOC_URB ( 0, GFP_ATOMIC );
    
    if ( !ins->urb_oam_write ) 
    {
        eu_err ("eu_init_postfirm : Can't allocate OAM WRITE URB \n");
        goto free_urb_write;
    }

    
    /*
     * Other init stuff
     */
    ins->AdiModemSm.HeartbeatCounter = 0; 
    ins->AdiModemSm.CurrentAdiState  = STATE_JUST_PLUGGED_IN;

    init_timer(&ins->OAMTimer);
    ins->OAMTimer.function  = OAMTimerFunction;
    ins->OAMTimer.data      = (unsigned long)ins;


    /*
     * Interface 1 is for outbound traffic
     */
    tmp = USB_DRIVER_CLAIM_INTERFACE(&eu_driver, GET_INTF_PTR(usb,1), ins);
    if ( tmp != 0 ) 
    {
        eu_report ("Failed to claim interface 1 (%d)\n",-tmp);
        goto free_oam_timer;
    }
    
    
    /*
     * Interface 2 is for inbound traffic
     */
    tmp = USB_DRIVER_CLAIM_INTERFACE(&eu_driver, GET_INTF_PTR(usb,2), ins);
    if ( tmp != 0 ) 
    {
        eu_report ("Failed to claim interface 2 (%d)\n",-tmp);
        goto release_intf_1;
    }

    ins->users = 3;
    
    
#ifndef USEBULK

    /*
     * Set alternate interface to 8, which is ISO transport
     * with the max. packet size ( about 1007 bytes)
     */
    if (usb_set_interface(usb, USB_INTF_IN, FASTEST_ISO_INTF) < 0)
    {                
        eu_err ("usb_set_interface failed on iso alt 8\n");
    }
#endif /* USEBULK */
            
            
    /*
     * Setup our /proc interface
     */
    snprintf ( path,32,"%03d-%03d",ins->usbdev->bus->busnum, ins->usbdev->devnum);
    
    procfile=create_proc_read_entry(path,0, eu_procdir, eu_read_proc, ins);
            
    if (procfile)
    {
        procfile->owner=THIS_MODULE;
        eu_report ("created proc entry at : /proc/driver/eagle-usb/%s\n",
                   path);
    }
    else
    {
        eu_err ("failed to create proc entry at : /proc/driver/%s !\n",
                path);
    }

    /*
     * Get MAC address
     */
    eu_get_mac_addr ( ins );
    
    
    /*
     * Initialize wait queue
     */
    init_waitqueue_head (&ins->thr_wait);            

    if ( if_name ) 
    {
        strncpy ( ins->if_name, if_name, IFNAMSIZ-1);
        ins->if_name[IFNAMSIZ-1] = '\0';
    }
    
    /*
     * Initialize tasklets
     */
    tasklet_init ( &ins->rcv_complete_tasklet,
                   eu_process_rcv,
                   (unsigned long) ins );
#ifdef LINUX_2_6
    /*
     * And work
     */
    INIT_WORK (&ins->create_eth,eu_eth_create, ins);
#endif

    /*
     * And boot SM
     */
#ifdef LINUX_2_6
    INIT_WORK (&ins->boot_sm, eu_boot_sm, ins);
#elif defined(LINUX_2_4)
    INIT_TQUEUE (&ins->boot_sm, eu_boot_sm, ins);        
#endif    

    /*
     * And boot state
     */
    ins->boot_state = PRE_BOOT;
    
    
    /*
     * We're successfull !!!!
     */
    eu_dbg (DBG_INIT,"nb of users: %u\n",ins->users);
    goto byebye;

  release_intf_1:
    usb_driver_release_interface(&eu_driver, GET_INTF_PTR(usb,1));    

  free_oam_timer:
    if ( timer_pending ( &ins->OAMTimer ) ) 
    {
        del_timer ( &ins->OAMTimer );
    }
  
  free_urb_write:
    usb_free_urb ( ins->urb_write );
    ins->urb_write = NULL;

  free_urb_int:
    usb_free_urb ( ins->urb_int );
    ins->urb_int = NULL;
    
  free_oam_cell:
    FREE_KBUFFER (ins->pOAMCell );
    
  free_urbs:
    
    for ( i=0; i < INCOMING_Q_SIZE; i++ ) 
    {
        if ( ins->read_urb[i] ) {
            usb_free_urb ( ins->read_urb[i] );
            ins->read_urb[i] = NULL;
        }
        else
        {
            break;
        }
    }
    
    kmem_cache_destroy ( ins->rb_cache );
    
  free_seg_buff:
    FREE_KBUFFER ( ins->segmentation_buffer );

  free_int_buff:
    FREE_KBUFFER ( ins->intr_data );
    
  free_instance:
    FREE_KBUFFER (ins);
    ins = NULL;
    
  byebye:
    eu_leaves ( DBG_INIT);
   return (ins);
            
}


/**
 * eu_disconnect  -  Disconnect an USB device from the system
 *
 * @usb   -   USB Device to disconnect
 * @ptr   -   User Data ( pointer to associated instance )
 *
 */
#ifdef LINUX_2_4
static void eu_disconnect ( struct usb_device *usb, void *ptr )
#elif defined(LINUX_2_6)
static void eu_disconnect ( struct usb_interface *intf )
#endif    
{
#ifdef LINUX_2_4    
    eu_instance_t *ins = (eu_instance_t *)ptr;
#elif defined (LINUX_2_6)
    eu_instance_t *ins = usb_get_intfdata (intf);
    struct usb_device *usb = interface_to_usbdev (intf);
#endif
    
    uint32_t pid = usb->descriptor.idProduct;

    eu_enters (DBG_INIT);
    
    /*
     * We can't do anything if we don't have a valid eu_instance_t pointer
     */
    if (ins == NULL)
    {
        eu_err ("eu_disconnect : No device.\n");
        goto dis_done;
    }

#ifdef LINUX_2_6    
    eu_dbg (DBG_INIT,"intf=0x%p intfdate=0x%p ins = 0x%p\n",
            intf,usb_get_intfdata (intf),ins);
#endif
    
    /*
     * Do nothing if you also have no usbdev associated
     */
    if (ins->usbdev == NULL ) 
    {
        eu_dbg (DBG_INIT,"No usb dev associated.\n");
        goto dis_done;
    }
    
    eu_dbg (DBG_INIT,"ins->usbdev = 0x%p\n",ins->usbdev);
    
    EU_SET_FLAG (ins, EU_UNPLUG);

    eu_dbg (DBG_INIT,"Let check PID\n");
    
    switch (pid)
    {
    case CASE_PREFIRM:
        
        list_del(&ins->list);
        
        FREE_KBUFFER(ins);
        
        eu_report ("Pre-firmware modem removed\n");
	break;
        
    case CASE_POSTFIRM:

/*         ins->usbdev = NULL; */

#ifdef LINUX_2_6
    usb_set_intfdata (intf, NULL);
#endif
        eu_disconnect_postfirm (ins,usb);
        
        ins = NULL;
	break;
    }
    
#ifdef LINUX_2_6
    eu_dbg (DBG_INIT,"intf=0x%p intfdata=0x%p ins=0x%p\n",intf,usb_get_intfdata (intf),ins);
#endif
    
dis_done:
    eu_leaves (DBG_INIT);
}


/**
 * eu_disconnect_postfirm  -  Perform destruction of a postfirmware device
 *
 * @ins  -  Instance to destroy
 *
 */
static void eu_disconnect_postfirm ( eu_instance_t *ins , struct usb_device *usb ) 
{
    int i;
    char path[32];

    /*
     * Unlink pending interrupt URBs
     */
    if ( EU_TEST_FLAG ( ins, EU_HAS_INT ) )
    {
        eu_dbg (DBG_INIT,"Stop interrupt URB\n");
        USB_KILL_URB(ins->urb_int);
        usb_free_urb (ins->urb_int);
        EU_CLEAR_FLAG (ins, EU_HAS_INT);
    }


    /*
     * Decrement the number of user, and destroy this instance only if this
     * number reaches 0
     */
    ins->users --;
    
    eu_dbg (DBG_INIT,"nb of users: %u\n",ins->users);

    if ( ins->users > 0 ) 
    {
        eu_dbg (DBG_INIT,"Still %u user(s).. Do nothing\n",ins->users);
        return;
    }
    
    /*
     * If timers are currently running, delete them:
     */
    if ( timer_pending (&ins->AdiModemSm.timerSM) )
    {
        del_timer ( &ins->AdiModemSm.timerSM );
    }
            
    if ( timer_pending (&ins->ctrl_urb_q_timer) )
    {
        del_timer ( &ins->ctrl_urb_q_timer );
    }
    
    if ( timer_pending (&ins->ctrl_urb_retry) ) 
    {
        del_timer ( &ins->ctrl_urb_retry );
    }
    
    /*
     * Flush all pending task ...
     */
    EU_FLUSH (eu_>boot_sm);
    
    
    /*
     * Free the DSP code:
     */

    
    /*
     * FIXME : don't duplicate DSP code for every modem ?
     */
    FreeDspData ( &ins->MainPage, &ins->pSwapPages, &ins->SwapPageCount );
	    
    if ( timer_pending ( &ins->OAMTimer ) ) 
    {
        del_timer ( &ins->OAMTimer );
    }
    
    
    /*
     * Remove our network interface from the kernel:
     */    
    
    if ( ins->eth )
    {
        /*
         * This will call the eu_close method:
         */
        unregister_netdev ( ins->eth );
        EU_FREE_NETDEV ( ins->eth );
        ins->eth = NULL;
    }
    
    EU_CLEAR_FLAG (ins, EU_ETH_REGISTERED);

    
    /*
     * Free memory we alloced
     */
    FREE_KBUFFER ( ins->intr_data );

    for ( i=0; i<INCOMING_Q_SIZE; i++ )
    {

        if ( ( ins->read_urb[i] ) &&
             ( ins->read_urb[i]->transfer_buffer ) )
        {
            kmem_cache_free ( ins->rb_cache,
                              GET_RBUF (ins->read_urb[i]) );
        }

        if ( ins->read_urb[i] ) 
        {
            USB_KILL_URB ( ins->read_urb[i] );
            usb_free_urb ( ins->read_urb[i] );
            ins->read_urb[i] = NULL;
        }
    }
            
    kmem_cache_destroy ( ins->rb_cache );            


    tasklet_kill ( &ins->rcv_complete_tasklet);

    
    FREE_KBUFFER ( ins->pOAMCell ); 

    unlink_ipg_ctrl_urb ( ins );
    free_queued_urb_ctrl (&ins->ctrl_urb_free_q);
    free_queued_urb_ctrl (&ins->ctrl_urb_ipg_q);

    /*
     * Free write urb: it has been unlinked in eu_eth_close
     */
    USB_KILL_URB ( ins->urb_write );
    usb_free_urb ( ins->urb_write );

    /*
     * Free urb_oam_write
     */
    USB_KILL_URB(ins->urb_oam_write);
    usb_free_urb ( ins->urb_oam_write );
    

    /*
     * Tell usb that we no longer claim these interfaces as our property
     */
    
#if 0
    /*
     * Release interrupt interface
     */
    usb_driver_release_interface ( &eu_driver, GET_INTF_PTR (usb,0) );
    
    /*
     * Release outbound interface
     */
    usb_driver_release_interface ( &eu_driver, GET_INTF_PTR (usb,1) );

    /*
     * Release inbound interface
     */
    usb_driver_release_interface ( &eu_driver, GET_INTF_PTR (usb,2) );
#endif

#if USE_CMVS
    if ( ins->pDriverCMVs )
    {
        FREE_VBUFFER ( ins->pDriverCMVs );
    }
#endif /* USE_CMVS */
            
    list_del ( &ins->list );
            
    snprintf ( path,32,"%03d-%03d",usb->bus->busnum, usb->devnum );
    remove_proc_entry ( path, eu_procdir );
            
    FREE_KBUFFER ( ins );
            
    eu_report ("ADSL device removed\n");
}


/*******************************************************************************/
/* eu_irq								       */
/*******************************************************************************/
static USB_COMPLETION_PROTO (eu_irq,urb,regs)
{
    eu_instance_t *ins;

    eu_enters (DBG_INTS);
    
    /*
     * Instance is in the URB context.
     */
    ins = (eu_instance_t *)urb->context;
    
    if ( NULL == ins )
    {
        eu_err ("eu_irq : No device.\n");
        goto irq_done;
    }
    
    if ( !EU_TEST_FLAG(ins, EU_HAS_INT) )
    {
        eu_err ("eu_irq : Callback on unregistered interrupt handler!\n");
        goto irq_done;
    }

    if  (
         ( urb->status == -ENOENT )     ||
         ( urb->status == -ECONNRESET ) ||
         ( urb->status == -ESHUTDOWN )  ||
         ( urb->status == -EILSEQ )     ||
         ( urb->status == -ENODEV )
        )
    {
        /*
         *  User cancelled this URB : do nothing, and don't reset status to 0,
         *  or the URB will be re-used
         */
        eu_report ("eu_irq : URB canceled by user.\n");
        EU_CLEAR_FLAG (ins, EU_HAS_INT);
        
        goto irq_done;
    }
        
    /*
     * Lets check the status first, to make sure this was succesfull
     */
    if ( urb->status < 0 )
    {        
        eu_err ("eu_irq : URB status indicates error (%d)\n",
                urb->status);
        urb->status = 0;
        goto resubmit;
    }

    /*
     * All is well, we can process the data!
     */
    if ( ins->intr_data->bRequestType == 0x08 ) /* device-to-host interrupt */
    {
        eu_devint_t *pintr;

        pintr = (eu_devint_t *) (&(ins->intr_data->data[0]));
        
        switch ( cpu_to_le16 (pintr->intr) )
        {
            case EU_INT_LOADSWAPPAGE:
            {
                ins->swap_data = cpu_to_le16 (pintr->intr_info.swap_data);
                
                eu_dbg (DBG_INTS,"interrupt = EU_INT_LOADSWAPPAGE\n");
                
                if ( ins->AdiModemSm.CurrentAdiState != STATE_HARD_RESET_INITIATE )
                {
                    ins->boot_state = UPLOAD_P;
                    EU_SCHEDULE ( &ins->boot_sm ) ;                    
                }
            }
            break;
            
            case EU_INT_INCOMINGCMV:
            {
                eu_dbg (DBG_INTS,"interrupt = EU_INT_INCOMINGCMV\n");
                
                ProcessIncomingCmv ( ins, pintr->intr_info.cmv_data );
            }
            break;
            
            default:
                eu_dbg (DBG_INTS,"interrupt = unsupported(%d)\n", pintr->intr);
                
                break;
        }
    }

  resubmit:
    
#ifdef LINUX_2_6
    {
        int ret;

        ret = USB_SUBMIT_URB ( urb, GFP_ATOMIC );
        if ( ret ) 
        {
            eu_err ("eu_irq: URB submission failure (%d)\n",ret);
        }
    }
    
#endif
    
  irq_done:
    eu_leaves (DBG_INTS);
    
}


/**
 * find_hardware - Look for the given hardware in instance list
 *
 */
static eu_instance_t *find_hardware ( struct usb_device *dev )
{
    struct list_head *ptr;
    eu_instance_t         *ins = NULL;
    
    for ( ptr = modem_list.next; ptr != &modem_list; ptr = ptr->next )
    {
        eu_instance_t *entry;
        entry=list_entry (ptr, eu_instance_t, list);
        if ( entry->usbdev == dev )
        {
            ins=entry;
            break;
        }
    }
    
    return ins;
}


/**
 * eu_user  -  ioctl handler for ioctl emited on the USB device file. Ioctls emited
 *             on the network device will be treated by eu_eth_ioctl
 */
#ifdef LINUX_2_4
static int eu_user ( struct usb_device *dev, unsigned int code, void *buf )
#elif defined (LINUX_2_6)
static int eu_user ( struct usb_interface *intf, unsigned int code, void *buf )
#endif    
{
#ifdef LINUX_2_6
    struct usb_device *dev = interface_to_usbdev (intf);
#endif    
    eu_instance_t *ins;
    int retval = -ENOTTY;
    uint32_t pid = dev->descriptor.idProduct;

    /*
     * USB automatically transfers user ioctl structure in kernel
     * address space (the size is already encoded in the ioctl code):
     */
    struct eu_ioctl_info *pIOCTLinfo = (struct eu_ioctl_info *) buf;
    
    MODULE_USER_GET;
    
    
    if ( !(ins = find_hardware(dev)) )
    {
        eu_report ("Could not find eu_instance_t for USB device %03d/%03d\n",
                    dev->bus->busnum, dev->devnum);
        MODULE_USER_RELEASE;
        
        return -ENOTTY;
    }

    /*
     * Set/Get debug flags works for pre/post firmwares.
     */
    if ( code == EU_IO_SETDBG ) 
    {
        if ( NULL == pIOCTLinfo ) 
        {
            eu_err ("EU_IO_SETDBG : No data\n");
            retval = -EINVAL;
            goto byebye;
        }
                
        module_dbg_mask = pIOCTLinfo->idma_start;
        eu_report ("Debug mask set to 0x%x\n",module_dbg_mask);
        retval = 0;
        goto byebye;
    }
    
    if ( code == EU_IO_GETDBG ) 
    {
        if ( NULL == pIOCTLinfo ) 
        {
            eu_err ("EU_IO_GETDBG : No data\n");
            retval = -EINVAL;
            goto byebye;    
        }
        
        pIOCTLinfo->idma_start = module_dbg_mask;
        retval = 0;                
        goto byebye;
    }
    
    /* Check this ioctl if for one of our devices: */
    switch (pid)
    {
        case CASE_PREFIRM:
            eu_err ("In pre-firmware mode only get/set debug ioctl is permitted.\n");
            retval = -EINVAL;
            break;
        case CASE_POSTFIRM:
            switch (code)
            {                    
#if USE_CMVS           
                case EU_IO_CMVS:
                    retval = -ERESTARTSYS;
                    if ( ins->flags & EU_OPEN ) 
                    {
                        retval = -EBUSY;
                        eu_err ("EU_IO_CMVS : Eth device already open.\n");                        
                        goto end_set_cmvs;
                    }

                    
                    if (NULL == pIOCTLinfo) 
                    {
                        eu_err ("EU_IO_CMVS : No data.\n");
                        retval = -EFAULT;
                        goto end_set_cmvs;
                    }

                    if (pIOCTLinfo->buffer == NULL ) 
                    {
                        eu_err ("EU_IO_CMVS: Null input buffer\n");
                        retval = -EINVAL;
                        goto end_set_cmvs;
                    }

                    if ( pIOCTLinfo->buffer_size == 0 ) 
                    {
                        eu_err ("EU_IO_CMVS: Invalid Buffersize\n");
                        retval = -EINVAL;
                        goto end_set_cmvs;    
                    }
                    
                    /* Get a buffer for CMVs */
                    eu_dbg (DBG_INIT,"Allocating %d bytes for CMVs\n",pIOCTLinfo->buffer_size);
                    
                    ins->pDriverCMVs = GET_VBUFFER (pIOCTLinfo->buffer_size);
                    if ( ins->pDriverCMVs == NULL )
                    {
                        eu_err ("EU_IO_CMVS: Not enough memory to get %d bytes\n",
                                 pIOCTLinfo->buffer_size);
                        retval = -ENOMEM;
                        goto end_set_cmvs;
                    }                    
                    
                
                    if (copy_from_user(ins->pDriverCMVs, pIOCTLinfo->buffer, pIOCTLinfo->buffer_size))
                    {
                        retval = -EFAULT;
                        FREE_VBUFFER (ins->pDriverCMVs);
                        eu_err ("EU_IO_CMVS : copy from user failed.\n");
                        goto end_set_cmvs;
                    }


                    eu_report ("ioctl EU_IO_CMVS received and treated.\n");
                    retval = 0;
                    
              end_set_cmvs:
                    
                    break;
#endif /* USE_CMVS */
                    
                case EU_IO_OPTIONS:		/* Set driver options */
                    /*
                     * Option cannot be set while network interface is up
                     */
            

#if USE_CMVS
                    if ( ins->pDriverCMVs == NULL ) 
                    {
                        /* User as not yet sent EU_IO_CMVS */
                        eu_err ("EU_IO_OPTIONS: CMVs not yet sent.\n");
                        retval = -ERESTARTSYS;
                        break;
                    }
#endif /* USE_CMVS */
            
                    if (!( EU_TEST_FLAG (ins, EU_OPEN ) ) )
                    {
                        eu_options_t opt;
                
                        memcpy(&opt, &default_options, sizeof(eu_options_t));
                
                        if (NULL == pIOCTLinfo) 
                        {
                            eu_err ("EU_IO_OPTIONS : No data.\n");
                            retval = -EFAULT;
                            break;
                        }
                
                        /* Check the option array size: */
                        if (sizeof(opt) != pIOCTLinfo->buffer_size) 
                        {
                            eu_err ("EU_IO_OPTIONS : Invalid Data size\n");
                            retval = -EINVAL;
                            break;
                        }
                
                        if (copy_from_user(&opt, pIOCTLinfo->buffer, sizeof(opt)))
                        {
                            retval = -EFAULT;
                            eu_err ("EU_IO_OPTIONS : copy from user failed.\n");
                            break;
                        }

                        
                        /* Options retrieved, check them: */
                        if ( eu_check_options ( opt ) )
                        {
                            /* OK, initialize Mpoa and Msg: */
                            eu_report ("ioctl EU_IO_OPTIONS received\n");
                    
                            /*
                             * Initialize the message handling
                             */
                            if ( !spin_trylock ( &ins->lock ) ) 
                            {
                                eu_err ("EU_IO_OPTIONS : Can't lock\n");
                                retval = -ERESTARTSYS;
                                break;
                            }

                            eu_msg_initialize ( ins, opt );
                            MpoaInitialize(ins, opt);

                            EU_SET_FLAG (ins, EU_MSG_INITIALIZED);
                            
                            spin_unlock ( &ins->lock );

                            retval = 0;
                        }
                    }
                    else
                    {
                        retval = -EBUSY;
                        eu_err ("EU_IO_OPTIONS : Eth device already open.\n");                
                    }
            
                    
                    break;
            
                case EU_IO_DSP:
                    /*
                     * Load DSP code
                     */

                    if ( !EU_TEST_FLAG (ins, EU_MSG_INITIALIZED) ) 
                    {
                        eu_err ("Message handling not initialized."
                                 " Please send options.\n");
                        retval = -EINVAL;
                        break;
                    }
            
                    if ( !EU_TEST_FLAG( ins, EU_OPEN ) )
                    {
                        uint8_t *pBuf = NULL;
                        IDMAPage MainPage;
                        IDMAPage *pSwapPages;
                        uint32_t   SwapPageCount;
                
                        if (NULL == pIOCTLinfo) 
                        {
                            eu_err ("EU_IO_DSP : No data.\n");
                            retval = -EFAULT;
                            break;
                        }
                
                        if (pIOCTLinfo->idma_start >= pIOCTLinfo->buffer_size) 
                        {
                            eu_err ("EU_IO_DSP : Invalid Data Size\n");
                            retval = -EFAULT;
                            break;
                        }
                
                        pBuf = GET_VBUFFER(pIOCTLinfo->buffer_size);
                        
                        if (NULL == pBuf) 
                        {
                            eu_err ("EU_IO_DSP : No memory.\n");
                            retval = -ENOMEM;
                            break;
                        }
                
                        if (copy_from_user(pBuf, pIOCTLinfo->buffer, pIOCTLinfo->buffer_size))
                        {
                            eu_err ("EU_IO_DSP : copy from user failed.\n");
                            retval = -EFAULT;
                            goto free_adsl_dsp;
                        }
                
                        /*
                         * Get our DSP code ready for IDMA booting
                         */
                        eu_report ("ioctl EU_IO_DSP received\n");


                        if ( EU_TEST_FLAG( ins, EU_DSP_IPG ) ) 
                        {
                            retval = -ERESTARTSYS;
                            goto free_adsl_dsp;
                        }

                        EU_SET_FLAG (ins, EU_DSP_IPG);                        
                        
                        retval = ProcessDSPCode ( ins,
                                                  pBuf,
                                                  pIOCTLinfo->idma_start,
                                                  pIOCTLinfo->buffer_size,
                                                  &MainPage,
                                                  &pSwapPages,
                                                  &SwapPageCount
                                                );
                        
                        if (retval != 0)
                        {
                            eu_err ("EU_IO_DSP : ProcessDSPCode failed (%d)\n",
                                     retval);
                            FreeDspData ( &MainPage, &pSwapPages, &SwapPageCount );
                            goto free_adsl_dsp;
                        }

                        EU_SET_FLAG (ins, EU_DSP_LOADED);

                        /*
                         * Flush all pending task ...
                         */
                        EU_FLUSH (eu_>boot_sm);
                        
                        
                        /*
                         * Ok, we got our DSP code, replace the old DSP code by
                         * the new one:
                         */
                        
                        FreeDspData ( &ins->MainPage, &ins->pSwapPages, &ins->SwapPageCount );
                        
                        ins->MainPage.BlockCount = MainPage.BlockCount;
                        ins->MainPage.Blocks = MainPage.Blocks;
                        ins->pSwapPages = pSwapPages;
                        ins->SwapPageCount = SwapPageCount;
                        
                
                        /*
                         * Load the DSP code if necessary:
                         */
                        eu_report ("Loading DSP code to device...\n");
                
                        
                        if ( !EU_TEST_FLAG(ins, EU_HAS_INT) )
                        {
                            /*
                             * Get the USB endpoint for IDMA uploading:
                             */
#ifdef LINUX_2_4
                            struct usb_endpoint_descriptor *epint =
                                GET_INTF_PTR (dev,0)->altsetting[0].endpoint + 0;
                            uint8_t interval = epint->bInterval;
#elif defined(LINUX_2_6)
                            uint8_t interval = 
                                GET_INTF_PTR (dev,0)->altsetting[0].endpoint[0].desc.bInterval;
#endif                            
                            /*
                             * Install the interrupt handler to send IDMA pages to the modem
                             * and handle further incoming data
                             */
                            usb_fill_int_urb ( ins->urb_int,
                                               dev,
                                               ins->pipe_int_in,
                                               ins->intr_data,
                                               sizeof(eu_cdc_t),
                                               eu_irq,
                                               ins,
                                               interval );

                            EU_SET_FLAG (ins, EU_HAS_INT);
                            
                            if ( USB_SUBMIT_URB ( ins->urb_int , GFP_KERNEL ) < 0 )
                            {
                                eu_err ("Unable to submit interrupt URB!\n");
                                retval = -EIO;
                                EU_CLEAR_FLAG (ins, EU_HAS_INT);
                                goto free_adsl_dsp;
                            }

                        }

                        EU_CLEAR_FLAG (ins, EU_DSP_IPG);
                
                        /*
                         * Send the DSP code to the modem:
                         */
                        ins->boot_state = PRE_BOOT;
                        if ( !EU_TEST_FLAG(ins,EU_HAS_INT) || boot_the_modem (ins) != 0)
                        {
                            eu_err ("Unable to load DSP code to device!\n");
                    
                            goto free_adsl_dsp;
                        }
                        else
                        {
                            eu_report ("DSP code successfully loaded to device\n");
                        }
                
                        retval = 0;
                        
                      free_adsl_dsp:
                        FREE_VBUFFER(pBuf);
                    }
                    else
                    {
                        retval = -EBUSY;
                        eu_err ("EU_IO_DSP : Eth device already open\n");
                    }            
                    break;
            
                case EU_IO_GETITF:		/* Get the network interface name */
                {
                    uint32_t length;
                
                    if (NULL == pIOCTLinfo) 
                    {
                        eu_err ("EU_IO_GETITF : No data.\n");
                        retval = -EFAULT;
                        break;
                    }                
                
                    if ( !ins->eth )
                    {
                        eu_err ("EU_IO_GETIF: eth not yet created !!\n");
                        retval = -EFAULT;
                        break;
                    }
                
                    length = strlen(ins->eth->name) + sizeof(char);
                    if ( NULL == pIOCTLinfo->buffer )
                    {
                        /* Get the requested size */
                        pIOCTLinfo->buffer_size = length;
                        retval = 0;
                    }
                    else
                    {
                        /* Get the interface name */
                        if ( pIOCTLinfo->buffer_size < length )
                        {
                            eu_err ("EU_IO_GETITF : Invalid Buffer size\n");
                            retval = -EINVAL;
                            break;
                        }
                        
                        if ( copy_to_user ( pIOCTLinfo->buffer, ins->eth->name, length ) != 0 )
                        {
                            eu_err ("EU_IO_GETITF : copy to user failed\n");
                            retval = -EFAULT;
                            break;
                        }
                        retval = 0;
                    }
                    break;
                }
                break;
            
                case EU_IO_SYNC:		/* Wait for modem "operational" state */
                    if (wait_event_interruptible(ins->sync_q,
                                                 ((ins->AdiModemSm.CurrentAdiState & STATE_OPERATIONAL) == STATE_OPERATIONAL)))
                        retval = -EINTR;
                    else
                        retval = 0;
                    break;
            
                default:
                    /* Bad ioctl */
                    break;
            }
            break;
        default:
            /* Bad device */
            break;
    }

  byebye:
    MODULE_USER_RELEASE;
    return retval;
}

/**
 * eu_read_proc  - /proc interface.
 *
 */
static int eu_read_proc ( char *page,
                          char **start,
                          off_t off,
                          int count,
                          int *eof,
                          void *data
                        )
{
    eu_instance_t *ins = (eu_instance_t *)data;
    int size;
    char *p = page;
    /*
     * If this output needs to be larger than 4K (PAGE_SIZE), we need to do this
     * differently
     */
    p += sprintf(p, "eagle-usb status display\n");
    p += sprintf(p, "-------------------------------------------------------------\n");
    p += sprintf(p, "Driver version %s     Chipset: Eagle%1.1d\n",
                 EAGLEUSBVERSION, chipset_version);
    
    if (ins->usbdev != NULL)
    {
        p += sprintf (p,"Vendor ID : 0x%x     Product ID : 0x%x   Rev: 0x%x\n",
                      ins->usbdev->descriptor.idVendor,
                      ins->usbdev->descriptor.idProduct,
                      ins->usbdev->descriptor.bcdDevice);
        p += sprintf(p, "USB Bus : %03d\t USB Device : %03d\t Dbg mask: 0x%x\n",
                     ins->usbdev->bus->busnum, ins->usbdev->devnum, module_dbg_mask);
    }
    
    if (ins->eth->name)
    {
        p += sprintf(p, "Ethernet Interface : %s\n",ins->eth->name);
    }
    else
    {
        p += sprintf(p, "Ethernet Interface : none\n");
    }
    p += sprintf(p, "MAC: %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
                 ins->mac[0],ins->mac[1],ins->mac[2],ins->mac[3],ins->mac[4],
                 ins->mac[5]);
    
    p += sprintf(p, "Tx Rate  %10d  Rx Rate  %10d\n",
                 ins->AdiModemSm.UpRate / 1024, 
                 ins->AdiModemSm.DownRate / 1024);
    p += sprintf(p, "FEC      %10d  Margin   %10d  Atten    %10d dB\n",
                 ins->AdiModemSm.stats_Uncorr_Blks,
                 ins->AdiModemSm.stats_Cur_SNR & 0xFF,
                 (ins->AdiModemSm.stats_Cur_Atten & 0xFF)/2);
    p += sprintf(p, "VID-CPE  %10d  VID-CO   %10d  HEC      %10d\n",
                 ins->AdiModemSm.INFO14,
                 ins->AdiModemSm.INFO08,
                 ins->AdiModemSm.DIAG03);

    p += sprintf(p, "VPI      %10d  VCI      %10d  Delin         ", 
                 ins->Vc.vpi, ins->Vc.vci);

    /*Delineation is the only one where we print a string instead of a number*/
    if (ins->AdiModemSm.flags & 0x0C00)
	p += sprintf(p, "ERROR\n");
    else
	if (ins->AdiModemSm.flags & 0x0030) 
	    p += sprintf(p, " LOSS\n");
	else
	    p += sprintf(p, " GOOD\n");
    p += sprintf(p, "Cells Tx %10d  Cells Rx %10d\n",
                 ins->Statistics[STAT_CELLS_TX],
                 ins->Statistics[STAT_CELLS_RX]);
    p += sprintf(p, "Pkts Tx  %10d  Pkts Rx  %10d\n",
                 ins->Statistics[STAT_PAKTS_TX],
                 ins->Statistics[STAT_PAKTS_RX]);
    p += sprintf(p, "OAM      %10d  Bad VPI  %10d  Bad CRC  %10d\n",
                  ins->Statistics[STAT_CELLS_OAM_RCVD],
                  ins->Statistics[STAT_CELLS_LOST_VPIVCI],
                  ins->Statistics[STAT_CELLS_LOST_CRC]
                  );
    p += sprintf(p, "Oversiz. %10d\n\n",
                ins->Statistics[STAT_CELLS_LOST_OTHER] );
    
    switch (ins->AdiModemSm.CurrentAdiState)
    {
        case STATE_UNDEFINED:
            p += sprintf(p, "Modem is unplugged from USB.\n");
            break;
        case STATE_JUST_PLUGGED_IN:
            p += sprintf(p, "Modem waiting for driver response.\n");
            p += sprintf (p,"Please send DSP (eaglectrl -d)\n");
            break;
        case STATE_UNTRAIN:
        case STATE_UNTRAIN_TX:
        case STATE_UNTRAIN_RX:
            p += sprintf(p, "Modem is initializing(UNTRAIN)\n");
            break;
        case STATE_INITIALIZING:
        case STATE_INITIALIZING_TX:
        case STATE_INITIALIZING_RX:
            p += sprintf(p, "Modem is initializing(INITIALIZING)\n");
            break;
        case STATE_HARD_RESET_INITIATE:
        case STATE_HARD_RESET_END:
        case STATE_BOOT_WAIT:
        case STATE_BOOT_STAGE_1:
        case STATE_BOOT_STAGE_2:
        case STATE_BOOT_STAGE_3:
            p += sprintf(p, "Modem is booting\n");
            break;
        case STATE_OPERATIONAL:
        case STATE_OPERATIONAL_TX:
        case STATE_OPERATIONAL_RX:
            p += sprintf(p, "Modem is operational\n");
            break;
        case STATE_STALLED_FOREVER:
            p += sprintf(p, "Modem cannot boot. Unplug other devices and retry.\n");
            break;
        default:
            p += sprintf(p, "Unhandled state\n");
            eu_report ("eu_read_proc: unhandled state 0x%X\n", ins->AdiModemSm.CurrentAdiState);            
            break;
    }
    p += sprintf(p, "\n");
    /*
     * Figure out how much of the page we used
     */
    size  = p - page;
    size -= off;
    
    if (size < count)
    {
	*eof = 1; 
	if (size <= 0)
	    return 0;
    }
    else
	size = count;
    /*
     * Fool caller into thinking we started where he told us to in the page
     */
    *start = page + off;
    return size;
}


/**
 * eu_process_rcv  -  Process completed receive queue
 *                    Called as a tasklet
 *
 */
static void eu_process_rcv ( unsigned long data ) 
{
    eu_instance_t *ins = (eu_instance_t *) data;
    unsigned long flags;
/*     uint8_t *pbuf; */
    eu_rb_t *pbuf;
#ifdef USEBULK    
    eu_bulk_rb_t *pbulk;
#else
    int i;
    eu_iso_rb_t *piso;
    eu_iso_frame_t *pf;
#endif    
    int result;
    int count = 0;
    
    eu_enters( DBG_READ );
    
    
    if ( ins == NULL ) 
    {
        eu_err ("eu_process_rcv: Null instance !\n");
        return;
    }
    
    
    /*
     * Treat all messages in the list
     */
    while ( !list_empty ( &ins->comp_read_q ) ) 
    {
        /*
         * Remove head of the list
         */
        spin_lock_irqsave (&ins->comp_read_q_lock, flags);
        /*
         * FIXME : we should use a list function ...
         * if list field position change .. we must be able to find it again ...
         *
         */
        pbuf = list_entry (ins->comp_read_q.next, eu_rb_t, next);
        list_del(ins->comp_read_q.next);
        
        spin_unlock_irqrestore (&ins->comp_read_q_lock, flags);

#ifdef USEBULK

        pbulk = (eu_bulk_rb_t *) pbuf;

        count ++;
        
        result = eu_uni_process_in_data ( ins, &pbulk->data[0], pbulk->length );
        if (result) 
        {
            eu_dbg (DBG_READ,"Error %d from eu_uni_process_in_data\n", result);        
        }

#else        
        /*
         * Now process each frame
         */
        piso = (eu_iso_rb_t *) pbuf;
        pf = &piso->frames[0];
        
        for ( i=0 ; i<FRAMES_PER_ISO_URB; i++ ) 
        {

            if ( pf->status == 0 ) 
            {
                count ++;
                
                result = eu_uni_process_in_data ( ins,
                                                  &pf->data[0],
                                                  pf->length
                                                );
                if ( result != 0 ) 
                {
                    eu_dbg (DBG_READ,"Error %d from eu_uni_process_in_data\n",
                             result);
                }   
            }

            pf ++;    
        }
#endif /* USEBULK */
        
        /*
         * And free the transfer buffer
         */
        kmem_cache_free ( ins->rb_cache, pbuf );            

    }
    eu_dbg( DBG_READ ," %d frames processed.\n",count);
    
    eu_leaves ( DBG_READ );
    
}


/* ------------------------------------ Misc ------------------------------------ */


/**
 * eu_check_options - Check given options are accepted
 */
static int eu_check_options ( const eu_options_t opt )
{
    int result = 1;
    if (opt[CFG_ENCAPS].value != MPOA_MODE_BRIDGED_ETH_LLC &&
	opt[CFG_ENCAPS].value != MPOA_MODE_BRIDGED_ETH_VC &&
	opt[CFG_ENCAPS].value != MPOA_MODE_ROUTED_IP_LLC &&
	opt[CFG_ENCAPS].value != MPOA_MODE_ROUTED_IP_VC &&
	opt[CFG_ENCAPS].value != MPOA_MODE_PPPOA_LLC &&
	opt[CFG_ENCAPS].value != MPOA_MODE_PPPOA_VC)
	result = 0;
    return result;
}

