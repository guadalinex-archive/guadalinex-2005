/*
 *
 * Copyright (c) 2003, Frederick Ros (sl33p3r@free.fr)
 *										    
 * eu_utils.c  - Various utils functions
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
 * $Id: eu_utils.c,v 1.11 2005/01/17 20:54:42 sleeper Exp $
 */

#include "Adiutil.h"
#include "eagle-usb.h"
#include "eu_utils.h"
#include "macros.h"
#include "debug.h"
#include "eu_firmware.h"


/* ----------------------- Private Macros/Variables ------------------------ */

/*
 * Firmware loading
 */
#define LOAD_INTERNAL     0xA0
#define F8051_USBCS       0x7f92
#define F8051_RETRY_COUNT 3

#define CDC_REQ_SEND_ENCAPSULATED_CMD 0
#define POLYNOMIAL 0x04c11db7L

static uint32_t crc_table[256];

#define MAC_ADDRESS_INDEX 4


/* ---------------------------- Private Declarations ---------------------------- */

static queued_urb_t *get_queued_urb(struct list_head *q);
static void rm_queued_urb(struct urb *urb);
static int eu_send_modem_cmd (
                              eu_instance_t *ins,
                              uint16_t addr,
                              uint16_t count,
                              uint8_t *buff
                             );
static uint8_t htoi ( uint8_t h );


/* ----------------------------- Exported Functions ----------------------------- */


/**
 * eu_load_firmware - Load firmware into pre-firmware devices.
 *
 */
int eu_load_firmware ( eu_instance_t *ins, uint32_t pid, unsigned int *ver ) 
{
    int ret= 0;
    uint8_t value;
    uint32_t i = 0;
    eu_hex_record_t *pfirm;
    
    eu_enters (DBG_UTILS);

    value = 1;

    /*
     * Send reset
     */
    ret = eu_send_modem_cmd ( ins, F8051_USBCS, sizeof(value), &value );
    
    if ( ret < 0 ) 
    {
        eu_err ("eu_send_modem_cmd reset failure with error %d\n",ret);
        goto byebye;
    }

    if ( IS_EAGLE_I (pid) ) 
    {
        pfirm = &eu_eagle_I_firmware[0];
        if (ver != NULL) 
        {
            *ver = 1;
        }
        
    }
    else  if ( !IS_EAGLE_III (pid) )
    {
        pfirm = &eu_eagle_II_firmware[0];
        if (ver != NULL) 
        {
            *ver = 2;
        }
    }
    else 
    {
  		pfirm = &eu_eagle_III_firmware[0];      
        if (ver != NULL) 
        {
            *ver = 3;
        }
    }
    
    i = 0;

    while ( pfirm->type == 0 ) 
    {
        ret = eu_send_modem_cmd (ins,
                                 pfirm->address,
                                 pfirm->length,
                                 pfirm->data );
        if ( ret < 0 ) 
        {
            eu_err ("eu_send_modem_cmd of download data failed (%d)\n",ret);
            goto byebye;
        }
        pfirm ++;
    }

    /*
     * De-assert reset
     */
    value = 0;
    ret = eu_send_modem_cmd ( ins, F8051_USBCS, 1, &value );
    if ( ret < 0 ) 
    {
        eu_err ("eu_send_modem_cmd de-reset failure (%d)\n",ret);
        goto byebye;
    }

    
  byebye:  
    eu_leaves (DBG_UTILS);
    return (ret);
}




/**
 * eu_cmd_to_modem - Send command to modem
 *
 */
