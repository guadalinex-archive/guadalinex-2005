/*
 *
 * Copyright (c) 2003, Frederick Ros (sl33p3r@free.fr)
 *										    
 * eu_types.h  - Types Declarations.
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
 * $Id: eu_types.h,v 1.8 2004/11/07 09:06:55 sleeper Exp $
 */
#ifndef __EU_TYPES_H__
#define __EU_TYPES_H__

#include <linux/if_ether.h>
#include <asm/bitops.h>
#include <linux/types.h>

/**
 * eu_ioctl_info  -  Used to exchange data between kernl and user space via
 *                   ioctls
 *
 */
struct eu_ioctl_info 
{
    uint32_t  idma_start;
    uint32_t  buffer_size;
    uint8_t   *buffer;
};


/*
 * ioctl commands
 */
#define EU_IO_OPTIONS _IOW('U',  101, struct eu_ioctl_info)
#define EU_IO_DSP     _IOW('U',  102, struct eu_ioctl_info)
#define EU_IO_GETITF  _IOWR('U', 103, struct eu_ioctl_info)
#define EU_IO_SYNC    _IO('U',   104)
#define EU_IO_GETDBG  _IOWR('U', 105, struct eu_ioctl_info)
#define EU_IO_SETDBG  _IOWR('U', 106, struct eu_ioctl_info)
#if USE_CMVS
#define EU_IO_CMVS    _IOW('U',  107, struct eu_ioctl_info)
#endif

/*
 * Options description structure
 */

#define MAX_OPTION_NAME_LENGTH      64

typedef struct
{
	char	 name[MAX_OPTION_NAME_LENGTH];
	uint32_t value;
} eu_opt_t;

enum
{
    CFG_OPT_0 = 0,
    CFG_OPT_2,
    CFG_OPT_3,
    CFG_OPT_4,
    CFG_OPT_5,
    CFG_OPT_6,
    CFG_OPT_7,
    CFG_OPT_15,
    CFG_VPI,
    CFG_VCI,
    CFG_ENCAPS,
    CFG_LINE,
    CFG_RATE_POLL_FREQ,
    CFG_OPT_COUNT
};

#define NUM_DRV_OPTIONS CFG_OPT_COUNT
typedef eu_opt_t eu_options_t[NUM_DRV_OPTIONS];

#ifdef __KERNEL__

#define TRANSMIT_TIMEOUT (HZ*5) /* 5 seconds */

#define ETHERTYPE_IP    0x0008
#define ETHERTYPE_IPV6  0xdd86
#define ETHERTYPE_ARP   0x0608

/*
 * Custom ADI vendor control commands
 */
#define EU_CMD_GET_BLOCK      0x00
#define EU_CMD_SET_BLOCK      0x01
#define EU_CMD_GET_STAT       0x02
#define EU_CMD_SET_MODE       0x03
#define EU_CMD_SET_2183_DATA  0x04
#define EU_CMD_GET_2183_DATA  0x05
#define EU_CMD_GET_STAT_VAR   0x06
#define EU_CMD_GET_INTERRUPT  0x07
#define EU_CMD_SET_FPGA_DATA  0x0E
#define EU_CMD_SET_TIMEOUT    0x11

/*
 * Modes for EU_CMD_SET_MODE
 */
#define MODE_LOOPBACK_OFF      0x02
#define MODE_LOOPBACK_ON       0x03
#define MODE_BOOTMODE_IDMA     0x06
#define MODE_START_RESET       0x07
#define MODE_END_RESET         0x08

/*
 * Mailbox addrs
 */
#define DSP_MP_TX_MAILBOX      0x3FD6
#define DSP_MP_TX_START        0x3FCE
#define DSP_MP_RX_MAILBOX      0x3FDF
#define DSP_SWAP_MAILBOX       0x3FCD

/*
 * Mailbox states
 */
#define MAILBOX_EMPTY          0
#define MAILBOX_FULL           1

/*
 * Maximums
 */
#define MAX_DSP_ALLOCS 128

/*
 * MPOA Encapsulations
 */
