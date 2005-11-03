/*****************************************************************************/
/* Copyright 2002 Christian Casteyde (casteyde.christian@free.fr)            */
/*                                                                           */
/* eaglectrl is free software; you can redistribute it                       */
/* and/or modify it under the terms of the GNU General Public License as     */
/* published by the Free Software Foundation; either version 2 of the        */
/* License, or (at your option) any later version.                           */
/*                                                                           */
/* "ADI USB ADSL Driver for Linux" is distributed in the hope that it will   */
/* be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU General Public License for more details.                              */
/*                                                                           */
/* You should have received a copy of the GNU General Public License         */
/* along with "ADI USB ADSL Driver for Linux"; if not, write to the Free     */
/* Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,            */
/* MA  02111-1307  USA                                                       */
/*                                                                           */
/* Written by Fred. Ros (sl33p3r@free.fr) based on the adictrl utils by      */
/* Christian Casteyde (casteyde.christian@free.fr)                           */ 
/*                                                                           */
/*****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <assert.h>
#include <linux/usbdevice_fs.h>

#include "../eagle-usb.h"
#include "../debug.h"



/* ----------------------------------- Macros ----------------------------------- */

#define REVISION "-1.0"

#define EAGLECTRLVERSION EAGLEUSBVERSION REVISION

#define USB_PROC_DIR  "/proc/bus/usb"

#define PRE_FIRMWARE   0x01
#define POST_FIRMWARE  0x02

#define SECTION_BEGINNING "<eaglectrl>"
#define SECTION_END       "</eaglectrl>"


#define NB_SUPPORTED_PRODS  (sizeof(supported_products)/sizeof(supported_products[0]))

#ifndef CONF_DIR
#define CONF_DIR            "/etc/eagle-usb"
#endif

#ifndef BIN_DIR
#define BIN_DIR             "/etc/eagle-usb"
#endif

#define ISDN_BIN_FILE    "dsp_code_isdn.bin"
#define POTS_BIN_FILE    "dsp_code_pots.bin"

#define OPTS_FILE    "eagle-usb.conf"

#if USE_CMVS
#define DEFAULT_CMV_ISDN_FILE   CONF_DIR "/CMVei.txt"
#define DEFAULT_CMV_POTS_FILE   CONF_DIR "/CMVep.txt"
#endif /* USE_CMVS */

#define MAX_PATH_LENGTH     255

#define IS_VALID_ENTRY(n)   ( (strncmp ((n),".",1) != 0 ) &&    \
                              ( strncmp ((n),"..",2) != 0 ) ) 

#define STATE_TO_STR(s)     ((s) == POST_FIRMWARE ? "post" : "pre")

/*
 * Compute open file size
 */
#define GET_FILE_SIZE(f,s)                      \
   do                                           \
   {                                            \
       fseek ((f),0L, SEEK_END);                \
       (s)=ftell ((f));                         \
       fseek ((f),0L, SEEK_SET);                \
   } while(0)


/*
 * Options related defines
 */
#define DO_DSP        (1<<1)
#define DO_OPTS       (1<<2)
#define DO_IF         (1<<3)
#define DO_SYNCHRO    (1<<4)
#define DO_WIZARD     (1<<5)
#define DO_GETDBG     (1<<6)
#define DO_SETDBG     (1<<7)
#if USE_CMVS
#define DO_CMVS       (1<<8)
#endif /* USE_CMVS */

#define SKIP_BLANKS(p)                                          \
    while ((*(p) !='\0') &&                                     \
           ((*(p) == ' ') || (*(p) == '\t') || (*(p) == '\n'))) \
    {                                                           \
        p++;                                                    \
    }

#define SKIP_NON_BLANKS(p)                                      \
    while ((*(p) !='\0') &&                                     \
           ((*(p) != ' ') && (*(p) != '\t') && (*(p) != '\n'))) \
    {                                                           \
        p++;                                                    \
    }


#define REMOVE_CHAR(p,c)                        \
{                                               \
    char *__ptr= (p);                           \
                                                \
    while ((*__ptr != '\0') && (*__ptr != c))   \
    {                                           \
        __ptr ++;                               \
    }                                           \
    if ( *__ptr == c )                          \
    {                                           \
        *__ptr = '\0';                          \
    }                                           \
}

#define NB_OPTIONS (sizeof(eu_options_t)/sizeof(eu_opt_t))

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define MODEM_ISDN   0x01
#define MODEM_POTS   0x02
#define PREAMBLE                0x535C
#define RECEIVER_MODEM          0x0000
#define SENDER_HOST             0x0010
#define SUBTYPE_MEM_REQWRITE    0x0100
#define TYPE_MEMACCESS          0x1000
#define WRITE_TO_MODEM (RECEIVER_MODEM + SENDER_HOST + SUBTYPE_MEM_REQWRITE + TYPE_MEMACCESS)

#if USE_CMVS
#define MAX_CMVS_COUNT   100
#endif

/* ----------------------------------- Types ------------------------------------ */

/**
 * product_t - Description of supported USB product
 *
 * @idVendor  Vendor ID
 * @idProduct Product ID
 * @state     Pre or Post firmware
 *
 */
typedef struct 
{
    uint16_t  idVendor;
    uint16_t  idProduct;
    uint16_t  state;            /* Pre/Post firmware */
} product_t;

/**
 * USB device descriptor - Taken from kernel sources
 *
 * @bLength
 * @bDescriptorType
 * @bcdUSB
 * @bDeviceClass
 * @bDeviceSubClass
 * @bDeviceProtocol
 * @bMaxPacketSize0
 * @idVendor
 * @idProduct
 * @bcdDevice
 * @iManufacturer
 * @iProduct
 * @iSerialNumber
 * @bNumConfigurations
 *
 */

typedef struct usb_device_descriptor 
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed)) usb_dev_desc_t;

/**
 * cmv_msg_t - CMV message description.
 *   Copied and adapted from Struct.sh in order to not enter
 *   the mess of including this fu***ng includes
 *
 */
typedef struct
{
    uint16_t preamble;
    uint16_t function;
    uint16_t index;
    uint8_t  sym_addr_hi_hi;
    uint8_t  sym_addr_hi_lo;
    uint8_t  sym_addr_lo_hi;
    uint8_t  sym_addr_lo_lo;
    uint16_t offset;
    uint32_t data;
} cmv_msg_t;

/**
 * detected_dev_t - Description of supported device detected
 *
 * @next         Pointer to next detected device
 * @prod_index   Index in the supported_products array for this device
 * @usb_dev      USB descriptor of this device
 *
 */

typedef struct detected_dev_s
{
    struct detected_dev_s *next;
    char                  *dev_name;
    uint16_t               prod_index;
    uint16_t               bus;
    uint16_t               type; /* ISDN / POTS */
    usb_dev_desc_t         usb_dev;
}
detected_dev_t;


/**
 * Supported options
 */
static struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'v'},
    {"options", optional_argument, 0, 'o'},
#if USE_CMVS   
    {"cmvs", optional_argument, 0, 'c'},
#endif /* USE_CMVS */
    {"dsp", optional_argument, 0, 'd'},
    {"sync", optional_argument, 0, 's'},
    {"print", no_argument, 0, 'p'},
    {"if", no_argument, 0, 'i'},
    {"wizard", no_argument,0,'w'},
    {"get-debug",no_argument,0,'g'},
    {"set-debug",required_argument,0,'x'},
    {"list-debug",no_argument,0,'l'},
    {0, 0, 0, 0}
};

static char short_options[] = "hvf::d::s::o::ipwglx:";

/* Debug level accepted */
typedef struct 
{
    char *flagstr;
    char *description;
    unsigned int flag;
} dbg_opt_t;