int eu_cmd_to_modem (
                     eu_instance_t *ins,
                     uint32_t cmd,
                     uint32_t idx,
                     uint32_t count,
                     uint8_t *data
                    )
{
    /*get space for data and request struct*/
    struct urb *urb;
    devrequest *pdr;
    uint8_t *xfer_buff= NULL;
    int ret = 0;

     /*xfer_buff = GET_KBUFFER (count+sizeof(devrequest));*/
     xfer_buff = kmalloc(count+sizeof(devrequest), GFP_ATOMIC);

    
    if ( !xfer_buff )
    {
        eu_err ("eu_cmd_to_modem: Cannot allocate %#lx bytes\n",(unsigned long)count+sizeof(devrequest));        
	ret = -ENOMEM;
	goto byebye;
    }

    if ( count != 0 ) 
    {
        memcpy ( xfer_buff, data, count );
    }
    
    pdr = (devrequest *)( xfer_buff + CMV_DATA_WORDS*2 );

    /*
     * Get an URB and prepare it for submission
     */
    urb = USB_ALLOC_URB ( 0, GFP_ATOMIC );

    FILL_USB_CTRL_REQUEST ( pdr,
                            USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
                            CDC_REQ_SEND_ENCAPSULATED_CMD,
                            cmd, idx, count );

    usb_fill_control_urb ( urb,
                           ins->usbdev,
                           usb_sndctrlpipe(ins->usbdev,0),
                           (uint8_t *) pdr,
                           xfer_buff,
                           count,
                           ctrl_urb_completion,
                           NULL);
    
    queue_ctrl_urb(ins, urb);
    
  byebye:
    return ret;
} 

/*
 * Allocates the number (count) of queued_urb_t and
 * put them in the free queue
 *
 * Return:
 *    0     :   OK
 * -ENOMEM  : Not enough memory. No data has been allocated
 */
int alloc_queued_urb_ctrl ( eu_instance_t *ins, unsigned int count )
{
    unsigned int i;
    queued_urb_t *pu;
    int retcode = 0;
    
    /*
     * Initialize the free control urb queue
     */
    INIT_LIST_HEAD( &ins->ctrl_urb_free_q );
    
    for (i=0; i< count; i++ ) 
    {
        pu = (queued_urb_t *) GET_KBUFFER (sizeof(queued_urb_t));
        if (pu == NULL ) 
        {
            retcode = -ENOMEM;
            eu_err ( "Not enough memory to init. free urb q.\n");
            goto byebye;
        }

        memset ( pu, 0, sizeof(queued_urb_t) );
        
        /*
         * And add it to the list
         */
        list_add ( &pu->list, &ins->ctrl_urb_free_q );
    }

  byebye:
    if ( retcode != 0 ) 
    {
        /*
         * We have to free previously allocated queued_urb_t
         */
        free_queued_urb_ctrl ( &ins->ctrl_urb_free_q );
    }
    else
    {
        eu_dbg(DBG_UTILS,"%d queued_urb_t allocated.\n",count);
    }
    
    return (retcode);
}

/*
 * Free all previously allocated queued_urb_t
 *
 */

void free_queued_urb_ctrl ( struct list_head *q ) 
{
    queued_urb_t *pu;
    
    while ( !list_empty (q) ) 
    {
        pu = list_entry (q->next, queued_urb_t, list );
        list_del (&pu->list);
	FREE_KBUFFER(pu);
    }    
    
}


/*
 * This watcher is used only to rescue the modem, from an unacknowledged
 * Control Urb.
 *
 */
void ctrl_urb_q_watcher ( unsigned long ptr ) 
{    
    eu_instance_t *ins = (eu_instance_t *)ptr;
    queued_urb_t *qu;
    unsigned long flags;
    
    
    eu_enters (DBG_UTILS);
    

    spin_lock_irqsave (&ins->ctrl_q_lock, flags);

    qu = list_entry ( ins->ctrl_urb_ipg_q.next, queued_urb_t, list);
    
    /* We need to delete the head of the list,
     * and submit the next one if there's one.
     * ???? Should we also CANCEL the pending URB ????
     * To do this the simplest approach is to call ctrl_urb_completion
    */
    
    if  ( (list_empty (  &ins->ctrl_urb_ipg_q ) ) ||
          ( qu == NULL ) )
    {
        eu_err ("Ctrl Urb Watcher expires but not entry in ipg queue\n");
        return;
    }
    
    eu_err ("Ctrl Urb Watcher expired. About to remove URB %p\n",
            qu->urb );
    
    rm_queued_urb ( qu->urb );


    spin_unlock_irqrestore (&ins->ctrl_q_lock,flags);
    
    eu_leaves (DBG_UTILS);
    
}