#define MPOA_MODE_PPPOA_LLC         5
#define MPOA_MODE_PPPOA_VC          6


/**
 * eu_cdc_t - Comm Device Class notification buffer
 *
 * @req: request as defined but USB standard
 * @data: 20 bytes array used to pass data to the modem
 *
 * This is an extended version of the USB standard CDC, as ADI modem
 * use this to pass data.
 */
typedef struct 
{
    uint8_t bRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
    uint8_t data[20];
} __attribute__ ((packed)) eu_cdc_t;


/**
 * eu_devint_t - returned by the modem on interrupt
 */
#define CMV_DATA_WORDS      8

typedef struct
{
    uint16_t intr;
    union
    {
        uint16_t  swap_data;
        uint16_t  cmv_data[CMV_DATA_WORDS];
    } intr_info ;
    uint16_t data_size;
    
} eu_devint_t;


/**
 * ISO Frame 
 *
 * @status  -  Status of this received frame
 * @length  -  Actual Data Buffer Length (how much data we received)
 * @data    -  Received Data
 */
#define FASTEST_ISO_RATE       1007

typedef struct eu_iso_frame
{
    int status;
    int length;
    char data[FASTEST_ISO_RATE];
} eu_iso_frame_t;

/**
 * ISO Receive Buffer
 *
 * @next   -  To link buffers in completed receive list
 * @frames -  Frames for this receive buffer
 *
 */
#define FRAMES_PER_ISO_URB     16
typedef struct eu_iso_rb 
{
    struct list_head next;
    eu_iso_frame_t  frames[FRAMES_PER_ISO_URB];
} eu_iso_rb_t;

/************************************************************************************/
/* This will be the receiver buffer for incoming network data. We will queue        */
/* up a bunch of reads with USB with these buffers, then process them into          */
/* real ethernet packets as they come in. This enables us to support situations     */
/* where the entire ethernet packet does not come in in one USBBulkRead. We	    */
/* do require that the data received into these buffers is an exact number          */
/* of ATM cells									    */
/* LINUX NOTE :									    */
/* In kernel 2.4.7-10 (the stock kernel with RedHat 7.2), the usb-uhci module	    */
/* has a problem queuing multiple read bulk urbs). Therefore, we set this to one so */
/* we'll work with uhci or ohci. However, higher performance can be seen on ohci    */
/* if you allow multiple read urbs. So, if you're on an ohci-based machine,	    */
/* set INCOMING_Q_SIZE to > 1 to achieve greater performance.			    */
/************************************************************************************/
/* FIXME */
#ifdef USEBULK
#define INCOMING_Q_SIZE 1
#else
/*We'll set it to 6 when we're dealing with ISOCHRONOUS data.*/
#define INCOMING_Q_SIZE 6
#endif

#define OUTGOING_Q_SIZE    64
#define OUTGOING_DATA_SIZE 1802 /* same logic as for INCOMING_DATA_SIZE */


/************************************************************************************/
/* Here's how I got this - Max ethernet   = 1500 bytes + 			    */
/*          possible Ethernet FCS         = 4 bytes +				    */
/*          possible MPOA encapsulation   = 10 bytes +				    */
/*          CPCS PDU trailer              = 8 bytes +				    */
/*          at most 2 ATM payloads of pad = 96 bytes				    */
/*                                       ----------				    */
/*                                         1618 bytes				    */
/*          Divided among ATM cells = 34 ATM cells * 53 bytes			    */
/*                      Grand total = 1802					    */
/* NOTE NOTE NOTE NOTE								    */
/* This logic all makes sense IF the device received data in ATM cell sized chunks, */
/* however, it doesn't. The USB pipes receive 64bytes at a time, so we have         */
/* to have space for a size that is the LCM of 64 and 53, which is 64*53            */
/************************************************************************************/
/* FIXME */
#define INCOMING_DATA_SIZE (64 * 53)

/**
 * BULK Receive Buffer
 *
 * @next   -  To link buffers in completed receive list
 * length  - Received Data Length
 * data    - Received Data
 */