static dbg_opt_t debug_flags[15] = {
    { "SAR","        Segmentation and Reassembly subsystem",DBG_SAR},
    { "UNI",        "        User-Network Inferface subsystem",DBG_UNI},
    { "MPOA", "        Multi-Protocol Over ATM subsystem",DBG_MPOA},
    { "INIT", "        Initialization subsystem",DBG_INIT},
    { "ENET","        Ethernet subsystem",DBG_ENET},
    { "BOOT","        Boot subsystem",DBG_BOOT},
    { "UTILS","        Utils subsystem",DBG_UTILS},
    { "MSG","        Messages subsystem",DBG_MSG},
    { "SM","        State Machine subsystem",DBG_SM},
    { "READ","        Read-side subsystem",DBG_READ},
    { "INTS","        Interrupts subsystem",DBG_INTS},
    { "CRC","        CRC subsystem",DBG_CRC},
    { "DSP","        DSP subsystem",DBG_DSP},
    { "OAM","        OAM subsystem",DBG_OAM},
    { "ROUTEDIP","Routed-Ip subsystem",DBG_ROUTEIP}
};

#define NB_DEBUG_FLAGS    15      

/* ------------------------------ Global Variables ------------------------------ */

/**
 * List of supported products
 */
static product_t supported_products[] = {
    /*
     * Pre-Firmware products
     */
    {  EAGLE_VID, EAGLE_I_PID_PREFIRM,      PRE_FIRMWARE },
    {  EAGLE_VID, EAGLE_II_PID_PREFIRM,     PRE_FIRMWARE },
    {  EAGLE_VID, EAGLE_IIC_PID_PREFIRM,    PRE_FIRMWARE },
    {  EAGLE_VID, EAGLE_III_PID_PREFIRM,    PRE_FIRMWARE }, /* Eagle III */
    {  USR_VID,   MILLER_A_PID_PREFIRM,     PRE_FIRMWARE },
    {  USR_VID,   MILLER_B_PID_PREFIRM,     PRE_FIRMWARE },
    {  USR_VID,   HEINEKEN_A_PID_PREFIRM,   PRE_FIRMWARE },
    {  USR_VID,   HEINEKEN_B_PID_PREFIRM,   PRE_FIRMWARE },
    /*
     * Post-Firmware products
     */
    {  EAGLE_VID, EAGLE_I_PID_PSTFIRM,      POST_FIRMWARE },
    {  EAGLE_VID, EAGLE_II_PID_PSTFIRM,     POST_FIRMWARE },
    {  EAGLE_VID, EAGLE_IIC_PID_PSTFIRM,    POST_FIRMWARE },
    {  EAGLE_VID, EAGLE_III_PID_PSTFIRM,    POST_FIRMWARE }, /* Eagle III */
    {  USR_VID,   MILLER_A_PID_PSTFIRM,     POST_FIRMWARE },
    {  USR_VID,   MILLER_B_PID_PSTFIRM,     POST_FIRMWARE },
    {  USR_VID,   HEINEKEN_A_PID_PSTFIRM,   POST_FIRMWARE },
    {  USR_VID,   HEINEKEN_B_PID_PSTFIRM,   POST_FIRMWARE }
};

/**
 * List of detected devices
 */
static struct 
{
    detected_dev_t *dev_head;
    unsigned int nb_devices;
} alive_devices = {0};



/**
 * List of default options
 */
  
static  eu_options_t _default_options = 
{
    { "OPTN0",0x80020066 },
    { "OPTN2", 0x23700000 },
    { "OPTN3", 0 },
    { "OPTN4", 0 },
    { "OPTN5", 0 },
    { "OPTN6", 0 },
    { "OPTN7", 0x2CD8044 },
    { "OPTN15", 0x09090909 },
    { "VPI", 0 },
    { "VCI", 0x20 },
    { "Encapsulation", 6 },
    { "Linetype", 1 },
    { "RatePollFreq", 10 },
};

static unsigned int synchro_timeout = 60;


/* ------------------------ Private Function Prototypes ------------------------- */
static int get_if ( void );
static int parse_file ( FILE *in , eu_options_t *opts);
static void free_devices (void );
static detected_dev_t * find_first_dev (unsigned int state );
static void display_device_type_help (int asked_state);
static int open_dev (int state, unsigned int *type, int *other );
static int synchronize ( unsigned int seconds );
static int display_devices(void);
static void version (void);
static void usage (void);
static void check_device ( usb_dev_desc_t *ud , const char *dev_name, int bus);
static int get_devices ( void );
static int send_dsp ( const char *file );
static int send_options ( const char *file );
static void synchro_timed_out ( int sig );
static int wizard (const char *cmvs, const char *opts, const char *dsp,
                   unsigned int seconds );
static int manage_dbg_mask ( unsigned int options, unsigned int dbg_mask );
static void list_debug_flags (void );
static int parse_dbg_flags ( char *flagstr, unsigned int *flags );
static void format_dbg_string (char *buf,unsigned int mask );
#if USE_CMVS
static int send_cmvs ( const char *file );
static int read_cmv_from_file (
                               const char *filename,
                               cmv_msg_t *drv_cmvs,
                               unsigned int *nb_cmvs
                              );
#endif /* USE_CMVS */


int main (int argc, char *argv[] ) 
{
    int           o;
    int           option_count  = 0;
    int           option_index  = 0;
    char         *dsp_file      = BIN_DIR "/";
    char         *options_file  = CONF_DIR "/" OPTS_FILE;
    char         *cmvs_file     = NULL;
    unsigned int  options_set   = 0;
    int           retcode       = EXIT_SUCCESS;
    unsigned int  dbg_mask      = 0;
    
    
    /* Parse options */
    while ( 1 )
    {
        o = getopt_long ( argc, argv, short_options, long_options, &option_index);

        if ( o == -1) 
        {
            if ( option_count == 0 ) 
            {
                /* No options => usage */
                usage();
                goto byebye;
                retcode = EXIT_FAILURE;
            }
            /* Else .. go on .. */
            break;
        }

        option_count ++;
        
        switch (o) 
        {
            case 'h':
                usage();
                
                retcode = EXIT_SUCCESS;
                goto byebye;                
                break;

            case 'l':
                list_debug_flags();

                retcode = EXIT_SUCCESS;
                goto byebye;                
                break;
                
            case 'v':
                version();
                retcode = EXIT_SUCCESS;
                goto byebye;
                
                break;
                
                
            case 'd':
                if ( optarg ) 
                {
                    /* User gave a DSP file */
                    dsp_file = optarg;
                }
                options_set |= DO_DSP;
                break;
                
            case 'o':
                if ( optarg ) 
                {
                    /* User gave an opt file */
                    options_file = optarg;
                }
                options_set |= DO_OPTS;
                break;

#if USE_CMVS               
            case 'c':
                if ( optarg ) 
                {
                    /* User gave a CMVs file */
                    cmvs_file = optarg;
                }
                options_set |= DO_CMVS;
                break;
#endif /* USE_CMVS */
                
            case 's':

                if ( optarg ) 
                {
                    synchro_timeout = atoi (optarg);
                    printf ("Using synchro timeout: %u\n",synchro_timeout);
                    
                }
                
                options_set |= DO_SYNCHRO;
                
                break;
                
            case 'i':
                options_set |= DO_IF;
                break;

            case 'p':
                if ( !get_devices() ) 
                {
                    fprintf (stderr,"An error occured while trying to get devices list.\n");
                    retcode = EXIT_FAILURE;
                    goto byebye;
                }
                retcode = display_devices();
                goto byebye;
                
                break;
                
            case 'w':
                options_set |= DO_WIZARD;
                break;
                
            case 'g':
                options_set |= DO_GETDBG;
                break;

            case 'x':
                options_set |= DO_SETDBG;
                /* Debug mask could be a number : 0/0xFF
                 *   or a list of options
                 */
                if ( isdigit (optarg[0]) ) 
                {
                    dbg_mask = strtoul(optarg, (char **)NULL, 16);
                }
                else
                {
                    retcode = parse_dbg_flags ( optarg, &dbg_mask );
                    if ( retcode != EXIT_SUCCESS ) 
                    {
                        goto byebye;
                    }
                }
                break;
                
        }        
    }

    if ( ( options_set & DO_GETDBG ) || (options_set & DO_SETDBG) )
    {
        retcode = manage_dbg_mask ( options_set, dbg_mask );
        goto byebye;
    }
    if ( options_set & DO_WIZARD ) 
    {
        
        retcode = wizard (  cmvs_file,options_file, dsp_file, synchro_timeout);
    }
    else 
    {
        if ( options_set & DO_SYNCHRO ) 
        {
            if ( !get_devices() ) 
            {
                fprintf (stderr,"An error occured while trying to get devices list.\n");
                retcode = EXIT_FAILURE;
                goto byebye;
            }
            
            retcode = synchronize ( synchro_timeout );
            goto byebye;
        }
        
    
        /*
         * We need to check that options are not redundant
         */
        
        
        if ( (options_set & DO_IF) && ((options_set & ~DO_IF) != 0 ) )
        {
            fprintf (stderr,"\n**ERR** -i can't be used in conjunction with another option.\n\n");
            usage();
            return(EXIT_FAILURE);
        }
        
        /* -d alone implies -o and -c */
        if ( options_set & DO_DSP ) 
        {
            options_set |= DO_OPTS;
#if USE_CMVS           
            options_set |= DO_CMVS;
#endif /* USE_CMVS */
        }
        
        
        if ( !get_devices() ) 
        {
            fprintf (stderr,"An error occured while trying to get devices list.\n");
            retcode = EXIT_FAILURE;
            goto byebye;
        }        
        
        /* Now .. go on */
        
        if ( options_set & DO_IF ) 
        {
            retcode = get_if();
        }
        
        
        if ( options_set & DO_DSP ) 
        {
#if USE_CMVS           
            retcode = send_cmvs ( cmvs_file );
            if ( retcode == EXIT_FAILURE ) 
            {
                goto byebye;
            }
#endif /* USE_CMVS */
            retcode = send_options ( options_file );
            if ( retcode == EXIT_FAILURE ) 
            {
                goto byebye;
            }
            
            retcode = send_dsp ( dsp_file );
        }
    }
    
  byebye:
    
    if ( alive_devices.dev_head ) 
    {
        free_devices();
    }
    
    return (retcode);
}