/**
 * Retry to send urb
 */
void ctrl_urb_retry_send ( unsigned long ptr ) 
{    
    eu_instance_t *ins = (eu_instance_t *)ptr;
    unsigned long flags;
    queued_urb_t *qu;
    int ret;
    
        
    eu_enters (DBG_UTILS);
    eu_dbg(DBG_UTILS,"ctrl_urb_retry_send: WAKE UP !!\n");
    

    spin_lock_irqsave (&ins->ctrl_q_lock, flags);

    if ( ins->ctrl_urb_failed == NULL ) 
    {
        eu_err ("ctrl_urb_retry_send expires but failed urb is NULL !\n");
        return;
    }
        
    
    qu = list_entry ( ins->ctrl_urb_ipg_q.next, queued_urb_t, list);    
    
    if  ( (list_empty (  &ins->ctrl_urb_ipg_q ) ) ||
          ( qu == NULL ) )
    {
        eu_err ("ctrl_urb_retry_send expires but not entry in ipg queue\n");
        return;
    }

    if ( qu->urb != ins->ctrl_urb_failed ) 
    {
        eu_warn ("faulty urb not at beginning of list !\n");
    }
    
    
    ret = USB_SUBMIT_URB ( ins->ctrl_urb_failed , GFP_ATOMIC );
    if ( ret != 0 ) 
    {
        if ( ret == -ENODEV ) 
        {
            eu_err ("ctrl_urb_retry_send: -ENODEV received ... I give up.\n");
        }
        else 
        {
            eu_err ("ctrl_urb_retry_send: Failed to send faulty ctrl urb (%p) with err=%d\n",
                    ins->ctrl_urb_failed ,ret);
            eu_err ("ctrl_urb_retry_send: retry in 100ms\n");

            /*
             * Schedule retry send in 100 ms
             */
            ins->ctrl_urb_retry.expires = jiffies + MSEC_TO_JIFFIES (100);
            add_timer ( &ins->ctrl_urb_retry );
        }
    }
    else
    {
        ins->ctrl_urb_failed = NULL;

        if ( !timer_pending ( &ins->ctrl_urb_q_timer ) ) 
        {
            /*
             * For safety we also set a timer : if something weird happens, like
             * lost urb acknowledgement, we will be able to submit other URBs
             */
            ins->ctrl_urb_q_timer.expires = jiffies + MSEC_TO_JIFFIES(2000);
            add_timer ( &ins->ctrl_urb_q_timer );
        }        
    }

    spin_unlock_irqrestore (&ins->ctrl_q_lock,flags);
    
    eu_leaves (DBG_UTILS);
    
    
}

    

/**
 * Unlink 1st control urb if it's pending
 */
void unlink_ipg_ctrl_urb ( eu_instance_t *ins ) 
{
    queued_urb_t *qhead;
    unsigned long flags;
    
    /*
     * Take lock
     */
    spin_lock_irqsave (&ins->ctrl_q_lock, flags);    

    if ( !list_empty ( &ins->ctrl_urb_ipg_q ) ) {
			
        /*
         * We have to unlink it
         */
        qhead = list_entry ( ins->ctrl_urb_ipg_q.next, queued_urb_t, list);
        
        USB_KILL_URB(qhead->urb);
    }
    
    spin_unlock_irqrestore (&ins->ctrl_q_lock, flags);
}

/*
 * Completion handler for Control Urbs
 * Remove the head of the ipgt list, and eventually submit a new
 * control urb to the modem
 *
 */