typedef struct eu_bulk_rb 
{
    struct list_head next;
    int length;
    char data[INCOMING_DATA_SIZE];
} eu_bulk_rb_t;

/*
 * To ease code ...
 */

#ifdef USEBULK
typedef eu_bulk_rb_t eu_rb_t;
#else
typedef eu_iso_rb_t eu_rb_t;
#endif /* USEBULK */


/*
 * Queued Control Urb
 */
#define CTRL_URB_Q_SIZE 16

typedef struct 
{
    struct list_head list;
    struct urb *urb;
    void *dev;
} queued_urb_t;


/**
 * eu_msg_t  - Decoded modem messages
 *
 * @preamble     Message preamble
 * @type         Function type
 * @subtype      Function Sub Type
 * @sender_id    ID of the Sender   (unused)
 * @receiver_id  ID of the receiver (unused)
 * @index        message index      (unused)
 * @symb_addr    Symbolic addr      (unused)
 * @offset_addr  Offset             (unused)
 * @data         Data from modem
 *
 * Field marked with (unused) tag are only used to check message is not
 * corrupted
 */
typedef struct 
{
    uint16_t preamble;
    uint16_t type;
    uint16_t subtype;
    uint16_t sender_id;
    uint16_t receiver_id;
    uint16_t index;
    uint8_t  saddr[4];
    uint32_t offset_addr;
    uint32_t data;
} eu_msg_t;

typedef struct
{
    uint16_t preamble;
    uint16_t function;
    uint16_t idx;
    uint8_t  saddr_hihi;
    uint8_t  saddr_hilo;
    uint8_t  saddr_lohi;
    uint8_t  saddr_lolo;
    uint16_t offset;
    uint32_t data;
} eu_cmv_msg_t;

/* FIXME */
#define MAX_CMV_MESSAGES    20

typedef struct
{
    union
    {
        eu_cmv_msg_t *Msgs[ MAX_CMV_MESSAGES ];
        uint16_t *RawCmd[ MAX_CMV_MESSAGES ];
    } MsgMax;
    uint32_t MsgCount;
} MsgSequence;


/*  State machine control structure*/
/* FIXME */
typedef struct
{
    /* state machine flags and options*/
    uint32_t   ADPT0;
    uint32_t   ADPT1;
    uint32_t   ADPT2;
    uint32_t   INFO9;
    uint32_t   CNTL0;
    uint32_t   OPTN0;
    uint32_t   OPTN2;
    uint32_t   OPTN4;
    uint32_t   OPTN7;
    uint32_t   OPTN3;
    uint32_t   OPTN5;
    uint32_t   OPTN6;
    uint32_t   OPTN15;
    uint32_t   PFCL1;
    uint32_t   MASK8;
    uint32_t   MASK9;
             
    uint32_t   INFO8;
    uint32_t   PSDM0;
    uint32_t   UNH;
    uint32_t   ISDN;
    uint32_t   FLAG0;
    
    /* OPERATIONAL values*/
    uint32_t   sw_status;
    uint32_t   crc_status;
    uint32_t   flags;

    /* statistics*/
    uint32_t   stats_ES_count;
    uint32_t   stats_Cur_Atten;
    uint32_t   stats_Cur_SNR;
    uint32_t   stats_Rx_Blks;
    uint32_t   stats_Tx_Blks;
    uint32_t   stats_Corr_Blks;
    uint32_t   stats_Uncorr_Blks;

    uint32_t   stats_Rx_Blks_Delta;
    uint32_t   stats_Corr_Blks_Delta;
    uint32_t   stats_Uncorr_Blks_Delta;

    /* transfer rate*/
    uint32_t   XferRate0;
    uint32_t   DownRate;
    uint32_t   UpRate;
    uint32_t   FwRxTimeout;

    /* miscell*/
    uint32_t   DIAG03;
    uint32_t   DIAG47;
    uint32_t   DIAG49;
    uint32_t   INFO10;
    uint32_t   PSDM01;
    uint32_t   INFO08;
    uint32_t   INFO14;

    /* following counters work in single digits before reset is tridgerred*/
    uint8_t    LOS_count;
    uint8_t    CRC_count;

    uint8_t    Block_CRC90;
    uint8_t    Block_CRC97;

    uint32_t  stats_Ne_Fast_Lod_Failure;
    uint32_t  stats_Ne_Fast_Hec_Failure;

    /* -------- status variables ------------------------*/
    uint8_t    LineType;

    uint16_t   ModemReplyExpected;
    uint16_t   CurrentAdiState;
    uint16_t   PrevAdiState;

    uint16_t   MsgStage;
    uint16_t   ReTrain;
    uint16_t   RetryCount;

    uint32_t   HeartbeatCounter;

    /* pre-composed messages used for fast retrain sequence*/
    MsgSequence MsgSeq_Retrainer;
    MsgSequence MsgSeq_SoftReset;
    MsgSequence MsgSeq_OpStat;
    MsgSequence MsgSeq_Stat;
    MsgSequence MsgSeq_ModemEna;
    MsgSequence MsgSeq_EnaFR;

    struct timer_list timerSM;

    /* flags and handles*/
    uint32_t   CurrentExpirationTime;
    uint32_t   SwapPageRequiredTime;

    uint32_t  OutboundPending;
    uint32_t  InboundAsyncPending;
    uint32_t  InboundSyncPending;

    uint32_t  watchBadBlocks;
}  AdiMSM;    