static int manage_dbg_mask ( unsigned int options, unsigned int dbg_mask ) 
{
    int                   retcode = EXIT_SUCCESS;
    int                   fd;
    int                   res;
    struct usbdevfs_ioctl ioc;
    struct eu_ioctl_info ioc_info;
    int                   other;
    char                  dbg_str[255];
    
    if ( geteuid() != 0 ) 
    {
        fprintf (stderr,"You must be root to do that !\n");
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    if ( !get_devices() ) 
    {
        fprintf (stderr,"An error occured while trying to get devices list.\n");
        retcode = EXIT_FAILURE;
        goto byebye;
    }
    
    fd = open_dev ( POST_FIRMWARE,NULL,&other );

    if ( fd == -1 ) 
    {
        retcode = EXIT_FAILURE;

        if (other) 
        {
            display_device_type_help (POST_FIRMWARE);
        }
        
        goto byebye;
    }

    
    ioc_info.buffer_size = 0;
    ioc_info.buffer = NULL;
    ioc.ifno = 1;
    if ( options & DO_GETDBG )
    {
        ioc.ioctl_code = EU_IO_GETDBG;
        ioc_info.idma_start = 0;
    }
    else
    {
        ioc.ioctl_code = EU_IO_SETDBG;
        ioc_info.idma_start = dbg_mask;
    }
    
    
    ioc.data = &ioc_info;

    res = ioctl ( fd , USBDEVFS_IOCTL, &ioc ) ;

    if ( res == -1 ) 
    {
        perror ("Unable to send ioctl to driver");
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    format_dbg_string (dbg_str,ioc_info.idma_start);

    if ( options & DO_GETDBG ) 
    {
        
        printf ("Debug Mask is :%s(0x%x)\n",dbg_str,ioc_info.idma_start);
    }
    else
    {

        printf ("Debug mask set to %s(0x%x)\n",dbg_str,ioc_info.idma_start);
    }
    
  byebye:
        return (retcode);
        
}



/**
 * Send option to a post-firmware device
 */
static int send_options ( const char *file ) 
{
    int                    fd      = -1;
    FILE                  *optf    = NULL;
    int                    retcode = EXIT_SUCCESS;
    eu_options_t          drv_options;
    int                    res;
    struct usbdevfs_ioctl  ioc;
    struct eu_ioctl_info  ioc_info;
    int                    other;
    
    if ( geteuid() != 0 ) 
    {
        fprintf (stderr,"You must be root to do that !\n");
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    fd = open_dev ( POST_FIRMWARE, NULL, &other );

    if ( fd == -1 ) 
    {
        retcode = EXIT_FAILURE;

        if (other) 
        {
            display_device_type_help (POST_FIRMWARE);
        }
        
        goto byebye;
    }

    
    /* Open options file  */
    optf = fopen (file,"r");
    if ( optf == NULL ) 
    {
        fprintf (stderr,"Can't open options file %s for reading! \n",file);
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    /* Parse file */
    if ( !parse_file (optf, &drv_options ) ) 
    {
        fprintf (stderr,"There was at least one error in %s."
                 " Options not sent to driver.\n",
                 file);
        goto byebye;
    }

    
    ioc_info.idma_start = 0;
    ioc_info.buffer_size = sizeof(drv_options);
    ioc_info.buffer = (uint8_t *) &drv_options;
    ioc.ifno = 1;
    ioc.ioctl_code = EU_IO_OPTIONS;
    ioc.data = &ioc_info;

    res = ioctl ( fd , USBDEVFS_IOCTL, &ioc ) ;

    if ( res == -1 ) 
    {
        perror ("Unable to send options to driver");
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    printf ("Options successfully sent to driver.\n");
    
    
  byebye:
    if ( fd != -1 ) 
    {
        close (fd);
    }
    if ( optf ) 
    {
        fclose (optf);
    }
    
    return (retcode);
    
}

#if USE_CMVS
static int send_cmvs ( const char *file ) 
{
    int                   fd      = -1;
    int                   retcode = EXIT_SUCCESS;
    int                   res;
    struct usbdevfs_ioctl ioc;
    struct eu_ioctl_info ioc_info;
    cmv_msg_t             drv_cmvs[MAX_CMVS_COUNT];
    unsigned int          type;
    char                  filename[PATH_MAX+1];
    unsigned int          nb_cmvs = 0;
    int                   other   = 0;
    
        
    if ( geteuid() != 0 ) 
    {
        fprintf (stderr,"You must be root to do that !\n");
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    fd = open_dev ( POST_FIRMWARE, &type, &other );

    if ( fd == -1 ) 
    {
        retcode = EXIT_FAILURE;

        if (other) 
        {
            display_device_type_help (POST_FIRMWARE);
        }
        
        goto byebye;
    }
    /*
     * Open CMV file according to modem type if user doesn't specify one
     */
    if ( file == NULL ) 
    {
        
        switch (type)
        {
            case MODEM_POTS:
                strncpy (&filename[0],DEFAULT_CMV_POTS_FILE,PATH_MAX);
                break;
            case MODEM_ISDN:
                strncpy (&filename[0],DEFAULT_CMV_ISDN_FILE,PATH_MAX);
                break;
            default:
                fprintf (stderr,"** ERROR: unknown modem type (0x%x)\n",type);
                retcode = EXIT_FAILURE;
                goto byebye;
                break;
        }
    }
    else
    {
        strncpy (&filename[0], file, PATH_MAX);
    }

    /*
     * Set max. number of CMVS in the furnished array
     */
    nb_cmvs = MAX_CMVS_COUNT;
    
    retcode = read_cmv_from_file ( filename, drv_cmvs, &nb_cmvs );
    if ( retcode != EXIT_SUCCESS ) 
    {
        fprintf (stderr,"Failed while reading CMV file.\n");
        goto byebye;
    }
    
    
    ioc_info.idma_start = nb_cmvs;
    ioc_info.buffer_size = sizeof(cmv_msg_t) * nb_cmvs;
    ioc_info.buffer = (uint8_t *) &drv_cmvs;
    ioc.ifno = 1;
    ioc.ioctl_code = EU_IO_CMVS;
    ioc.data = &ioc_info;

    res = ioctl ( fd , USBDEVFS_IOCTL, &ioc ) ;

    if ( res == -1 ) 
    {
        perror ("Unable to send CMVs to driver");
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    printf ("CMVs successfully sent to driver.\n");
    
  byebye:
    if ( fd != -1 ) 
    {
        close (fd);
    }
    
    return (retcode);
    
}
#endif /* USE_CMVS */

/**
 * send_dsp - Send DSP data to driver
 */

static int send_dsp ( const char *path )
{
    int                    retcode = EXIT_SUCCESS;
    FILE                  *dsp     = NULL;
    int                    fd      = -1;
    size_t                 fsize   = 0;
    void                  *pdsp    = NULL;
    struct usbdevfs_ioctl  ioc;
    struct eu_ioctl_info   ioc_info;    
    int                    res;
    int                    other;
    char                  *file    = NULL;
    struct stat            buf;
    unsigned int           type;
    
    
    if ( geteuid() != 0 ) 
    {
        fprintf (stderr,"You must be root to do that !\n");
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    fd = open_dev ( POST_FIRMWARE, &type, &other );

    if ( fd == -1 ) 
    {
        if ( other ) 
        {
            display_device_type_help (POST_FIRMWARE);
        }
        
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    /*
     * Check if given  path points to a file or a directory
     * and compute what file to load :)
     */
    if ( stat ( path, &buf) ) 
    {
        fprintf (stderr,"Cannot stat %s (%d)\n", path, errno);
        retcode = EXIT_FAILURE;
        goto byebye;
    }
    
    if ( S_ISDIR (buf.st_mode) ) 
    {
        /*
         * OK .. we must look for dsp_code_pots.bin or dsp_code_isdn.bin,
         * depending on wether modem is POTS or ISDN
         */
        file = (char *) malloc ( MAX_PATH_LENGTH );
        if ( !file ) 
        {
            fprintf (stderr,"Can't allocate %d bytes.\n",MAX_PATH_LENGTH);
            retcode = EXIT_FAILURE;
            goto byebye;
        }
        
        switch (type) 
        {
            case MODEM_ISDN:
                snprintf (file,MAX_PATH_LENGTH,"%s/%s",path,ISDN_BIN_FILE);
                printf("Using DSP code for ISDN line\n");
                break;
            case MODEM_POTS:
                snprintf (file,MAX_PATH_LENGTH,"%s/%s",path,POTS_BIN_FILE);
				printf("Using DSP code for POTS line\n");
                break;
            default:
                fprintf (stderr,"Unknown modem type 0x%x",type);
                retcode = EXIT_FAILURE;
                goto byebye;
        }
        
    }
    else if ( S_ISREG (buf.st_mode) ) 
    {
        /*
         * Use it ..
         */
        file = strdup (path);
    }
    else
    {
        fprintf (stderr,"\"%s\" neither a file nor a directory. \n",path);
        retcode = EXIT_FAILURE;
        goto byebye;
        
    }

    
    /* Open DSP file  */
    dsp = fopen (file,"r");
    if ( dsp == NULL ) 
    {
        fprintf (stderr,"Can't open DSP file %s for reading! \n",file);
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    GET_FILE_SIZE (dsp,fsize);
    
    /* Get space for DSP */
    pdsp = malloc (fsize);
    if ( pdsp == NULL ) 
    {
        fprintf (stderr,"Can't allocate %lu bytes !\n", (unsigned long)fsize);
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    /* Load firmware in memory */
    if ( fread (pdsp, 1, fsize, dsp) != fsize ) 
    {
        fprintf (stderr,"Unable to read DSP file %s!\n",file);
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    ioc_info.idma_start = ntohl (*((uint32_t *)pdsp));
    ioc_info.buffer_size = fsize - sizeof(uint32_t);
    ioc_info.buffer = (void *)(((uint32_t *)pdsp) + 1);
    ioc.ifno = 1;
    ioc.ioctl_code = EU_IO_DSP;
    ioc.data = &ioc_info;

    res = ioctl ( fd, USBDEVFS_IOCTL, &ioc );

    if ( res == -1 ) 
    {
        perror ("Unable to send DSP code to the modem");
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    printf ("DSP code successfully loaded.\n");
    
    
  byebye:
    if ( pdsp ) 
    {
        free (pdsp);
    }
    
    if ( fd != -1 ) 
    {
        close (fd);
    }

    if ( dsp ) 
    {
        fclose (dsp);
    }

    if ( file ) 
    {
        free (file);
    }
    
    
    return (retcode);
    

}


/**
 * get_devices - Parses /proc/bus/usb to find all the present devices
 *               we're supporting.
 *
 * @return TRUE  : Everything was OK
 *         FALSE :  An error has been encountered
 *
 */
static int get_devices ( void ) 
{
    DIR *usb_dir, *bus_dir;
    struct dirent *bus_entry, *dev_entry;
    char bus_name[PATH_MAX];
    char dev_name[PATH_MAX];
    usb_dev_desc_t usb_desc;
    ssize_t read_bytes = 0;
    int fd;
    char *ptr;

    
    
    usb_dir = opendir (USB_PROC_DIR);

    if ( usb_dir == NULL ) 
    {
        fprintf (stderr,"Unbale to open %s\n", USB_PROC_DIR);
        fprintf (stderr,"Please check usbdevfs filesystem is mounted.\n");
        return (FALSE);
    }
    
    bus_entry = readdir (usb_dir);

    while ( bus_entry != NULL ) 
    {
        /* Skip . and .. */
        if ( !IS_VALID_ENTRY (bus_entry->d_name) ) 
        {
            goto next_bus;
        }

        snprintf (bus_name, PATH_MAX,"%s/%s",USB_PROC_DIR, bus_entry->d_name);

        bus_dir = opendir (bus_name);
        
        if ( bus_dir == NULL ) 
        {
            goto next_bus;
        }

        
        dev_entry = readdir ( bus_dir ) ;
        while ( dev_entry != NULL ) 
        {
            /* Skip . and .. */
            if ( IS_VALID_ENTRY (dev_entry->d_name) ) 
            {
                snprintf (dev_name, PATH_MAX,"%s/%s",bus_name,
                          dev_entry->d_name);

                fd = open ( dev_name, O_RDWR );

                if ( fd != -1 ) 
                {

                    read_bytes = 0;
                    ptr = (char *)&usb_desc;
                    do 
                    {
                        read_bytes = read ( fd, (char *)(ptr+read_bytes),
                                            sizeof(usb_dev_desc_t) - read_bytes );
                        if ( read_bytes == -1 ) 
                        {
                            if ( errno == EINTR )
                            {
                                /* We've been interrupted .. */
                                read_bytes = 0;
                                continue;
                            }
                            /* Else .. error */
                            fprintf (stderr,"**ERROR** while reading %s (errno=%d)\n",
                                     dev_name, errno);
                            return(FALSE);
                        }
                    } while (read_bytes < sizeof (usb_dev_desc_t) ) ;

                    /* Check this is one of our supported devices */
                    check_device ( &usb_desc, dev_name,atoi (dev_entry->d_name));
                    
                    close (fd);
                }   
                    
            }

            /* Goto next device */
            dev_entry = readdir (bus_dir);
        }
        closedir ( bus_dir );
        
      next_bus:
        /* Goto next bus */
        bus_entry = readdir (usb_dir);
    }
    
    return (TRUE);
}

/**
 * check_device - check if the given device is supported.
 *                If so add it to the list of present devices
 */
static void check_device ( usb_dev_desc_t *ud , const char *dev_name, int bus) 
{
    int i;
    product_t *p = &supported_products[0];
    detected_dev_t *dd, *tail = NULL;
    
    
    for ( i=0; i < NB_SUPPORTED_PRODS; i++, p++ ) 
    {
        
        if ( ( ud->idVendor == p->idVendor) &&
             ( ud->idProduct == p->idProduct ) )
        {
            /* Match !! */
            
            dd = malloc ( sizeof(detected_dev_t) );
            if ( dd == NULL ) 
            {
                fprintf (stderr,"**ERROR** No more memory !!!!!!! \n");
                return;
            }
            
            dd->next = NULL;
            dd->prod_index = i;
            dd->bus = bus;
            dd->dev_name = strdup ( dev_name );

            if ( (ud->bcdDevice & 0x80) == 0x80 ) 
            {
                dd->type = MODEM_ISDN;
            }
            else
            {
                dd->type = MODEM_POTS;
            }
            
            memcpy (&(dd->usb_dev), ud, sizeof(usb_dev_desc_t));

            /* Insert it */
            

            if ( alive_devices.dev_head == NULL )
            {
                alive_devices.dev_head = dd;
            }
            else
            {
                tail = alive_devices.dev_head;
                
                while ( tail->next != NULL )
                {
                    tail = tail->next;
                }
                
                tail->next = dd;
            }
      
            alive_devices.nb_devices ++;
        }
        
    }

}

/**
 * Display usage
 */
static void usage (void)
{
    printf ("Usage: eaglectrl [-h|--help] [-v|--version] \n");
    printf ("                 [-o|--options [file]] [-d|--dsp [file]] [-i|--if]\n");
    printf ("                 [-s|--sync [sec]] [-p|--print] [-w|--wizard]\n");
    printf ("                 [-g|--get-debug] [-x|--set-debug debug-mask]\n");
    printf ("                 [-c|--cmvs file] [-l|--list-debug]\n");
    

    printf ("  -h|--help            : display this help\n");
    printf ("  -v|--version         : display version number\n");
    printf ("  -o|--options [file]  : set options file \n");
    printf ("                         (file defaults to %s)\n",CONF_DIR "/" OPTS_FILE);
#if USE_CMVS    
    printf ("  -c|--cmvs [file]     : set CMVs file (autodetected)\n");
#endif    
    printf ("  -d|--dsp [path]      : send Options, CMVS, and DSP file\n");
    printf ("                         path is either a path to the DSP file or to a directory\n"
            "                         containing DSP files for ISDN and POTS. In this case eaglectrl\n"
            "                         will choose which file to send to modem depending on the modem\n"
            "                         type. Defaults to %s\n",BIN_DIR);
    printf ("  -i|--if              : print modem interface name\n");
    printf ("  -s|--sync  [sec]     : wait sec seconds for modem to be operational.\n"); 
    printf ("                         If sec is 0 wait until synchro happens (infinite).\n");
    printf ("                         Default is 60\n");
    printf ("  -p|--print           : display detected devices\n");
    printf ("  -w|--wizard          : wizard mode. Works if you have ONLY 1 eagle modem\n");
    printf ("                          plugged in. Choose itself sequence to send to \n");
    printf ("                          modem (firmware/DSP ..)\n");
    printf ("                          NOTE: You can use it in conjunction with -o, \n");
    printf ("                                -d and -s to specify the options file, \n"
            "                                DSP file and synchro timeout values to use.\n");
    printf ("  -g|--get-debug       :  Get debug mask\n");
    printf ("  -x|--set-debug dbg   : Set driver debug-mask to the given value,\n");
    printf ("                         which can either be an hexadecimal number (0x0, 0xFF)\n");
    printf ("                         or a list of debug flags separated by a comma. A list of\n");
    printf ("                         available flags can be given by the --list-debug option.\n");
    printf ("  -l|--list-debug      : Display a list of available flags for the debug-mask.\n");
    
    printf ("\n");
}

static void version (void) 
{
    printf ("eaglectrl version %s\n",EAGLECTRLVERSION);
    printf ("Compiled with gcc %s\n\n",__VERSION__);
    printf ("Copyright 2004 Frederick Ros\n"
	    "This program is free software; you can redistribute it and/or modify it\n"
	    "under the terms of the GNU General Public License as published\n"
	    "by the Free Software Foundation; either version 2 of the License,\n"
	    "or (at your option) any later version.\n");
    printf ("Written by Fred. \"Sleeper\" Ros based on adictrl (C. Casteyde)\n");
    printf ("\n");
}


/**
 * Display detected devices
 */
static int display_devices(void) 
{
    detected_dev_t *pd;
    int retcode = EXIT_SUCCESS;
    
    /* Detect devices */
    
    pd = alive_devices.dev_head;

    while ( pd ) 
    {
        printf ("%s-firmware device on USB bus %d (type=%s)\n",
                (supported_products[pd->prod_index].state == PRE_FIRMWARE ?
                 "Pre" : "Post"),pd->bus,
                (pd->type == MODEM_POTS ? "POTS" : "ISDN"));

        pd = pd->next;
        
    }
    
    return (retcode);
    
}

/**
 * synchro_timed_out - SIGALRM handler
 */
static void synchro_timed_out ( int sig ) 
{
    fprintf (stderr,"Can't get synchro within %u seconds.\n",synchro_timeout);

    free_devices();

    exit ( EXIT_FAILURE );
    
}


/**
 * synchronize - wait for the modem to be operationnal
 */
static int synchronize ( unsigned int seconds ) 
{
    struct usbdevfs_ioctl ioc;
    int code;
    int ret_code = EXIT_FAILURE;
    int fd;
    int other;
    
    /* Should be root .. */
    if ( geteuid() != 0 ) 
    {
        fprintf (stderr,"You must be root to do that !\n");
        return (ret_code);
    }

    fd = open_dev (POST_FIRMWARE, NULL, &other);

    if ( fd == -1 ) 
    {
        if ( other ) 
        {
            display_device_type_help (POST_FIRMWARE);
        }

        return (ret_code);
    }


    if ( seconds != 0 ) 
    {
        /*
         * Install SIGALRM handler + alarm
         */
        signal (SIGALRM, &synchro_timed_out);
        alarm ( seconds );
    }
    
    ioc.ifno = 1;
    ioc.ioctl_code = EU_IO_SYNC;
    ioc.data = NULL;
    code = ioctl ( fd , USBDEVFS_IOCTL, &ioc);

    if ( seconds != 0 ) 
    {
        /*
         * Cancel alarm
         */
        alarm (0);
    }
    
    if ( code == -1 ) 
    {
        perror ("Unable to synchronise with modem\n");
    }
    else
    {
        ret_code = EXIT_SUCCESS;
    }

    close (fd);
    
    return (ret_code);
}


/**
  * Opens the first device found (pre or post firmware)
  * the one specified in the DEVICE env variable
  * (WATCH OUT: in this case no check on the pre/post-iness are done )
  *
  *  If a device of the requested type is not found ( i.e the function
  *  returns -1), and other is non-NULL, *other is set to TRUE if
  *  an device of the other type is found (i.e if the user asks for a
  *  pre-firmware, that none is found, but a post-one is found)
  *
  */
static int open_dev (int state, unsigned int *type, int *other ) 
{
    char           *path;
    int             fd = -1;
    detected_dev_t *pd;
    
    
    if ( (path=getenv ("DEVICE")) )
    {
        fd = open ( path, O_RDWR );

        if ( fd != -1 ) 
        {
            if ( type ) 
            {
                
                usb_dev_desc_t usb_desc;
                ssize_t read_bytes = 0;
                char *ptr;
                /*
                 * Get the type
                 */
                read_bytes = 0;
                ptr = (char *)&usb_desc;
                do 
                {
                    read_bytes = read ( fd, (char *)(ptr+read_bytes),
                                        sizeof(usb_dev_desc_t) - read_bytes );
                    if ( read_bytes == -1 ) 
                    {
                        if ( errno == EINTR )
                        {
                            /* We've been interrupted .. */
                            read_bytes = 0;
                            continue;
                        }
                        /* Else .. error */
                        fprintf (stderr,"**ERROR** while reading %s (errno=%d)\n",
                                 path, errno);
                        return(FALSE);
                    }
                } while (read_bytes < sizeof (usb_dev_desc_t) ) ;

                (*type) = ( (usb_desc.bcdDevice & 0x80 ) == 0x80 ?
                            MODEM_ISDN : MODEM_POTS );
            }
            
            return (fd);
        }
        else
        {
            fprintf (stderr,"**ERROR** Unable to open %s: errno=%d\n",
                     getenv ("DEVICE"), errno);
            return -1;
        }
        
    }

    if ( other ) 
    {
        *other = FALSE;
    }
    
    /* Found first post-firmware device */
    pd = find_first_dev ( state );
    
    if ( pd ) 
    {
        /* Found one :) */
        /* Let's open :) */
        fd = open ( pd->dev_name, O_RDWR );

        if (type ) 
        {
            *type = pd->type;
        }    
    }
    else 
    {
        /**
         * Just some search to help user :)
         */
            
        if ( state == PRE_FIRMWARE ) 
        {
            state = POST_FIRMWARE;
        }
        else
        {
            state = PRE_FIRMWARE;
        }
            
        /* No post-firmware devices found ..*/
        pd = find_first_dev ( state );

        if ( pd ) 
        {
            if ( other ) 
            {
                *other = TRUE;
            }                
        }
        else
        {
            fprintf (stderr,"Can't find any PRE or POST firmware devices.\n");
            fprintf (stderr,"Is your device plugged in ?\n");
        }
            
    }
        
        

    return (fd);
    
}

static void display_device_type_help (int asked_state) 
{

    int found_state;

    if ( asked_state == PRE_FIRMWARE )
    {
        found_state = POST_FIRMWARE;
    }
    else
    {
        found_state = PRE_FIRMWARE;
    }
    
    fprintf (stderr,"Can't find any %s-firmware devices.\n",
             STATE_TO_STR (asked_state));
    fprintf (stderr,"But I've been able to find a %s-firmware one.\n",
             STATE_TO_STR (found_state));

    if ( asked_state == POST_FIRMWARE ) 
    {
        fprintf (stderr,"Wow .. eagle-usb driver should have transform this in post-firmware...\n");
    }

}



static detected_dev_t * find_first_dev (unsigned int state ) 
{
    detected_dev_t *pd;
    
    pd = alive_devices.dev_head;
    
    while ( (pd) && (supported_products[pd->prod_index].state != state) ) 
    {
        pd = pd->next;
    }

    return (pd);

}

static void free_devices (void ) 
{
    detected_dev_t *pd,*opd;

    pd = alive_devices.dev_head;

    while ( pd ) 
    {
        free (pd->dev_name);
        opd = pd;
        pd = pd->next;
        free (opd);
    }
    
    alive_devices.dev_head = NULL;
    alive_devices.nb_devices = 0;
}

static int parse_file ( FILE *in , eu_options_t *opts) 
{
    char           line[256];
    char          *ptr;
    unsigned int   line_nb    = 0;
    unsigned int   in_section = FALSE;
    unsigned int   section_detected = FALSE;
    size_t         length;
    int            i;
    int            retcode    = TRUE;
    
    
    /* First initialize options with default */
    memcpy (opts, _default_options, sizeof(eu_options_t));
    
    while ( fgets (&line[0], 256, in ) != NULL ) 
    {
        ptr = &line[0];
        line_nb ++;
        
        REMOVE_CHAR (ptr,'#');
        SKIP_BLANKS (ptr);

        if (strlen (ptr) == 0 ) 
        {
            /* Line is empty */
            continue;
        }

        /*
         * Is line beginning of section ?
         */
        if ( strncmp (ptr,SECTION_BEGINNING,strlen (SECTION_BEGINNING))==0 ) 
        {
            section_detected = TRUE;
            
            if ( in_section == TRUE ) 
            {
                /*
                 * We're already in section ...
                 */
                fprintf (stderr,"Found \"%s\" but we're already in section. Skipping.\n",
                         SECTION_BEGINNING);
            }
            
            in_section = TRUE;
            continue;
        }

        /*
         * Is line end of section ?
         */
        if ( strncmp (ptr,SECTION_END,strlen (SECTION_END))==0 ) 
        {
            if ( in_section == FALSE ) 
            {
                /*
                 * We were not in section !
                 */
                fprintf (stderr,"Found \"%s\" but we're not in section. Skipping.\n",
                         SECTION_END);
                continue;
            }
            
            in_section = FALSE;
            
            /*
             * Being hre means that we saw a <eaglectr> some line before,
             * and now a </eaglectr> : this means this is a "newstyle" eaglectrl
             * conf file. We should now stop. PAssed this point nothing for us.
             */
            goto byebye;
        }

        
        /*
         * If we're not in section: just skip it ...
         */
        if ( ( section_detected == TRUE) && ( in_section == FALSE ) )
        {
            continue;
        }
        
        /* Check that line has the format xxx = yyy */
        length = strcspn (ptr," \t=");

        if ( length == 0 ) 
        {
            fprintf (stderr,"line %d is malformed. Skipping\n",line_nb);
            retcode = FALSE;
            continue;
        }

        /* Look for that option */
        for (i=0; i< NB_OPTIONS; i++ ) 
        {
            if ( strncmp (ptr,_default_options[i].name, length ) == 0 ) 
            {
                /* Match !! */
                break;
            }
            
        }

        if ( i < NB_OPTIONS ) 
        {
            /* We found one :) */
            /* Now get the associated value */
            ptr += length;
            /* Skip spaces and = */
            length = strspn (ptr," \t=");
            ptr += length;

            /* Check that there's something .. */
            if ( strlen (ptr) == 0 ) 
            {
                fprintf (stderr,"Error line %d : option %s found but no value\n",
                         line_nb, _default_options[i].name);
                retcode = FALSE;
                continue;
            }

            REMOVE_CHAR (ptr,'\n');
            
            (*opts)[i].value = (uint32_t) strtoul (ptr,NULL,16);
            if (errno == ERANGE) 
            {
                fprintf (stderr,"Value of option overflows on line %d\n", line_nb);
		retcode = FALSE;
		goto byebye;
            }
            
        }
        else
        {
            fprintf (stderr,"Unknown option on line %d\n",line_nb);
            continue;
        }
        
    }

  byebye:
    return (retcode);
    
}

/**
 * get_if - print interface name
 */
static int get_if ( void )
{
    int                    retcode = EXIT_SUCCESS;
    struct usbdevfs_ioctl  ioc;
    struct eu_ioctl_info  ioc_info;    
    int                    res;
    int                    fd      = -1;
    char                  *name    = NULL;
    int other;
    
    if ( geteuid() != 0 ) 
    {
        fprintf (stderr,"You must be root to do that !\n");
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    fd = open_dev ( POST_FIRMWARE, NULL, &other );

    if ( fd == -1 ) 
    {
        if ( other ) 
        {
            display_device_type_help (POST_FIRMWARE);
        }
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    ioc_info.idma_start = 0;
    ioc_info.buffer_size = 0;
    ioc_info.buffer = NULL;
    ioc.ifno = 1;
    ioc.ioctl_code = EU_IO_GETITF;
    ioc.data = &ioc_info;

    res = ioctl ( fd, USBDEVFS_IOCTL, &ioc );

    if ( res == -1 ) 
    {
        perror ("Unable to get modem network interface name length");
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    /* Get space to store name */
    name = ( char *) malloc ( ioc_info.buffer_size );
    if ( name == NULL ) 
    {
        fprintf (stderr,"Can't allocate %d bytes !\n",ioc_info.buffer_size);
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    ioc_info.buffer = (uint8_t *) name;

    res = ioctl ( fd, USBDEVFS_IOCTL, &ioc );

    if ( res == -1 ) 
    {
        perror ("Unable to get modem network interface name");
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    printf ("%s\n",name);
    
    
  byebye:
    if ( fd != -1 ) 
    {
        close (fd);
    }
    if ( name ) 
    {
        free (name);
    }
    
    return (retcode);
    
}

/**
 * Wizard mode : choose what to do :)
 */
static int wizard (
                   const char *cmvs,
                   const char *opts,
                   const char *dsp,
                   unsigned int seconds
                  ) 
{
    int retcode = EXIT_SUCCESS;
    detected_dev_t *dd;
    int max_loops = 10;
    int loop;
    
    
    /*
     * First of all get alive devices
     */
    if ( !get_devices() ) 
    {
        fprintf (stderr,"An error occured while trying to get devices list.\n");
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    /*
     * Wizard allowed if only 1 supported device is plugged in
     */
    
    if ( alive_devices.nb_devices != 1 ) 
    {
        fprintf (stderr,"wizard mode only supported if you have only 1 device"
                 " plugged-in.\n");
        fprintf (stderr,"I currently found %u plugged device(s)\n",
                 alive_devices.nb_devices);
        retcode = EXIT_FAILURE;
        goto byebye;
    }
        
    /* Check for the state of the device :
     *   o pre-fir,ware : we need to send firmware and get_devices again
     *     code
     *   o post-firmware : we need to send DSP code
     */
    dd = alive_devices.dev_head;
    
    switch ( supported_products[dd->prod_index].state ) 
    {
        case PRE_FIRMWARE:
            printf ("Waiting for pre-firmware device to re-pop as post-firmware device\n");

            loop = 0;
            do 
            {
                
                free_devices();
                sleep (1);
                if ( !get_devices() ) 
                {
                    fprintf (stderr,"An error occured while trying to get devices list.\n");
                    retcode = EXIT_FAILURE;
                    goto byebye;
                }
                printf (".");
                fflush (stdout);
            } while ( ( ( alive_devices.nb_devices == 0 ) ||
                        ( supported_products [alive_devices.dev_head->prod_index].state == PRE_FIRMWARE)) &&
                      ( loop < max_loops ) );

            printf ("\n");
            
            if ( ( alive_devices.nb_devices == 0 ) ||
                 ( supported_products [alive_devices.dev_head->prod_index].state != POST_FIRMWARE) )
            {
                fprintf (stderr,"Modem didn't re-pop as a post-firmware device !\n");
                retcode = EXIT_FAILURE;
                goto byebye;
            }

            printf ("Waiting for USBDEV fs to sync...\n");
            sleep (2);
            
            /* FALLTHROUGH */
            
        case POST_FIRMWARE:
#if USE_CMVS
            /*
             * We need to send CMVs, options and then DSP code
             */
            fprintf (stdout,"Sending CMVs to device %s\n",dd->dev_name);
            retcode = send_cmvs ( cmvs );
            
            if ( retcode == EXIT_FAILURE ) 
            {
                fprintf (stderr,"Failed to send CMVs to device %s\n",
                         dd->dev_name);
                goto byebye;
            }
#endif /* USE_CMVS */
            fprintf (stdout,"Sending options to device %s\n",dd->dev_name);
            retcode = send_options ( opts );
            
            if ( retcode == EXIT_FAILURE ) 
            {
                fprintf (stderr,"Failed to send options to device %s\n",
                         dd->dev_name);
                goto byebye;
            }

            fprintf (stdout,"Sending DSP code to device %s\n",dd->dev_name);

            retcode = send_dsp ( dsp );
            
            if ( retcode == EXIT_FAILURE ) 
            {
                fprintf (stderr,"Failed to send DSP code to device %s\n",
                         dd->dev_name);
                goto byebye;
            }
            
            break;
        default:
            fprintf (stderr,"Error : detected device in unknown state: %u\n",
                     supported_products[dd->prod_index].state);
            retcode = EXIT_FAILURE;
            goto byebye;
    }

    /*
     * Being here means we have successfully send the DSP code
     * to the modem. Now wait for the synchro
     */
    fprintf (stdout,"Waiting for synchro...\n");
    retcode = synchronize ( seconds );
    
    if ( retcode == EXIT_FAILURE ) 
    {
        fprintf (stderr,"Failed to synchronize code to device %s\n",
                 dd->dev_name);
        goto byebye;
    }

    printf ("OK .. Modem is synchronized.\n");
    
    /* SUCCESS !!! */
    
  byebye:
    if ( alive_devices.dev_head) 
    {
        free_devices();
    }
    
    
    return (retcode);
    

}

#if USE_CMVS
/**
 * Read the CMVs file.
 *
 * filename: path to the CMVs file
 * drv_cmvs: pointer to CMV array.
 * nb_cmvs: number of read CMVs. When the function is called it should hold the
 *          max. number of CMVs the array can hold.
 *
 * return: EXIT_FAILURE / EXIT_SUCCESS
 */
static int read_cmv_from_file (
                               const char *filename,
                               cmv_msg_t *drv_cmvs,
                               unsigned int *nb_cmvs
                              ) 
{
    cmv_msg_t    *pcmvs   = drv_cmvs;
    char          line[256];
    char         *ptr;
    int           i;
    FILE         *fp      = NULL;
    unsigned int max_cmvs;
    int           retcode = EXIT_SUCCESS;
    
    pcmvs = drv_cmvs;
    max_cmvs = *nb_cmvs;
    *nb_cmvs = 0;
    
    for (i=0,pcmvs = drv_cmvs; i<max_cmvs; i++, pcmvs++ ) 
    {
        pcmvs->preamble = PREAMBLE;
        pcmvs->function = WRITE_TO_MODEM;
        pcmvs->index = 0x1234+i;
        pcmvs->sym_addr_hi_hi = 'O';
        pcmvs->sym_addr_hi_lo = 'P';
        pcmvs->sym_addr_lo_hi = 'T';
        pcmvs->sym_addr_lo_lo = 'N';
        pcmvs->offset = 0;
        pcmvs->data = 0xF9F9F9F9;
    }

    
    fp = fopen (filename,"rt");
    if (fp == NULL) 
    {
        fprintf (stderr,"CMV file %s does not exists\n",filename);
        retcode = EXIT_FAILURE;
        goto byebye;
    }

    /*
     * Now treat file
     */
    pcmvs = drv_cmvs;    
    while (feof (fp) == 0 ) 
    {
        if ( fgets (line, 256, fp) == NULL ) 
        {
            continue;
        }

        if ( (*nb_cmvs) >= (max_cmvs-1) ) 
        {
            fprintf (stderr,"ERR: maximum number of CMVs (%u) reached.\n",
                     max_cmvs-1);
            retcode = EXIT_FAILURE;
            goto byebye;
        }

        
        /* Skip spaces at beginning */
        ptr = &line[0];
        
        REMOVE_CHAR(ptr,'\n');
        SKIP_BLANKS (ptr);

        switch (*ptr) 
        {
            case '\0':          /* Line empty */
                continue;
                break;

            case '#':
            case ';':           /* Comments */
                continue;
                break;
        }
        /* Next 2 chars should be CW */
        if ( strncasecmp (ptr,"CW",2) != 0 ) 
        {
            fprintf (stderr,"[ERR] CMV file line \"%s\" not treated.\n",line);
            retcode = EXIT_FAILURE;
            goto byebye;
        }

        /* Advance ptr */
        ptr += 2;
        
        
        SKIP_BLANKS (ptr);
        if ( *ptr == '\0' ) 
        {
            fprintf (stderr,"[ERR] CMV file line too short (%s)\n",line);
            retcode = EXIT_FAILURE;
            goto byebye;
        }
        
        
        /* Next 4 chars are either cntl or optn */
/*         if ( ( strncasecmp (ptr,"OPTN",4) != 0 ) && */
/*              ( strncasecmp (ptr,"CNTL",4) != 0 ) )  */
/*         { */
/*             fprintf (stderr,"[ERR] CMV file line \"%s\" not treated (OPTN/CNTL)\n",line); */
/*             retcode = EXIT_FAILURE; */
/*             goto byebye; */
/*         } */

/*
 * FIXME: Add a check on the type of message. Currently in last version
 * furnished by ADI there are: optn, mask, diag, flag, cntl
 * 
 */
        
        pcmvs->sym_addr_hi_hi = toupper(ptr[0]);
        pcmvs->sym_addr_hi_lo = toupper(ptr[1]);
        pcmvs->sym_addr_lo_hi = toupper(ptr[2]);
        pcmvs->sym_addr_lo_lo = toupper(ptr[3]);

        /* Advance ptr */
        ptr += 4;

        SKIP_BLANKS (ptr);
        if ( *ptr == '\0' ) 
        {
            fprintf (stderr,"[ERR] CMV file line too short (%s)\n",line);
            retcode = EXIT_FAILURE;
            goto byebye;
        }

        /* Next is the offset */
        pcmvs->offset = atoi(ptr);

        /* Advance */
        SKIP_NON_BLANKS (ptr);
        
        SKIP_BLANKS (ptr);
        if ( *ptr == '\0' ) 
        {
            fprintf (stderr,"[ERR] CMV file line too short (%s)\n",line);
            retcode = EXIT_FAILURE;
            goto byebye;
        }

        /* And Data */
        
        pcmvs->data = (unsigned int) strtoul (ptr,NULL,16);
        if (errno == ERANGE) 
        {
            fprintf (stderr,"Value of option overflows (%s)\n", line);
            retcode = EXIT_FAILURE;
            goto byebye;
        }

        pcmvs ++;
        (*nb_cmvs) ++;        
    }    

    /*
     * We add 1 to the nb_cmvs as there should be a last (not used) CMVs to mark
     * the end
     */
    (*nb_cmvs) ++ ;
    
  byebye:
    if ( fp ) 
    {
        fclose (fp);
    }
    
    return(retcode);
}
#endif /* USE_CMVS */

static void list_debug_flags (void ) 
{
    int i = 0;

    printf ("\nAvailable debug flags: \n");

    for (i=0; i< NB_DEBUG_FLAGS; i++ ) 
    {
        printf ("\t%s\t%s\n",
                debug_flags[i].flagstr,debug_flags[i].description);
    }

    printf ("\n");

}

/*
 * WATCH OUT - NO TEST ARE MADE TO SEE IF THE BUFFER IS LONG ENOUGH BETTER TO
 * SIZE IT to (7+2) * NB_DEBUG_FLAGS (i.e longest falg size + pad multiplied by
 * the maximum number of options
 */
static void format_dbg_string (char *buf,unsigned int mask ) 
{
    char *p = buf;

    *p = '\0';
    
    if ( mask & DBG_SAR )
    {
        p += sprintf (p,"SAR ");
    }
    
    
    if ( mask & DBG_UNI )
    {
        p += sprintf (p,"UNI ");
    }
    
    if ( mask & DBG_MPOA )
    {
        p += sprintf (p,"MPOA ");
    }
    
    if ( mask & DBG_INIT )
    {
        p += sprintf (p,"INIT ");
    }
    
    if ( mask & DBG_ENET )
    {
        p += sprintf (p,"ENET ");
    }
    
    if ( mask & DBG_BOOT )
    {
        p += sprintf (p,"BOOT ");
    }
    
    if ( mask & DBG_UTILS )
    {
        p += sprintf (p,"UTILS ");
    }
    
    if ( mask & DBG_MSG )
    {
        p += sprintf (p,"MSG ");
    }
    
    if ( mask & DBG_SM )
    {
        p += sprintf (p,"SM ");
    }
    
    if ( mask & DBG_READ )
    {
        p += sprintf (p,"READ ");
    }
    
    if ( mask & DBG_INTS )
    {
        p += sprintf (p,"INTS ");
    }
    
    if ( mask & DBG_CRC )
    {
        p += sprintf (p,"CRC ");
    }
    
    if ( mask & DBG_DSP )
    {
        p += sprintf (p,"DSP ");
    }
    
    if ( mask & DBG_OAM )
    {
        p += sprintf (p,"OAM ");
    }
    
    if ( mask & DBG_ROUTEIP )
    {
        p += sprintf (p,"ROUTEIP ");
    }

    if ( p == buf ) 
    {
        sprintf (p,"NOTHING ");
    }
    
    
}


/*
 * Parse the given flag string (flags separated by ,) and return
 * a flag mask
 * NOTE: string should not have any trailing/leading space, and is composed
 *       of maks names separated by , (there's NO space)
 */
static int parse_dbg_flags ( char *flagstr, unsigned int *flags ) 
{
    char *ptr;
    char *mark;
    int result = EXIT_SUCCESS;
    unsigned int mask = 0;
    int i;
    int terminated = 0;
    
    *flags = 0;

    ptr = mark = flagstr;
    
    while ( !terminated ) 
    {
        if ( ((*ptr) == ',' ) ||
             ((*ptr) == '\0') )
        {
            /* We're at end of a flag .. but which one ? */
            if ( mark == ptr ) 
            {
                /* Pb: for eaxample ",UNI,MPOA" */
                result = EXIT_FAILURE;
                goto byebye;
            }

            /* Look for mask in list */
            i = 0;
            while (( i< NB_DEBUG_FLAGS) &&
                   ( strncmp (mark, debug_flags[i].flagstr,(int)(ptr-mark)) != 0 ) ) 
            {
                i++;
            }

            if ( i== NB_DEBUG_FLAGS ) 
            {
                /* Not found .. */
                fprintf (stderr,"Unknown flag mask (%*s). Please use --list-debug option.\n",
                         (int)(ptr-mark),mark);
                result = EXIT_FAILURE;
                goto byebye;
            }

            mask |= debug_flags[i].flag;
            printf ("Dbg mask %s found. (%d)\n", debug_flags[i].description, i);
            
            /*
             * Check if we're at end ...
             */
            if ( *ptr != '\0' ) {
                if (*(ptr+1) == '\0' ) 
                {
                    /* Huue... MPOA,UNI, ..*/
                    fprintf (stderr,"Error : trailing comma in list.\n");
                    result =EXIT_FAILURE;
                    goto byebye;
                }
                
                /* Skip , */
                ptr ++;
                mark = ptr;
            }
            else
            {
                terminated = 1;
            }            
        }
        else 
        {
            ptr ++;
        }
    }
    
  byebye:
    *flags = mask;
    return (result);
    
}