USB_COMPLETION_PROTO (ctrl_urb_completion,urb,regs)
{
    queued_urb_t *pq;
    eu_instance_t *ins;
    unsigned long flags;
    
    eu_enters (DBG_UTILS);

    if (
        ( urb->status ==  -ENOENT ) ||
        ( urb->status == -ECONNRESET ) ||
        ( urb->status == -ESHUTDOWN )
       )
    {
        eu_report (" Control Urb unlinked.\n");
        goto byebye;
    }
    
    pq = urb->context;
    ins = (eu_instance_t *)(pq->dev);
    
    spin_lock_irqsave (&ins->ctrl_q_lock, flags);
    rm_queued_urb (urb );
    spin_unlock_irqrestore (&ins->ctrl_q_lock,flags);

  byebye:
    eu_leaves (DBG_UTILS);
}

/*
 * Put the given urb in the in-progress control queue,
 * and eventually send it to the modem
 *
 */
void queue_ctrl_urb(eu_instance_t *ins, struct urb *urb)
{
    int ret;
    unsigned int list_was_empty;
    queued_urb_t *pq;
    unsigned long flags;
    
    
    eu_enters (DBG_UTILS);
    
    eu_dbg(DBG_UTILS,"queue_ctrl_urb (%p)\n", urb);

    /*
     * Lock spinlock
     */
    spin_lock_irqsave (&ins->ctrl_q_lock, flags);    
    
    /*
     * Look at the status of the list
     */
    list_was_empty = list_empty ( &ins->ctrl_urb_ipg_q );

    /*
     * Get a free queued_urb_t
     */
    pq = get_queued_urb ( &ins->ctrl_urb_free_q );

    if ( pq == NULL ) 
    {
        /*
         * Too bad : no more free queued_urb_t
         */
        eu_err ("Can't get free queued_urb_t !\n");
        goto byebye;
    }
    
    pq->urb = urb;
    pq->dev = ins;
    urb->context = pq;          /* usefull in the complete handler ! */
    
    /*
     * Enqueue Control Urb
     */
    list_add_tail ( &pq->list, &ins->ctrl_urb_ipg_q);

    /*
     * Now, if the list was empty before we introduce our urb, we can safely
     * send it to the modem.
     */ 
    if ( list_was_empty ) 
    {
        eu_dbg(DBG_UTILS,"Submit urb %p immediately\n", urb);
        ret = USB_SUBMIT_URB ( urb, GFP_ATOMIC );
        if ( ret != 0 ) 
        {
            eu_err ("Failed to send ctrl urb (%p) with err=%d\n",
                    urb,ret);
            if ( ret == -ENODEV ) 
            {
                eu_err ("This is a \"no device\" error ... I give up.\n");
                goto byebye;
            }
            else
            {            
                eu_err ("Let's retry in 100ms\n");
            }
            /*
             * Schedule retry send in 200 ms
             */
            ins->ctrl_urb_failed = urb;
            ins->ctrl_urb_retry.expires = jiffies + MSEC_TO_JIFFIES (100);
            add_timer ( &ins->ctrl_urb_retry );
            
        }
        else
        {
            ins->ctrl_urb_failed = NULL;
        
            /*
             * For safety we also set a timer : if something weird happens, like
             * lost urb acknowledgement, we will be able to submit other URBs
             */
            ins->ctrl_urb_q_timer.expires = jiffies + MSEC_TO_JIFFIES(2000);
            add_timer ( &ins->ctrl_urb_q_timer );
        }        

    }

  byebye:
    spin_unlock_irqrestore (&ins->ctrl_q_lock, flags);
    
    eu_leaves (DBG_UTILS);
    
    return;
}

/**
 * eu_crc_generate - Generates a table of CRC remainders for all possible bytes
 *
 * NOTE - must be called prior to eu_crc_calculate
 *
 */
 