/*
 * Boot states: they are copied over the value of the modem internal states
 */
typedef enum {
    PRE_BOOT = 0,
    STAGE_1  = 1,
    STAGE_2  = 2,
    STAGE_3  = 3,
    REBOOT   = 4,
    UPLOAD_P = 5,
    STATE_MAX
} boot_state_t;

/* Represents a block of IDMA memory, either Program or Data Memory (PM or DM)*/
/* FIXME */
typedef struct
{
    uint8_t  *MemOffset;
    uint32_t  DSPAddr;
    uint32_t  DSPSize;
    uint32_t  DSPExtendedSize;
} IDMABlock;

/* Represents a page of IDMA memory, either a Main page or a Swap page*/
/* FIXME */
typedef struct
{
    uint32_t  BlockCount;
    uint32_t  PageOffset;
    IDMABlock *Blocks;
} IDMAPage;

/**
 * eu_sar_r_t  - Sar Reassembly structure
 *
 *  @skb                  -  Pointer to the used skb
 *  @pdu_len_from_trailer -  Length as reported by cell trailer
 *  @running_crc          -  CRC in the process to be calculated
 *  @cell_count           -  Number of prcessed cells for this frame.
 */
typedef struct 
{
    struct sk_buff *skb;
    uint32_t        pdu_len_from_trailer;
    uint32_t        running_crc;
    uint32_t        cell_count;
} eu_sar_r_t;

/**
 * eu_sar_s_t  -  Sar Segmentation structure
 *
 * @out_buff   -   Buffer used to segemnt data
 * @size -
 * 
 */
typedef struct 
{
    uint8_t  *out_buff;
    uint32_t  size;
    uint8_t  *cell_hdr;
    uint32_t  raw_pdu_length;
    uint32_t  padding_len;
    uint32_t  bytes_left_in_curr_cell;
    uint32_t  cell_count;
    uint32_t  running_crc;
} eu_sar_s_t;


/**
 * eu_aal5_trailer_t - AAL5 Trailer
 *
 *  @cpcs_uu  -
 *  @cpi      -
 *  @pdu_len  -  PDU Length for this frame
 *  @crc      -  CRC for this frame
 *
 */

typedef struct 
{
    uint8_t  cpcs_uu;
    uint8_t  cpi;
    uint16_t pdu_len;
    uint32_t crc;
}
eu_aal5_trailer_t;



/**
 * eu_atm_vc_t  -  ATM Virtual Channel descriptor
 *
 * @reassembler        -   SAR Reassembler
 * @vpi                -   Virtual Path Identifier
 * @vci                -   Virtual Channel Identifier
 * @vpi_vci            -   Agregated VPI/VCI
 * @max_sdu_size       -   Maximum Data User Size
 * @reassembly_ipg     -   uint32_t : Is a Reassembly I ProGress ?
 * @encaps_size        -   Encapsulation size on segmentation ( RFC 2684 )
 * @encaps_hdr         -   Encapsulation Header
 * @cell_hdr           -   Header of current cell
 * @cpcs_uu            -
 * @cpi                -
 *
 */

