#ifndef __DEBUG_H__

#define __DEBUG_H__

/*
 * The module mask : in Adiusbadsl.c
 */
extern unsigned int module_dbg_mask;

#ifdef __KERNEL__

/*
 * Output debug statement for module m
 */
#define eu_dbg(m,format,args...)                                \
  do                                                            \
  {                                                             \
      if ( module_dbg_mask & (m) )                              \
      {                                                         \
          printk(KERN_DEBUG"[eu - " #m "]  " format, ##args);   \
      }                                                         \
  } while (0)


/*
 * Based on dbg : enters and leaves macro
 */
#define eu_enters(m)    eu_dbg(m,"Entering %s\n",__FUNCTION__ )
#define eu_leaves(m)    eu_dbg(m,"Leaving  %s\n",__FUNCTION__ )

/*
 * Output error message (unconditionally)
 */
#define eu_err(format,args...)   printk(KERN_ERR "[EAGLE-USB] " format, ##args)

/*
 * Output warning message (unconditionally)
 */
#define eu_warn(format,args...)  printk(KERN_WARNING "[Eagle-usb] " format, ##args)

/*
 * Output informational message (unconditionally)
 */
#define eu_report(format,args...)  printk(KERN_INFO "[eagle-usb] " format, ##args)

#endif /* __KERNEL__ */

/*
 * Modules
 */

typedef enum {
    DBG_SAR     = (1<<0),
    DBG_UNI     = (1<<1),
    DBG_MPOA    = (1<<2),
    DBG_INIT    = (1<<3),
    DBG_ENET    = (1<<4),
    DBG_BOOT    = (1<<5),
    DBG_UTILS   = (1<<6),
    DBG_MSG     = (1<<7),
    DBG_SM      = (1<<8),
    DBG_READ    = (1<<9),
    DBG_INTS    = (1<<10),
    DBG_CRC     = (1<<11),
    DBG_DSP     = (1<<12),
    DBG_OAM     = (1<<13),
    DBG_ROUTEIP = (1<<14)
} dbg_sec_t;


#endif /* __DEBUG_H__ */