void eu_crc_generate ( void )
{ 
   uint32_t i;  
   uint8_t  j;
   uint32_t crc_accum;
   
   for ( i = 0; i < 256; i++ )
   { 
      crc_accum = ( (uint32_t) i << 24 );
      for ( j = 0;  j < 8;  j++ )
      { 
          if ( crc_accum & 0x80000000L )
          {
             crc_accum = ( crc_accum << 1 ) ^ POLYNOMIAL;
          }
          else
         {
             crc_accum = ( crc_accum << 1 ); 
         }
      }
      crc_table[i] = crc_accum; 
   } 
   return; 
}

/**
 * eu_crc_calculate - Compute the CRC of len bytes in the given cell payload, using
 *                    the previous partial crc value.
 *
 * NOTE - eu_crc_generate should be called prior to this function
 *
 */
uint32_t eu_crc_calculate ( uint8_t *cell, uint32_t len, uint32_t crc )
{ 
   uint32_t i, j;
   uint32_t new_crc;

   eu_enters (DBG_CRC);
   eu_dbg (DBG_CRC,"cell=%p, len=%x, crc=%x\n",
            cell, len, crc);

   new_crc = crc;

   for ( j = 0;  j < len;  j++ )
   {
       i = ( (int32_t) ( new_crc >> 24) ^ cell[j]) & 0xff;
       new_crc = ( new_crc << 8 ) ^ crc_table[i]; 
   }
   
   eu_dbg (DBG_CRC,"new_crc=0x%x\n",new_crc);
   
   eu_leaves (DBG_CRC);
   
   return( new_crc); 
}


/**
 * eu_get_mac_addr - Get MAC address from USB firmware, and stores it in ins->mac in
 * numeric
 *
 */
void eu_get_mac_addr ( eu_instance_t *ins )
{
    int maclen, i;
    uint8_t macbuf[ETH_ALEN*2+1];
    
    maclen = usb_string(ins->usbdev, MAC_ADDRESS_INDEX, macbuf, ETH_ALEN*2+1);
    
    /*
     * Make sure we got a string of the right length
     */
    if ( maclen != ETH_ALEN*2 )
    {
        eu_dbg(DBG_UTILS,"eu_get_mac_addr: invalid length %x.\n", maclen);
        return;
    }
    
    /*
     * Convert the string into a numeric format
     * (each string byte is a nibble in the result)
     */
    for ( i=0; i<ETH_ALEN; i++ )
    {
        ins->mac[i] = (htoi(macbuf[i*2])<<4) + (htoi(macbuf[i*2+1]));
    }
}


/* ----------------------------- Private Functions ------------------------------ */


/**
 * eu_send_modem_cmd - Send a command to the modem (firmware loading)
 *
 */
static int eu_send_modem_cmd (
                              eu_instance_t *ins,
                              uint16_t addr,
                              uint16_t count,
                              uint8_t *buff
                             ) 
{
    int ret = -ENOMEM;
    uint8_t *xfer_buff = NULL;

    xfer_buff = GET_KBUFFER (count);

    if ( xfer_buff ) 
    {
        memcpy ( xfer_buff, buff, count );
        ret = usb_control_msg ( ins->usbdev,
                                usb_sndctrlpipe( ins->usbdev, 0 ),
                                LOAD_INTERNAL,
                                USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
                                addr, 0, xfer_buff, count, 300);
	FREE_KBUFFER(xfer_buff); 
    }
    
    return (ret);
}


/*
 * Remove the first urb of the ipg queue, and if the queue is not empty, send
 * the first URB.
 * Also deals with the Control Urb Queue watcher timer ( stop it or modify it)
 *
 * This function is called by ctrl_urb_completion and ctrl_urb_q_watcher
 *
 */