#define ATM_CELL_HEADER_SIZE                5

typedef struct 
{
    eu_sar_r_t  reassembler;
    eu_sar_s_t  segmenter;
    uint32_t     vpi;
    uint32_t     vci;
    uint32_t     vpi_vci;
    uint32_t     max_sdu_size;
    uint32_t     reassembly_ipg;
    uint32_t     encaps_size;
    uint8_t     *encaps_hdr;
    uint8_t      cell_hdr[ATM_CELL_HEADER_SIZE];
    uint8_t      cpcs_uu;
    uint8_t      cpi;
} eu_atm_vc_t;

/*
 * Encapsulation modes
 */
#define MPOA_MODE_BRIDGED_ETH_LLC   1
#define MPOA_MODE_BRIDGED_ETH_VC    2
#define MPOA_MODE_ROUTED_IP_LLC     3
#define MPOA_MODE_ROUTED_IP_VC      4

/**
 * Instance flags
 */
#define EU_OPEN            0
#define EU_WRITING         1
#define EU_HAS_INT         2
#define EU_READING         3
#define EU_MSG_INITIALIZED 4
#define EU_UNPLUG          5
#define EU_LOW_RATE        6
#define EU_DSP_IPG         7
#define EU_DSP_LOADED      8
#define EU_ETH_REGISTERED  9    /* Successfully registered */
#define EU_ETH_REGISTERING 10   /* Registration in progress */

#define EU_SET_FLAG(i,f)    set_bit((f), &(i)->flags)
#define EU_CLEAR_FLAG(i,f)  clear_bit((f),&(i)->flags)
#define EU_TEST_FLAG(i,f)   test_bit((f),&(i)->flags)

#define STAT_COUNT        0x0015
#define ATM_CELL_SIZE     53

/* ---------------------------- Stats Counters ----------------------------- */

/*
 * Received ATM Cells
 */
#define STAT_CELLS_RX               0x0000
/*
 * Transmited ATM Cells
 */
#define STAT_CELLS_TX               0x0001

/*
 * Number of cells drop because of CRC error
 */
#define STAT_CELLS_LOST_CRC         0x0002
/*
 * Stats Lost because of invalid VPI/VCI
 */
#define STAT_CELLS_LOST_VPIVCI      0x0003
/*
 * Number of cells lost because reassembly buffer is to short
 */
#define STAT_CELLS_LOST_OTHER       0x0004

/*
 * Received Ethernet/IP Packets
 */
#define STAT_PAKTS_RX               0x0005
/*
 * Transmited Ethernet/IP Packets
 */
#define STAT_PAKTS_TX               0x0006
/*
 * Packets lost because over-sized
 */
#define STAT_PAKTS_LOST_OSIZE       0x0007
/*
 * Nb Packets droped after filter
 */
#define STAT_PAKTS_FILTERED         0x0008

#define STAT_ATMVCI                 0x0009
#define STAT_ATMVPI                 0x000A
#define STAT_ATMHEC                 0x000B
#define STAT_ATMDELIN               0x000C

#define STAT_DSLMARGIN              0x000D
#define STAT_DSLATTEN               0x000E
#define STAT_DSLFEC                 0x000F
#define STAT_DSLVIDCPE              0x0010
#define STAT_DSLVIDCO               0x0011
#define STAT_DSLTXRATE              0x0012
#define STAT_DSLRXRATE              0x0013
/*
 * Number of OAM cells received
 */
#define STAT_CELLS_OAM_RCVD	    0x0014