static void rm_queued_urb(struct urb *urb)
{
    queued_urb_t *qu,*qhead;
    eu_instance_t *ins;
    
    /*
     * Check if we're able to find queued_urb_t related to this urb
     */
    qu = urb->context;

    if ( qu == NULL ) 
    {
        eu_err ("URB (%p) has not context.\n",urb);
        goto free_urb_and_quit;
    }

    /*
     * Get back hardware
     */
    ins = (eu_instance_t *) qu->dev;
    if ( !ins ) 
    {
        eu_err ("URB %p not associated to Hdw\n",urb);
        goto free_urb_and_quit;
    }
    
    if ( list_empty ( &ins->ctrl_urb_ipg_q ) ) 
    {
        eu_err ("URB %p not present in list !\n",urb);
        goto free_urb_and_quit;
    }
    
    /*
     * Check that URB is at the head of the list
     */
    qhead = list_entry ( ins->ctrl_urb_ipg_q.next, queued_urb_t, list);
    
    if ( qhead->urb != urb ) 
    {
        eu_err ("Head of IPG queue(%p) is not equal to URB %p\n",
                qhead->urb,urb);
    }
    /*
     * Remove this queued_urb_t entry and put it in the
     *  free queued_urb_t queue
     */
    list_del ( &qu->list );
    list_add_tail ( &qu->list, &ins->ctrl_urb_free_q);
    

    /*
     * Check if there's another pending URB
     */
    if ( list_empty ( &ins->ctrl_urb_ipg_q ) ) 
    {
        /* The only pending URB has been acknowledged, and
         * nothing is pending : we can stop the running timer
        */
        eu_dbg(DBG_UTILS,"rm_queued_urb: Stopping timer\n");
        del_timer ( &ins->ctrl_urb_q_timer );
    }
    else
    {
        int ret;
        
        /*
         * Send the 1st entry of the list
         */
        qhead = list_entry ( ins->ctrl_urb_ipg_q.next, queued_urb_t, list);

        eu_dbg(DBG_UTILS,"rm_queued_urb: Submitting urb %p\n",qhead->urb);
        

        ret = USB_SUBMIT_URB ( qhead->urb , GFP_ATOMIC );
        if ( ret != 0 ) 
        {
            eu_err ("Failed to send pending ctrl urb (%p) with err=%d\n",
                    qhead->urb,ret);
                        if ( ret == -ENODEV ) 
            {
                eu_err ("This is a \"no device\" error ... I give up.\n");
                goto free_urb_and_quit;
            }
            else
            {            
                eu_err ("Let's retry in 100ms\n");
            }

            
            /*
             * Schedule retry send in 200 ms
             */
            ins->ctrl_urb_failed = urb;
            ins->ctrl_urb_retry.expires = jiffies + MSEC_TO_JIFFIES (100);
            add_timer ( &ins->ctrl_urb_retry );
        }
        else
        {
            /*
             * Set the timer to expire in 2s : just to be sure nothing is lost
             */
            mod_timer ( &ins->ctrl_urb_q_timer, jiffies + MSEC_TO_JIFFIES(2000) );
        }
    }
    
  free_urb_and_quit:
    /*
     * Now free transfer buffer and URB
     */
    FREE_KBUFFER(urb->transfer_buffer);
    usb_free_urb(urb);                 
}

/*
 * Return the next queued_urb_t from the list, and delete it
 */
 
static queued_urb_t *get_queued_urb(struct list_head *q)
{
    queued_urb_t *pu;
    

    /*
     * Check first for an empty list
     */
    if (list_empty(q))
    {
	pu = NULL;
    }
    else
    {
        
        pu = list_entry (q->next, queued_urb_t, list );
        list_del(q->next);
    }
    
    return (pu);
}

/**
 * htoi - Converts hex to integer.
 *
 */
static uint8_t htoi ( uint8_t h ) 
{
    uint8_t d;
    
    if ( isxdigit (h) ) 
    {
        /*
         * This is an hexa-digit
         */
        d = isdigit (h) ? h - '0' : toupper (h) -'A'+ 10 ;
    }
    else
    {
        d = 0;
    }

    return (d);
}