typedef struct eu_instance_s
{
    /*
     * To count the number of interfaces linked to this structure ...
     */
    uint32_t users;

    
    /*
     * To protect accesses to this structure.
     */
    spinlock_t lock;
    
    
    /*
     * To link all of our instances
     */
    struct list_head list;

    /*
     * Holds USB device information (dev, interfaces, pipes, etc.)
     */
    struct usb_device *usbdev;

    /*
     * URBs allocated for this device
     */
    struct urb  *urb_int;
    struct urb  *urb_write;
    struct urb  *urb_oam_write;
    struct urb  *read_urb[INCOMING_Q_SIZE];      /* Array of receive urbs  */


    /*
     * Device's endpoints
     */
    uint32_t pipe_int_in;
    uint32_t pipe_bulk_idma_out;
    uint32_t pipe_bulk_data_out;
    uint32_t pipe_bulk_data_in;
    uint32_t pipe_iso_data_in;

    /*
     * Data returned on interrupt by modem
     */
    eu_cdc_t *intr_data;
    

    /*
     * Cache used to allocate read buffers
     */
    kmem_cache_t *rb_cache;

    /*
     * Maximum Receive Unit - depends on the encapsulation
     */
    unsigned int mru;

    /*
     * Size of the header to prepend in front of inbound data
     */
    unsigned int eth_hdr;
    

    /*
     * To hold outbound data
     */
    uint8_t   *segmentation_buffer;
    
    

    /*
     * Modem state machine info
     */
    AdiMSM            AdiModemSm;
    wait_queue_head_t sync_q;

    
    eu_cmv_msg_t   *pDriverCMVs;

    /*DSP code info*/
    IDMAPage  MainPage;
    IDMAPage *pSwapPages;
    uint32_t    SwapPageCount;

    /*URB queues*/
    struct list_head ctrl_urb_free_q;  /* Free queued_urb_t queue */
    struct list_head ctrl_urb_ipg_q;  /* In-progress control urb queue */
    struct urb      *ctrl_urb_failed;
    spinlock_t       ctrl_q_lock;
    
    

    /*Kernel timer(s)*/
    struct timer_list ctrl_urb_q_timer;
    struct timer_list ctrl_urb_retry;
    
    
    /*
     * Tasklets
     */
    struct tasklet_struct rcv_complete_tasklet;
    
#ifdef LINUX_2_6
    /*
     * Work queue for eth creation
     */
    struct work_struct   create_eth;
#endif

    /*
     * Boot queue / task
     */
#ifdef LINUX_2_6
    struct work_struct   boot_sm;
#elif defined(LINUX_2_4)
    struct tq_struct boot_sm;
#endif    

    boot_state_t boot_state;
    uint16_t  swap_data;
    
    /*
     * Wait Queue
     */
    wait_queue_head_t thr_wait;

    /*
     * Completed Read Queue: data to be treated by kernel
     */
    struct list_head  comp_read_q;
    
    /*
     * Associated spinlock
     */
    spinlock_t       comp_read_q_lock;

    
    
    /*
     * Misc data
     */
    volatile unsigned long flags;
    
#ifdef DELAY_ISO_STARTUP
    uint32_t          IsReadPipeStarted;
#endif
    uint32_t           MpoaMode;
    uint8_t            mac[ETH_ALEN];

    /*
     * Ethernet interface data
     */
    uint32_t                 out_pkt_size;
    struct net_device       *eth;
    struct net_device_stats  LinuxStats;
    char                     if_name[IFNAMSIZ];
    
    /*
     * Statistics
     */
    uint32_t Statistics[STAT_COUNT];

    /*
     * ATM data
     */
    eu_atm_vc_t Vc;

    /*
     * State variables used for OAM stuff
     */
    uint32_t           OAMState_TimerOn;
    uint32_t           OAMState_SendingCC;
    uint32_t           OAMState_ReceiveCC;
    uint32_t           OAMState_AIS;
    uint32_t           OAMState_CCSink;
    uint8_t            OAMCell[ATM_CELL_SIZE*2];
    uint8_t           *pOAMCell;
    uint8_t            OAMCellHeader[ATM_CELL_HEADER_SIZE*2];
    struct timer_list  OAMTimer;

} eu_instance_t;


#endif /* __KERNEL__ */

#endif /* __EU_TYPES_H__ */
