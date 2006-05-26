/******************************************************************************

  Copyright(c) 2003 - 2006 Intel Corporation. All rights reserved.

  802.11 status code portion of this file from ethereal-0.10.6:
    Copyright 2000, Axis Communications AB
    Ethereal - Network traffic analyzer
    By Gerald Combs <gerald@ethereal.com>
    Copyright 1998 Gerald Combs

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110, USA

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Contact Information:
  James P. Ketrenos <ipw2100-admin@linux.intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

******************************************************************************/

/*
 *
 * About the threading and lock model of the driver...
 *
 * There are three paths of execution through the driver.
 *
 * 1.  ioctl based (wireless extensions, netdev, etc.)
 * 2.  interrupt based
 * 3.  scheduled work queue items
 *
 * As soon as an interrupt comes in, it schedules a tasklet.  That tasklet,
 * when run, does any HW checks, pulls any data from the read queue,
 * and schedules other layers to do the actual work.
 *
 * About scanning
 *
 * There are several states related to scanning:
 * !STATUS_SCANNING -- no scan requests, no active scan
 *
 * The states can transition as follows:
 *
 * !STATUS_SCANNING => (SCANNING_DIRECT | SCANNING_INDIRECT)
 * SCANNING_DIRECT => (SCANNING_INDIRECT | !STATUS_SCANNING)
 * SCANNING_INDIRECT => !STATUS_SCANNING
 *
 * Each band requires a seperate scan request.  This means that in a
 * configuration where the user has specified an ESSID in an ABG network,
 * we need to execute 4 scan passes -- one for each band (2.4 or 5.2) and
 * then again with each band one directed and one indirect.
 *
 */
#include <net/ieee80211.h>
#include <net/ieee80211_radiotap.h>
#include <asm/div64.h>

#include "ipw3945.h"

#define IPW3945_VERSION "0.0.70"
#define DRV_DESCRIPTION	"Intel(R) PRO/Wireless 3945 Network Connection driver for Linux"
#define DRV_COPYRIGHT	"Copyright(c) 2003-2006 Intel Corporation"
#define DRV_VERSION     IPW3945_VERSION

MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_VERSION(DRV_VERSION);
MODULE_AUTHOR(DRV_COPYRIGHT);
MODULE_LICENSE("GPL");

static int debug = 0;
static int channel = 0;
static int mode = 0;

static u32 ipw_debug_level;
static int associate = 0;	/* default: don't assoc until user sets assoc parms */
static int auto_create = 1;	/* default: create new adhoc network if needed */
static int led = 1;		/* default: use LEDs */
static int disable = 0;		/* default: enable radio */
static int antenna = 0;		/* default 0 = both antennas (use diversity) */
static const char ipw_modes[] = {
	'a', 'b', 'g', '?'
};

#define LD_TIME_LINK_ON 300

static int from_priority_to_tx_queue[] = {
	IPW_TX_QUEUE_1, IPW_TX_QUEUE_2, IPW_TX_QUEUE_2, IPW_TX_QUEUE_1,
	IPW_TX_QUEUE_3, IPW_TX_QUEUE_3, IPW_TX_QUEUE_4, IPW_TX_QUEUE_4
};

static int ipw_rate_scale_init_handle(struct ipw_priv *priv, s32 window_size);

static int ipw_update_power_cmd(struct ipw_priv *priv,
				struct ipw_powertable_cmd *cmd, u32 mode);
static int ipw_power_init_handle(struct ipw_priv *priv);

static int ipw_queue_tx_hcmd(struct ipw_priv *priv, struct ipw_host_cmd *cmd);

static int ipw_update_rate_scaling(struct ipw_priv *priv, u8 mode);

static int ipw_rate_scale_tx_resp_handle(struct ipw_priv *priv,
					 struct ipw_tx_resp *tx_resp);

static int ipw_rate_scale_rxon_handle(struct ipw_priv *priv, s32 sta_id);
static u8 ipw_find_station(struct ipw_priv *priv, u8 * bssid);

static int ipw_rx_queue_update_write_ptr(struct ipw_priv *priv,
					 struct ipw_rx_queue *q);
static int ipw_tx_queue_update_write_ptr(struct ipw_priv *priv,
					 struct ipw_tx_queue *txq, int tx_id);

#define SLP PMC_TCMD_FLAG_DRIVER_ALLOW_SLEEP_MSK

#define MSEC_TO_USEC 1024

/* default power management (not Tx power) table values */
static struct ipw_power_vec_entry range_0[IPW_POWER_AC] = {	// for tim  0-10
	{{0, 0 * MSEC_TO_USEC, 0 * MSEC_TO_USEC, {0, 0, 0, 0, 0}}, 0},
	{{SLP, 200 * MSEC_TO_USEC, 500 * MSEC_TO_USEC, {1, 2, 3, 4, 4}}, 0},
	{{SLP, 200 * MSEC_TO_USEC, 300 * MSEC_TO_USEC, {2, 4, 6, 7, 7}}, 0},
	{{SLP, 50 * MSEC_TO_USEC, 100 * MSEC_TO_USEC, {2, 6, 9, 9, 10}}, 0},
	{{SLP, 50 * MSEC_TO_USEC, 25 * MSEC_TO_USEC, {2, 7, 9, 9, 10}}, 1},
	{{SLP, 25 * MSEC_TO_USEC, 25 * MSEC_TO_USEC, {4, 7, 10, 10, 10}}, 1}
};

static struct ipw_power_vec_entry range_1[IPW_POWER_AC] = {	// fot tim > 10
	{{0, 0 * MSEC_TO_USEC, 0 * MSEC_TO_USEC, {0, 0, 0, 0, 0}}, 0},
	{{SLP, 200 * MSEC_TO_USEC, 500 * MSEC_TO_USEC, {1, 2, 3, 4, 0xFF}}, 0},
	{{SLP, 200 * MSEC_TO_USEC, 300 * MSEC_TO_USEC, {2, 4, 6, 7, 0xFF}}, 0},
	{{SLP, 50 * MSEC_TO_USEC, 100 * MSEC_TO_USEC, {2, 6, 9, 9, 0xFF}}, 0},
	{{SLP, 50 * MSEC_TO_USEC, 25 * MSEC_TO_USEC, {2, 7, 9, 9, 0xFF}}, 0},
	{{SLP, 25 * MSEC_TO_USEC, 25 * MSEC_TO_USEC, {4, 7, 10, 10, 0xFF}}, 0}
};

/************************************************/
static void ipw_remove_current_network(struct ipw_priv *priv);
static void ipw_rx_handle(struct ipw_priv *priv);
static int ipw_queue_tx_reclaim(struct ipw_priv *priv, int qindex, int index);
static int ipw_queue_reset(struct ipw_priv *priv);

static void ipw_tx_queue_free(struct ipw_priv *);

static int ipw_stop_tx_queue(struct ipw_priv *priv);

static struct ipw_rx_queue *ipw_rx_queue_alloc(struct ipw_priv *);
static void ipw_rx_queue_free(struct ipw_priv *, struct ipw_rx_queue *);
static void ipw_rx_queue_replenish(void *);

static int ipw_up(struct ipw_priv *);
static void ipw_bg_up(void *);
static void ipw_down(struct ipw_priv *);
static void ipw_bg_down(void *);

static int init_supported_rates(struct ipw_priv *priv,
				struct ipw_supported_rates *prates);
static int ipw_card_show_info(struct ipw_priv *priv);
static int ipw_query_eeprom(struct ipw_priv *priv, u32 offset, u32 len,
			    u8 * buf);

static void ipw_bg_alive_start(void *data);
static int ipw_send_card_state(struct ipw_priv *priv, u32 flags, u8 meta_flag);

static void ipw_link_down(struct ipw_priv *priv);
static u8 ipw_add_station(struct ipw_priv *priv, u8 * bssid, int is_ap,
			  u8 flags);
static u8 ipw_remove_station(struct ipw_priv *priv, u8 * bssid, int is_ap);
static int ipw_card_bss_active_changed_notify(struct ipw_priv *priv,
					      struct ieee80211_network
					      *network);

static int snprint_line(char *buf, size_t count,
			const u8 * data, u32 len, u32 ofs)
{
	int out, i, j, l;
	char c;

	out = snprintf(buf, count, "%08X", ofs);

	for (l = 0, i = 0; i < 2; i++) {
		out += snprintf(buf + out, count - out, " ");
		for (j = 0; j < 8 && l < len; j++, l++)
			out += snprintf(buf + out, count - out, "%02X ",
					data[(i * 8 + j)]);
		for (; j < 8; j++)
			out += snprintf(buf + out, count - out, "   ");
	}
	out += snprintf(buf + out, count - out, " ");
	for (l = 0, i = 0; i < 2; i++) {
		out += snprintf(buf + out, count - out, " ");
		for (j = 0; j < 8 && l < len; j++, l++) {
			c = data[(i * 8 + j)];
			if (!isascii(c) || !isprint(c))
				c = '.';

			out += snprintf(buf + out, count - out, "%c", c);
		}

		for (; j < 8; j++)
			out += snprintf(buf + out, count - out, " ");
	}

	return out;
}

#ifdef CONFIG_IPW3945_DEBUG
static void printk_buf(int level, const u8 * data, u32 len)
{
	char line[81];
	u32 ofs = 0;
	if (!(ipw_debug_level & level))
		return;

	while (len) {
		snprint_line(line, sizeof(line), &data[ofs],
			     min(len, 16U), ofs);
		printk(KERN_DEBUG "%s\n", line);
		ofs += 16;
		len -= min(len, 16U);
	}
}
#else
#define printk_buf(x, y, z) do {} while (0)
#endif

#define IGNORE_RETURN(x) if (x);else;

#define IPW_RATE_SCALE_MAX_WINDOW  32
#define IPW_INVALID_VALUE  -1
/*
 * IO, register, and NIC memory access functions
 *
 * NOTE on naming convention and macro usage for these
 *
 * A single _ prefix before a an access function means that no state
 * check or debug information is printed when that function is called.
 *
 * A double __ prefix before an access function means that state is checked
 * (in the case of *restricted calls) and the current line number is printed
 * in addition to any other debug output.
 *
 * The non-prefixed name is the #define that maps the caller into a
 * #define that provides the caller's __LINE__ to the double prefix version.
 *
 * If you wish to call the function without any debug or state checking,
 * you should use the single _ prefix version (as is used by dependent IO
 * routines, for example _ipw_read_restricted calls the non-check version of
 * _ipw_read32.
 *
 */

#define _ipw_write32(ipw, ofs, val) writel((val), (ipw)->hw_base + (ofs))
#define ipw_write32(ipw, ofs, val) \
 IPW_DEBUG_IO("%s %d: write_direct32(0x%08X, 0x%08X)\n", __FILE__, __LINE__, (u32)(ofs), (u32)(val)); \
 _ipw_write32(ipw, ofs, val)

#define _ipw_read32(ipw, ofs) readl((ipw)->hw_base + (ofs))
static inline u32 __ipw_read32(char *f, u32 l, struct ipw_priv *ipw, u32 ofs)
{
	IPW_DEBUG_IO("%s %d: read_direct32(0x%08X)\n", f, l, (u32) (ofs));
	return _ipw_read32(ipw, ofs);
}

#define ipw_read32(ipw, ofs) __ipw_read32(__FILE__, __LINE__, ipw, ofs)

static inline int ipw_poll_bit(struct ipw_priv *priv, u32 addr, u32 bits,
			       u32 mask, int timeout)
{
	int i = 0;

	do {
		if ((_ipw_read32(priv, addr) & mask) == (bits & mask))
			return i;
		mdelay(10);
		i += 10;
	} while (i < timeout);

	return -ETIMEDOUT;
}

static inline void ipw_set_bit(struct ipw_priv *priv, u32 reg, u32 mask)
{
	_ipw_write32(priv, reg, ipw_read32(priv, reg) | mask);
}

static inline void ipw_clear_bit(struct ipw_priv *priv, u32 reg, u32 mask)
{
	_ipw_write32(priv, reg, ipw_read32(priv, reg) & ~mask);
}

static inline int _ipw_grab_restricted_access(struct ipw_priv *priv)
{
	int rc;
	ipw_set_bit(priv, CSR_GP_CNTRL, CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
	rc = ipw_poll_bit(priv, CSR_GP_CNTRL,
			  CSR_GP_CNTRL_REG_VAL_MAC_ACCESS_EN,
			  (CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY |
			   CSR_GP_CNTRL_REG_FLAG_GOING_TO_SLEEP), 4000);
	if (rc < 0) {
		IPW_ERROR("MAC is in deep sleep!\n");
//              ipw_set_bit(priv, CSR_RESET, CSR_RESET_REG_FLAG_FORCE_NMI);
		return -EIO;
	}

	priv->status |= STATUS_RESTRICTED;

	return 0;
}

static inline int __ipw_grab_restricted_access(u32 line, struct ipw_priv *priv)
{
	if (priv->status & STATUS_RESTRICTED)
		IPW_ERROR("Grabbing access while already held at line %d.\n",
			  line);

	return _ipw_grab_restricted_access(priv);
}

#define ipw_grab_restricted_access(priv) \
__ipw_grab_restricted_access(__LINE__, priv)

static inline void _ipw_release_restricted_access(struct ipw_priv *priv)
{
	ipw_clear_bit(priv, CSR_GP_CNTRL, CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);

	priv->status &= ~STATUS_RESTRICTED;
}

static inline void __ipw_release_restricted_access(u32 line,
						   struct ipw_priv *priv)
{
	if (!(priv->status & STATUS_RESTRICTED))
		IPW_ERROR("Release unheld restricted access at line %d.\n",
			  line);

	_ipw_release_restricted_access(priv);
}

#define ipw_release_restricted_access(priv) \
__ipw_release_restricted_access(__LINE__, priv)

static inline u32 _ipw_read_restricted(struct ipw_priv *priv, u32 reg)
{
	u32 val;
//      _ipw_grab_restricted_access(priv);
	val = _ipw_read32(priv, reg);
//      _ipw_release_restricted_access(priv);
	return val;
}

static u32 __ipw_read_restricted(u32 line, struct ipw_priv *priv, u32 reg)
{
	u32 value;

	if (!(priv->status & STATUS_RESTRICTED))
		IPW_ERROR("Unrestricted access from line %d\n", line);

	value = _ipw_read_restricted(priv, reg);

	IPW_DEBUG_IO(" reg = 0x%4X : value = 0x%4x \n", reg, value);

	return value;
}

#define ipw_read_restricted(a, b) __ipw_read_restricted(__LINE__, a, b)

static void inline _ipw_write_restricted(struct ipw_priv *priv, u32 reg,
					 u32 value)
{
//      _ipw_grab_restricted_access(priv);
	_ipw_write32(priv, reg, value);
//      _ipw_release_restricted_access(priv);
}
static void __ipw_write_restricted(u32 line,
				   struct ipw_priv *priv, u32 reg, u32 value)
{
	if (!(priv->status & STATUS_RESTRICTED))
		IPW_ERROR("Unrestricted access from line %d\n", line);
	_ipw_write_restricted(priv, reg, value);
}

#define ipw_write_restricted(priv, reg, value) \
__ipw_write_restricted(__LINE__, priv, reg, value)

static void ipw_write_buffer_restricted(struct ipw_priv *priv, u32 reg, u32 len,
					u32 * values)
{
	u32 count = sizeof(u32);
	if ((priv != NULL) && (values != NULL)) {
		for (; 0 < len; len -= count, reg += count, values++)
			_ipw_write_restricted(priv, reg, *values);
	}
}

static inline int ipw_poll_restricted_bit(struct ipw_priv *priv, u32 addr,
					  u32 mask, int timeout)
{
	int i = 0;

	do {
		if ((_ipw_read_restricted(priv, addr) & mask) == mask)
			return i;
		mdelay(10);
		i += 10;
	} while (i < timeout);

	return -ETIMEDOUT;
}

static inline u32 _ipw_read_restricted_reg(struct ipw_priv *priv, u32 reg)
{
	_ipw_write_restricted(priv, HBUS_TARG_PRPH_RADDR, reg | (3 << 24));
	return _ipw_read_restricted(priv, HBUS_TARG_PRPH_RDAT);
}
static inline u32 __ipw_read_restricted_reg(u32 line,
					    struct ipw_priv *priv, u32 reg)
{
	if (!(priv->status & STATUS_RESTRICTED))
		IPW_ERROR("Unrestricted access from line %d\n", line);
	return _ipw_read_restricted_reg(priv, reg);
}

#define ipw_read_restricted_reg(priv, reg) \
__ipw_read_restricted_reg(__LINE__, priv, reg)

static inline void _ipw_write_restricted_reg(struct ipw_priv *priv, u32 addr,
					     u32 val)
{
	_ipw_write_restricted(priv, HBUS_TARG_PRPH_WADDR,
			      ((addr & 0x0000FFFF) | (3 << 24)));
	_ipw_write_restricted(priv, HBUS_TARG_PRPH_WDAT, val);
}
static inline void __ipw_write_restricted_reg(u32 line, struct ipw_priv *priv,
					      u32 addr, u32 val)
{
	if (!(priv->status & STATUS_RESTRICTED))
		IPW_ERROR("Unrestricted access from line %d\n", line);
	_ipw_write_restricted_reg(priv, addr, val);
}

#define ipw_write_restricted_reg(priv, addr, val) \
__ipw_write_restricted_reg(__LINE__, priv, addr, val)

#define _ipw_set_bits_restricted_reg(priv, reg, mask) \
	_ipw_write_restricted_reg(priv, reg, \
				  (_ipw_read_restricted_reg(priv, reg) | mask))
static void inline __ipw_set_bits_restricted_reg(u32 line,
						 struct ipw_priv *priv, u32 reg,
						 u32 mask)
{
	if (!(priv->status & STATUS_RESTRICTED))
		IPW_ERROR("Unrestricted access from line %d\n", line);
	_ipw_set_bits_restricted_reg(priv, reg, mask);
}

#define ipw_set_bits_restricted_reg(priv, reg, mask) \
__ipw_set_bits_restricted_reg(__LINE__, priv, reg, mask)

#define _ipw_set_bits_mask_restricted_reg(priv, reg, bits, mask) \
        _ipw_write_restricted_reg( \
            priv, reg, ((_ipw_read_restricted_reg(priv, reg) & mask) | bits))
static void inline __ipw_set_bits_mask_restricted_reg(u32 line,
						      struct ipw_priv *priv,
						      u32 reg, u32 bits,
						      u32 mask)
{
	if (!(priv->status & STATUS_RESTRICTED))
		IPW_ERROR("Unrestricted access from line %d\n", line);
	_ipw_set_bits_mask_restricted_reg(priv, reg, bits, mask);
}

#define ipw_set_bits_mask_restricted_reg(priv, reg, bits, mask) \
__ipw_set_bits_mask_restricted_reg(__LINE__, priv, reg, bits, mask)

static inline void ipw_write_restricted_reg_buffer(struct ipw_priv *priv,
						   u32 reg, u32 len,
						   u8 * values)
{
	u32 reg_offset = reg;
	u32 aligment = reg & 0x3;

	if (len < sizeof(u32)) {
		if ((aligment + len) <= sizeof(u32)) {
			u8 size;
			u32 value = 0;
			size = len - 1;
			memcpy(&value, values, len);
			reg_offset = (reg_offset & 0x0000FFFF);

			_ipw_write_restricted(priv, HBUS_TARG_PRPH_WADDR,
					      (reg_offset | (size << 24)));
			_ipw_write_restricted(priv, HBUS_TARG_PRPH_WDAT, value);
		}

		return;
	}

	for (; reg_offset < (reg + len);
	     reg_offset += sizeof(u32), values += sizeof(u32))
		_ipw_write_restricted_reg(priv, reg_offset, *((u32 *) values));
}

static inline void ipw_clear_bits_restricted_reg(struct ipw_priv *priv, u32 reg,
						 u32 mask)
{
	u32 val = _ipw_read_restricted_reg(priv, reg);
	_ipw_write_restricted_reg(priv, reg, (val & ~mask));
}

static inline void ipw_enable_interrupts(struct ipw_priv *priv)
{
	if (priv->status & STATUS_INT_ENABLED)
		return;
	priv->status |= STATUS_INT_ENABLED;
	ipw_write32(priv, CSR_INT_MASK, CSR_INI_SET_MASK);
}

static inline void ipw_disable_interrupts(struct ipw_priv *priv)
{
	if (!(priv->status & STATUS_INT_ENABLED))
		return;
	priv->status &= ~STATUS_INT_ENABLED;
	ipw_write32(priv, CSR_INT_MASK, 0x00000000);
	ipw_write32(priv, CSR_INT, CSR_INI_SET_MASK);
	ipw_write32(priv, CSR_FH_INT_STATUS, 0xff);
	ipw_write32(priv, CSR_FH_INT_STATUS, 0x00070000);
}

static u32 ipw_read_restricted_mem(struct ipw_priv *priv, u32 addr)
{
	ipw_write_restricted(priv, HBUS_TARG_MEM_RADDR, addr);
	return ipw_read_restricted(priv, HBUS_TARG_MEM_RDAT);
}

/************************* END *******************************/

static const char *desc_lookup(int i)
{
	switch (i) {
	case 1:
		return "FAIL";
	case 2:
		return "BAD_PARAM";
	case 3:
		return "BAD_CHECKSUM";
	case 4:
		return "NMI_INTERRUPT";
	case 5:
		return "SYSASSERT";
	case 6:
		return "FATAL_ERROR";
	}

	return "UNKNOWN";
}

#define ERROR_START_OFFSET  (1 * sizeof(u32))
#define ERROR_ELEM_SIZE     (7 * sizeof(u32))
static void ipw_dump_nic_error_log(struct ipw_priv *priv)
{
	u32 desc, time, blnk, blink2, ilnk, ilink2, idata, i, count, base;
	int rc;

	base = priv->card_alive.error_event_table_ptr;

	if (!VALID_RTC_DATA_ADDR(base)) {
		IPW_ERROR("Not valid error log pointer 0x%08X\n", base);
		return;
	}

	rc = ipw_grab_restricted_access(priv);
	if (rc) {
		IPW_WARNING("Can not read from adapter at this time.\n");
		return;
	}

	count = ipw_read_restricted_mem(priv, base);

	if (ERROR_START_OFFSET <= count * ERROR_ELEM_SIZE) {
		IPW_ERROR("Start IPW Error Log Dump:\n");
		IPW_ERROR("Status: 0x%08X, Config: %08X count: %d\n",
			  priv->status, priv->config, count);
	}

	IPW_ERROR("Desc       Time       asrtPC     const      "
		  "ilink1     nmiPC      Line\n");
	for (i = ERROR_START_OFFSET;
	     i < (count * ERROR_ELEM_SIZE) + ERROR_START_OFFSET;
	     i += ERROR_ELEM_SIZE) {
		desc = ipw_read_restricted_mem(priv, base + i);
		time =
		    ipw_read_restricted_mem(priv, base + i + 1 * sizeof(u32));
		blnk =
		    ipw_read_restricted_mem(priv, base + i + 2 * sizeof(u32));
		blink2 =
		    ipw_read_restricted_mem(priv, base + i + 3 * sizeof(u32));
		ilnk =
		    ipw_read_restricted_mem(priv, base + i + 4 * sizeof(u32));
		ilink2 =
		    ipw_read_restricted_mem(priv, base + i + 5 * sizeof(u32));
		idata =
		    ipw_read_restricted_mem(priv, base + i + 6 * sizeof(u32));

		IPW_ERROR
		    ("%-8s (#%d) 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X %u\n",
		     desc_lookup(desc), desc, time, blnk, blink2, ilnk, ilink2,
		     idata);
	}

	ipw_release_restricted_access(priv);

}

#define EVENT_START_OFFSET  (4 * sizeof(u32))
static void ipw_dump_nic_event_log(struct ipw_priv *priv)
{
	int rc;
	u32 ev, time, data = 0, i, count = 0, base;
	u32 event_size, type;

	base = priv->card_alive.log_event_table_ptr;
	if (!VALID_RTC_DATA_ADDR(base)) {
		IPW_ERROR("Not valid log event pointer 0x%08X\n", base);
		return;
	}

	rc = ipw_grab_restricted_access(priv);
	if (rc) {
		IPW_WARNING("Can not read from adapter at this time.\n");
		return;
	}

	count = ipw_read_restricted_mem(priv, base);
	type = ipw_read_restricted_mem(priv, base + sizeof(u32));

	if (type == 0)
		event_size = 2 * sizeof(u32);
	else
		event_size = 3 * sizeof(u32);

	if (count > 0)
		IPW_ERROR("Start IPW Event Log Dump: count %d type 0x%02X\n",
			  count, type);

	for (i = EVENT_START_OFFSET;
	     i < ((count * event_size) + EVENT_START_OFFSET); i += event_size) {
		ev = ipw_read_restricted_mem(priv, base + i);
		time =
		    ipw_read_restricted_mem(priv, base + i + 1 * sizeof(u32));
		if ((type == 0) && (ev || time)) {
			IPW_ERROR("0x%08x\t0x%08x\n", ev, time);
		} else if (type != 0) {
			data =
			    ipw_read_restricted_mem(priv,
						    base + i + 2 * sizeof(u32));
			if (ev || time || data)
				IPW_ERROR("0x%08x\t0x%08x\t0x%08x\n", ev, time,
					  data);
		}
	}

	ipw_release_restricted_access(priv);
}

static inline int ipw_is_ready(struct ipw_priv *priv)
{
	/* The adapter is 'ready' if READY and GEO_CONFIGURED bits are
	 * set but EXIT_PENDING is not */
	return ((priv->status & (STATUS_READY |
				 STATUS_GEO_CONFIGURED |
				 STATUS_EXIT_PENDING)) ==
		(STATUS_READY | STATUS_GEO_CONFIGURED)) ? 1 : 0;
}

static inline int ipw_is_alive(struct ipw_priv *priv)
{
	return (priv->status & STATUS_ALIVE) ? 1 : 0;
}

static inline int ipw_is_init(struct ipw_priv *priv)
{
	return (priv->status & STATUS_INIT) ? 1 : 0;
}

static inline int ipw_is_calibrated(struct ipw_priv *priv)
{
	return (priv->status & STATUS_CALIBRATE) ? 1 : 0;
}

#define IPW_CMD(x) case x : return #x
#define IPW_CMD3945(x) case REPLY_ ## x : return #x

static const char *get_cmd_string(u8 cmd)
{
	switch (cmd) {
		IPW_CMD(SCAN_START_NOTIFICATION);
		IPW_CMD(SCAN_RESULTS_NOTIFICATION);
		IPW_CMD(SCAN_COMPLETE_NOTIFICATION);
		IPW_CMD(STATISTICS_NOTIFICATION);
		IPW_CMD3945(ALIVE);
		IPW_CMD3945(ERROR);
		IPW_CMD3945(RX_ON_ASSOC);
		IPW_CMD3945(QOS_PARAM);
		IPW_CMD3945(RX_ON_TIMING);
		IPW_CMD3945(ADD_STA);
		IPW_CMD3945(RX);
		IPW_CMD3945(TX);
		IPW_CMD3945(BCON);
		IPW_CMD3945(RATE_SCALE);
		IPW_CMD3945(LEDS_CMD);
		IPW_CMD3945(SCAN_ABORT_CMD);
		IPW_CMD3945(TX_BEACON);
		IPW_CMD3945(BT_CONFIG);
		IPW_CMD3945(STATISTICS_CMD);
		IPW_CMD3945(CARD_STATE_CMD);
	case POWER_TABLE_CMD:
		return "POWER_TABLE_CMD";
	default:
		return "DAEMON";

	}
}

#define HOST_COMPLETE_TIMEOUT (HZ / 2)
#define DAEMON_COMPLETE_TIMEOUT (HZ / 2)

static inline int is_cmd_sync(struct ipw_host_cmd *cmd)
{
	return !(cmd->meta.flags & CMD_ASYNC);
}

static inline int is_cmd_small(struct ipw_host_cmd *cmd)
{
	return !(cmd->meta.flags & CMD_SIZE_HUGE);
}

static inline int cmd_needs_lock(struct ipw_host_cmd *cmd)
{
	return !(cmd->meta.flags & CMD_NO_LOCK);
}

static inline int is_daemon_cmd(struct ipw_host_cmd *cmd)
{
	return (cmd->meta.flags & CMD_DAEMON);
}

static int ipw_send_daemon_cmd(struct ipw_priv *priv,
			       struct ipw_daemon_cmd_list *element)
{
	unsigned long flags;
	int rc = 0, want_result = 0;

	if (priv->status & STATUS_EXIT_PENDING) {
		IPW_DEBUG_INFO("Not attempting to send daemon command while "
			       "exit pending.\n");
		return -EIO;
	}

	spin_lock_irqsave(&priv->daemon_lock, flags);
	if ((element->cmd.flags & DAEMON_FLAG_WANT_RESULT) &&
	    (priv->status & STATUS_DCMD_ACTIVE)) {
		spin_unlock_irqrestore(&priv->daemon_lock, flags);
		IPW_ERROR("Error sending %s: "
			  "Already sending a synchronous daemon command\n",
			  get_cmd_string(element->cmd.cmd));
		kfree(element);
		return -EBUSY;
	}

	if (element->cmd.flags & DAEMON_FLAG_WANT_RESULT) {
		want_result = 1;
		priv->status |= STATUS_DCMD_ACTIVE;
	}

	element->cmd.token = 0;
	element->skb_resp = NULL;

	IPW_DEBUG_DAEMON("Queueing cmd %s (#%02X)\n",
			 get_cmd_string(element->cmd.cmd), element->cmd.cmd);

	list_add_tail(&element->list, &priv->daemon_out_list);

	spin_unlock_irqrestore(&priv->daemon_lock, flags);

	wake_up_interruptible_all(&priv->wait_daemon_out_queue);

	if (!want_result)
		return 0;

	rc = wait_event_interruptible_timeout(priv->
					      wait_daemon_cmd_done,
					      !(priv->status &
						STATUS_DCMD_ACTIVE),
					      DAEMON_COMPLETE_TIMEOUT);
	spin_lock_irqsave(&priv->daemon_lock, flags);
	if (rc == 0) {
		if (priv->status & STATUS_DCMD_ACTIVE) {
			IPW_ERROR("Error sending #%02X to daemon: "
				  "time out after %dms.\n",
				  element->cmd.cmd,
				  jiffies_to_msecs(DAEMON_COMPLETE_TIMEOUT));
			priv->status &= ~STATUS_DCMD_ACTIVE;
			rc = -ETIMEDOUT;
		} else
			rc = priv->daemon_cmd_rc;
	} else {
		if (priv->status & STATUS_DCMD_ACTIVE) {
			IPW_WARNING("Daemon command aborted.\n");
			priv->status &= ~STATUS_DCMD_ACTIVE;
			rc = -ECONNABORTED;
		} else {
			IPW_DEBUG_DAEMON("driver requested daemon command "
					 "completed in %dms.\n",
					 jiffies_to_msecs
					 (DAEMON_COMPLETE_TIMEOUT - rc));
			rc = priv->daemon_cmd_rc;
		}
	}
	spin_unlock_irqrestore(&priv->daemon_lock, flags);

	return rc;
}

#ifdef CONFIG_IPW3945_DEBUG
#define DAEMON_STR(x) case DAEMON_SYNC_ ## x : return #x
static const char *get_daemon_sync_string(u16 state)
{
	switch (state) {
		DAEMON_STR(INIT);
		DAEMON_STR(UNINIT);
		DAEMON_STR(SCAN_COMPLETE);
		DAEMON_STR(TXPOWER_LIMIT);
		DAEMON_STR(MEASURE_REPORT);
		DAEMON_STR(TX_STATUS);
		DAEMON_STR(SUSPEND);
		DAEMON_STR(RESUME);

	default:
		return "UNKNOWN";

	}
}
#endif

static int ipw_send_daemon_sync(struct ipw_priv *priv, u16 state,
				u16 len, void *data)
{
	struct ipw_daemon_cmd_list *daemon_cmd;
	struct daemon_sync_cmd *sync;
	unsigned long flags;

	daemon_cmd = kmalloc(sizeof(*daemon_cmd) +
			     sizeof(struct daemon_sync_cmd) + len, GFP_ATOMIC);
	if (!daemon_cmd)
		return -ENOMEM;

	daemon_cmd->cmd.cmd = DAEMON_SYNC;
	daemon_cmd->cmd.data_len = sizeof(struct daemon_sync_cmd) + len;
	daemon_cmd->cmd.flags = 0;
	daemon_cmd->cmd.token = 0;
	daemon_cmd->skb_resp = NULL;
	sync = (struct daemon_sync_cmd *)daemon_cmd->cmd.data;

	sync->len = len;
	sync->state = state;
	if (len)
		memcpy(sync->data, data, len);

	IPW_DEBUG_DAEMON("queueing driver->daemon DAEMON_SYNC::%s frame.\n",
			 get_daemon_sync_string(state));

	spin_lock_irqsave(&priv->daemon_lock, flags);
	list_add_tail(&daemon_cmd->list, &priv->daemon_out_list);
	spin_unlock_irqrestore(&priv->daemon_lock, flags);

	wake_up_interruptible_all(&priv->wait_daemon_out_queue);

	return 0;
}

static int ipw_send_daemon_frame(struct ipw_priv *priv,
				 u8 channel, u16 rssi, u64 tsf, u32 beacon_time,
				 u8 * payload, int len)
{
	struct ipw_daemon_cmd_list *daemon_cmd;
	struct daemon_80211_frame *frame;
	unsigned long flags;

	daemon_cmd = kmalloc(sizeof(*daemon_cmd) +
			     sizeof(struct daemon_80211_frame) +
			     len, GFP_ATOMIC);
	if (!daemon_cmd)
		return -ENOMEM;

	daemon_cmd->cmd.cmd = DAEMON_FRAME_RX;
	daemon_cmd->cmd.data_len = sizeof(struct daemon_80211_frame) + len;
	daemon_cmd->cmd.flags = 0;
	daemon_cmd->cmd.token = 0;
	daemon_cmd->skb_resp = NULL;
	frame = (struct daemon_80211_frame *)daemon_cmd->cmd.data;
	frame->channel = channel;
	frame->rssi = rssi;
	frame->tsf = tsf;
	frame->beacon_time = beacon_time;
	frame->frame_len = len;
	memcpy(frame->frame, payload, len);

	IPW_DEBUG_DAEMON("Queueing DAEMON_FRAME_RX.\n");

	spin_lock_irqsave(&priv->daemon_lock, flags);
	list_add_tail(&daemon_cmd->list, &priv->daemon_out_list);
	spin_unlock_irqrestore(&priv->daemon_lock, flags);

	wake_up_interruptible_all(&priv->wait_daemon_out_queue);

	return 0;
}

/*
  low level function to send 3945 command.

  The caller can control if the command is async/sync.
  sync    : 1 do sync call and sleep until notified the command is complese
  resp    : pointer to command response.
  do_lock : 1 use spin lock, 0 assume the user has lock.
            we can not have do_lock=0 and sync = 1.
  is_huge_cmd : used for scan since this command is very large, we use
                special buffer to copy the command.
*/
static int ipw_send_cmd(struct ipw_priv *priv, struct ipw_host_cmd *cmd)
    __attribute__ ((warn_unused_result));
static int ipw_send_cmd(struct ipw_priv *priv, struct ipw_host_cmd *cmd)
{
	int rc;
	unsigned long flags = 0;

	/* If this is an asynchronous command, and we are in a shutdown
	 * process then don't let it start */
	if (!is_cmd_sync(cmd) && (priv->status & STATUS_EXIT_PENDING))
		return -EBUSY;

	/*
	 * The following BUG_ONs are meant to catch programming API misuse
	 * and not run-time failures due to timing, resource constraint, etc.
	 */

	/* A command can not be asynchronous AND expect an SKB to be set */
	BUG_ON((cmd->meta.flags & CMD_ASYNC)
	       && (cmd->meta.flags & CMD_WANT_SKB));

	/* The skb/callback union must be NULL if an SKB is requested */
	BUG_ON(cmd->meta.u.skb && (cmd->meta.flags & CMD_WANT_SKB));

	/* A command can not be synchronous AND have a callback set */
	BUG_ON(is_cmd_sync(cmd) && cmd->meta.u.callback);

	/* An asynchronous command MUST have a callback */
	BUG_ON((cmd->meta.flags & CMD_ASYNC) && !cmd->meta.u.callback);

	/* A command can not be synchronous AND not use locks */
	BUG_ON(is_cmd_sync(cmd) && (cmd->meta.flags & CMD_NO_LOCK));

	if (cmd_needs_lock(cmd))
		spin_lock_irqsave(&priv->lock, flags);

	if (is_cmd_sync(cmd) && (priv->status & STATUS_HCMD_ACTIVE)) {
		IPW_ERROR("Error sending %s: "
			  "Already sending a host command\n",
			  get_cmd_string(cmd->id));
		if (cmd_needs_lock(cmd))
			spin_unlock_irqrestore(&priv->lock, flags);
		return -EBUSY;
	}

	if (is_cmd_sync(cmd))
		priv->status |= STATUS_HCMD_ACTIVE;

	/* When the SKB is provided in the tasklet, it needs
	 * a backpointer to the originating caller so it can
	 * actually copy the skb there */
	if (cmd->meta.flags & CMD_WANT_SKB)
		cmd->meta.u.source = &cmd->meta;

	cmd->meta.len = cmd->len;

	rc = ipw_queue_tx_hcmd(priv, cmd);
	if (rc) {
		if (is_cmd_sync(cmd))
			priv->status &= ~STATUS_HCMD_ACTIVE;
		if (cmd_needs_lock(cmd))
			spin_unlock_irqrestore(&priv->lock, flags);

		IPW_ERROR("Error sending %s: "
			  "ipw_queue_tx_hcmd failed: %d\n",
			  get_cmd_string(cmd->id), rc);

		return -ENOSPC;
	}
	if (cmd_needs_lock(cmd))
		spin_unlock_irqrestore(&priv->lock, flags);

	if (is_cmd_sync(cmd)) {
		rc = wait_event_interruptible_timeout(priv->wait_command_queue,
						      !(priv->
							status &
							STATUS_HCMD_ACTIVE),
						      HOST_COMPLETE_TIMEOUT);
		if (rc == 0) {
			if (cmd_needs_lock(cmd))
				spin_lock_irqsave(&priv->lock, flags);

			if (priv->status & STATUS_HCMD_ACTIVE) {
				IPW_ERROR("Error sending %s: "
					  "time out after %dms.\n",
					  get_cmd_string(cmd->id),
					  jiffies_to_msecs
					  (HOST_COMPLETE_TIMEOUT));
				priv->status &= ~STATUS_HCMD_ACTIVE;
				if ((cmd->meta.flags & CMD_WANT_SKB) &&
				    cmd->meta.u.skb) {
					dev_kfree_skb_any(cmd->meta.u.skb);
					cmd->meta.u.skb = NULL;
				}

				if (cmd_needs_lock(cmd))
					spin_unlock_irqrestore(&priv->lock,
							       flags);
				return -ETIMEDOUT;
			}

			if (cmd_needs_lock(cmd))
				spin_unlock_irqrestore(&priv->lock, flags);
		}
	}

	if (priv->status & STATUS_RF_KILL_HW) {
		if ((cmd->meta.flags & CMD_WANT_SKB) && cmd->meta.u.skb) {
			dev_kfree_skb_any(cmd->meta.u.skb);
			cmd->meta.u.skb = NULL;
		}

		IPW_DEBUG_INFO("Command %s aborted: RF KILL Switch\n",
			       get_cmd_string(cmd->id));

		return -ECANCELED;
	}

	if (priv->status & STATUS_FW_ERROR) {
		if ((cmd->meta.flags & CMD_WANT_SKB) && cmd->meta.u.skb) {
			dev_kfree_skb_any(cmd->meta.u.skb);
			cmd->meta.u.skb = NULL;
		}

		IPW_DEBUG_INFO("Command %s failed: FW Error\n",
			       get_cmd_string(cmd->id));

		return -EIO;
	}

	if ((cmd->meta.flags & CMD_WANT_SKB) && !cmd->meta.u.skb) {
		IPW_ERROR("Error: Response NULL in '%s'\n",
			  get_cmd_string(cmd->id));
		return -EIO;
	}

	return 0;
}

static int ipw_send_cmd_pdu(struct ipw_priv *priv, u8 id, u16 len, void *data)
    __attribute__ ((warn_unused_result));
static int ipw_send_cmd_pdu(struct ipw_priv *priv, u8 id, u16 len, void *data)
{
	struct ipw_host_cmd cmd = {
		.id = id,
		.len = len,
		.data = data,
	};

	return ipw_send_cmd(priv, &cmd);
}

static int ipw_send_cmd_u32(struct ipw_priv *priv, u8 id, u32 val)
    __attribute__ ((warn_unused_result));
static int ipw_send_cmd_u32(struct ipw_priv *priv, u8 id, u32 val)
{
	struct ipw_host_cmd cmd = {
		.id = id,
		.len = sizeof(val),
		.data = &val,
	};

	return ipw_send_cmd(priv, &cmd);
}

static struct ipw_link_blink link_led_table[] = {
	[IPW_LED_LINK_RADIOOFF] = {1000, 0, 0},
	[IPW_LED_LINK_UNASSOCIATED] = {10000, 200, 2},
	[IPW_LED_LINK_SCANNING] = {1000, 60, 10},
	[IPW_LED_LINK_ASSOCIATED] = {1000, 0, 1},
	[IPW_LED_LINK_ROAMING] = {1000, 10, 100},
};

static void ipw_update_link_led(struct ipw_priv *priv)
{
	int state;
	struct ipw_led_cmd led_cmd = {
		.id = IPW_LED_LINK,
	};

	/* If we are in RF KILL then we can't send the LED
	 * command, so cache that the LED is in the
	 * RADIOOFF state so we'll turn it back on when
	 * we come back from RF KILL. */
	if (priv->status & STATUS_RF_KILL_MASK) {
		IPW_DEBUG_LED("Not sending LINK LED off cmd due to RF KILL.\n");
		priv->led_state = IPW_LED_LINK_RADIOOFF;
		return;
	}

	if (priv->status & STATUS_IN_SUSPEND) {
		IPW_DEBUG_LED("Not sending LINK LED off cmd due to SUSPEND.\n");
		priv->led_state = IPW_LED_LINK_RADIOOFF;
		return;
	}

	if ((priv->config & CFG_NO_LED) ||
	    (priv->status & STATUS_EXIT_PENDING) ||
	    !(priv->status & STATUS_READY))
		state = IPW_LED_LINK_RADIOOFF;
	else if (priv->status & STATUS_ROAMING)
		state = IPW_LED_LINK_ROAMING;
	else if (priv->status & STATUS_SCANNING)
		state = IPW_LED_LINK_SCANNING;
	else if (priv->status & STATUS_ASSOCIATED)
		state = IPW_LED_LINK_ASSOCIATED;
	else
		state = IPW_LED_LINK_UNASSOCIATED;

	if (state == priv->led_state)
		return;

	led_cmd.interval = link_led_table[state].interval;
	led_cmd.on = link_led_table[state].on;
	led_cmd.off = link_led_table[state].off;

	priv->led_state = state;

#ifdef CONFIG_IPW3945_DEBUG
	switch (state) {
	case IPW_LED_LINK_RADIOOFF:
		IPW_DEBUG_LED("Link state: RADIO OFF\n");
		break;
	case IPW_LED_LINK_ROAMING:
		IPW_DEBUG_LED("Link state: ROAMING\n");
		break;
	case IPW_LED_LINK_ASSOCIATED:
		IPW_DEBUG_LED("Link state: ASSOCIATED\n");
		break;
	case IPW_LED_LINK_UNASSOCIATED:
		IPW_DEBUG_LED("Link state: UNASSOCIATED\n");
		break;
	case IPW_LED_LINK_SCANNING:
		IPW_DEBUG_LED("Link state: SCANNING\n");
		break;
	default:
		IPW_DEBUG_LED("Link state: UNKNOWN\n");
		break;
	}
#endif

	IPW_DEBUG_LED("On: %d, Off: %d, Interval: %d\n",
		      led_cmd.on, led_cmd.off, led_cmd.interval);

	IGNORE_RETURN(ipw_send_cmd_pdu(priv, REPLY_LEDS_CMD,
				       sizeof(struct ipw_led_cmd), &led_cmd));
}

static struct ipw_activity_blink activity_led_table[] = {
	{300, 25, 25},
	{200, 40, 40},
	{100, 55, 55},
	{70, 65, 65},
	{50, 75, 75},
	{20, 85, 85},
	{10, 95, 95},
	{5, 110, 110},
	{1, 130, 130},
	{0, 167, 167},
};

/*
  set to correct blink rate. set to solid blink we can not find correct
  rate value or the blink valus exceed the blink threshold
*/
static void get_led_blink_rate(struct ipw_priv *priv,
			       struct ipw_activity_blink *blink)
{
	/* Adjust to Mbs throughput table */
	u32 bit_count = (priv->led_packets * 10) >> 17;
	u32 index = 0;

	/* If < 1mbs then just quick blink over long duration to
	 * indicate "some" activity */
	if (!bit_count) {
		blink->on = 10;
		blink->off = 200;
		return;
	}

	while ((bit_count <= activity_led_table[index].throughput) &&
	       index < ARRAY_SIZE(activity_led_table))
		index++;

	if (index == ARRAY_SIZE(activity_led_table)) {
		blink->on = 1;	/* turn on */
		blink->off = 0;	/* never turn off */
		return;
	}

	blink->on = activity_led_table[index].on;
	blink->off = activity_led_table[index].off;
}

#define IPW_ACTIVITY_PERIOD msecs_to_jiffies(100)

static void ipw_update_activity_led(struct ipw_priv *priv)
{
	static struct ipw_activity_blink last_blink = { 0, 0, 0 };
	struct ipw_activity_blink blink;
	struct ipw_led_cmd led_cmd = {
		.id = IPW_LED_ACTIVITY,
		.interval = IPW_LED_INTERVAL,
	};

	/* If configured to not use LEDs or LEDs are disabled,
	 * then we don't toggle a activity led */
	if (priv->config & CFG_NO_LED || (priv->status & STATUS_EXIT_PENDING)) {
		blink.on = blink.off = 0;
	} else {
		IPW_DEBUG_LED("total Tx/Rx bytes = %lu\n", priv->led_packets);
		get_led_blink_rate(priv, &blink);
		priv->led_packets = 0;
	}

	if (last_blink.on != blink.on || last_blink.off != blink.off) {
		last_blink = blink;
		IPW_DEBUG_LED
		    ("Blink rate is %d On, %d ms Off, at %d interval.\n",
		     blink.on, blink.off, led_cmd.interval);

		led_cmd.off = blink.off;
		led_cmd.on = blink.on;

		IGNORE_RETURN(ipw_send_cmd_pdu(priv, REPLY_LEDS_CMD,
					       sizeof(struct ipw_led_cmd),
					       &led_cmd));
	}
}

static void ipw_setup_activity_timer(struct ipw_priv *priv)
{
	if (priv->activity_timer_active)
		return;

	priv->activity_timer_active = 1;
	queue_delayed_work(priv->workqueue, &priv->activity_timer,
			   IPW_ACTIVITY_PERIOD);
}

static void ipw_bg_activity_timer(void *data)
{
	struct ipw_priv *priv = data;

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);

	ipw_update_activity_led(priv);

	/* If we haven't Tx/Rx any packets, then don't bother
	 * running this timer any more until we do one of those things */
	if (!priv->led_packets)
		priv->activity_timer_active = 0;
	else
		queue_delayed_work(priv->workqueue, &priv->activity_timer,
				   IPW_ACTIVITY_PERIOD);

	mutex_unlock(&priv->mutex);
}

static void ipw_update_tech_led(struct ipw_priv *priv)
{
}

#define IPW_EEPROM_LED_FROM_EEPROM     	0x80
#define IPW_EEPROM_LED_MODE             0x03
#define IPW_EEPROM_LED_SAVE		0x04

static int ipw_block_until_driver_ready(struct ipw_priv *priv)
{
	if (!ipw_is_alive(priv))
		return -EAGAIN;

	return 0;
}

/*
 * The following adds a new attribute to the sysfs representation
 * of this device driver (i.e. a new file in /sys/bus/pci/drivers/ipw/)
 * used for controling the debug level.
 *
 * See the level definitions in ipw for details.
 */
static ssize_t show_debug_level(struct device_driver *d, char *buf)
{
	return sprintf(buf, "0x%08X\n", ipw_debug_level);
}
static ssize_t store_debug_level(struct device_driver *d, const char *buf,
				 size_t count)
{
	char *p = (char *)buf;
	u32 val;

	if (p[1] == 'x' || p[1] == 'X' || p[0] == 'x' || p[0] == 'X') {
		p++;
		if (p[0] == 'x' || p[0] == 'X')
			p++;
		val = simple_strtoul(p, &p, 16);
	} else
		val = simple_strtoul(p, &p, 10);
	if (p == buf)
		printk(KERN_INFO DRV_NAME
		       ": %s is not in hex or decimal form.\n", buf);
	else
		ipw_debug_level = val;

	return strnlen(buf, count);
}

static DRIVER_ATTR(debug_level, S_IWUSR | S_IRUGO,
		   show_debug_level, store_debug_level);

static int ipw_send_statistics_request(struct ipw_priv *priv)
{
	return ipw_send_cmd_u32(priv, REPLY_STATISTICS_CMD, 0);
}

static void ipw_clear_free_frames(struct ipw_priv *priv)
{
	struct list_head *element;

	IPW_DEBUG_INFO("%d frames on pre-allocated heap on clear.\n",
		       priv->frames_count);

	while (!list_empty(&priv->free_frames)) {
		element = priv->free_frames.next;
		list_del(element);
		kfree(list_entry(element, struct ipw_frame, list));
		priv->frames_count--;
	}

	if (priv->frames_count) {
		IPW_WARNING("%d frames still in use.  Did we lose one?\n",
			    priv->frames_count);
		priv->frames_count = 0;
	}
}

#define FREE_FRAME_THRESHOLD 5
static struct ipw_frame *ipw_get_free_frame(struct ipw_priv *priv)
{
	struct ipw_frame *frame;
	struct list_head *element;
	if (list_empty(&priv->free_frames)) {
		frame = kzalloc(sizeof(*frame), GFP_ATOMIC);
		if (!frame) {
			IPW_ERROR("Could not allocate frame!\n");
			return 0;
		}

		priv->frames_count++;

		if (priv->frames_count >= FREE_FRAME_THRESHOLD) {
			IPW_WARNING("%d frames allocated.  "
				    "Are we losing them?\n",
				    priv->frames_count);
		}

		return frame;
	}

	element = priv->free_frames.next;
	list_del(element);
	return list_entry(element, struct ipw_frame, list);
}

static void ipw_free_frame(struct ipw_priv *priv, struct ipw_frame *frame)
{
	memset(frame, 0, sizeof(*frame));
	list_add(&frame->list, &priv->free_frames);
}

static void ipw_clear_daemon_lists(struct ipw_priv *priv)
{
	struct list_head *element;
	struct ipw_daemon_cmd_list *daemon_cmd;

	/* NOTE: The daemon_lock must be held before calling this
	 * function */

	/* Clear out any pending sysfs commands */
	while (!list_empty(&priv->daemon_in_list)) {
		element = priv->daemon_in_list.next;
		daemon_cmd = list_entry(element,
					struct ipw_daemon_cmd_list, list);
		list_del(element);
		kfree(daemon_cmd);
	}

	/* Clear out any pending sysfs responses */
	while (!list_empty(&priv->daemon_out_list)) {
		element = priv->daemon_out_list.next;
		daemon_cmd = list_entry(element,
					struct ipw_daemon_cmd_list, list);
		list_del(element);
		if (daemon_cmd->skb_resp)
			dev_kfree_skb_any(daemon_cmd->skb_resp);
		kfree(daemon_cmd);
	}

	/* Clear out any storage buffers in the free list */
	while (!list_empty(&priv->daemon_free_list)) {
		element = priv->daemon_free_list.next;
		daemon_cmd = list_entry(element,
					struct ipw_daemon_cmd_list, list);
		list_del(element);
		kfree(daemon_cmd);
	}
}

static ssize_t show_temperature(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
				struct device_attribute *attr,
#endif
				char *buf)
{
	struct ipw_priv *priv = (struct ipw_priv *)d->driver_data;
	int rc;

	rc = ipw_block_until_driver_ready(priv);
	if (rc)
		return rc;

	return sprintf(buf, "%d\n", ipw_read32(priv, CSR_UCODE_DRV_GP2));
}

static DEVICE_ATTR(temperature, S_IRUGO, show_temperature, NULL);

static void ipw_bg_daemon_cmd(void *data);
static ssize_t store_cmd(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			 struct device_attribute *attr,
#endif
			 const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	struct daemon_cmd_hdr *in_cmd = (struct daemon_cmd_hdr *)buf;
	struct ipw_daemon_cmd_list *daemon_cmd;
	unsigned long flags;

	if (priv->status & STATUS_EXIT_PENDING)
		return -EAGAIN;

	if (priv->status & STATUS_IN_SUSPEND)
		return -ERESTARTSYS;

	if (count < sizeof(*in_cmd)) {
		IPW_WARNING
		    ("Bad command packet header received (%zd < %zd).\n",
		     count, sizeof(*in_cmd));
		return 0;
	}

	if (in_cmd->version != IPW_DAEMON_VERSION) {
		IPW_ERROR("driver / daemon (ipw3945d) protocol version "
			  "mismatch (v%d vs. v%d).\n",
			  IPW_DAEMON_VERSION, in_cmd->version);
		IPW_ERROR("You need to upgrade the %s.\n",
			  in_cmd->version > IPW_DAEMON_VERSION ?
			  "driver" : "daemon");
		return -EIO;
	}

	if (count < sizeof(*in_cmd) + in_cmd->data_len) {
		IPW_WARNING
		    ("Bad command packet payload received (%zd < %zd).\n",
		     count, sizeof(*in_cmd) + in_cmd->data_len);
		return 0;
	}

	IPW_DEBUG_DAEMON("driver <- daemon #%02X (%d bytes).\n",
			 in_cmd->cmd, in_cmd->data_len);

	/* Rather than allocate just enough storage for the incoming command,
	 * we allocate enough for the command to be used for any return
	 * as well. */
	daemon_cmd = kmalloc(sizeof(*daemon_cmd) + DAEMON_BUF_SIZE, GFP_KERNEL);
	if (!daemon_cmd)
		return count;

	memcpy(&daemon_cmd->cmd, buf, sizeof(*in_cmd) + in_cmd->data_len);
	daemon_cmd->skb_resp = NULL;

	spin_lock_irqsave(&priv->daemon_lock, flags);
	list_add_tail(&daemon_cmd->list, &priv->daemon_in_list);
	spin_unlock_irqrestore(&priv->daemon_lock, flags);

	queue_work(priv->daemonqueue, &priv->daemon_cmd_work);

	return sizeof(*in_cmd) + in_cmd->data_len;
}

static ssize_t show_cmd(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			struct device_attribute *attr,
#endif
			char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	struct list_head *element;
	struct ipw_daemon_cmd_list *daemon_cmd;
	struct ipw_rx_packet *res;
	int count;
	int rc;
	unsigned long flags;

	if (priv->status & STATUS_EXIT_PENDING)
		return -EAGAIN;

	while (1) {
		/* The regulatory daemon has to perform periodic calculations
		 * based on various conditions (most notably temperature).
		 *
		 * However, as sysfs does not have a polling capability
		 * and the daemon is single threaded, it blocks on reads
		 * from this sysfs entry.  We therefore have to
		 *
		 * The driver, therefore, will return ETIMEDOUT to the daemon
		 * if no commands are received to be sent to the daemon within
		 * DAEMON_POLL_INTERVAL ms.
		 */
		rc = wait_event_interruptible_timeout(priv->
						      wait_daemon_out_queue,
						      !list_empty(&priv->
								  daemon_out_list),
						      msecs_to_jiffies
						      (DAEMON_POLL_INTERVAL));

		if (rc < 0) {
			if (rc == -ERESTARTSYS)
				IPW_DEBUG_INFO("Returning ESYSRESTART to "
					       "daemon.\n");
			else
				IPW_DEBUG_DAEMON("wait returned failure: "
						 "%d\n", rc);
			return rc;
		}

		if (rc == 0) {
			struct daemon_cmd_hdr cmd = {
				.cmd = DAEMON_READ_TIMEOUT,
				.version = IPW_DAEMON_VERSION,
				.data_len = 0,
			};

			IPW_DEBUG_DAEMON("DAEMON_POLL_INTERVAL expired.\n");

			memcpy(buf, &cmd, sizeof(cmd));
			return sizeof(cmd);
		}

		spin_lock_irqsave(&priv->daemon_lock, flags);
		if (!list_empty(&priv->daemon_out_list))
			break;
		spin_unlock_irqrestore(&priv->daemon_lock, flags);

		IPW_DEBUG_INFO("Daemon queue drained on last wait.\n");

		continue;
	}

	/* daemon_lock still held... while() exited via break */
	element = priv->daemon_out_list.next;
	daemon_cmd = list_entry(element, struct ipw_daemon_cmd_list, list);
	list_del(element);
	spin_unlock_irqrestore(&priv->daemon_lock, flags);

	if (priv->status & STATUS_DCMD_ACTIVE)
		IPW_DEBUG_DAEMON("Synchronous daemon command pending.\n");

	count = 0;

	/* If token is not 0 then this is a response to a daemon request,
	 * else its a new request to daemon */
	if (daemon_cmd->cmd.token != 0) {
		if (daemon_cmd->skb_resp) {
			res = (struct ipw_rx_packet *)
			    daemon_cmd->skb_resp->data;

			daemon_cmd->cmd.data_len =
			    offsetof(struct ipw_rx_packet, hdr) + res->len;
			memcpy(&buf[sizeof(daemon_cmd->cmd)], res,
			       daemon_cmd->cmd.data_len);
			count += daemon_cmd->cmd.data_len;

			IPW_DEBUG_DAEMON("driver -> daemon skb ACK w/ response "
					 "to #%02X (%d bytes).\n",
					 daemon_cmd->cmd.cmd, res->len);

			dev_kfree_skb_any(daemon_cmd->skb_resp);
		} else if (daemon_cmd->cmd.data_len) {
			memcpy(&buf[sizeof(daemon_cmd->cmd)],
			       daemon_cmd->cmd.data, daemon_cmd->cmd.data_len);
			count += daemon_cmd->cmd.data_len;
			IPW_DEBUG_DAEMON("driver -> daemon ACK w/ response "
					 "to #%02X (%d bytes).\n",
					 daemon_cmd->cmd.cmd,
					 daemon_cmd->cmd.data_len);
		} else {
			IPW_DEBUG_DAEMON("driver -> daemon ACK on #%02X.\n",
					 daemon_cmd->cmd.cmd);
			/* In the case of an ACK, no data is returned back
			 * to the daemon; just the command packet */
		}
	} else {
		memcpy(&buf[sizeof(daemon_cmd->cmd)],
		       daemon_cmd->cmd.data, daemon_cmd->cmd.data_len);
		count += daemon_cmd->cmd.data_len;

		IPW_DEBUG_DAEMON("driver -> daemon #%02X (%d bytes).\n",
				 daemon_cmd->cmd.cmd, daemon_cmd->cmd.data_len);
	}

	daemon_cmd->cmd.version = IPW_DAEMON_VERSION;
	memcpy(buf, &daemon_cmd->cmd, sizeof(daemon_cmd->cmd));
	count += sizeof(daemon_cmd->cmd);

	kfree(daemon_cmd);

	return count;
}

static DEVICE_ATTR(cmd, S_IWUSR | S_IRUSR, show_cmd, store_cmd);

static ssize_t show_eeprom(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			   struct device_attribute *attr,
#endif
			   char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	memcpy(buf, priv->eeprom, EEPROM_IMAGE_SIZE);
	return EEPROM_IMAGE_SIZE;
}

static DEVICE_ATTR(eeprom, S_IRUSR, show_eeprom, NULL);

#define GEO_PRINT(b,x) ((geo-> b[i].flags & IEEE80211_CH_##x) ? # x " " : "")
static ssize_t show_channels(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			     struct device_attribute *attr,
#endif
			     char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	const struct ieee80211_geo *geo = ieee80211_get_geo(priv->ieee);
	int len = 0, i;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	len += sprintf(&buf[len], "Displaying %d channels in 2.4Ghz band "
		       "(802.11bg):\n", geo->bg_channels);

	for (i = 0; i < geo->bg_channels; i++) {
		len += sprintf(&buf[len], "%d: %ddBm: BSS%s%s, %s.\n",
			       geo->bg[i].channel,
			       geo->bg[i].max_power,
			       geo->bg[i].flags & IEEE80211_CH_RADAR_DETECT ?
			       " (IEEE 802.11h requried)" : "",
			       ((geo->bg[i].flags & IEEE80211_CH_NO_IBSS) ||
				(geo->bg[i].flags &
				 IEEE80211_CH_RADAR_DETECT)) ? "" : ", IBSS",
			       geo->bg[i].flags & IEEE80211_CH_PASSIVE_ONLY ?
			       "passive only" : "active/passive");
	}

	len += sprintf(&buf[len], "Displaying %d channels in 5.2Ghz band "
		       "(802.11a):\n", geo->a_channels);
	for (i = 0; i < geo->a_channels; i++) {
		len += sprintf(&buf[len], "%d: %ddBm: BSS%s%s, %s.\n",
			       geo->a[i].channel,
			       geo->a[i].max_power,
			       geo->a[i].flags & IEEE80211_CH_RADAR_DETECT ?
			       " (IEEE 802.11h requried)" : "",
			       ((geo->a[i].flags & IEEE80211_CH_NO_IBSS) ||
				(geo->a[i].flags &
				 IEEE80211_CH_RADAR_DETECT)) ? "" : ", IBSS",
			       geo->a[i].flags & IEEE80211_CH_PASSIVE_ONLY ?
			       "passive only" : "active/passive");
	}

	return len;
}

static DEVICE_ATTR(channels, S_IRUSR, show_channels, NULL);

static ssize_t show_statistics(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			       struct device_attribute *attr,
#endif
			       char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
	u32 size = sizeof(priv->statistics), len = 0, ofs = 0;
	u8 *data = (u8 *) & priv->statistics;
	int rc = 0;

	if (!ipw_is_alive(priv))
		return -EAGAIN;

	mutex_lock(&priv->mutex);
	rc = ipw_send_statistics_request(priv);
	mutex_unlock(&priv->mutex);

	if (rc) {
		len = sprintf(buf,
			      "Error sending statistics request: 0x%08X\n", rc);
		return len;
	}

	while (size && (PAGE_SIZE - len)) {
		len += snprint_line(&buf[len], PAGE_SIZE - len, &data[ofs],
				    min(size, 16U), ofs);
		if (PAGE_SIZE - len)
			buf[len++] = '\n';

		ofs += 16;
		size -= min(size, 16U);
	}

	return len;
}

static DEVICE_ATTR(statistics, S_IRUGO, show_statistics, NULL);

static ssize_t show_scan_age(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			     struct device_attribute *attr,
#endif
			     char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	if (!ipw_is_alive(priv))
		return -EAGAIN;

	return sprintf(buf, "%d\n", priv->ieee->scan_age);
}

static ssize_t store_scan_age(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			      struct device_attribute *attr,
#endif
			      const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);
#ifdef CONFIG_IPW3945_DEBUG
	struct net_device *dev = priv->net_dev;
#endif
	char buffer[] = "00000000";
	unsigned long len =
	    (sizeof(buffer) - 1) > count ? count : sizeof(buffer) - 1;
	unsigned long val;
	char *p = buffer;

	IPW_DEBUG_INFO("enter\n");

	strncpy(buffer, buf, len);
	buffer[len] = 0;

	if (p[1] == 'x' || p[1] == 'X' || p[0] == 'x' || p[0] == 'X') {
		p++;
		if (p[0] == 'x' || p[0] == 'X')
			p++;
		val = simple_strtoul(p, &p, 16);
	} else
		val = simple_strtoul(p, &p, 10);
	if (p == buffer) {
		IPW_DEBUG_INFO("%s: user supplied invalid value.\n", dev->name);
	} else {
		priv->ieee->scan_age = val;
		IPW_DEBUG_INFO("set scan_age = %u\n", priv->ieee->scan_age);
	}

	IPW_DEBUG_INFO("exit\n");
	return len;
}

static DEVICE_ATTR(scan_age, S_IWUSR | S_IRUGO, show_scan_age, store_scan_age);

static ssize_t show_roam(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			 struct device_attribute *attr,
#endif
			 char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	if (!ipw_is_alive(priv))
		return -EAGAIN;

	return sprintf(buf, "%d\n", (priv->config & CFG_NO_ROAMING) ? 0 : 1);
}

static ssize_t store_roam(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			  struct device_attribute *attr,
#endif
			  const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	switch (buf[0]) {
	case '1':
		priv->config &= ~CFG_NO_ROAMING;
		break;
	case '0':
		priv->config |= CFG_NO_ROAMING;
		break;
	}
	return count;
}

static DEVICE_ATTR(roam, S_IWUSR | S_IRUGO, show_roam, store_roam);

static ssize_t show_antenna(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			    struct device_attribute *attr,
#endif
			    char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	if (!ipw_is_alive(priv))
		return -EAGAIN;

	return sprintf(buf, "%d\n", priv->antenna);
}

static ssize_t store_antenna(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			     struct device_attribute *attr,
#endif
			     const char *buf, size_t count)
{
	int ant;
	struct ipw_priv *priv = dev_get_drvdata(d);

	if (count == 0)
		return 0;

	if (sscanf(buf, "%1i", &ant) != 1) {
		IPW_DEBUG_INFO("not in hex or decimal form.\n");
		return count;
	}

	if ((ant >= 0) && (ant <= 2)) {
		IPW_DEBUG_INFO("Setting antenna select to %d.\n", ant);
		priv->antenna = ant;
	} else {
		IPW_DEBUG_INFO("Bad antenna select value %d.\n", ant);
	}

	return count;
}

static DEVICE_ATTR(antenna, S_IWUSR | S_IRUGO, show_antenna, store_antenna);

static ssize_t show_led(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			struct device_attribute *attr,
#endif
			char *buf)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	if (!ipw_is_alive(priv))
		return -EAGAIN;

	return sprintf(buf, "%d\n", (priv->config & CFG_NO_LED) ? 0 : 1);
}

static ssize_t store_led(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			 struct device_attribute *attr,
#endif
			 const char *buf, size_t count)
{
	struct ipw_priv *priv = dev_get_drvdata(d);

	if (count == 0)
		return 0;

	if (*buf == '0') {
		IPW_DEBUG_LED("Disabling LED control.\n");
		priv->config |= CFG_NO_LED;
	} else {
		IPW_DEBUG_LED("Enabling LED control.\n");
		priv->config &= ~CFG_NO_LED;
	}

	ipw_update_link_led(priv);
	ipw_update_activity_led(priv);
	ipw_update_tech_led(priv);

	return count;
}

static DEVICE_ATTR(led, S_IWUSR | S_IRUGO, show_led, store_led);

static ssize_t show_status(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			   struct device_attribute *attr,
#endif
			   char *buf)
{
	struct ipw_priv *priv = (struct ipw_priv *)d->driver_data;
	if (!ipw_is_alive(priv))
		return -EAGAIN;
	return sprintf(buf, "0x%08x\n", (int)priv->status);
}

static DEVICE_ATTR(status, S_IRUGO, show_status, NULL);

static ssize_t show_cfg(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			struct device_attribute *attr,
#endif
			char *buf)
{
	struct ipw_priv *priv = (struct ipw_priv *)d->driver_data;

	if (!ipw_is_alive(priv))
		return -EAGAIN;

	return sprintf(buf, "0x%08x\n", (int)priv->config);
}

static DEVICE_ATTR(cfg, S_IRUGO, show_cfg, NULL);

static ssize_t dump_error_log(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			      struct device_attribute *attr,
#endif
			      const char *buf, size_t count)
{
	char *p = (char *)buf;

	if (p[0] == '1')
		ipw_dump_nic_error_log((struct ipw_priv *)d->driver_data);

	return strnlen(buf, count);
}

static DEVICE_ATTR(dump_errors, S_IWUSR, NULL, dump_error_log);

static ssize_t dump_event_log(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			      struct device_attribute *attr,
#endif
			      const char *buf, size_t count)
{
	char *p = (char *)buf;

	if (p[0] == '1')
		ipw_dump_nic_event_log((struct ipw_priv *)d->driver_data);

	return strnlen(buf, count);
}

static DEVICE_ATTR(dump_events, S_IWUSR, NULL, dump_event_log);

static ssize_t show_rf_kill(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			    struct device_attribute *attr,
#endif
			    char *buf)
{
	/* 0 - RF kill not enabled
	   1 - SW based RF kill active (sysfs)
	   2 - HW based RF kill active
	   3 - Both HW and SW baed RF kill active */
	struct ipw_priv *priv = (struct ipw_priv *)d->driver_data;
	int val = ((priv->status & STATUS_RF_KILL_SW) ? 0x1 : 0x0) |
	    ((priv->status & STATUS_RF_KILL_HW) ? 0x2 : 0x0);

	if (!ipw_is_alive(priv))
		return -EAGAIN;

	return sprintf(buf, "%i\n", val);
}

static int ipw_scan_schedule(struct ipw_priv *priv, unsigned long ms)
{
	if (priv->status & STATUS_SCAN_ABORTING) {
		IPW_DEBUG_SCAN("Scan abort in progress.  Deferring scan "
			       "request.\n");
		priv->status |= STATUS_SCAN_PENDING;
		return 0;
	}

	if (ms)
		queue_delayed_work(priv->workqueue, &priv->request_scan,
				   msecs_to_jiffies(ms));
	else
		queue_work(priv->workqueue, &priv->request_scan);

	return 0;
}

static int ipw_scan_initiate(struct ipw_priv *priv, int bands, unsigned long ms)
{
	if (!ipw_is_calibrated(priv)) {
		IPW_DEBUG_SCAN("Scan not initiated -- not calibrated.\n");
		return -EAGAIN;
	}

	if (priv->status & STATUS_SCANNING) {
		IPW_DEBUG_SCAN("Scan already in progress.\n");
		return 0;
	}

	if (priv->status & STATUS_EXIT_PENDING) {
		IPW_DEBUG_SCAN("Aborting scan due to device shutdown\n");
		priv->status |= STATUS_SCAN_PENDING;
		return 0;
	}

	if (priv->status & STATUS_SCAN_ABORTING) {
		IPW_DEBUG_HC("Scan request while abort pending.  Queuing.\n");
		priv->status |= STATUS_SCAN_PENDING;
		return 0;
	}

	if (priv->status & STATUS_RF_KILL_MASK) {
		IPW_DEBUG_HC("Aborting scan due to RF Kill activation\n");
		priv->status |= STATUS_SCAN_PENDING;
		return 0;
	}

	if (!(priv->status & STATUS_READY)) {
		IPW_DEBUG_HC("Scan request while uninitialized.  Queuing.\n");
		priv->status |= STATUS_SCAN_PENDING;
		return 0;
	}

	if (priv->status & STATUS_ASSOCIATING) {
		IPW_DEBUG_HC("Scan request while associating.  Queuing.\n");
		priv->status |= STATUS_SCAN_PENDING;
		return 0;
	}

	priv->scan_bands = bands;
	priv->scan_bands_remaining = bands;

	IPW_DEBUG_INFO("Setting scan to on\n");
	priv->status |= STATUS_SCANNING;
	priv->scan_passes = 0;
	priv->scan_start = jiffies;
	priv->scan_pass_start = priv->scan_start;

	return ipw_scan_schedule(priv, ms);
}

static int ipw_scan_cancel(struct ipw_priv *priv)
{
	if (priv->status & STATUS_SCAN_PENDING) {
		IPW_DEBUG_SCAN("Cancelling pending scan request.\n");
		priv->status &= ~STATUS_SCAN_PENDING;
		cancel_delayed_work(&priv->request_scan);
	}

	if (priv->status & STATUS_SCANNING) {
		if (!(priv->status & STATUS_SCAN_ABORTING)) {
			IPW_DEBUG_SCAN("Queuing scan abort.\n");
			priv->status |= STATUS_SCAN_ABORTING;
			queue_work(priv->workqueue, &priv->abort_scan);
		} else {
			IPW_DEBUG_SCAN("Scan abort already in progress.\n");
		}
	}

	return 0;
}

static inline unsigned long elapsed_jiffies(unsigned long start,
					    unsigned long end)
{
	if (end > start)
		return end - start;

	return end + (MAX_JIFFY_OFFSET - start);
}

static int ipw_scan_completed(struct ipw_priv *priv, int success)
{
	/* The HW is no longer scanning */
	priv->status &= ~STATUS_SCAN_HW;

	/* The scan completion notification came in, so kill that timer... */
	cancel_delayed_work(&priv->scan_check);

	/* Notify the user space daemon that we are out of scan... */
	ipw_send_daemon_sync(priv, DAEMON_SYNC_SCAN_COMPLETE, 0, NULL);

	priv->scan_passes++;
	IPW_DEBUG_INFO("Scan pass #%d on %sGhz took %dms\n",
		       priv->scan_passes,
		       (priv->scan_flags & DAEMON_SCAN_FLAG_24GHZ) ?
		       "2.4" : "5.2",
		       jiffies_to_msecs(elapsed_jiffies(priv->scan_pass_start,
							jiffies)));

	/* Remove this scanned band from the list
	 * of pending bands to scan */
	if (priv->scan_flags & DAEMON_SCAN_FLAG_24GHZ)
		priv->scan_bands_remaining &= ~IEEE80211_24GHZ_BAND;
	else
		priv->scan_bands_remaining &= ~IEEE80211_52GHZ_BAND;

	/* If a request to abort was given, or the scan did not succeed
	 * then we reset the scan state machine and terminate,
	 * re-queuing another scan if one has been requested */
	if (priv->status & STATUS_SCAN_ABORTING) {
		IPW_DEBUG_INFO("Aborted scan completed.\n");
		priv->status &= ~STATUS_SCAN_ABORTING;
	} else {
		/* If there are more bands on this scan pass reschedule */
		if (priv->scan_bands_remaining)
			goto reschedule;
	}

	IPW_DEBUG_INFO("Setting scan to off\n");

	priv->status &= ~STATUS_SCANNING;
	priv->ieee->scans++;

	IPW_DEBUG_INFO("Scan took %dms\n",
		       jiffies_to_msecs(elapsed_jiffies(priv->scan_start,
							jiffies)));

	queue_work(priv->workqueue, &priv->update_link_led);

	if (!(priv->status & (STATUS_ASSOCIATED | STATUS_ASSOCIATING |
			      STATUS_ROAMING | STATUS_DISASSOCIATING))) {
		queue_work(priv->workqueue, &priv->associate);
		return 0;
	}

	if (priv->status & STATUS_ROAMING) {
		/* If a scan completed and we are in roam mode, then
		 * the scan that completed was the one requested as a
		 * result of entering roam... so, schedule the
		 * roam work */
		queue_work(priv->workqueue, &priv->roam);
		return 0;
	}

	if ((priv->config & CFG_BACKGROUND_SCAN) &&
	    (priv->status & STATUS_ASSOCIATED)) {
		ipw_scan_initiate(priv, priv->ieee->freq_band, SCAN_INTERVAL);

		return 0;
	}

	if (priv->status & STATUS_SCAN_PENDING) {
		ipw_scan_initiate(priv, priv->ieee->freq_band, 0);
		return 0;
	}

      reschedule:
	priv->scan_pass_start = jiffies;
	ipw_scan_schedule(priv, 0);

	return 0;
}

static void ipw_radio_kill_sw(struct ipw_priv *priv, int disable_radio)
{
	if ((disable_radio ? 1 : 0) ==
	    ((priv->status & STATUS_RF_KILL_SW) ? 1 : 0))
		return;

	IPW_DEBUG_RF_KILL("Manual SW RF KILL set to: RADIO %s\n",
			  disable_radio ? "OFF" : "ON");

	if (disable_radio) {
		ipw_update_link_led(priv);
		ipw_scan_cancel(priv);
		ipw_write32(priv, CSR_UCODE_DRV_GP1_SET,
			    CSR_UCODE_SW_BIT_RFKILL);
		ipw_send_card_state(priv, CARD_STATE_CMD_DISABLE, 0);
		priv->status |= STATUS_RF_KILL_SW;
		ipw_write32(priv, CSR_UCODE_DRV_GP1_SET,
			    CSR_UCODE_DRV_GP1_BIT_CMD_BLOCKED);
		return;
	}

	ipw_write32(priv, CSR_UCODE_DRV_GP1_CLR, CSR_UCODE_SW_BIT_RFKILL);

	priv->status &= ~STATUS_RF_KILL_SW;

	if (priv->status & STATUS_RF_KILL_HW) {
		IPW_DEBUG_RF_KILL("Can not turn radio back on - "
				  "disabled by HW switch\n");
		return;
	}

	queue_work(priv->workqueue, &priv->down);

	return;
}

static ssize_t store_rf_kill(struct device *d,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
			     struct device_attribute *attr,
#endif
			     const char *buf, size_t count)
{
	struct ipw_priv *priv = (struct ipw_priv *)d->driver_data;

	mutex_lock(&priv->mutex);
	ipw_radio_kill_sw(priv, buf[0] == '1');
	mutex_unlock(&priv->mutex);

	return count;
}

static DEVICE_ATTR(rf_kill, S_IWUSR | S_IRUGO, show_rf_kill, store_rf_kill);

static void notify_wx_assoc_event(struct ipw_priv *priv)
{
	union iwreq_data wrqu;
	wrqu.ap_addr.sa_family = ARPHRD_ETHER;
	if (priv->status & STATUS_ASSOCIATED)
		memcpy(wrqu.ap_addr.sa_data, priv->bssid, ETH_ALEN);
	else
		memset(wrqu.ap_addr.sa_data, 0, ETH_ALEN);
	wireless_send_event(priv->net_dev, SIOCGIWAP, &wrqu, NULL);
}

#ifdef CONFIG_IPW3945_DEBUG
static void ipw_print_rx_config_cmd(struct daemon_rx_config *rxon)
{
	IPW_DEBUG_RADIO("RX CONFIG:\n");
	printk_buf(IPW_DL_RADIO, (u8 *) rxon, sizeof(*rxon));
	IPW_DEBUG_RADIO("u16 channel: 0x%x\n", rxon->channel);
	IPW_DEBUG_RADIO("u32 flags: 0x%08X " BIT_FMT32 "\n",
			rxon->flags, BIT_ARG32(rxon->flags));
	IPW_DEBUG_RADIO("u32 filter_flags: 0x%08x " BIT_FMT32 "\n",
			rxon->filter_flags, BIT_ARG32(rxon->filter_flags));
	IPW_DEBUG_RADIO("u8 dev_type: 0x%x\n", rxon->dev_type);
	IPW_DEBUG_RADIO("u8 ofdm_basic_rates: 0x%02x " BIT_FMT8 "\n",
			rxon->ofdm_basic_rates,
			BIT_ARG8(rxon->ofdm_basic_rates));
	IPW_DEBUG_RADIO("u8 cck_basic_rates: 0x%02x " BIT_FMT8 "\n",
			rxon->cck_basic_rates, BIT_ARG8(rxon->cck_basic_rates));
	IPW_DEBUG_RADIO("u8[6] node_addr: " MAC_FMT "\n",
			MAC_ARG(rxon->node_addr));
	IPW_DEBUG_RADIO("u8[6] bssid_addr: " MAC_FMT "\n",
			MAC_ARG(rxon->bssid_addr));
	IPW_DEBUG_RADIO("u16 assoc_id: 0x%x\n", rxon->assoc_id);
}
#endif

static void ipw_irq_handle_error(struct ipw_priv *priv)
{
	/* Set the FW error flag -- cleared on ipw_down */
	priv->status |= STATUS_FW_ERROR;

	/* Cancel currently queued command. */
	priv->status &= ~STATUS_HCMD_ACTIVE;

#ifdef CONFIG_IPW3945_DEBUG
	if (ipw_debug_level & IPW_DL_FW_ERRORS) {
		ipw_dump_nic_error_log(priv);
		ipw_dump_nic_event_log(priv);
		ipw_print_rx_config_cmd(&priv->rxon);
	}
#endif

	wake_up_interruptible(&priv->wait_command_queue);

	/* Keep the restart process from trying to send host
	 * commands by clearing the INIT status bit */
	priv->status &= ~STATUS_READY;
	if (!(priv->status & STATUS_EXIT_PENDING)) {
		IPW_DEBUG(IPW_DL_INFO | IPW_DL_FW_ERRORS,
			  "Restarting adapter due to uCode error.\n");
		queue_work(priv->workqueue, &priv->down);
	}
}

static void ipw_irq_tasklet(struct ipw_priv *priv)
{
	u32 inta, inta_mask, handled = 0;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	inta = ipw_read32(priv, CSR_INT);
	inta_mask = ipw_read32(priv, CSR_INT_MASK);
	ipw_write32(priv, CSR_INT, inta);
	inta &= (CSR_INI_SET_MASK & inta_mask);

	/* Add any cached INTA values that need to be handled */
	inta |= priv->isr_inta;

	if (inta & BIT_INT_ERR) {
		IPW_ERROR("Microcode HW error detected.  Restarting.\n");

		/* tell the device to stop sending interrupts */
		ipw_disable_interrupts(priv);

		ipw_irq_handle_error(priv);

		handled |= BIT_INT_ERR;

		spin_unlock_irqrestore(&priv->lock, flags);

		return;
	}

	if (inta & BIT_INT_SWERROR) {
		IPW_ERROR("Microcode SW error detected.  Restarting.\n");
		ipw_irq_handle_error(priv);
		handled |= BIT_INT_SWERROR;
	}

	if (inta & BIT_INT_WAKEUP) {
		IPW_DEBUG_ISR("Wakeup interrupt\n");
		ipw_rx_queue_update_write_ptr(priv, priv->rxq);
		ipw_tx_queue_update_write_ptr(priv, &priv->txq[0], 0);
		ipw_tx_queue_update_write_ptr(priv, &priv->txq[1], 1);
		ipw_tx_queue_update_write_ptr(priv, &priv->txq[2], 2);
		ipw_tx_queue_update_write_ptr(priv, &priv->txq[3], 3);
		ipw_tx_queue_update_write_ptr(priv, &priv->txq[4], 4);
		ipw_tx_queue_update_write_ptr(priv, &priv->txq[5], 5);

		handled |= BIT_INT_WAKEUP;
	}

	if (inta & BIT_INT_ALIVE) {
		IPW_DEBUG_ISR("Alive interrupt\n");
		handled |= BIT_INT_ALIVE;
	}

	/* handle all the justifications for the interrupt */
	if (inta & BIT_INT_RX) {
		//      IPW_DEBUG_ISR("Rx interrupt\n");
		ipw_rx_handle(priv);
		handled |= BIT_INT_RX;
	}

	if (inta & BIT_INT_TX) {
		IPW_DEBUG_ISR("Command completed.\n");
		ipw_write32(priv, CSR_FH_INT_STATUS, (1 << 6));
		if (ipw_grab_restricted_access(priv)) {
			ipw_write_restricted(priv,
					     FH_TCSR_CREDIT(ALM_FH_SRVC_CHNL),
					     0x0);
			ipw_release_restricted_access(priv);
		}

		handled |= BIT_INT_TX;
	}

	if (handled != inta) {
		IPW_ERROR("Unhandled INTA bits 0x%08x\n", inta & ~handled);
	}

	/* enable all interrupts */
	ipw_enable_interrupts(priv);

	spin_unlock_irqrestore(&priv->lock, flags);
}

/****************** 3945ABG-FUNCTIONS *********************/

static int ipw_rf_eeprom_ready(struct ipw_priv *priv)
{
	u8 sku_cap;
	int rc;

	rc = ipw_query_eeprom(priv, EEPROM_SKU_CAP, sizeof(u8),
			      (u8 *) & sku_cap);
	if (rc) {
		IPW_WARNING("failed to read EEPROM_SKU_CAP\n");
		return rc;
	}

	if (sku_cap & EEPROM_SKU_CAP_SW_RF_KILL_ENABLE)
		priv->capability |= CAP_RF_SW_KILL;
	else
		priv->capability &= ~CAP_RF_SW_KILL;

	if (sku_cap & EEPROM_SKU_CAP_HW_RF_KILL_ENABLE)
		priv->capability |= CAP_RF_HW_KILL;
	else
		priv->capability &= ~CAP_RF_HW_KILL;

	switch (priv->capability & (CAP_RF_HW_KILL | CAP_RF_SW_KILL)) {
	case CAP_RF_HW_KILL:
		IPW_DEBUG_RF_KILL("HW RF KILL supported in EEPROM.\n");
		break;
	case CAP_RF_SW_KILL:
		IPW_DEBUG_RF_KILL("SW RF KILL supported in EEPROM.\n");
		break;
	case (CAP_RF_HW_KILL | CAP_RF_SW_KILL):
		IPW_DEBUG_RF_KILL("HW & HW RF KILL supported in EEPROM.\n");
		break;
	default:
		IPW_DEBUG_RF_KILL("NO RF KILL supported in EEPROM.\n");
		break;
	}

	return 0;
}

static int ipw_send_rates_scaling_cmd(struct ipw_priv *priv,
				      struct RateScalingCmdSpecifics *rate)
{
	return ipw_send_cmd_pdu(priv, REPLY_RATE_SCALE,
				sizeof(struct RateScalingCmdSpecifics), rate);
}

static int ipw_send_rx_config(struct ipw_priv *priv)
{
	struct ipw_daemon_cmd_list *element;
	int rc = 0;

	element = kmalloc(sizeof(*element) + sizeof(struct daemon_rx_config),
			  GFP_ATOMIC);

	if (!element)
		return -ENOMEM;

	element->cmd.cmd = DAEMON_RX_CONFIG;
	element->cmd.data_len = sizeof(struct daemon_rx_config);
	element->cmd.flags = DAEMON_FLAG_WANT_RESULT;
	memcpy(element->cmd.data, &priv->rxon, sizeof(struct daemon_rx_config));

	rc = ipw_send_daemon_cmd(priv, element);

	IPW_DEBUG_INFO("Returned from ipw_send_daemon_cmd.\n");

	return rc;
}

static int ipw_send_rxon_assoc(struct ipw_priv *priv)
{
	int rc = 0;
	struct ipw_rx_packet *res = NULL;
	struct ipw_rxon_assoc_cmd rxon_assoc;
	struct ipw_host_cmd cmd = {
		.id = REPLY_RX_ON_ASSOC,
		.len = sizeof(struct ipw_rxon_assoc_cmd),
		.meta.flags = CMD_WANT_SKB,
		.data = &rxon_assoc,
	};

	rxon_assoc.flags = priv->rxon.flags;
	rxon_assoc.filter_flags = priv->rxon.filter_flags;
	rxon_assoc.ofdm_basic_rates = priv->rxon.ofdm_basic_rates;
	rxon_assoc.cck_basic_rates = priv->rxon.cck_basic_rates;
	rxon_assoc.reserved = 0;

	rc = ipw_send_cmd(priv, &cmd);
	if (rc)
		return rc;

	res = (struct ipw_rx_packet *)cmd.meta.u.skb->data;
	if (res->hdr.flags & 0x40) {
		IPW_ERROR("Bad return from REPLY_RX_ON_ASSOC command\n");
		rc = -EIO;
	}

	dev_kfree_skb_any(cmd.meta.u.skb);

	return rc;
}

static int ipw_add_sta_sync_callback(struct ipw_priv *priv,
				     struct ipw_cmd *cmd, struct sk_buff *skb)
{
	struct ipw_rx_packet *res = NULL;

	if (!skb) {
		IPW_ERROR("Error: Response NULL in " "REPLY_ADD_STA.\n");
		return 1;
	}

	res = (struct ipw_rx_packet *)skb->data;
	if (res->hdr.flags & 0x40) {
		IPW_ERROR("Bad return from REPLY_ADD_STA (0x%08X)\n",
			  res->hdr.flags);
		return 1;
	}

	switch (res->u.add_sta.status) {
	case ADD_STA_SUCCESS_MSK:
		break;
	default:
		break;
	}

	return 1;		/* We didn't cache the SKB; let the caller free it */
}

static int ipw_send_add_station(struct ipw_priv *priv,
				struct ipw_addsta_cmd *sta, u8 flags)
{
	struct ipw_rx_packet *res = NULL;
	int rc = 0;
	struct ipw_host_cmd cmd = {
		.id = REPLY_ADD_STA,
		.len = sizeof(struct ipw_addsta_cmd),
		.meta.flags = flags,
		.data = sta,
	};

	if (flags & CMD_ASYNC)
		cmd.meta.u.callback = ipw_add_sta_sync_callback;
	else
		cmd.meta.flags |= CMD_WANT_SKB;

	rc = ipw_send_cmd(priv, &cmd);

	if (rc || (flags & CMD_ASYNC))
		return rc;

	res = (struct ipw_rx_packet *)cmd.meta.u.skb->data;
	if (res->hdr.flags & 0x40) {
		IPW_ERROR("Bad return from REPLY_ADD_STA (0x%08X)\n",
			  res->hdr.flags);
		rc = -EIO;
	}

	if (rc == 0) {
		switch (res->u.add_sta.status) {
		case ADD_STA_SUCCESS_MSK:
			IPW_DEBUG_INFO("REPLY_ADD_STA PASSED\n");
			break;
		default:
			rc = -EIO;
			IPW_WARNING("REPLY_ADD_STA failed\n");
			break;
		}
	}
	dev_kfree_skb_any(cmd.meta.u.skb);

	return rc;
}

static int ipw_card_state_sync_callback(struct ipw_priv *priv,
					struct ipw_cmd *cmd,
					struct sk_buff *skb)
{

	return 1;
}

/*
 * CARD_STATE_CMD
 *
 * Use: Sets the internal card state to enable, disable, or halt
 *
 * When in the 'enable' state the card operates as normal.
 * When in the 'disable' state, the card enters into a low power mode.
 * When in the 'halt' state, the card is shut down and must be fully
 * restarted to come back on.
 */
static int ipw_send_card_state(struct ipw_priv *priv, u32 flags, u8 meta_flag)
{
	struct ipw_host_cmd cmd = {
		.id = REPLY_CARD_STATE_CMD,
		.len = sizeof(u32),
		.data = &flags,
		.meta.flags = meta_flag,
	};

	if (meta_flag & CMD_ASYNC)
		cmd.meta.u.callback = ipw_card_state_sync_callback;

	return ipw_send_cmd(priv, &cmd);
}

static int ipw_send_rxon_timing(struct ipw_priv *priv,
				struct ipw_rxon_time_cmd *rxontime)
{
	return ipw_send_cmd_pdu(priv, REPLY_RX_ON_TIMING,
				sizeof(struct ipw_rxon_time_cmd), rxontime);
}

/**
* rate  duration
**/
#define MMAC_SCHED_2_4GHZ_SIFS               10
#define MMAC_SCHED_5_2GHZ_SIFS               16
#define MMAC_SCHED_CCK_PHY_OVERHEAD_SHORT    96
#define MMAC_SCHED_CCK_PHY_OVERHEAD_LONG    192
#define MMAC_SCHED_OFDM_SYMBOL_TIME           4
#define MMAC_SCHED_OFDM_PHY_OVERHEAD         20
#define MMAC_SCHED_UCODE_OVERHEAD             8

#define IPW_INVALID_RATE                   0xFF
static struct ipw_rate_info rate_table_info[] = {
/*  CCK rate info   */
	{10, 2, 8, 0, 112, 160, 112, 1216},	/*   1mbps */
	{20, 4, 9, 0, 56, 80, 56, 608},	/*   2mbps */
	{55, 11, 10, 0, 21, 29, 21, 222},	/* 5.5mbps */
	{110, 22, 11, 0, 11, 15, 11, 111},	/*  11mbps */
/*  OFDM rate info   */
	{13, 6 * 2, 0, 24, 44, 52, 44, 228},	/*   6mbps */
	{15, 9 * 2, 1, 36, 36, 44, 36, 160},	/*   9mbps */
	{5, 12 * 2, 2, 48, 32, 36, 32, 124},	/*  12mbps */
	{7, 18 * 2, 3, 72, 28, 32, 28, 92},	/*  18mbps */
	{9, 24 * 2, 4, 96, 28, 32, 28, 72},	/*  24mbps */
	{11, 36 * 2, 5, 144, 24, 28, 24, 56},	/*  36mbps */
	{1, 48 * 2, 6, 192, 24, 24, 24, 48},	/*  48mbps */
	{3, 54 * 2, 7, 216, 24, 24, 24, 44},	/*  54mbps */
};

static inline int ipw_rate_plcp2index(u8 x)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(rate_table_info); i++) {
		if (rate_table_info[i].rate_plcp == x)
			return i;
	}
	return -1;
}

static inline int ipw_rate_scale2index(int x)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(rate_table_info); i++) {
		if (rate_table_info[i].rate_scale_index == x)
			return i;
	}
	return -1;
}

static inline u8 ipw_rate_scaling2rate_plcp(int x)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(rate_table_info); i++) {
		if (rate_table_info[i].rate_scale_index == x)
			return rate_table_info[i].rate_plcp;
	}
	return IPW_INVALID_RATE;
}

static inline int ipw_rate_plcp2rate_scaling(u8 x)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(rate_table_info); i++) {
		if (rate_table_info[i].rate_plcp == x)
			return rate_table_info[i].rate_scale_index;
	}
	return -1;
}

static int ipw_rate_ieee2index(u8 x)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(rate_table_info); i++) {
		if (rate_table_info[i].rate_ieee == x)
			return i;
	}
	return -1;
}

static u8 ipw_rate_index2ieee(int x)
{

	if (x < ARRAY_SIZE(rate_table_info))
		return rate_table_info[x].rate_ieee;

	return IPW_INVALID_RATE;
}

static int ipw_rate_index2rate_scale(int x)
{

	if (x < ARRAY_SIZE(rate_table_info))
		return rate_table_info[x].rate_scale_index;

	return -1;
}

static u8 ipw_rate_index2plcp(int x)
{

	if (x < ARRAY_SIZE(rate_table_info))
		return rate_table_info[x].rate_plcp;

	return IPW_INVALID_RATE;
}

static u8 ipw_rate_get_lowest_plcp(int rate_mask)
{
	int start, i;

	if (rate_mask & 0xF)
		start = 0;
	else if (rate_mask & 0xF0)
		start = 4;
	else
		start = 8;

	for (i = start; i < 12; i++) {
		if ((rate_mask & (1 << i)) != 0)
			return rate_table_info[i].rate_plcp;
	}
	return IPW_INVALID_RATE;
}

/**
* RXON functions
**/
static u8 BROADCAST_ADDR[ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/*
  validate rxon command before issuing the command to the card.
*/
static int ipw_check_rx_config_cmd(struct daemon_rx_config *rxon)
{
	int error = 0;
	int counter = 1;

	if (rxon->flags & RXON_FLG_BAND_24G_MSK) {
		error |= (rxon->flags & RXON_FLG_TGJ_NARROW_BAND_MSK);
		error |= (rxon->flags & RXON_FLG_RADAR_DETECT_MSK);
		if (error)
			IPW_WARNING("check 24G fields %d | %d\n", counter++,
				    error);
	} else {
		error |= ((rxon->flags & RXON_FLG_SHORT_SLOT_MSK) !=
			  RXON_FLG_SHORT_SLOT_MSK);
		if (error)
			IPW_WARNING("check 52 fields %d | %d\n", counter++,
				    error);
		error |= (rxon->flags & RXON_FLG_CCK_MSK);
		if (error)
			IPW_WARNING("check 52 CCK %d | %d\n", counter++, error);

	}
	error |= (rxon->node_addr[0] | rxon->bssid_addr[0]) & 0x1;
	if (error)
		IPW_WARNING("check mac addr %d | %d\n", counter++, error);

	// make sure basic rates 6Mbps and 1Mbps are supported
	error |= (((rxon->ofdm_basic_rates & R_6M_MSK) == 0) &&
		  ((rxon->cck_basic_rates & R_1M_MSK) == 0));

	if (error)
		IPW_WARNING("check basic rate %d | %d\n", counter++, error);
	error |= (rxon->assoc_id > 2007);
	if (error)
		IPW_WARNING("check assoc id %d | %d\n", counter++, error);

	error |=
	    ((rxon->flags & (RXON_FLG_CCK_MSK | RXON_FLG_SHORT_SLOT_MSK)) ==
	     (RXON_FLG_CCK_MSK | RXON_FLG_SHORT_SLOT_MSK));

	if (error)
		IPW_WARNING("check CCK and short slot %d | %d\n", counter++,
			    error);
	error |=
	    ((rxon->flags & (RXON_FLG_CCK_MSK | RXON_FLG_AUTO_DETECT_MSK)) ==
	     (RXON_FLG_CCK_MSK | RXON_FLG_AUTO_DETECT_MSK));

	if (error)
		IPW_WARNING("check CCK & auto detect %d | %d\n", counter++,
			    error);
	error |=
	    ((rxon->
	      flags & (RXON_FLG_AUTO_DETECT_MSK | RXON_FLG_TGG_PROTECT_MSK)) ==
	     RXON_FLG_TGG_PROTECT_MSK);

	if (error)
		IPW_WARNING("check TGG %d | %d\n", counter++, error);
	if (rxon->flags & RXON_FLG_BAND_24G_MSK)
		error |= ((rxon->channel < MIN_B_CHANNELS) ||
			  (rxon->channel > MAX_B_CHANNELS));
	else
		error |= ((rxon->channel < MIN_A_CHANNELS) ||
			  (rxon->channel > MAX_A_CHANNELS));
	if (error)
		IPW_WARNING("check channel number %d | %d\n", counter++, error);
	if ((rxon->flags & RXON_FLG_DIS_DIV_MSK))
		error |=
		    ((rxon->
		      flags & (RXON_FLG_ANT_B_MSK | RXON_FLG_ANT_A_MSK)) == 0);

	if (error)
		IPW_WARNING("check antenna %d %d\n", counter++, error);
	if (error)
		IPW_WARNING("Tuning to channel %d\n", rxon->channel);
	if (error) {
		IPW_ERROR
		    ("Error not a valid ipw_rxon_assoc_cmd field values\n");
		return -1;
	}

	return 0;
}

/* Get antenna flags for RxOn command. */
static int ipw_get_antenna_flags(struct ipw_priv *priv)
{
	int rc = 0;
	u8 invert = 0;

	/* "diversity", NIC selects best antenna by itself */
	if (priv->antenna == 0) {
		return 0;
	}

	rc = ipw_query_eeprom(priv, EEPROM_ANTENNA_SWITCH_TYPE, sizeof(u8),
			      (u8 *) & invert);
	if (rc) {
		IPW_WARNING("failed to read EEPROM_ANTENNA_SWITCH_TYPE\n");
	}

	/* force Main antenna */
	if (priv->antenna == 1) {
		if (invert)
			return RXON_FLG_DIS_DIV_MSK | RXON_FLG_ANT_B_MSK;
		else
			return RXON_FLG_DIS_DIV_MSK | RXON_FLG_ANT_A_MSK;
	}
	/* force Aux antenna */
	else if (priv->antenna == 2) {
		if (invert)
			return RXON_FLG_DIS_DIV_MSK | RXON_FLG_ANT_A_MSK;
		else
			return RXON_FLG_DIS_DIV_MSK | RXON_FLG_ANT_B_MSK;
	}

	/* bad antenna selector value */
	else {
		IPW_ERROR("Bad antenna selector value (0x%x)\n", priv->antenna);
	}

	return 0;		/* "diversity" is default if error */
}

/*
  check rxon command then call the correct rxon command (associated vs. not)
*/
static int ipw_rxon_call(struct ipw_priv *priv, int is_assoc)
{
	int rc = 0;

	if (!ipw_is_alive(priv))
		return -1;

	if (!is_assoc) {
		/* always get timestamp with Rx frame */
		priv->rxon.flags |= RXON_FLG_TSF2HOST_MSK;

		/* select antenna */
		priv->rxon.flags &=
		    ~(RXON_FLG_DIS_DIV_MSK | RXON_FLG_ANT_SEL_MSK);
		priv->rxon.flags |= ipw_get_antenna_flags(priv);
	}
	rc = ipw_check_rx_config_cmd(&priv->rxon);
	if (rc)
		return rc;

	/* If we're associated, we use the ipw_rxon_assoc_cmd which
	 * is used to reconfigure filter and other flags for the current
	 * radio configuration.
	 *
	 * If we're not associated, we need to request the regulatory
	 * daemon to tune and configure the radio via ipw_send_rxon. */
	if (is_assoc)
		rc = ipw_send_rxon_assoc(priv);
	else
		rc = ipw_send_rx_config(priv);

	return rc;
}

/*
  add AP station into station table. there is only one AP
  station with id=0
*/
static int ipw_rxon_add_station(struct ipw_priv *priv, u8 * addr, int is_ap)
{
	/* Remove this station if it happens to already exist */
	ipw_remove_station(priv, addr, is_ap);

	return ipw_add_station(priv, addr, is_ap, 0);
}

static int ipw_send_bt_config(struct ipw_priv *priv)
{
	struct ipw_bt_cmd bt_cmd = {
		.flags = 3,
		.leadTime = 0xAA,
		.maxKill = 1,
		.killAckMask = 0,
		.killCTSMask = 0,
	};

	return ipw_send_cmd_pdu(priv, REPLY_BT_CONFIG,
				sizeof(struct ipw_bt_cmd), &bt_cmd);
}

/*
  initilize rxon structure with default values fromm eeprom
*/
static void ipw_connection_init_rx_config(struct ipw_priv *priv)
{
	int a_band = 0;
	memset(&priv->rxon, 0, sizeof(struct daemon_rx_config));

	switch (priv->ieee->iw_mode) {
	case IW_MODE_MASTER:
		priv->rxon.dev_type = RXON_DEV_TYPE_AP;
		break;

	case IW_MODE_INFRA:
		priv->rxon.dev_type = RXON_DEV_TYPE_ESS;
		priv->rxon.filter_flags = RXON_FILTER_ACCEPT_GRP_MSK;
		break;

	case IW_MODE_ADHOC:
		priv->rxon.dev_type = RXON_DEV_TYPE_IBSS;
		priv->rxon.flags = RXON_FLG_SHORT_PREAMBLE_MSK;
		priv->rxon.filter_flags = 0;
		break;

	case IW_MODE_MONITOR:
		/* priv->rxon.dev_type = RXON_DEV_TYPE_SNIFFER; */
		priv->rxon.dev_type = RXON_DEV_TYPE_SNIFFER;
		priv->rxon.filter_flags = RXON_FILTER_PROMISC_MSK
		    | RXON_FILTER_CTL2HOST_MSK
		    | RXON_FILTER_ACCEPT_GRP_MSK
		    | RXON_FILTER_DIS_DECRYPT_MSK
		    | RXON_FILTER_DIS_GRP_DECRYPT_MSK;
		break;
	}

	if (priv->config & CFG_PREAMBLE_LONG)
		priv->rxon.flags &= ~RXON_FLG_SHORT_PREAMBLE_MSK;

	if (!(priv->config & CFG_STATIC_CHANNEL)) {
		/* Priority to 802.11bg band */
		if (priv->ieee->freq_band & IEEE80211_24GHZ_BAND)
			a_band = 0;
		else
			a_band = 1;
	} else {
		if (priv->channel > IEEE80211_24GHZ_MAX_CHANNEL)
			a_band = 1;
		else
			a_band = 0;
	}

	if (!a_band) {
		priv->rxon.flags |=
		    (RXON_FLG_BAND_24G_MSK | RXON_FLG_AUTO_DETECT_MSK);
		priv->rxon.flags &= ~RXON_FLG_SHORT_SLOT_MSK;
		if (priv->ieee->freq_band & IEEE80211_52GHZ_BAND)
			IPW_DEBUG_INFO("Mixed band, choosing 2.4 GHz. \n");
		if (priv->ieee->modulation & IEEE80211_OFDM_MODULATION)
			IPW_DEBUG_INFO("Mixed modulation, choosing OFDM. \n");
		if (!(priv->config & CFG_STATIC_CHANNEL))
			priv->channel = 9;
	} else {
		priv->rxon.flags |= RXON_FLG_SHORT_SLOT_MSK;
		priv->rxon.flags &= ~(RXON_FLG_BAND_24G_MSK |
				      RXON_FLG_AUTO_DETECT_MSK);
		if (!(priv->config & CFG_STATIC_CHANNEL)) {
			IPW_DEBUG_INFO("TODO: Set to the first "
				       "*valid* channel that supports this mode.\n");
			priv->channel = 36;
		}
	}

	IPW_DEBUG_INFO("Channel set to %d%s\n", priv->channel,
		       (priv->config & CFG_STATIC_CHANNEL) ? "(static)" : "");
	priv->rxon.channel = priv->channel;
	priv->rxon.ofdm_basic_rates =
	    R_6M_MSK | R_24M_MSK | R_36M_MSK | R_48M_MSK | R_54M_MSK | R_9M_MSK
	    | R_12M_MSK | R_18M_MSK;
	priv->rxon.cck_basic_rates =
	    R_5_5M_MSK | R_1M_MSK | R_11M_MSK | R_2M_MSK;
}

/***************** END ***********************************/

#define IPW_SCAN_CHECK_WATCHDOG (7 * HZ)

static void ipw_bg_scan_check(void *data)
{
	struct ipw_priv *priv = data;

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	if (priv->status & (STATUS_SCANNING | STATUS_SCAN_ABORTING)) {
		IPW_DEBUG(IPW_DL_INFO | IPW_DL_SCAN,
			  "Scan completion watchdog resetting "
			  "adapter (%dms).\n",
			  jiffies_to_msecs(IPW_SCAN_CHECK_WATCHDOG));
		if (!(priv->status & STATUS_EXIT_PENDING))
			ipw_down(priv);
	}
	mutex_unlock(&priv->mutex);
}

static int ipw_send_scan_abort(struct ipw_priv *priv)
{
	int rc = 0;
	struct ipw_rx_packet *res;
	struct ipw_host_cmd cmd = {
		.id = REPLY_SCAN_ABORT_CMD,
		.meta.flags = CMD_WANT_SKB,
	};

	/* If there isn't a scan actively going on in the hardware
	 * then we are in between scan bands and not actually
	 * actively scanning, so don't send the abort command */
	if (!(priv->status & STATUS_SCAN_HW)) {
		priv->status &= ~STATUS_SCAN_ABORTING;
		return 0;
	}

	rc = ipw_send_cmd(priv, &cmd);
	if (rc) {
		priv->status &= ~STATUS_SCAN_ABORTING;
		return rc;
	}

	res = (struct ipw_rx_packet *)cmd.meta.u.skb->data;
	if (res->u.status != CAN_ABORT_STATUS) {
		/* The scan abort will return 1 for success or
		 * 2 for "failure".  A failure condition can be
		 * due to simply not being in an active scan which
		 * can occur if we send the scan abort before we
		 * the microcode has notified us that a scan is
		 * completd. */
		IPW_DEBUG_INFO("SCAN_ABORT returned %d.\n", res->u.status);
		priv->status &= ~(STATUS_SCAN_ABORTING | STATUS_SCAN_HW);
	}

	dev_kfree_skb_any(cmd.meta.u.skb);

	return rc;
}

#define MAX_UCODE_BEACON_INTERVAL	1024
#define INTEL_CONN_LISTEN_INTERVAL	0xA

static u16 ipw_adjust_beacon_interval(u16 beacon_val)
{
	u16 new_val = 0;
	u16 beacon_factor = 0;

	beacon_factor =
	    (beacon_val +
	     MAX_UCODE_BEACON_INTERVAL) / MAX_UCODE_BEACON_INTERVAL;
	new_val = beacon_val / beacon_factor;

	return new_val;
}

static int ipw_setup_rxon_timing(struct ipw_priv *priv,
				 struct ipw_associate *associate)
{
	int rc = 0;
	u64 interval_tm_unit;
	u64 tsf, result;

	priv->rxon_timing.timestamp.dw[1] = associate->assoc_tsf_msw;
	priv->rxon_timing.timestamp.dw[0] = associate->assoc_tsf_lsw;

	priv->rxon_timing.listenInterval = INTEL_CONN_LISTEN_INTERVAL;

	tsf = associate->assoc_tsf_msw;
	tsf = tsf << 32 | associate->assoc_tsf_lsw;

	if (priv->ieee->iw_mode == IW_MODE_INFRA) {
		if (associate->beacon_interval == 0) {
			priv->rxon_timing.beaconInterval = 100;
			priv->rxon_timing.beaconTimerInitVal = 102400;
		} else {
			priv->rxon_timing.beaconInterval =
			    associate->beacon_interval;
			priv->rxon_timing.beaconInterval =
			    ipw_adjust_beacon_interval(priv->rxon_timing.
						       beaconInterval);

		}

		priv->rxon_timing.atimWindow = 0;
	} else {
		priv->rxon_timing.beaconInterval =
		    ipw_adjust_beacon_interval(associate->beacon_interval);

		priv->rxon_timing.atimWindow = associate->atim_window;
	}

	interval_tm_unit = (priv->rxon_timing.beaconInterval * 1024);
	result = do_div(tsf, interval_tm_unit);
	priv->rxon_timing.beaconTimerInitVal =
	    (u32) ((u64) interval_tm_unit - result);

	IPW_DEBUG_ASSOC("beacon interval %d beacon timer %d beacon tim %d\n",
			priv->rxon_timing.beaconInterval,
			priv->rxon_timing.beaconTimerInitVal,
			priv->rxon_timing.atimWindow);
	return rc;
}

/*
  fill in the supported rate in IE fiels
  return : set the bit for each supported rate insert in ie
*/
static u16 ipw_supported_rate_to_ie(struct ieee80211_info_element *ie,
				    u16 supported_rate,
				    u16 basic_rate, int max_count)
{
	u16 ret_rates = 0, bit;
	int i;
	u8 *rates;

	rates = &ie->data[0];

	for (bit = 1, i = 0; i < IPW_MAX_RATES; i++, bit <<= 1) {
		if (bit & supported_rate) {
			ret_rates |= bit;
			rates[ie->len++] = ipw_rate_index2ieee(i) |
			    ((bit & basic_rate) ? 0x80 : 0x00);
			if (ie->len >= max_count)
				break;
			//todoG for IBSS return only cck rates only in the first ie
		}
	}

	return ret_rates;
}

static void *ieee80211_next_info_element(struct ieee80211_info_element
					 *info_element)
{
	return &info_element->data[info_element->len];
}

static int ipw_add_power_capability(struct ipw_priv *priv,
				    struct ieee80211_info_element *info_element,
				    size_t left)
{
	const struct ieee80211_channel *channel =
	    ieee80211_get_channel(priv->ieee, priv->channel);

	if (left < sizeof(*info_element) + 2)
		return 0;

	IPW_DEBUG_11H("Setting minimum power: %d and maximum power: %d\n",
		      0, channel->max_power);

	info_element->id = MFIE_TYPE_POWER_CAPABILITY;
	info_element->len = 2;
	info_element->data[0] = 0;	/* chan_info->min_power; */
	info_element->data[1] = channel->max_power;

	return sizeof(*info_element) + info_element->len;
}

static int ipw_add_supported_channels(struct ipw_priv *priv, struct ieee80211_info_element
				      *info_element, size_t left)
{
	int i = 0;

	if (!(priv->assoc_network->mode & IEEE_A))
		return 0;

	/* We know we'll have at least one first_channel / count tuple */
	left -= sizeof(*info_element);
	if (left < 0)
		return 0;

	/* Per 802.11h 17.3.8.3.3, there are three bands */
	if (!(ieee80211_get_channel_flags(priv->ieee, 36) &
	      IEEE80211_CH_INVALID)) {
		if (left < 2)
			return 0;
		left -= 2;
		info_element->data[i] = 36;
		info_element->data[i + 1] = 4;
		i += 2;
	}

	if (!(ieee80211_get_channel_flags(priv->ieee, 52) &
	      IEEE80211_CH_INVALID)) {
		if (left < 2)
			return 0;
		left -= 2;
		info_element->data[i] = 52;
		info_element->data[i + 1] = 4;
		i += 2;
	}

	if (!(ieee80211_get_channel_flags(priv->ieee, 149) &
	      IEEE80211_CH_INVALID)) {
		if (left < 2)
			return 0;
		left -= 2;
		info_element->data[i] = 149;
		info_element->data[i + 1] = 4;
		i += 2;
	}

	/* This band was added in 802.11h */
	if (!(ieee80211_get_channel_flags(priv->ieee, 100) &
	      IEEE80211_CH_INVALID)) {
		if (left < 2)
			return 0;
		left -= 2;
		info_element->data[i] = 100;
		info_element->data[i + 1] = 11;
		i += 2;
	}

	if (!i)
		return 0;

	info_element->id = MFIE_TYPE_SUPP_CHANNELS;
	info_element->len = i;

	IPW_DEBUG_11H("Advertised support of %d bands.\n", i / 2);

	return sizeof(*info_element) + info_element->len;
}

static int ipw_fill_association_req(struct ipw_priv *priv,
				    struct ieee80211_hdr *hdr, int left)
{
	struct ieee80211_info_element *info_element;
	struct ieee80211_assoc_request *frame =
	    (struct ieee80211_assoc_request *)hdr;
	u16 ret_rates;
	u16 capability = 0;
	int len = 0;

	/* Make sure there is enough space for the association request,
	 * two mandatory IEs and the data */
	left -= (sizeof(struct ieee80211_assoc_request) +
		 sizeof(struct ieee80211_info_element) * 2 +
		 priv->essid_len + IPW_SUPPORTED_RATES_IE_LEN);
	if (left < 0)
		return 0;

	frame->header.frame_ctl = IEEE80211_FTYPE_MGMT |
	    IEEE80211_STYPE_ASSOC_REQ;
	memcpy(frame->header.addr1, priv->ieee->bssid, ETH_ALEN);
	memcpy(frame->header.addr2, priv->mac_addr, ETH_ALEN);
	memcpy(frame->header.addr3, priv->ieee->bssid, ETH_ALEN);
	frame->header.seq_ctl = 0;

	capability =
	    WLAN_CAPABILITY_ESS |
	    WLAN_CAPABILITY_CF_POLLABLE |
	    WLAN_CAPABILITY_CF_POLL_REQUEST |
	    WLAN_CAPABILITY_PRIVACY |
	    WLAN_CAPABILITY_SHORT_PREAMBLE |
	    WLAN_CAPABILITY_SHORT_SLOT_TIME |
	    WLAN_CAPABILITY_DSSS_OFDM | WLAN_CAPABILITY_SPECTRUM_MGMT;

#ifdef CONFIG_IPW3945_QOS
	if (priv->qos_data.qos_enable)
		capability |= WLAN_CAPABILITY_QOS;
#endif

	frame->capability = priv->assoc_request.capability & capability;

	if (priv->config & CFG_PREAMBLE_LONG)
		frame->capability &= ~WLAN_CAPABILITY_SHORT_PREAMBLE;

	frame->listen_interval = 10;

	info_element = frame->info_element;

	/* fill in ssid broadcast */
	info_element->id = MFIE_TYPE_SSID;
	info_element->len = priv->essid_len;
	memcpy(info_element->data, priv->essid, priv->essid_len);

	/* Advance to next IE... */
	info_element = ieee80211_next_info_element(info_element);

	/* fill in supported rate */
	info_element->id = MFIE_TYPE_RATES;
	info_element->len = 0;
	ret_rates = ipw_supported_rate_to_ie(info_element, priv->active_rate,
					     priv->active_rate_basic,
					     IPW_SUPPORTED_RATES_IE_LEN);

	ret_rates = ~ret_rates & priv->active_rate;

	/* Account for the size we know... */
	len = sizeof(struct ieee80211_assoc_request) +
	    sizeof(struct ieee80211_info_element) * 2 +
	    priv->essid_len + info_element->len;
	/* left is set at start of function for to reflect where we are
	 * now */

	if (ret_rates) {
		/* Now see how much space is left.... */
		if (left < sizeof(struct ieee80211_info_element) +
		    IPW_SUPPORTED_RATES_IE_LEN)
			return 0;

		info_element = ieee80211_next_info_element(info_element);
		info_element->id = MFIE_TYPE_RATES_EX;
		info_element->len = 0;
		ipw_supported_rate_to_ie(info_element,
					 ret_rates, priv->active_rate_basic,
					 IPW_SUPPORTED_RATES_IE_LEN);
		if (info_element->len > 0) {
			/* account for this IE */
			len += sizeof(struct ieee80211_info_element) +
			    info_element->len;
			left -= sizeof(struct ieee80211_info_element) +
			    info_element->len;
		}
	}

	if (priv->ieee->wpa_ie_len) {
		/* Advance to WPA IE... */
		info_element = ieee80211_next_info_element(info_element);

		if (priv->ieee->wpa_ie_len >= 6 &&
		    priv->ieee->wpa_ie[2] == 0x00 &&
		    priv->ieee->wpa_ie[3] == 0x50 &&
		    priv->ieee->wpa_ie[4] == 0xf2 &&
		    priv->ieee->wpa_ie[5] == 0x01)
			info_element->id = MFIE_TYPE_GENERIC;
		else if (priv->ieee->wpa_ie[0] == MFIE_TYPE_RSN)
			info_element->id = MFIE_TYPE_RSN;
		else {
			IPW_DEBUG_ASSOC("bad wpa/rsn ie\n");
			goto fill_end;
		}

		info_element->len = priv->ieee->wpa_ie_len - 2;
		memcpy(info_element->data, priv->ieee->wpa_ie + 2,
		       info_element->len);

		len += sizeof(struct ieee80211_info_element) +
		    info_element->len;
		left -= sizeof(struct ieee80211_info_element) +
		    info_element->len;
	}

	/* 802.11h */
	if (priv->assoc_network->capability & WLAN_CAPABILITY_SPECTRUM_MGMT) {
		int size;
		/* If spectrum management required, we must provide the
		 * Power Capability and Suspported Channels */

		IPW_DEBUG_11H("Adding spectrum management IEs to "
			      "association (len=%d, ssid len=%d).\n",
			      len, priv->essid_len);

		info_element = ieee80211_next_info_element(info_element);
		size = ipw_add_power_capability(priv, info_element, left);
		left -= size;
		len += size;

		info_element = ieee80211_next_info_element(info_element);
		size = ipw_add_supported_channels(priv, info_element, left);
		left -= size;
		len += size;
	}

      fill_end:

	return len;
}

/*
  fill in all required fields and ie for probe request frame
*/
static int ipw_fill_probe_req(struct ipw_priv *priv,
			      struct ieee80211_probe_request *frame, int left,
			      int is_direct)
{
	struct ieee80211_info_element *info_element;
	u16 ret_rates;
	int len = 0;

	/* Make sure there is enough space for the probe request,
	 * two mandatory IEs and the data */
	left -= sizeof(struct ieee80211_probe_request);
	if (left < 0)
		return 0;
	len += sizeof(struct ieee80211_probe_request);

	frame->header.frame_ctl = IEEE80211_FTYPE_MGMT |
	    IEEE80211_STYPE_PROBE_REQ;
	memcpy(frame->header.addr1, BROADCAST_ADDR, ETH_ALEN);
	memcpy(frame->header.addr2, priv->mac_addr, ETH_ALEN);
	memcpy(frame->header.addr3, BROADCAST_ADDR, ETH_ALEN);
	frame->header.seq_ctl = 0;

	/* fill in our indirect SSID IE */
	/* ...next IE... */
	info_element = frame->info_element;
	left -= sizeof(struct ieee80211_info_element);
	if (left < 0)
		return 0;
	len += sizeof(struct ieee80211_info_element);
	/* ... fill it in... */
	info_element->id = MFIE_TYPE_SSID;
	info_element->len = 0;

	/* fill in our direct SSID IE... */
	if (is_direct) {
		/* ...next IE... */
		info_element = ieee80211_next_info_element(info_element);
		left -=
		    (sizeof(struct ieee80211_info_element) + priv->essid_len);
		if (left < 0)
			return 0;
		/* ... fill it in... */
		info_element->id = MFIE_TYPE_SSID;
		info_element->len = priv->essid_len;
		memcpy(info_element->data, priv->essid, priv->essid_len);
		len +=
		    sizeof(struct ieee80211_info_element) + info_element->len;
	}

	/* fill in supported rate */
	/* ...next IE... */
	info_element = ieee80211_next_info_element(info_element);
	left -= sizeof(struct ieee80211_info_element);
	if (left < 0)
		return 0;
	/* ... fill it in... */
	info_element->id = MFIE_TYPE_RATES;
	info_element->len = 0;
	ret_rates = ipw_supported_rate_to_ie(info_element, priv->active_rate,
					     priv->active_rate_basic, left);
	len += sizeof(struct ieee80211_info_element) + info_element->len;

	ret_rates = ~ret_rates & priv->active_rate;

	if (ret_rates == 0)
		goto fill_end;

	/* fill in supported extended rate */
	/* ...next IE... */
	info_element = ieee80211_next_info_element(info_element);
	left -= sizeof(struct ieee80211_info_element);
	if (left < 0)
		return 0;
	/* ... fill it in... */
	info_element->id = MFIE_TYPE_RATES_EX;
	info_element->len = 0;
	ipw_supported_rate_to_ie(info_element,
				 ret_rates, priv->active_rate_basic, left);
	if (info_element->len > 0)
		/* account for this IE */
		len += sizeof(struct ieee80211_info_element) +
		    info_element->len;

      fill_end:

	return len;
}

#define IEEE80211_ERP_PRESENT                  (0x01)
#define IEEE80211_ERP_USE_PROTECTION           (0x02)
#define IEEE80211_ERP_BARKER_PREAMBLE_MODE     (0x04)

static int ipw_fill_beacon_frame(struct ipw_priv *priv,
				 struct ieee80211_hdr *hdr, u8 * dest, int left)
{
	struct ieee80211_probe_response *frame = NULL;
	struct ieee80211_info_element *info_element = NULL;
	int len = 0;
	u16 ret_rates;

	if (!(priv->status & (STATUS_ASSOCIATED | STATUS_ASSOCIATING)) ||
	    (priv->ieee->iw_mode != IW_MODE_ADHOC))
		return 0;

	frame = (struct ieee80211_probe_response *)hdr;
	frame->header.frame_ctl = IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON;

	memcpy(frame->header.addr1, dest, ETH_ALEN);
	memcpy(frame->header.addr2, priv->mac_addr, ETH_ALEN);
	memcpy(frame->header.addr3, priv->assoc_request.bssid, ETH_ALEN);

	frame->header.seq_ctl = 0;

	/* fill the probe fields */
	frame->capability = priv->assoc_request.capability;
	frame->time_stamp[0] = priv->assoc_request.assoc_tsf_lsw;
	frame->time_stamp[1] = priv->assoc_request.assoc_tsf_msw;

	frame->beacon_interval = priv->assoc_request.beacon_interval;
	frame->capability |= WLAN_CAPABILITY_IBSS;
	frame->capability &= ~WLAN_CAPABILITY_SHORT_SLOT_TIME;

	info_element = frame->info_element;

	/* fill in ssid broadcast */
	info_element->id = MFIE_TYPE_SSID;
	info_element->len = priv->essid_len;
	memcpy(info_element->data, priv->essid, priv->essid_len);

	/* Account for the size we know... */
	len = sizeof(struct ieee80211_probe_response) +
	    sizeof(struct ieee80211_info_element) + priv->essid_len;

	/* Advance to next IE... */
	info_element = ieee80211_next_info_element(info_element);

	/*  atim window */
	info_element->id = MFIE_TYPE_IBSS_SET;
	info_element->data[0] = (u8) priv->assoc_request.atim_window;
	info_element->data[1] = 0x0;
	info_element->len = 2;

	len += sizeof(struct ieee80211_info_element) + info_element->len;

	/* Advance to next IE... */
	info_element = ieee80211_next_info_element(info_element);

	info_element->id = MFIE_TYPE_RATES;
	info_element->len = 0;
	ret_rates = ipw_supported_rate_to_ie(info_element, priv->active_rate,
					     priv->active_rate_basic,
					     IPW_SUPPORTED_RATES_IE_LEN);

	ret_rates = ~ret_rates & priv->active_rate;

	len += sizeof(struct ieee80211_info_element) + info_element->len;

	if (ret_rates != 0) {

		/* Now see how much space is left.... */
		if (left < sizeof(struct ieee80211_info_element) +
		    IPW_SUPPORTED_RATES_IE_LEN)
			return 0;

		info_element = ieee80211_next_info_element(info_element);
		info_element->id = MFIE_TYPE_RATES_EX;
		info_element->len = 0;
		ipw_supported_rate_to_ie(info_element,
					 ret_rates, priv->active_rate_basic,
					 IPW_SUPPORTED_RATES_IE_LEN);
		if (info_element->len > 0)
			/* account for this IE */
			len += sizeof(struct ieee80211_info_element) +
			    info_element->len;
	}

	/* add ERP present IE if not 11a or 11b */
	if (priv->assoc_request.ieee_mode == IPW_G_MODE) {
		info_element = ieee80211_next_info_element(info_element);

		info_element->id = MFIE_TYPE_ERP_INFO;
		info_element->len = 1;

		info_element->data[0] = priv->assoc_request.erp_value;

		if (info_element->data[0] & IEEE80211_ERP_BARKER_PREAMBLE_MODE) {
			frame->capability &= ~WLAN_CAPABILITY_SHORT_PREAMBLE;
		}
		len += sizeof(struct ieee80211_info_element) +
		    info_element->len;
	}
	/* add DS present IE */
	if (priv->assoc_request.ieee_mode != IPW_A_MODE) {
		info_element = ieee80211_next_info_element(info_element);

		info_element->id = MFIE_TYPE_DS_SET;
		info_element->len = 1;

		info_element->data[0] = priv->channel;

		len += sizeof(struct ieee80211_info_element) +
		    info_element->len;

	}

	return len;
}

static int ipw_send_beacon_cmd(struct ipw_priv *priv)
{
	struct ipw_frame *frame;
	struct ipw_tx_beacon_cmd *tx_beacon_cmd;
	int frame_size, rc;

	frame = ipw_get_free_frame(priv);

	if (!frame) {
		IPW_ERROR("Coult not obtain free frame buffer for beacon "
			  "command.\n");
		return -ENOMEM;
	}

	tx_beacon_cmd = (struct ipw_tx_beacon_cmd *)&frame->u;
	memset(tx_beacon_cmd, 0, sizeof(*tx_beacon_cmd));

	tx_beacon_cmd->tx.sta_id = 24;
	tx_beacon_cmd->tx.u.life_time = 0xFFFFFFFF;

	frame_size = ipw_fill_beacon_frame(priv,
					   tx_beacon_cmd->frame,
					   BROADCAST_ADDR,
					   sizeof(frame->u) -
					   sizeof(*tx_beacon_cmd));

	tx_beacon_cmd->tx.len = frame_size;

	if (!(priv->rxon.flags & RXON_FLG_BAND_24G_MSK)) {
		tx_beacon_cmd->tx.rate =
		    ipw_rate_get_lowest_plcp(priv->active_rate_basic & 0xFF0);

		if (tx_beacon_cmd->tx.rate <= 0)
			tx_beacon_cmd->tx.rate = R_6M;
	} else {
		tx_beacon_cmd->tx.rate =
		    ipw_rate_get_lowest_plcp(priv->active_rate_basic & 0xF);

		if (tx_beacon_cmd->tx.rate <= 0)
			tx_beacon_cmd->tx.rate = R_1M;
	}

	tx_beacon_cmd->tx.tx_flags = (TX_CMD_FLG_SEQ_CTL_MSK |
				      TX_CMD_FLG_TSF_MSK);

	tx_beacon_cmd->tx.supp_rates[0] = priv->active_rate >> 4;
	tx_beacon_cmd->tx.supp_rates[1] = priv->active_rate & 0xF;

	rc = ipw_send_cmd_pdu(priv, REPLY_TX_BEACON,
			      frame_size + sizeof(struct ipw_tx_beacon_cmd),
			      tx_beacon_cmd);

	ipw_free_frame(priv, frame);

	return rc;
}

#define BEACON_JIFFIES(x) msecs_to_jiffies(x->assoc_network->beacon_interval)

static int ipw_post_associate(struct ipw_priv *priv, u16 assoc_id)
{
	struct daemon_rx_config *rxon = &priv->rxon;
	int rc = 0;

	if (!(priv->status & STATUS_ASSOCIATING)) {
		IPW_WARNING("association process canceled\n");
		return 0;
	}

	memset(&priv->rxon_timing, 0, sizeof(struct ipw_rxon_time_cmd));
	ipw_setup_rxon_timing(priv, &priv->assoc_request);
	ipw_send_rxon_timing(priv, &priv->rxon_timing);

	rxon->filter_flags |= RXON_FILTER_ASSOC_MSK;
	rxon->assoc_id = assoc_id;
	rxon->beacon_interval = priv->assoc_request.beacon_interval;
/*	switch (priv->ieee->sec.level) {
	case SEC_LEVEL_1:
	rxon->flags |= */
	rc = ipw_rxon_call(priv, 0);
	if (rc)
		return rc;

	switch (priv->ieee->iw_mode) {
	case IW_MODE_INFRA:
		ipw_rate_scale_rxon_handle(priv, AP_ID);
		memcpy(priv->ieee->bssid, priv->bssid, ETH_ALEN);

		/* Kick off roaming watchdog timer */
		if (!(priv->config & CFG_NO_ROAMING))
			mod_timer(&priv->roaming_wdt, jiffies +
				  priv->roaming_threshold *
				  BEACON_JIFFIES(priv));

		break;

	case IW_MODE_ADHOC:
		memcpy(priv->ieee->bssid, priv->bssid, ETH_ALEN);

		/* clear out the station table */
		priv->num_stations = 0;

		ipw_rxon_add_station(priv, BROADCAST_ADDR, 0);
		ipw_rxon_add_station(priv, priv->bssid, 0);
		ipw_rate_scale_rxon_handle(priv, STA_ID);
		ipw_send_beacon_cmd(priv);

		break;
	}

	/* Kick off disassociate watchdog timer */
	mod_timer(&priv->disassociate_wdt, jiffies +
		  priv->missed_beacon_threshold * BEACON_JIFFIES(priv));

	priv->status &= ~STATUS_ASSOCIATING;
	priv->status |= STATUS_ASSOCIATED;
	queue_work(priv->workqueue, &priv->link_up);

	return 0;
}

static int ipw_send_associate(struct ipw_priv *priv,
			      struct ipw_associate *associate)
{
	struct ieee80211_auth frame = {
		.header.frame_ctl = IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_AUTH,
		.transaction = 1,
	};
	int rc = 0;
	struct daemon_rx_config *rxon = &priv->rxon;

	frame.algorithm = (associate->auth_type == AUTH_SHARED_KEY) ?
	    WLAN_AUTH_SHARED_KEY : WLAN_AUTH_OPEN;

	memcpy(frame.header.addr1, priv->bssid, ETH_ALEN);
	memcpy(frame.header.addr2, priv->mac_addr, ETH_ALEN);
	memcpy(frame.header.addr3, priv->ieee->bssid, ETH_ALEN);

	if (priv->status & STATUS_ASSOCIATED) {
		IPW_DEBUG_ASSOC("Attempting to send associate while already "
				"associated.\n");
		return -EINVAL;
	}

	memcpy(priv->ieee->bssid, priv->bssid, ETH_ALEN);
	memcpy(rxon->bssid_addr, associate->bssid, ETH_ALEN);
	rxon->channel = associate->channel;
	rxon->flags =
	    (RXON_FLG_DIS_DIV_MSK | RXON_FLG_ANT_A_MSK | RXON_FLG_ANT_B_MSK);

	rxon->filter_flags = RXON_FILTER_ACCEPT_GRP_MSK;	// todoG is this needed

	if (associate->ieee_mode == IPW_A_MODE)
		rxon->flags |= RXON_FLG_SHORT_SLOT_MSK;
	else
		rxon->flags |=
		    (RXON_FLG_BAND_24G_MSK | RXON_FLG_AUTO_DETECT_MSK);

	switch (priv->ieee->iw_mode) {
	case IW_MODE_INFRA:
		rxon->dev_type = RXON_DEV_TYPE_ESS;
		break;

	case IW_MODE_ADHOC:
		rxon->dev_type = RXON_DEV_TYPE_IBSS;
		rxon->filter_flags |= RXON_FILTER_BCON_AWARE_MSK;
		break;

	case IW_MODE_MONITOR:
		rxon->dev_type = RXON_DEV_TYPE_SNIFFER;
		rxon->filter_flags = (1 << 0) | (1 << 1) |
		    (1 << 2) | (1 << 3) | (1 << 4);
		break;

	default:
		return -EINVAL;
	}

	if (associate->ieee_mode == IPW_A_MODE) {
		priv->rxon.flags &=
		    ~(RXON_FLG_BAND_24G_MSK | RXON_FLG_AUTO_DETECT_MSK |
		      RXON_FLG_CCK_MSK);
		priv->rxon.flags |= RXON_FLG_SHORT_SLOT_MSK;
	} else {
		priv->rxon.flags &= ~RXON_FLG_SHORT_SLOT_MSK;
		priv->rxon.flags |= RXON_FLG_BAND_24G_MSK;
		priv->rxon.flags |= RXON_FLG_AUTO_DETECT_MSK;
		priv->rxon.flags &= ~RXON_FLG_CCK_MSK;
	}

	if ((associate->capability & WLAN_CAPABILITY_SHORT_PREAMBLE))
		priv->rxon.flags |= RXON_FLG_SHORT_PREAMBLE_MSK;
	else
		priv->rxon.flags &= ~RXON_FLG_SHORT_PREAMBLE_MSK;

	if ((priv->rxon.flags & RXON_FLG_BAND_24G_MSK) &&
	    (associate->ieee_mode == IPW_G_MODE)) {
		if (associate->capability & WLAN_CAPABILITY_SHORT_SLOT_TIME)
			priv->rxon.flags |= RXON_FLG_SHORT_SLOT_MSK;
		else
			priv->rxon.flags &= ~RXON_FLG_SHORT_SLOT_MSK;

		if (associate->erp_value & IEEE80211_ERP_USE_PROTECTION)
			priv->rxon.flags |= RXON_FLG_TGG_PROTECT_MSK;
		else
			priv->rxon.flags &= ~RXON_FLG_TGG_PROTECT_MSK;
	}

	priv->rxon.cck_basic_rates =
	    ((priv->active_rate_basic & 0xF) | R_1M_MSK);
	priv->rxon.ofdm_basic_rates =
	    ((priv->active_rate_basic >> 4) | R_6M_MSK);

	if ((priv->active_rate_basic & 0xF) == 0)
		priv->rxon.cck_basic_rates =
		    R_1M_MSK | R_2M_MSK | R_5_5M_MSK | R_11M_MSK;
	if (priv->active_rate_basic >> 4 == 0)
		priv->rxon.ofdm_basic_rates = R_6M_MSK | R_12M_MSK | R_24M_MSK;

	rc = ipw_rxon_call(priv, 0);
	if (rc)
		return rc;

	if (rxon->dev_type == RXON_DEV_TYPE_SNIFFER)
		return 0;

	if (rxon->dev_type == RXON_DEV_TYPE_IBSS) {
		rc = ipw_post_associate(priv, 0);
		priv->status &= ~STATUS_ASSOCIATING;
		priv->status |= STATUS_ASSOCIATED;

		return rc;
	} else {
		if (ipw_rxon_add_station(priv, priv->bssid, 1) ==
		    IPW_INVALID_STATION) {
			IPW_WARNING("Could not add STA " MAC_FMT "\n",
				    MAC_ARG(priv->bssid));
			rc = -EIO;
		} else {
			unsigned long flags;
			spin_lock_irqsave(&priv->lock, flags);
			rc = ieee80211_tx_frame(priv->ieee,
						(struct ieee80211_hdr *)&frame,
						sizeof(frame));
			spin_unlock_irqrestore(&priv->lock, flags);
		}
	}

	ipw_update_rate_scaling(priv, associate->ieee_mode);
	if (rc) {
		priv->status &= ~STATUS_ASSOCIATING;
		priv->status &= ~STATUS_ASSOCIATED;
	}

	return rc;
}

/* TODO:  Look at scheduling this callback outside of ISR so we can switch
 * the daemon_lock spin_lock to not have to disable interrutps */
static int ipw_daemon_cmd_callback(struct ipw_priv *priv,
				   struct ipw_cmd *cmd, struct sk_buff *skb)
{
	struct list_head *element;
	struct ipw_daemon_cmd_list *daemon_cmd;
	unsigned long flags;

	BUG_ON(!skb);

	IPW_DEBUG_DAEMON(":: #%02X completed by adapter.\n", cmd->hdr.cmd);

	spin_lock_irqsave(&priv->daemon_lock, flags);

	if (list_empty(&priv->daemon_free_list)) {
		IPW_DEBUG_DAEMON("No room on free list...\n");
		spin_unlock_irqrestore(&priv->daemon_lock, flags);
		return 1;	/* We didn't cache the SKB */
	}

	element = priv->daemon_free_list.next;
	list_del(element);

	daemon_cmd = list_entry(element, struct ipw_daemon_cmd_list, list);

	daemon_cmd->cmd.cmd = cmd->hdr.cmd;
	daemon_cmd->cmd.data_len = cmd->meta.len;
	daemon_cmd->cmd.flags = cmd->meta.flags & CMD_DAEMON_MASK;
	daemon_cmd->skb_resp = skb;
	daemon_cmd->cmd.token = cmd->meta.token;
	list_add_tail(&daemon_cmd->list, &priv->daemon_out_list);

	spin_unlock_irqrestore(&priv->daemon_lock, flags);

	wake_up_interruptible_all(&priv->wait_daemon_out_queue);

	/* We did cache the SKB, so tell caller not to free it */
	return 0;
}

static int ipw_handle_daemon_request_info(struct ipw_priv *priv,
					  struct daemon_cmd_hdr *daemon_cmd)
{
	struct daemon_request_info *request = (void *)daemon_cmd->data;
	s32 *temperature = (void *)daemon_cmd->data;

	switch (request->request) {
	case DAEMON_REQUEST_TEMPERATURE:
		*temperature = ipw_read32(priv, CSR_UCODE_DRV_GP2);
		daemon_cmd->data_len = sizeof(*temperature);
		break;

	case DAEMON_REQUEST_EEPROM:
		memcpy(daemon_cmd->data, priv->eeprom, EEPROM_IMAGE_SIZE);
		daemon_cmd->data_len = EEPROM_IMAGE_SIZE;
		break;

	default:
		daemon_cmd->data_len = 0;
		break;
	}

	return 0;
}

static int ipw_send_power_mode(struct ipw_priv *priv, u32 mode)
{
	u32 final_mode = mode;
	int rc = 0;
	unsigned long flags;
	struct ipw_powertable_cmd cmd;

	/* If on battery, set to 3, if AC set to CAM, else user
	 * level */
	switch (mode) {
	case IPW_POWER_BATTERY:
		final_mode = IPW_POWER_INDEX_3;
		break;
	case IPW_POWER_AC:
		final_mode = IPW_POWER_MODE_CAM;
		break;
	default:
		final_mode = mode;
		break;
	}

	ipw_update_power_cmd(priv, &cmd, final_mode);

	rc = ipw_send_cmd_pdu(priv, POWER_TABLE_CMD, sizeof(cmd), &cmd);

	spin_lock_irqsave(&priv->lock, flags);

	if (final_mode == IPW_POWER_MODE_CAM) {
		priv->status &= ~STATUS_POWER_PMI;
	} else {
		priv->status |= STATUS_POWER_PMI;
	}

	spin_unlock_irqrestore(&priv->lock, flags);
	return rc;
}

static void ipw_send_supported_rates(struct ipw_priv *priv,
				     struct ipw_supported_rates *rates)
{
	int index, i;
	u8 rate, basic;

	priv->active_rate = 0;
	priv->active_rate_basic = 0;
	for (i = 0; i < rates->num_rates; i++) {
		rate = rates->supported_rates[i];
		if (rate & IEEE80211_BASIC_RATE_MASK)
			basic = 1;
		else
			basic = 0;
		rate &= ~IEEE80211_BASIC_RATE_MASK;
		index = ipw_rate_ieee2index(rate);
		if (index != -1) {
			priv->active_rate |= (1 << index);
			if (basic == 1)
				priv->active_rate_basic |= (1 << index);
		}
	}
}

static void ipw_bg_calibrated_work(void *data)
{
	struct ipw_priv *priv = data;
	int rc = 0;
	mutex_lock(&priv->mutex);

	IPW_DEBUG_INFO("CALIBRATED state set by daemon.\n");
	priv->status |= STATUS_CALIBRATE;

	if (!priv->netdev_registered) {
		rc = register_netdev(priv->net_dev);
		if (rc) {
			IPW_ERROR("Failed to register network "
				  "device (error %d) - Scheduling re-init "
				  "in 5 seconds.\n", rc);
			queue_delayed_work(priv->workqueue, &priv->down,
					   5 * HZ);
			mutex_unlock(&priv->mutex);
			return;
		}

		priv->netdev_registered = 1;

		/* For some reason, this card comes up with IFF_RUNNING set.
		 * If you associate and disassociate, it resets that bit
		 * correctly... so force a netif_carrier_off here to
		 * simulate that behavior. */
		netif_carrier_off(priv->net_dev);
	}

	init_supported_rates(priv, &priv->rates);
	ipw_send_supported_rates(priv, &priv->rates);

#ifdef CONFIG_IPW3945_QOS
	ipw_qos_activate(priv, NULL);
#endif

	ipw_send_power_mode(priv, IPW_POWER_LEVEL(priv->power_mode));

	IPW_DEBUG_RADIO("connection mode %0x  modulation %0X  band %0x \n",
			priv->ieee->mode, priv->ieee->modulation,
			priv->ieee->freq_band);

	/* Initialize our rx_config data */
	ipw_connection_init_rx_config(priv);
	memcpy(priv->rxon.node_addr, priv->net_dev->dev_addr, ETH_ALEN);

	/* Configure BT coexistence */
	ipw_send_bt_config(priv);

	/* Configure the adapter for unassociated operation */
	ipw_rxon_call(priv, 0);

	/* Add the broadcast address so we can send probe requests */
	ipw_rxon_add_station(priv, BROADCAST_ADDR, 0);

	/* At this point, the NIC is initialized and operational */
	priv->notif_missed_beacons = 0;
	priv->status |= STATUS_READY;

	ipw_update_link_led(priv);

	/* If configured to try and auto-associate, or if we've received
	 * sufficient configuration information at this point, then kick
	 * off a scan */
	if ((priv->config & (CFG_ASSOCIATE | CFG_STATIC_ESSID |
			     CFG_STATIC_CHANNEL | CFG_STATIC_BSSID)) ||
	    (priv->status & STATUS_SCAN_PENDING))
		ipw_scan_initiate(priv, priv->ieee->freq_band, 0);

	mutex_unlock(&priv->mutex);
}

static void ipw_handle_daemon_set_state(struct ipw_priv *priv,
					struct daemon_cmd_hdr *cmd)
{
	struct daemon_set_state *set = (void *)cmd->data;

	mutex_lock(&priv->mutex);

	switch (set->state) {
	case DAEMON_DRIVER_STATE_UNINIT:
		IPW_DEBUG_INFO("UNINIT state requested by daemon.\n");
		queue_work(priv->workqueue, &priv->down);
		break;

	case DAEMON_DRIVER_STATE_INIT:
		IPW_DEBUG_INFO("INIT state requested by daemon.\n");
		queue_work(priv->workqueue, &priv->up);
		break;

	case DAEMON_DRIVER_STATE_CALIBRATED:
		IPW_DEBUG_INFO("CALIBRATED state requested by daemon.\n");
		queue_work(priv->workqueue, &priv->calibrated_work);
		break;
	}

	mutex_unlock(&priv->mutex);
}

/* Initialize 80211's geo/channel info based from daemon */
static void ipw_init_geos(struct ipw_priv *priv,
			  struct daemon_regulatory_info *info)
{
	struct daemon_channel_info *ch;
	struct ieee80211_geo *geo;
	struct ieee80211_channel *geo_ch;
	int i = 0, bg, a;

	geo = kzalloc(sizeof(*geo), GFP_KERNEL);
	if (!geo) {
		IPW_ERROR("Insufficient memory to work with channels.\n");
		return;
	}

	for (i = 0, a = 0, bg = 0; i < info->count; i++) {
		ch = &info->channel_info[i];
		if (ch->flags & DAEMON_A_BAND) {
			if (ch->channel < IEEE80211_52GHZ_MIN_CHANNEL ||
			    ch->channel > IEEE80211_52GHZ_MAX_CHANNEL) {
				IPW_WARNING
				    ("5.2Ghz channel %d not supported by "
				     "ieee80211 subsystem.\n", ch->channel);
				continue;
			}

			a = geo->a_channels++;
			geo_ch = &geo->a[a];
		} else {
			if (ch->channel < IEEE80211_24GHZ_MIN_CHANNEL ||
			    ch->channel > IEEE80211_24GHZ_MAX_CHANNEL) {
				IPW_WARNING
				    ("2.4Ghz channel %d not supported by "
				     "ieee80211 subsystem.\n", ch->channel);
				continue;
			}

			bg = geo->bg_channels++;
			geo_ch = &geo->bg[bg];
		}

		geo_ch->freq = ieee80211chan2mhz(ch->channel);
		geo_ch->channel = ch->channel;
		geo_ch->max_power = ch->max_txpower;

		if (!(ch->flags & DAEMON_IBSS_ALLOWED))
			geo_ch->flags |= IEEE80211_CH_NO_IBSS;

		if (!(ch->flags & DAEMON_ACTIVE_ALLOWED))
			geo_ch->flags |= IEEE80211_CH_PASSIVE_ONLY;

		if (ch->flags & DAEMON_RADAR_DETECT)
			geo_ch->flags |= IEEE80211_CH_RADAR_DETECT;

		if (ch->max_txpower > priv->max_channel_txpower_limit)
			priv->max_channel_txpower_limit = ch->max_txpower;
	}

	if ((geo->a_channels == 0) && priv->is_abg) {
		printk(KERN_INFO DRV_NAME
		       ": Incorrectly detected BG card as ABG.  Please send "
		       "your PCI ID 0x%04X:0x%04X to maintainer.\n",
		       priv->pci_dev->device, priv->pci_dev->subsystem_device);
		priv->ieee->mode &= ~IEEE_A;
		priv->ieee->freq_band &= ~IEEE80211_52GHZ_BAND;
		priv->is_abg = 0;
	}

	if (priv->is_abg)
		strncpy(geo->name, "ABG", sizeof(geo->name));
	else
		strncpy(geo->name, "xBG", sizeof(geo->name));

	ieee80211_set_geo(priv->ieee, geo);

	if ((priv->config & CFG_STATIC_CHANNEL) &&
	    !ieee80211_is_valid_channel(priv->ieee, priv->channel)) {
		IPW_WARNING("Invalid channel configured. Resetting to ANY.\n");
		priv->channel = 1;
		priv->config &= ~CFG_STATIC_CHANNEL;
	}

	printk(KERN_INFO DRV_NAME ": Detected geography %s (%d 802.11bg "
	       "channels, %d 802.11a channels)\n",
	       priv->ieee->geo.name, priv->ieee->geo.bg_channels,
	       priv->ieee->geo.a_channels);

	priv->status |= STATUS_GEO_CONFIGURED;

	kfree(geo);
}

static void ipw_bg_daemon_cmd(void *data)
{
	struct ipw_priv *priv = data;
	struct list_head *element;
	struct ipw_host_cmd cmd = {
		.id = 0,
	};
	struct ipw_daemon_cmd_list *daemon_cmd;
	int rc = 0;
	unsigned long flags;

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	spin_lock_irqsave(&priv->daemon_lock, flags);

	while (!list_empty(&priv->daemon_in_list)) {
		element = priv->daemon_in_list.next;
		list_del(element);
		spin_unlock_irqrestore(&priv->daemon_lock, flags);

		daemon_cmd = list_entry(element, struct ipw_daemon_cmd_list,
					list);

		IPW_DEBUG_DAEMON(":: #%02X.\n", daemon_cmd->cmd.cmd);

		if (daemon_cmd->cmd.flags & DAEMON_FLAG_NIC_CMD) {
			IPW_DEBUG_DAEMON("Queuing daemon requested cmd "
					 "#%02X\n", daemon_cmd->cmd.cmd);
			cmd.id = daemon_cmd->cmd.cmd;
			cmd.len = daemon_cmd->cmd.data_len;
			cmd.meta.token = daemon_cmd->cmd.token;
			cmd.meta.flags = CMD_INDIRECT | CMD_ASYNC;

			if (daemon_cmd->cmd.flags & DAEMON_FLAG_HUGE)
				cmd.meta.flags |= CMD_SIZE_HUGE;

			cmd.meta.u.callback = &ipw_daemon_cmd_callback;
			cmd.data = daemon_cmd->cmd.data;
			printk_buf(IPW_DL_DAEMON, daemon_cmd->cmd.data,
				   daemon_cmd->cmd.data_len);

			spin_lock_irqsave(&priv->daemon_lock, flags);
			list_add(&daemon_cmd->list, &priv->daemon_free_list);
			spin_unlock_irqrestore(&priv->daemon_lock, flags);

			rc = ipw_send_cmd(priv, &cmd);

			spin_lock_irqsave(&priv->daemon_lock, flags);

			if (rc) {
				IPW_DEBUG_INFO("Command failed: %d\n", rc);
				list_del(&daemon_cmd->list);
				kfree(daemon_cmd);
			}

			/* If the command succeeded, the daemon_cmd will be freed
			 * in the callback handler. */

			continue;
		}

		switch (daemon_cmd->cmd.cmd) {
		case DAEMON_CMD_DONE:{
				struct daemon_cmd_done *done =
				    (void *)daemon_cmd->cmd.data;

				daemon_cmd->cmd.data_len = 0;
				spin_lock_irqsave(&priv->daemon_lock, flags);
				priv->daemon_cmd_rc = done->return_code;
				priv->status &= ~STATUS_DCMD_ACTIVE;
				list_add_tail(&daemon_cmd->list,
					      &priv->daemon_out_list);
				spin_unlock_irqrestore(&priv->daemon_lock,
						       flags);

				wake_up_interruptible_all(&priv->
							  wait_daemon_cmd_done);

				wake_up_interruptible_all(&priv->
							  wait_daemon_out_queue);

				break;
			}

		case DAEMON_ERROR:{
#ifdef CONFIG_IPW3945_DEBUG
				struct daemon_error *error =
				    (struct daemon_error *)
				    daemon_cmd->cmd.data;
				IPW_DEBUG_INFO("error received from daemon "
					       "for command %s (#%02X): %d\n",
					       get_cmd_string(error->
							      cmd_requested),
					       error->cmd_requested,
					       error->return_code);
#endif
				daemon_cmd->cmd.data_len = 0;
				spin_lock_irqsave(&priv->daemon_lock, flags);
				list_add_tail(&daemon_cmd->list,
					      &priv->daemon_out_list);
				spin_unlock_irqrestore(&priv->daemon_lock,
						       flags);

				wake_up_interruptible_all(&priv->
							  wait_daemon_out_queue);
			}
			break;

		case DAEMON_FRAME_TX:
			ieee80211_tx_frame(priv->ieee, (struct ieee80211_hdr *)
					   daemon_cmd->cmd.data,
					   daemon_cmd->cmd.data_len);
			daemon_cmd->cmd.data_len = 0;
			spin_lock_irqsave(&priv->daemon_lock, flags);
			list_add_tail(&daemon_cmd->list,
				      &priv->daemon_out_list);
			spin_unlock_irqrestore(&priv->daemon_lock, flags);

			wake_up_interruptible_all(&priv->wait_daemon_out_queue);
			break;

		case DAEMON_REGULATORY_INFO:{
				struct daemon_regulatory_info *info =
				    (struct daemon_regulatory_info *)
				    daemon_cmd->cmd.data;

				IPW_DEBUG_DAEMON("Initializing %d channels.\n",
						 info->count);

				if (info->count != 1)
					ipw_init_geos(priv, info);
				else {
					if (priv->channel ==
					    info->channel_info[0].channel)
						priv->actual_txpower_limit =
						    info->channel_info[0].
						    txpower;
				}

				daemon_cmd->cmd.data_len = 0;

				spin_lock_irqsave(&priv->daemon_lock, flags);
				list_add_tail(&daemon_cmd->list,
					      &priv->daemon_out_list);
				spin_unlock_irqrestore(&priv->daemon_lock,
						       flags);

				wake_up_interruptible_all(&priv->
							  wait_daemon_out_queue);
				break;
			}

		case DAEMON_REQUEST_INFO:{
				if (ipw_handle_daemon_request_info(priv,
								   &daemon_cmd->
								   cmd))
					daemon_cmd->cmd.data_len = 0;

				spin_lock_irqsave(&priv->daemon_lock, flags);
				list_add_tail(&daemon_cmd->list,
					      &priv->daemon_out_list);
				spin_unlock_irqrestore(&priv->daemon_lock,
						       flags);
				wake_up_interruptible_all(&priv->
							  wait_daemon_out_queue);

				break;
			}

		case DAEMON_SET_STATE:{
				ipw_handle_daemon_set_state(priv,
							    &daemon_cmd->cmd);
				/* No response bytes */
				daemon_cmd->cmd.data_len = 0;
				spin_lock_irqsave(&priv->daemon_lock, flags);
				list_add_tail(&daemon_cmd->list,
					      &priv->daemon_out_list);
				spin_unlock_irqrestore(&priv->daemon_lock,
						       flags);
				wake_up_interruptible_all(&priv->
							  wait_daemon_out_queue);
				break;
			}

		default:
			IPW_DEBUG_INFO("Command unhandled: %d\n",
				       daemon_cmd->cmd.cmd);
			kfree(daemon_cmd);
			break;
		}

		/* lock needs to be held at start of loop */
		spin_lock_irqsave(&priv->daemon_lock, flags);
	}

	spin_unlock_irqrestore(&priv->daemon_lock, flags);
}

static void ipw_bg_post_associate(void *data)
{
	struct ipw_priv *priv = data;

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	ipw_post_associate(priv, priv->assoc_request.assoc_id);
	mutex_unlock(&priv->mutex);
}

/* get info from our private copy of the eeprom data */
static int ipw_query_eeprom(struct ipw_priv *priv, u32 offset, u32 len,
			    u8 * buf)
{
	if (EEPROM_IMAGE_SIZE < (offset + len))
		return -1;

	memcpy(buf, &(priv->eeprom[offset]), len);

	return 0;
}

static void eeprom_parse_mac(struct ipw_priv *priv, u8 * mac)
{
	ipw_query_eeprom(priv, EEPROM_MAC_ADDRESS, 6, mac);
}

/*
 * Either the device driver (i.e. the host) or the firmware can
 * load eeprom data into the designated region in SRAM.  If neither
 * happens then the FW will shutdown with a fatal error.
 *
 * In order to signal the FW to load the EEPROM, the EEPROM_LOAD_DISABLE
 * bit needs region of shared SRAM needs to be non-zero.
 */
static int ipw_eeprom_init_sram(struct ipw_priv *priv)
{
	u16 *e = (u16 *) priv->eeprom;
	u32 r;
	int to;
	u32 gp = ipw_read32(priv, CSR_EEPROM_GP);
	u16 sz = EEPROM_IMAGE_SIZE;
	int rc;
	u16 addr;

	if ((gp & 0x00000007) == 0x00000000) {
		IPW_ERROR("EEPROM not found, EEPROM_GP=0x%08x", gp);
		return -ENOENT;
	}

	ipw_clear_bit(priv, CSR_EEPROM_GP, 0x00000180);
	for (addr = 0, r = 0; addr < sz; addr += 2) {
		ipw_write32(priv, CSR_EEPROM_REG, addr << 1);
		ipw_clear_bit(priv, CSR_EEPROM_REG, 0x00000002);
		rc = ipw_grab_restricted_access(priv);
		if (rc)
			return rc;

		for (to = 0; to < 10; to++) {
			r = ipw_read_restricted(priv, CSR_EEPROM_REG);
			if (r & 1)
				break;
			udelay(5);
		}

		ipw_release_restricted_access(priv);

		if (!(r & 1)) {
			IPW_ERROR("Time out reading EEPROM[%d]", addr);
			return -ETIMEDOUT;
		}

		e[addr / 2] = r >> 16;
	}

	return 0;
}

/************************** END ***********************/

static void ipw_remove_current_network(struct ipw_priv *priv)
{
	struct list_head *element, *safe;
	struct ieee80211_network *network = NULL;
	unsigned long flags;

	IPW_DEBUG_INFO("remove current station\n");
	spin_lock_irqsave(&priv->ieee->lock, flags);
	list_for_each_safe(element, safe, &priv->ieee->network_list) {
		network = list_entry(element, struct ieee80211_network, list);
		if (!memcmp(network->bssid, priv->bssid, ETH_ALEN)) {
//                      ieee80211_reset_network(network);
			list_del(element);
			list_add_tail(&network->list,
				      &priv->ieee->network_free_list);
		}
	}

	priv->assoc_network = NULL;
	spin_unlock_irqrestore(&priv->ieee->lock, flags);
}

/************************** RX-FUNCTIONS ****************************/
/*
 * Rx theory of operation
 *
 * The host allocates 32 DMA target addresses and passes the host address
 * to the firmware at register IPW_RFDS_TABLE_LOWER + N * RFD_SIZE where N is
 * 0 to 31
 *
 * Rx Queue Indexes
 * The host/firmware share two index registers for managing the Rx buffers.
 *
 * The READ index maps to the first position that the firmware may be writing
 * to -- the driver can read up to (but not including) this position and get
 * good data.
 * The READ index is managed by the firmware once the card is enabled.
 *
 * The WRITE index maps to the last position the driver has read from -- the
 * position preceding WRITE is the last slot the firmware can place a packet.
 *
 * The queue is empty (no good data) if WRITE = READ - 1, and is full if
 * WRITE = READ.
 *
 * During initialization the host sets up the READ queue position to the first
 * INDEX position, and WRITE to the last (READ - 1 wrapped)
 *
 * When the firmware places a packet in a buffer it will advance the READ index
 * and fire the RX interrupt.  The driver can then query the READ index and
 * process as many packets as possible, moving the WRITE index forward as it
 * resets the Rx queue buffers with new memory.
 *
 * The management in the driver is as follows:
 * + A list of pre-allocated SKBs is stored in ipw->rxq->rx_free.  When
 *   ipw->rxq->free_count drops to or below RX_LOW_WATERMARK, work is scheduled
 *   to replensish the ipw->rxq->rx_free.
 * + In ipw_rx_queue_replenish (scheduled) if 'processed' != 'read' then the
 *   ipw->rxq is replenished and the READ INDEX is updated (updating the
 *   'processed' and 'read' driver indexes as well)
 * + A received packet is processed and handed to the kernel network stack,
 *   detached from the ipw->rxq.  The driver 'processed' index is updated.
 * + The Host/Firmware ipw->rxq is replenished at tasklet time from the rx_free
 *   list. If there are no allocated buffers in ipw->rxq->rx_free, the READ
 *   INDEX is not incremented and ipw->status(RX_STALLED) is set.  If there
 *   were enough free buffers and RX_STALLED is set it is cleared.
 *
 *
 * Driver sequence:
 *
 * ipw_rx_queue_alloc()       Allocates rx_free
 * ipw_rx_queue_replenish()   Replenishes rx_free list from rx_used, and calls
 *                            ipw_rx_queue_restock
 * ipw_rx_queue_restock()     Moves available buffers from rx_free into Rx
 *                            queue, updates firmware pointers, and updates
 *                            the WRITE index.  If insufficient rx_free buffers
 *                            are available, schedules ipw_rx_queue_replenish
 *
 * -- enable interrupts --
 * ISR - ipw_rx()             Detach ipw_rx_mem_buffers from pool up to the
 *                            READ INDEX, detaching the SKB from the pool.
 *                            Moves the packet buffer from queue to rx_used.
 *                            Calls ipw_rx_queue_restock to refill any empty
 *                            slots.
 * ...
 *
 */

static inline int ipw_rx_queue_space(struct ipw_rx_queue *q)
{
	int s = q->read - q->write;
	if (s <= 0)
		s += RX_QUEUE_SIZE;
	s -= 2;			// keep some buffer to not confuse full and empty queue
	if (s < 0)
		s = 0;
	return s;
}

static int ipw_rx_queue_update_write_ptr(struct ipw_priv *priv,
					 struct ipw_rx_queue *q)
{
	u32 reg = 0;
	int rc = 0;
	unsigned long flags;

	spin_lock_irqsave(&q->lock, flags);

	if (q->need_update == 0)
		goto exit_unlock;

	if (priv->status & STATUS_POWER_PMI) {
		reg = ipw_read32(priv, CSR_UCODE_DRV_GP1);

		if (reg & CSR_UCODE_DRV_GP1_BIT_MAC_SLEEP) {
			ipw_set_bit(priv, CSR_GP_CNTRL,
				    CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
			goto exit_unlock;
		}

		rc = ipw_grab_restricted_access(priv);
		if (rc)
			goto exit_unlock;

		ipw_write_restricted(priv, FH_RCSR_WPTR(0), q->write & ~0x7);
		ipw_release_restricted_access(priv);
	} else {
		ipw_write32(priv, FH_RCSR_WPTR(0), q->write & ~0x7);
	}

	q->need_update = 0;

      exit_unlock:
	spin_unlock_irqrestore(&q->lock, flags);
	return rc;
}

/*
 * If there are slots in the RX queue that  need to be restocked,
 * and we have free pre-allocated buffers, fill the ranks as much
 * as we can pulling from rx_free.
 *
 * This moves the 'write' index forward to catch up with 'processed', and
 * also updates the memory address in the firmware to reference the new
 * target buffer.
 */
static int ipw_rx_queue_restock(struct ipw_priv *priv)
{
	struct ipw_rx_queue *rxq = priv->rxq;
	struct list_head *element;
	struct ipw_rx_mem_buffer *rxb;
	unsigned long flags;
	int write;
	int counter = 0;
	int rc;

	spin_lock_irqsave(&rxq->lock, flags);
	write = rxq->write & ~0x7;
	while ((ipw_rx_queue_space(rxq) > 0) && (rxq->free_count)) {
		element = rxq->rx_free.next;
		rxb = list_entry(element, struct ipw_rx_mem_buffer, list);
		list_del(element);
		*(dma_addr_t *) & (((u32 *) rxq->bd)[rxq->write]) =
		    rxb->dma_addr;
		rxq->queue[rxq->write] = rxb;
		rxq->write = (rxq->write + 1) % RX_QUEUE_SIZE;
		rxq->free_count--;
		counter++;
	}
	spin_unlock_irqrestore(&rxq->lock, flags);
	/* If the pre-allocated buffer pool is dropping low, schedule to
	 * refill it */
	if (rxq->free_count <= RX_LOW_WATERMARK) {
		queue_work(priv->workqueue, &priv->rx_replenish);
	}

	counter = ipw_rx_queue_space(rxq);
	/* If we've added more space for the firmware to place data, tell it */
	if ((write != (rxq->write & ~0x7))
	    || (abs(rxq->write - rxq->read) > 7)) {
		spin_lock_irqsave(&rxq->lock, flags);
		rxq->need_update = 1;
		spin_unlock_irqrestore(&rxq->lock, flags);
		rc = ipw_rx_queue_update_write_ptr(priv, rxq);
		if (rc) {
			return rc;
		}
	}

	return 0;
}

/*
 * Move all used packet from rx_used to rx_free, allocating a new SKB for each.
 * Also restock the Rx queue via ipw_rx_queue_restock.
 *
 * This is called as a scheduled work item (except for during intialization)
 */
static void ipw_rx_queue_replenish(void *data)
{
	struct ipw_priv *priv = data;
	struct ipw_rx_queue *rxq = priv->rxq;
	struct list_head *element;
	struct ipw_rx_mem_buffer *rxb;
	unsigned long flags;
	spin_lock_irqsave(&rxq->lock, flags);
	while (!list_empty(&rxq->rx_used)) {
		element = rxq->rx_used.next;
		rxb = list_entry(element, struct ipw_rx_mem_buffer, list);
		rxb->skb = alloc_skb(IPW_RX_BUF_SIZE, GFP_ATOMIC);
		if (!rxb->skb) {
			printk(KERN_CRIT "%s: Can not allocate SKB buffers.\n",
			       priv->net_dev->name);
			/* We don't reschedule replenish work here -- we will
			 * call the restock method and if it still needs
			 * more buffers it will schedule replenish */
			break;
		}
		list_del(element);
		rxb->dma_addr =
		    pci_map_single(priv->pci_dev, rxb->skb->data,
				   IPW_RX_BUF_SIZE, PCI_DMA_FROMDEVICE);
		list_add_tail(&rxb->list, &rxq->rx_free);
		rxq->free_count++;
	}
	spin_unlock_irqrestore(&rxq->lock, flags);

	spin_lock_irqsave(&priv->lock, flags);
	ipw_rx_queue_restock(priv);
	spin_unlock_irqrestore(&priv->lock, flags);
}

static void ipw_bg_rx_queue_replenish(void *data)
{
	struct ipw_priv *priv = data;

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	ipw_rx_queue_replenish(data);
	mutex_unlock(&priv->mutex);
}

/* Assumes that the skb field of the buffers in 'pool' is kept accurate.
 * If an SKB has been detached, the POOL needs to have it's SKB set to NULL
 * This free routine walks the list of POOL entries and if SKB is set to
 * non NULL it is unmapped and freed
 */
static void ipw_rx_queue_free(struct ipw_priv *priv, struct ipw_rx_queue *rxq)
{
	int i;
	if (!rxq)
		return;
	for (i = 0; i < RX_QUEUE_SIZE + RX_FREE_BUFFERS; i++) {
		if (rxq->pool[i].skb != NULL) {
			pci_unmap_single(priv->pci_dev, rxq->pool[i].dma_addr,
					 IPW_RX_BUF_SIZE, PCI_DMA_FROMDEVICE);
			dev_kfree_skb(rxq->pool[i].skb);
		}
	}

	pci_free_consistent(priv->pci_dev, 4 * RX_QUEUE_SIZE, rxq->bd,
			    rxq->dma_addr);
	kfree(rxq);
}

static int ipw_rxq_stop(struct ipw_priv *priv)
{
	int rc;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	rc = ipw_grab_restricted_access(priv);
	if (rc) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return rc;
	}

	ipw_write_restricted(priv, FH_RCSR_CONFIG(0), 0);
	rc = ipw_poll_restricted_bit(priv, FH_RSSR_STATUS, (1 << 24), 1000);
	if (rc < 0)
		IPW_ERROR("Can't stop Rx DMA.\n");

	ipw_release_restricted_access(priv);
	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static int ipw_rx_init(struct ipw_priv *priv, struct ipw_rx_queue *rxq)
{
	int rc;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	rc = ipw_grab_restricted_access(priv);
	if (rc) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return rc;
	}

	ipw_write_restricted(priv, FH_RCSR_RBD_BASE(0), rxq->dma_addr);
	ipw_write_restricted(priv, FH_RCSR_RPTR_ADDR(0),
			     priv->shared_phys + offsetof(struct ipw_shared_t,
							  rx_read_ptr[0]));
	ipw_write_restricted(priv, FH_RCSR_WPTR(0), 0);
	ipw_write_restricted(priv, FH_RCSR_CONFIG(0),
			     ALM_FH_RCSR_RX_CONFIG_REG_VAL_DMA_CHNL_EN_ENABLE |
			     ALM_FH_RCSR_RX_CONFIG_REG_VAL_RDRBD_EN_ENABLE |
			     ALM_FH_RCSR_RX_CONFIG_REG_BIT_WR_STTS_EN |
			     ALM_FH_RCSR_RX_CONFIG_REG_VAL_MAX_FRAG_SIZE_128 |
			     (RX_QUEUE_SIZE_LOG <<
			      ALM_FH_RCSR_RX_CONFIG_REG_POS_RBDC_SIZE) |
			     ALM_FH_RCSR_RX_CONFIG_REG_VAL_IRQ_DEST_INT_HOST |
			     (1 << ALM_FH_RCSR_RX_CONFIG_REG_POS_IRQ_RBTH)
			     | ALM_FH_RCSR_RX_CONFIG_REG_VAL_MSG_MODE_FH);

	/* fake read to flush all prev I/O */
	ipw_read_restricted(priv, FH_RSSR_CTRL);

	ipw_release_restricted_access(priv);

	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static struct ipw_rx_queue *ipw_rx_queue_alloc(struct ipw_priv *priv)
{
	struct ipw_rx_queue *rxq;
	struct pci_dev *dev = priv->pci_dev;
	int i;
	rxq = (struct ipw_rx_queue *)kmalloc(sizeof(*rxq), GFP_ATOMIC);
	memset(rxq, 0, sizeof(*rxq));

	spin_lock_init(&rxq->lock);
	INIT_LIST_HEAD(&rxq->rx_free);
	INIT_LIST_HEAD(&rxq->rx_used);
	rxq->bd = pci_alloc_consistent(dev, 4 * RX_QUEUE_SIZE, &rxq->dma_addr);
	/* Fill the rx_used queue with _all_ of the Rx buffers */
	for (i = 0; i < RX_FREE_BUFFERS + RX_QUEUE_SIZE; i++)
		list_add_tail(&rxq->pool[i].list, &rxq->rx_used);
	/* Set us so that we have processed and used all buffers, but have
	 * not restocked the Rx queue with fresh buffers */
	rxq->read = rxq->write = 0;
	rxq->free_count = 0;
	rxq->need_update = 0;
	return rxq;
}

static inline void ipw_rx_queue_reset(struct ipw_priv *priv,
				      struct ipw_rx_queue *rxq)
{
	unsigned long flags;
	int i;
	spin_lock_irqsave(&rxq->lock, flags);
	INIT_LIST_HEAD(&rxq->rx_free);
	INIT_LIST_HEAD(&rxq->rx_used);
	/* Fill the rx_used queue with _all_ of the Rx buffers */
	for (i = 0; i < RX_FREE_BUFFERS + RX_QUEUE_SIZE; i++) {
		/* In the reset function, these buffers may have been allocated
		 * to an SKB, so we need to unmap and free potential storage */
		if (rxq->pool[i].skb != NULL) {
			pci_unmap_single(priv->pci_dev, rxq->pool[i].dma_addr,
					 IPW_RX_BUF_SIZE, PCI_DMA_FROMDEVICE);
			dev_kfree_skb(rxq->pool[i].skb);
			rxq->pool[i].skb = NULL;
		}
		list_add_tail(&rxq->pool[i].list, &rxq->rx_used);
	}

	/* Set us so that we have processed and used all buffers, but have
	 * not restocked the Rx queue with fresh buffers */
	rxq->read = rxq->write = 0;
	rxq->free_count = 0;
	spin_unlock_irqrestore(&rxq->lock, flags);
}

/************************ NIC-FUNCTIONS *******************/
/**
 * Check that card is still alive.
 * Reads debug register from domain0.
 * If card is present, pre-defined value should
 * be found there.
 *
 * @param priv
 * @return 1 if card is present, 0 otherwise
 */
static inline int ipw_alive(struct ipw_priv *priv)
{
	u32 val = ipw_read32(priv, CSR_RESET);
	if (val & CSR_RESET_REG_FLAG_NEVO_RESET)
		return 0;

	IPW_DEBUG_INFO("Card is alive \n");
	return 1;
}

/* These functions load the firmware and micro code for the operation of
 * the ipw hardware.  It assumes the buffer has all the bits for the
 * image and the caller is handling the memory allocation and clean up.
 */

static int ipw_nic_stop_master(struct ipw_priv *priv)
{
	int rc = 0;
	u32 reg_val;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	/* set stop master bit */
	ipw_set_bit(priv, CSR_RESET, CSR_RESET_REG_FLAG_STOP_MASTER);

	reg_val = ipw_read32(priv, CSR_GP_CNTRL);

	if (CSR_GP_CNTRL_REG_FLAG_MAC_POWER_SAVE ==
	    (reg_val & CSR_GP_CNTRL_REG_MSK_POWER_SAVE_TYPE)) {
		IPW_DEBUG_INFO
		    ("Card in power save, master is already stopped\n");
	} else {
		rc = ipw_poll_bit(priv,
				  CSR_RESET,
				  CSR_RESET_REG_FLAG_MASTER_DISABLED,
				  CSR_RESET_REG_FLAG_MASTER_DISABLED, 100);
		if (rc < 0) {
			spin_unlock_irqrestore(&priv->lock, flags);
			return rc;
		}
	}

	spin_unlock_irqrestore(&priv->lock, flags);
	IPW_DEBUG_INFO("stop master\n");

	return rc;
}

static int ipw_nic_set_pwr_src(struct ipw_priv *priv, int pwr_max)
{
	int rc = 0;
	unsigned long flags;

	//return 0;
	spin_lock_irqsave(&priv->lock, flags);
	rc = ipw_grab_restricted_access(priv);
	if (rc) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return rc;
	}

	if (!pwr_max) {
		u32 val;
		rc = pci_read_config_dword(priv->pci_dev, 0x0C8, &val);
		if (val & PCI_CFG_PMC_PME_FROM_D3COLD_SUPPORT) {
			ipw_set_bits_mask_restricted_reg(priv,
							 ALM_APMG_PS_CTL,
							 APMG_PS_CTRL_REG_VAL_POWER_SRC_VAUX,
							 ~APMG_PS_CTRL_REG_MSK_POWER_SRC);
			ipw_release_restricted_access(priv);

			ipw_poll_bit(priv, CSR_GPIO_IN, CSR_GPIO_IN_VAL_VAUX_PWR_SRC, CSR_GPIO_IN_BIT_AUX_POWER, 5000);	//uS
		}

	} else {
		ipw_set_bits_mask_restricted_reg(priv,
						 ALM_APMG_PS_CTL,
						 APMG_PS_CTRL_REG_VAL_POWER_SRC_VMAIN,
						 ~APMG_PS_CTRL_REG_MSK_POWER_SRC);

		ipw_release_restricted_access(priv);
		ipw_poll_bit(priv, CSR_GPIO_IN, CSR_GPIO_IN_VAL_VMAIN_PWR_SRC, CSR_GPIO_IN_BIT_AUX_POWER, 5000);	//uS
	}
	spin_unlock_irqrestore(&priv->lock, flags);

	return rc;
}

static void ipw_nic_start(struct ipw_priv *priv)
{
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	ipw_clear_bit(priv, CSR_RESET,
		      CSR_RESET_REG_FLAG_MASTER_DISABLED |
		      CSR_RESET_REG_FLAG_STOP_MASTER |
		      CSR_RESET_REG_FLAG_NEVO_RESET);
	spin_unlock_irqrestore(&priv->lock, flags);
}

static int ipw_nic_init(struct ipw_priv *priv)
{
	u8 rev_id, sku_cap, card_ver;
	u16 board_ver = 0;
	int rc;
	unsigned long flags;

	ipw_power_init_handle(priv);
	ipw_rate_scale_init_handle(priv, IPW_RATE_SCALE_MAX_WINDOW);

	spin_lock_irqsave(&priv->lock, flags);
	ipw_set_bit(priv, CSR_ANA_PLL_CFG, (1 << 24));
	ipw_set_bit(priv, CSR_GIO_CHICKEN_BITS,
		    CSR_GIO_CHICKEN_BITS_REG_BIT_L1A_NO_L0S_RX);

	ipw_set_bit(priv, CSR_GP_CNTRL, CSR_GP_CNTRL_REG_FLAG_INIT_DONE);
	rc = ipw_poll_bit(priv, CSR_GP_CNTRL,
			  CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY,
			  CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY, 25000);
	if (rc < 0) {
		spin_unlock_irqrestore(&priv->lock, flags);
		IPW_DEBUG_INFO("Failed to init the card\n");
		return rc;
	}

	rc = ipw_grab_restricted_access(priv);
	if (rc) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return rc;
	}
	ipw_write_restricted_reg(priv, ALM_APMG_CLK_EN,
				 APMG_CLK_REG_VAL_DMA_CLK_RQT |
				 APMG_CLK_REG_VAL_BSM_CLK_RQT);
	udelay(20);
	ipw_set_bits_restricted_reg(priv, ALM_APMG_PCIDEV_STT,
				    APMG_DEV_STATE_REG_VAL_L1_ACTIVE_DISABLE);
	ipw_release_restricted_access(priv);
	spin_unlock_irqrestore(&priv->lock, flags);

	/* Determine HW type */
	rc = pci_read_config_byte(priv->pci_dev, PCI_REVISION_ID, &rev_id);
	if (rc)
		return rc;
	IPW_DEBUG_INFO("HW Revision ID = 0x%X\n", rev_id);

	ipw_nic_set_pwr_src(priv, 1);
	spin_lock_irqsave(&priv->lock, flags);

	if (rev_id & PCI_CFG_REV_ID_BIT_RTP)
		IPW_DEBUG_INFO("RTP type \n");
	else if (rev_id & PCI_CFG_REV_ID_BIT_BASIC_SKU) {
		IPW_DEBUG_INFO("ALM-MB type\n");
		ipw_set_bit(priv, CSR_HW_IF_CONFIG_REG,
			    CSR_HW_IF_CONFIG_REG_BIT_ALMAGOR_MB);
	} else {
		IPW_DEBUG_INFO("ALM-MM type\n");
		ipw_set_bit(priv, CSR_HW_IF_CONFIG_REG,
			    CSR_HW_IF_CONFIG_REG_BIT_ALMAGOR_MM);
	}

	spin_unlock_irqrestore(&priv->lock, flags);
	/* Initialize the EEPROM */
	rc = ipw_eeprom_init_sram(priv);
	if (rc)
		return rc;

	spin_lock_irqsave(&priv->lock, flags);
	ipw_query_eeprom(priv, EEPROM_SKU_CAP, sizeof(u8), &sku_cap);
	if (EEPROM_SKU_CAP_OP_MODE_MRC == sku_cap) {
		IPW_DEBUG_INFO("SKU OP mode is mrc\n");
		ipw_set_bit(priv, CSR_HW_IF_CONFIG_REG,
			    CSR_HW_IF_CONFIG_REG_BIT_SKU_MRC);
	} else {
		IPW_DEBUG_INFO("SKU OP mode is basic\n");
	}

	ipw_query_eeprom(priv, EEPROM_BOARD_REVISION, sizeof(u16),
			 (u8 *) & board_ver);
	if ((board_ver & 0xF0) == 0xD0) {
		IPW_DEBUG_INFO("3945ABG revision is 0x%X\n", board_ver);
		ipw_set_bit(priv, CSR_HW_IF_CONFIG_REG,
			    CSR_HW_IF_CONFIG_REG_BIT_BOARD_TYPE);
	} else {
		IPW_DEBUG_INFO("3945ABG revision is 0x%X\n", board_ver);
		ipw_clear_bit(priv, CSR_HW_IF_CONFIG_REG,
			      CSR_HW_IF_CONFIG_REG_BIT_BOARD_TYPE);
	}

	ipw_query_eeprom(priv, EEPROM_ALMGOR_M_VERSION, sizeof(u8),
			 (u8 *) & card_ver);
	if (card_ver <= 1) {
		ipw_set_bit(priv, CSR_HW_IF_CONFIG_REG,
			    CSR_HW_IF_CONFIG_REG_BITS_SILICON_TYPE_A);
		IPW_DEBUG_INFO("Card M type A version is 0x%X\n", card_ver);
	} else {
		IPW_DEBUG_INFO("Card M type B version is 0x%X\n", card_ver);
		ipw_set_bit(priv, CSR_HW_IF_CONFIG_REG,
			    CSR_HW_IF_CONFIG_REG_BITS_SILICON_TYPE_B);
	}
	spin_unlock_irqrestore(&priv->lock, flags);

	/* Allocate the RX queue, or reset if it is already allocated */
	if (!priv->rxq)
		priv->rxq = ipw_rx_queue_alloc(priv);
	else
		ipw_rx_queue_reset(priv, priv->rxq);

	if (!priv->rxq) {
		IPW_ERROR("Unable to initialize Rx queue\n");
		return -ENOMEM;
	}
	ipw_rx_queue_replenish(priv);

	ipw_rx_init(priv, priv->rxq);

	spin_lock_irqsave(&priv->lock, flags);
	rc = ipw_grab_restricted_access(priv);
	if (rc) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return rc;
	}
	ipw_write_restricted(priv, FH_RCSR_WPTR(0), priv->rxq->write & ~7);
	ipw_release_restricted_access(priv);

	spin_unlock_irqrestore(&priv->lock, flags);

	rc = ipw_queue_reset(priv);
	if (rc)
		return rc;

	rc = ipw_rf_eeprom_ready(priv);
	if (rc)
		return rc;

	priv->status |= STATUS_INIT;

	return 0;
}

/* Call this function from process context, it will sleeps */
static int ipw_nic_reset(struct ipw_priv *priv)
{
	int rc = 0;
	unsigned long flags;

	ipw_nic_stop_master(priv);

	spin_lock_irqsave(&priv->lock, flags);
	ipw_set_bit(priv, CSR_RESET, CSR_RESET_REG_FLAG_SW_RESET);

	rc = ipw_poll_bit(priv, CSR_GP_CNTRL,
			  CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY,
			  CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY, 25000);

	rc = ipw_grab_restricted_access(priv);
	if (rc) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return rc;
	}

	ipw_write_restricted_reg(priv, APMG_CLK_CTRL_REG,
				 APMG_CLK_REG_VAL_BSM_CLK_RQT);

	udelay(10);

	ipw_set_bit(priv, CSR_GP_CNTRL, CSR_GP_CNTRL_REG_FLAG_INIT_DONE);

	ipw_write_restricted_reg(priv, ALM_APMG_LARC_INT_MSK, 0x0);
	ipw_write_restricted_reg(priv, ALM_APMG_LARC_INT, 0xFFFFFFFF);

	/* enable DMA */
	ipw_write_restricted_reg(priv, ALM_APMG_CLK_EN,
				 APMG_CLK_REG_VAL_DMA_CLK_RQT |
				 APMG_CLK_REG_VAL_BSM_CLK_RQT);
	udelay(10);

	ipw_set_bits_restricted_reg(priv, ALM_APMG_PS_CTL,
				    APMG_PS_CTRL_REG_VAL_ALM_R_RESET_REQ);
	udelay(5);
	ipw_clear_bits_restricted_reg(priv, ALM_APMG_PS_CTL,
				      APMG_PS_CTRL_REG_VAL_ALM_R_RESET_REQ);
	ipw_release_restricted_access(priv);

	/* Clear the 'host command active' bit... */
	priv->status &= ~STATUS_HCMD_ACTIVE;

	wake_up_interruptible(&priv->wait_command_queue);
	spin_unlock_irqrestore(&priv->lock, flags);

	return rc;
}

/********************* UCODE-DOWNLOAD-FUNCTIONS ***********************/
static int ipw_get_fw(struct ipw_priv *priv,
		      const struct firmware **fw, const char *name)
{
	int rc;

	/* ask firmware_class module to get the boot firmware off disk */
	rc = request_firmware(fw, name, &priv->pci_dev->dev);
	if (rc < 0) {
		IPW_ERROR("%s load failed: Reason %d\n", name, rc);
		return rc;
	}

	IPW_DEBUG_INFO("Loading firmware '%s' file (%zd bytes)\n",
		       name, (*fw)->size);
	return 0;
}

static int ipw_download_ucode_base(struct ipw_priv *priv, u8 * image, u32 len)
{
	u32 reg;
	u32 val;
	int rc;

	rc = ipw_grab_restricted_access(priv);
	if (rc)
		return rc;

	ipw_write_restricted_reg_buffer(priv, BSM_SRAM_LOWER_BOUND, len, image);
	ipw_write_restricted_reg(priv, BSM_WR_MEM_SRC_REG, 0x0);
	ipw_write_restricted_reg(priv, BSM_WR_MEM_DST_REG,
				 RTC_INST_LOWER_BOUND);
	ipw_write_restricted_reg(priv, BSM_WR_DWCOUNT_REG, len / sizeof(u32));
	ipw_write_restricted_reg(priv, BSM_WR_CTRL_REG,
				 BSM_WR_CTRL_REG_BIT_START_EN);

	val = ipw_read_restricted_reg(priv, BSM_WR_DWCOUNT_REG);
	for (reg = BSM_SRAM_LOWER_BOUND; reg < BSM_SRAM_LOWER_BOUND + len;
	     reg += sizeof(u32), image += sizeof(u32)) {
		val = ipw_read_restricted_reg(priv, reg);
		if (val != *(u32 *) image) {
			IPW_ERROR("uCode verification failed at "
				  "addr 0x%08X+%u (of %u)\n",
				  BSM_SRAM_LOWER_BOUND,
				  reg - BSM_SRAM_LOWER_BOUND, len);
			ipw_release_restricted_access(priv);
			return -EIO;
		}
	}

	ipw_release_restricted_access(priv);
	return 0;
}

static int attach_buffer_to_tfd_frame(struct tfd_frame *tfd, dma_addr_t addr,
				      u16 len)
{
	int count = 0;
	u32 pad;

	count = TFD_CTL_COUNT_GET(tfd->control_flags);
	pad = TFD_CTL_PAD_GET(tfd->control_flags);

	if ((count >= NUM_TFD_CHUNKS) || (count < 0)) {
		IPW_ERROR("Error can not send more than %d chunks\n",
			  NUM_TFD_CHUNKS);
		return -EINVAL;
	}

	tfd->pa[count].addr = (u32) addr;
	tfd->pa[count].len = len;

	count++;

	tfd->control_flags = TFD_CTL_COUNT_SET(count) | TFD_CTL_PAD_SET(pad);

	return 0;
}

static int ipw_verify_ucode(struct ipw_priv *priv)
{
	u32 *image;
	u32 len, val;
	int rc;

	rc = ipw_grab_restricted_access(priv);
	if (rc)
		return rc;

	ipw_write_restricted(priv, HBUS_TARG_MEM_RADDR, RTC_DATA_LOWER_BOUND);

	for (image = priv->ucode_data.v_addr, len = priv->ucode_data.len;
	     len > 0; len -= sizeof(u32), image++) {
		val = ipw_read_restricted(priv, HBUS_TARG_MEM_RDAT);
		if (val != *image) {
			IPW_ERROR("uCode INST section is invalid at "
				  "offset %u\n", len);
			rc = -EIO;
			goto done;
		}
	}

	ipw_write_restricted(priv, HBUS_TARG_MEM_RADDR, RTC_INST_LOWER_BOUND);

	for (image = priv->ucode_code.v_addr, len = priv->ucode_code.len;
	     len > 0; len -= sizeof(u32), image++) {
		val = ipw_read_restricted(priv, HBUS_TARG_MEM_RDAT);
		if (val != *image) {
			IPW_ERROR("uCode INST section is invalid at "
				  "offset %u\n", len);
			rc = -EIO;
			goto done;
		}
	}

	IPW_DEBUG_INFO("ucode image is good\n");

      done:
	ipw_release_restricted_access(priv);
	return rc;
}

static int ipw_download_ucode(struct ipw_priv *priv, struct fw_image_desc *desc,
			      u32 mem_size, dma_addr_t dst_addr)
{
	dma_addr_t phy_addr = 0;
	u32 len = 0;
	u32 count = 0;
	u32 pad;
	struct tfd_frame tfd;
	u32 tx_config = 0;
	int rc;

	memset(&tfd, 0, sizeof(struct tfd_frame));

	phy_addr = desc->p_addr;
	len = desc->len;

	if (mem_size < len) {
		IPW_ERROR("invalid image size, too big %d %d\n", mem_size, len);
		return -EINVAL;
	}

	while (len > 0) {
		if (ALM_TB_MAX_BYTES_COUNT < len) {
			attach_buffer_to_tfd_frame(&tfd, phy_addr,
						   ALM_TB_MAX_BYTES_COUNT);
			len -= ALM_TB_MAX_BYTES_COUNT;
			phy_addr += ALM_TB_MAX_BYTES_COUNT;
		} else {
			attach_buffer_to_tfd_frame(&tfd, phy_addr, len);
			break;
		}
	}

	pad = U32_PAD(len);
	count = TFD_CTL_COUNT_GET(tfd.control_flags);
	tfd.control_flags = TFD_CTL_COUNT_SET(count) | TFD_CTL_PAD_SET(pad);

	rc = ipw_grab_restricted_access(priv);
	if (rc)
		return rc;

	ipw_write_restricted(priv, FH_TCSR_CONFIG(ALM_FH_SRVC_CHNL),
			     ALM_FH_TCSR_TX_CONFIG_REG_VAL_DMA_CHNL_PAUSE);
	ipw_write_buffer_restricted(priv,
				    ALM_FH_TFDB_CHNL_BUF_CTRL_REG
				    (ALM_FH_SRVC_CHNL),
				    sizeof(struct tfd_frame), (u32 *) & tfd);
	ipw_write_restricted(priv, HBUS_TARG_MEM_WADDR, dst_addr);
	ipw_write_restricted(priv, FH_TCSR_CREDIT(ALM_FH_SRVC_CHNL),
			     0x000FFFFF);
	ipw_write_restricted(priv, FH_TCSR_BUFF_STTS(ALM_FH_SRVC_CHNL),
			     ALM_FH_TCSR_CHNL_TX_BUF_STS_REG_VAL_TFDB_VALID |
			     ALM_FH_TCSR_CHNL_TX_BUF_STS_REG_BIT_TFDB_WPTR);

	tx_config = ALM_FH_TCSR_TX_CONFIG_REG_VAL_DMA_CHNL_ENABLE |
	    ALM_FH_TCSR_TX_CONFIG_REG_VAL_DMA_CREDIT_DISABLE_VAL |
	    ALM_FH_TCSR_TX_CONFIG_REG_VAL_MSG_MODE_DRIVER;

	ipw_write_restricted(priv, FH_TCSR_CONFIG(ALM_FH_SRVC_CHNL), tx_config);

	rc = ipw_poll_restricted_bit(priv, FH_TSSR_TX_STATUS,
				     ALM_FH_TSSR_TX_STATUS_REG_MSK_CHNL_IDLE
				     (ALM_FH_SRVC_CHNL), 1000);
	if (rc < 0) {
		IPW_WARNING("3946ABG card ucode DOWNLOAD FAILED \n");
		goto done;
	}

	rc = 0;

	IPW_DEBUG_INFO("3946ABG card ucode download is good \n");

	ipw_write_restricted(priv, FH_TCSR_CREDIT(ALM_FH_SRVC_CHNL), 0x0);

      done:
	ipw_release_restricted_access(priv);
	return rc;
}

static int ipw_copy_ucode_images(struct ipw_priv *priv, u8 * image_code,
				 size_t image_len_code, u8 * image_data,
				 size_t image_len_data)
{
	int rc;

	if ((image_len_code > priv->ucode_code.actual_len) ||
	    (image_len_data > priv->ucode_data.actual_len)) {
		IPW_DEBUG_INFO("uCode size is too large to fit\n");
		return -EINVAL;
	}

	memcpy(priv->ucode_code.v_addr, image_code, image_len_code);
	priv->ucode_code.len = (u32) image_len_code;
	memcpy(priv->ucode_data.v_addr, image_data, image_len_data);
	priv->ucode_data.len = (u32) image_len_data;

	rc = ipw_grab_restricted_access(priv);
	if (rc)
		return rc;

	ipw_write_restricted_reg(priv, BSM_DRAM_INST_PTR_REG,
				 priv->ucode_code.p_addr);
	ipw_write_restricted_reg(priv, BSM_DRAM_DATA_PTR_REG,
				 priv->ucode_data.p_addr);
	ipw_write_restricted_reg(priv, BSM_DRAM_INST_BYTECOUNT_REG,
				 priv->ucode_code.len);
	ipw_write_restricted_reg(priv, BSM_DRAM_DATA_BYTECOUNT_REG,
				 priv->ucode_data.len);
	ipw_release_restricted_access(priv);

	return 0;
}

struct ipw_ucode {
	u32 ver;
	u32 inst_size;
	u32 data_size;
	u32 boot_size;
	u8 data[0];
};

static int ipw_load(struct ipw_priv *priv)
{
	struct ipw_ucode *ucode;
	int rc = 0;

	if (!priv->ucode_raw) {
		rc = ipw_get_fw(priv, &priv->ucode_raw, "ipw3945.ucode");
		if (rc)
			goto error;
	}

	if (priv->ucode_raw->size < sizeof(*ucode)) {
		rc = -EINVAL;
		goto invalid_error;
	}

	ucode = (void *)priv->ucode_raw->data;

	if (priv->ucode_raw->size < sizeof(*ucode) +
	    ucode->boot_size + ucode->inst_size + ucode->data_size) {
		rc = -EINVAL;
		goto invalid_error;
	}

	rc = ipw_copy_ucode_images(priv, &ucode->data[0],
				   ucode->inst_size,
				   &ucode->data[ucode->inst_size],
				   ucode->data_size);
	if (rc)
		goto error;

	rc = ipw_download_ucode_base(priv, &ucode->data[ucode->inst_size +
							ucode->data_size],
				     ucode->boot_size);
	if (rc)
		goto error;

	rc = ipw_download_ucode(priv, &(priv->ucode_code), ALM_RTC_INST_SIZE,
				RTC_INST_LOWER_BOUND);
	if (rc)
		goto error;

	rc = ipw_download_ucode(priv, &(priv->ucode_data), ALM_RTC_DATA_SIZE,
				RTC_DATA_LOWER_BOUND);
	if (rc)
		goto error;

	return 0;

      invalid_error:
	/* We only discard the cached ucode image if the version we grabbed
	 * from disk is invalid (wrong size) */
	if (priv->ucode_raw) {
		release_firmware(priv->ucode_raw);
		priv->ucode_raw = NULL;
	}

      error:
	return rc;
}

/*************** DMA-QUEUE-GENERAL-FUNCTIONS  *****/
/**
 * DMA services
 *
 * Theory of operation
 *
 * A queue is a circular buffers with 'Read' and 'Write' pointers.
 * 2 empty entries always kept in the buffer to protect from overflow.
 *
 * For Tx queue, there are low mark and high mark limits. If, after queuing
 * the packet for Tx, free space become < low mark, Tx queue stopped. When
 * reclaiming packets (on 'tx done IRQ), if free space become > high mark,
 * Tx queue resumed.
 *
 * The IPW operates with six queues, one receive queue in the device's
 * sram, one transmit queue for sending commands to the device firmware,
 * and four transmit queues for data.
 *
 * The four transmit queues allow for performing quality of service (qos)
 * transmissions as per the 802.11 protocol.  Currently Linux does not
 * provide a mechanism to the user for utilizing prioritized queues, so
 * we only utilize the first data transmit queue (queue1).
 */

/**
 * Driver allocates buffers of this size for Rx
 */

static inline int ipw_queue_space(const struct ipw_queue *q)
{
	int s = q->last_used - q->first_empty;
	if (q->last_used > q->first_empty)
		s -= q->n_bd;

	if (s <= 0)
		s += q->n_window;
	s -= 2;			/* keep some reserve to not confuse empty and full situations */
	if (s < 0)
		s = 0;
	return s;
}

static inline int ipw_queue_inc_wrap(int index, int n_bd)
{
	return (++index == n_bd) ? 0 : index;
}

static inline int x2_queue_used(const struct ipw_queue *q, int i)
{
	return q->first_empty > q->last_used ?
	    (i >= q->last_used && i < q->first_empty) :
	    !(i < q->last_used && i >= q->first_empty);
}

static inline u8 get_next_cmd_index(struct ipw_queue *q, u32 index, int is_huge)
{
	if (is_huge)
		return q->n_window;

	return (u8) (index % q->n_window);
}

/***************** END **************************/

/********************* TX-QUEUE-FUNCTIONS ******************/
static int ipw_stop_tx_queue(struct ipw_priv *priv)
{
	int queueId;
	int rc;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	rc = ipw_grab_restricted_access(priv);
	if (rc) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return rc;
	}

	/* stop SCD */
	ipw_write_restricted_reg(priv, SCD_MODE_REG, 0);

	/* reset TFD queues */
	for (queueId = TFD_QUEUE_MIN; queueId < TFD_QUEUE_MAX; queueId++) {
		ipw_write_restricted(priv, FH_TCSR_CONFIG(queueId), 0x0);
		ipw_poll_restricted_bit(priv,
					FH_TSSR_TX_STATUS,
					ALM_FH_TSSR_TX_STATUS_REG_MSK_CHNL_IDLE
					(queueId), 1000);
	}

	ipw_release_restricted_access(priv);
	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

/**
 * Initialize common DMA queue structure
 *
 * @param q                queue to init
 * @param count            Number of BD's to allocate. Should be power of 2
 * @param read_register    Address for 'read' register
 *                         (not offset within BAR, full address)
 * @param write_restrictedister   Address for 'write' register
 *                         (not offset within BAR, full address)
 * @param base_register    Address for 'base' register
 *                         (not offset within BAR, full address)
 * @param size             Address for 'size' register
 *                         (not offset within BAR, full address)
 */

static int ipw_tx_reset(struct ipw_priv *priv)
{
	int rc;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	rc = ipw_grab_restricted_access(priv);
	if (rc) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return rc;
	}

	ipw_write_restricted_reg(priv, SCD_MODE_REG, 0x2);	// bypass mode
	ipw_write_restricted_reg(priv, SCD_ARASTAT_REG, 0x01);	// RA 0 is active
	ipw_write_restricted_reg(priv, SCD_TXFACT_REG, 0x3f);	// all 6 fifo are active
	ipw_write_restricted_reg(priv, SCD_SBYP_MODE_1_REG, 0x010000);
	ipw_write_restricted_reg(priv, SCD_SBYP_MODE_2_REG, 0x030002);
	ipw_write_restricted_reg(priv, SCD_TXF4MF_REG, 0x000004);
	ipw_write_restricted_reg(priv, SCD_TXF5MF_REG, 0x000005);

	ipw_write_restricted(priv, FH_TSSR_CBB_BASE, priv->shared_phys);

	ipw_write_restricted(priv,
			     FH_TSSR_MSG_CONFIG,
			     ALM_FH_TSSR_TX_MSG_CONFIG_REG_VAL_SNOOP_RD_TXPD_ON
			     |
			     ALM_FH_TSSR_TX_MSG_CONFIG_REG_VAL_ORDER_RD_TXPD_ON
			     |
			     ALM_FH_TSSR_TX_MSG_CONFIG_REG_VAL_MAX_FRAG_SIZE_128B
			     | ALM_FH_TSSR_TX_MSG_CONFIG_REG_VAL_SNOOP_RD_TFD_ON
			     | ALM_FH_TSSR_TX_MSG_CONFIG_REG_VAL_ORDER_RD_CBB_ON
			     |
			     ALM_FH_TSSR_TX_MSG_CONFIG_REG_VAL_ORDER_RSP_WAIT_TH
			     | ALM_FH_TSSR_TX_MSG_CONFIG_REG_VAL_RSP_WAIT_TH);

	ipw_release_restricted_access(priv);

	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static int ipw_queue_init(struct ipw_priv *priv, struct ipw_queue *q,
			  int count, int size, u32 id)
{
	int rc;
	unsigned long flags;

	q->n_bd = count;
	q->n_window = size;
	q->id = id;

	q->low_mark = q->n_window / 4;
	if (q->low_mark < 4)
		q->low_mark = 4;

	q->high_mark = q->n_window / 8;
	if (q->high_mark < 2)
		q->high_mark = 2;

	q->first_empty = q->last_used = 0;
	priv->shared_virt->tx_base_ptr[id] = (u32) q->dma_addr;

	spin_lock_irqsave(&priv->lock, flags);
	rc = ipw_grab_restricted_access(priv);
	if (rc) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return rc;
	}
	ipw_write_restricted(priv, FH_CBCC_CTRL(id), 0);
	ipw_write_restricted(priv, FH_CBCC_BASE(id), 0);

	ipw_write_restricted(priv, FH_TCSR_CONFIG(id),
			     ALM_FH_TCSR_TX_CONFIG_REG_VAL_CIRQ_RTC_NOINT |
			     ALM_FH_TCSR_TX_CONFIG_REG_VAL_MSG_MODE_TXF |
			     ALM_FH_TCSR_TX_CONFIG_REG_VAL_CIRQ_HOST_IFTFD |
			     ALM_FH_TCSR_TX_CONFIG_REG_VAL_DMA_CREDIT_ENABLE_VAL
			     | ALM_FH_TCSR_TX_CONFIG_REG_VAL_DMA_CHNL_ENABLE);
	ipw_release_restricted_access(priv);

	ipw_read32(priv, FH_TSSR_CBB_BASE);	/* fake read to flush all prev. writes */

	spin_unlock_irqrestore(&priv->lock, flags);
	return 0;
}

static int ipw_queue_tx_init(struct ipw_priv *priv,
			     struct ipw_tx_queue *q, int count, u32 id)
{
	struct pci_dev *dev = priv->pci_dev;
	int len;

	q->txb = kmalloc(sizeof(q->txb[0]) * TFD_QUEUE_SIZE_MAX, GFP_ATOMIC);
	if (!q->txb) {
		IPW_ERROR("kmalloc for auxilary BD structures failed\n");
		return -ENOMEM;
	}

	q->bd = pci_alloc_consistent(dev, sizeof(q->bd[0]) * TFD_QUEUE_SIZE_MAX,
				     &q->q.dma_addr);
	if (!q->bd) {
		IPW_ERROR("pci_alloc_consistent(%zd) failed\n",
			  sizeof(q->bd[0]) * count);
		kfree(q->txb);
		q->txb = NULL;
		return -ENOMEM;
	}

	/* alocate command space + one big command for scan since scan
	 * command is very huge the system will not have two scan at the
	 * same time */
	len = (sizeof(struct ipw_cmd) * count) + DAEMON_MAX_SCAN_SIZE;
	q->cmd = pci_alloc_consistent(dev, len, &q->dma_addr_cmd);
	if (!q->cmd) {
		IPW_ERROR("pci_alloc_consistent(%zd) failed\n",
			  sizeof(q->cmd[0]) * count);
		kfree(q->txb);
		q->txb = NULL;

		pci_free_consistent(dev, sizeof(q->bd[0]) * TFD_QUEUE_SIZE_MAX,
				    q->bd, q->q.dma_addr);

		return -ENOMEM;
	}

	q->need_update = 0;
	ipw_queue_init(priv, &q->q, TFD_QUEUE_SIZE_MAX, count, id);
	return 0;
}

/**
 * Free one TFD, those at index [txq->q.last_used].
 * Do NOT advance any indexes
 */
static void ipw_queue_tx_free_tfd(struct ipw_priv *priv,
				  struct ipw_tx_queue *txq)
{
	struct tfd_frame *bd = &txq->bd[txq->q.last_used];
	struct pci_dev *dev = priv->pci_dev;
	int i;
	int counter = 0;
	/* classify bd */
	if (txq->q.id == CMD_QUEUE_NUM)
		/* nothing to cleanup after for host commands */
		return;

	/* sanity check */
	counter = TFD_CTL_COUNT_GET(le32_to_cpu(bd->control_flags));
	if (counter > NUM_TFD_CHUNKS) {
		IPW_ERROR("Too many chunks: %i\n", counter);
		/** @todo issue fatal error, it is quite serious situation */
		return;
	}

	/* unmap chunks if any */

	for (i = 1; i < counter; i++) {
		pci_unmap_single(dev, le32_to_cpu(bd->pa[i].addr),
				 le16_to_cpu(bd->pa[i].len), PCI_DMA_TODEVICE);
		if (txq->txb[txq->q.last_used]) {
			ieee80211_txb_free(txq->txb[txq->q.last_used]);
			txq->txb[txq->q.last_used] = NULL;
		}
	}
}

/**
 * Deallocate DMA queue.
 *
 * Empty queue by removing and destroying all BD's.
 * Free all buffers.
 *
 * @param dev
 * @param q
 */
static void ipw_queue_tx_free(struct ipw_priv *priv, struct ipw_tx_queue *txq)
{
	struct ipw_queue *q = &txq->q;
	struct pci_dev *dev = priv->pci_dev;
	int len;

	if (q->n_bd == 0)
		return;
	/* first, empty all BD's */
	for (; q->first_empty != q->last_used;
	     q->last_used = ipw_queue_inc_wrap(q->last_used, q->n_bd)) {
		ipw_queue_tx_free_tfd(priv, txq);
	}

	len = (sizeof(txq->cmd[0]) * q->n_window) + DAEMON_MAX_SCAN_SIZE;
	pci_free_consistent(dev, len, txq->cmd, txq->dma_addr_cmd);

	/* free buffers belonging to queue itself */
	pci_free_consistent(dev, sizeof(txq->bd[0]) * q->n_bd, txq->bd,
			    q->dma_addr);

	kfree(txq->txb);

	/* 0 fill whole structure */
	memset(txq, 0, sizeof(*txq));
}

/**
 * Destroy all DMA queues and structures
 *
 * @param priv
 */
static void ipw_tx_queue_free(struct ipw_priv *priv)
{

	/* Tx queues */
	ipw_queue_tx_free(priv, &priv->txq[0]);
	ipw_queue_tx_free(priv, &priv->txq[1]);
	ipw_queue_tx_free(priv, &priv->txq[2]);
	ipw_queue_tx_free(priv, &priv->txq[3]);
	ipw_queue_tx_free(priv, &priv->txq[4]);
	ipw_queue_tx_free(priv, &priv->txq[5]);
}

static int ipw_tx_queue_update_write_ptr(struct ipw_priv *priv,
					 struct ipw_tx_queue *txq, int tx_id)
{
	u32 reg = 0;
	int rc = 0;

	if (txq->need_update == 0)
		return rc;

	if (priv->status & STATUS_POWER_PMI) {
		reg = ipw_read32(priv, CSR_UCODE_DRV_GP1);

		if (reg & CSR_UCODE_DRV_GP1_BIT_MAC_SLEEP) {
			ipw_set_bit(priv, CSR_GP_CNTRL,
				    CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
			return rc;
		}

		rc = ipw_grab_restricted_access(priv);
		if (rc)
			return rc;
		ipw_write_restricted(priv, HBUS_TARG_WRPTR,
				     txq->q.first_empty | (tx_id << 8));
		ipw_release_restricted_access(priv);
	} else {
		ipw_write32(priv, HBUS_TARG_WRPTR,
			    txq->q.first_empty | (tx_id << 8));
	}

	txq->need_update = 0;

	return rc;
}

/**
 * Destroys all DMA structures and initialise them again
 *
 * @param priv
 * @return error code
 */
static int ipw_queue_reset(struct ipw_priv *priv)
{
	int rc = 0;

	ipw_tx_queue_free(priv);

	/* Tx CMD queue */
	ipw_tx_reset(priv);

	/* Tx queue(s) */
	rc = ipw_queue_tx_init(priv, &priv->txq[0], TFD_TX_CMD_SLOTS, 0);
	if (rc) {
		IPW_ERROR("Tx 0 queue init failed\n");
		goto error;
	}

	rc = ipw_queue_tx_init(priv, &priv->txq[1], TFD_TX_CMD_SLOTS, 1);
	if (rc) {
		IPW_ERROR("Tx 1 queue init failed\n");
		goto error;
	}
	rc = ipw_queue_tx_init(priv, &priv->txq[2], TFD_TX_CMD_SLOTS, 2);
	if (rc) {
		IPW_ERROR("Tx 2 queue init failed\n");
		goto error;
	}
	rc = ipw_queue_tx_init(priv, &priv->txq[3], TFD_TX_CMD_SLOTS, 3);
	if (rc) {
		IPW_ERROR("Tx 3 queue init failed\n");
		goto error;
	}

	rc = ipw_queue_tx_init(priv, &priv->txq[4], TFD_CMD_SLOTS,
			       CMD_QUEUE_NUM);
	if (rc) {
		IPW_ERROR("Tx Cmd queue init failed\n");
		goto error;
	}

	rc = ipw_queue_tx_init(priv, &priv->txq[5], TFD_TX_CMD_SLOTS, 5);
	if (rc) {
		IPW_ERROR("Tx service queue init failed\n");
		goto error;
	}

	return rc;

      error:
	ipw_tx_queue_free(priv);
	return rc;
}

/**************************************************************/

static inline void ipw_create_bssid(struct ipw_priv *priv, u8 * bssid)
{
	/* First 3 bytes are manufacturer */
	bssid[0] = priv->mac_addr[0];
	bssid[1] = priv->mac_addr[1];
	bssid[2] = priv->mac_addr[2];

	/* Last bytes are random */
	get_random_bytes(&bssid[3], ETH_ALEN - 3);

	bssid[0] &= 0xfe;	/* clear multicast bit */
	bssid[0] |= 0x02;	/* set local assignment bit (IEEE802) */
}

static inline int ipw_is_broadcast_ether_addr(const u8 * addr)
{
	return (addr[0] & addr[1] & addr[2] & addr[3] & addr[4] & addr[5]) ==
	    0xff;
}

static u8 ipw_remove_station(struct ipw_priv *priv, u8 * bssid, int is_ap)
{
	int index = IPW_INVALID_STATION;
	int i;

	if (is_ap) {
		index = AP_ID;
		if ((priv->stations[index].used))
			priv->stations[index].used = 0;
	} else if (ipw_is_broadcast_ether_addr(bssid)) {
		index = BROADCAST_ID;
		if ((priv->stations[index].used))
			priv->stations[index].used = 0;
	} else {
		for (i = STA_ID; i < (priv->num_stations + STA_ID); i++) {
			if ((priv->stations[i].used) &&
			    (!memcmp
			     (priv->stations[i].sta.sta.MACAddr, bssid,
			      ETH_ALEN))) {
				index = i;
				priv->stations[index].used = 0;
				break;
			}
		}
	}

	if (index != IPW_INVALID_STATION) {
		if (priv->num_stations > 0)
			priv->num_stations--;
	}

	return 0;
}

//todoG port to 3945 card
static u8 ipw_add_station(struct ipw_priv *priv, u8 * bssid, int is_ap,
			  u8 flags)
{
	int i = NUM_OF_STATIONS;
	int index = IPW_INVALID_STATION;

	if (is_ap) {
		index = AP_ID;
		if ((priv->stations[index].used) &&
		    (!memcmp
		     (priv->stations[index].sta.sta.MACAddr, bssid, ETH_ALEN)))
			return index;
	} else if (ipw_is_broadcast_ether_addr(bssid)) {
		index = BROADCAST_ID;
		if ((priv->stations[index].used) &&
		    (!memcmp
		     (priv->stations[index].sta.sta.MACAddr, bssid, ETH_ALEN)))
			return index;
	} else
		for (i = STA_ID; i < (priv->num_stations + STA_ID); i++) {
			if ((priv->stations[i].used) &&
			    (!memcmp
			     (priv->stations[i].sta.sta.MACAddr, bssid,
			      ETH_ALEN))) {
				/* Another node is active in network */
				priv->missed_adhoc_beacons = 0;
				if (!(priv->config & CFG_STATIC_CHANNEL))
					/* when other nodes drop out, we drop out */
					priv->config &= ~CFG_ADHOC_PERSIST;

				return i;
			}

			if ((priv->stations[i].used == 0) &&
			    (index == IPW_INVALID_STATION))
				index = i;
		}

	if (index != IPW_INVALID_STATION)
		i = index;

	if (i == NUM_OF_STATIONS)
		return IPW_INVALID_STATION;

	IPW_DEBUG_ASSOC("Adding STA ID %d: " MAC_FMT "\n", i, MAC_ARG(bssid));

	priv->stations[i].used = 1;
	priv->stations[i].current_rate = R_54M;
//      if (priv->
	memset(&priv->stations[i].sta, 0, sizeof(struct ipw_addsta_cmd));
	memcpy(priv->stations[i].sta.sta.MACAddr, bssid, ETH_ALEN);
	priv->stations[i].sta.ctrlAddModify = 0;
	priv->stations[i].sta.sta.staID = i;
	priv->stations[i].sta.station_flags = 0;

	//todoG do we need this
//      priv->stations[i].sta.tid_disable_tx = 0xffff;  /* all TID's disabled */
	if (priv->rxon.flags & RXON_FLG_BAND_24G_MSK)
		priv->stations[i].sta.tx_rate = R_1M;
	else
		priv->stations[i].sta.tx_rate = R_6M;

	priv->stations[i].current_rate = priv->stations[i].sta.tx_rate;

	priv->num_stations++;
	ipw_send_add_station(priv, &priv->stations[i].sta, flags);
	return i;
}

static u8 ipw_find_station(struct ipw_priv *priv, u8 * bssid)
{
	int i;
	int start = 0;

	if (priv->ieee->iw_mode == IW_MODE_ADHOC)
		start = STA_ID;

	if (ipw_is_broadcast_ether_addr(bssid))
		return BROADCAST_ID;

	for (i = start; i < (start + priv->num_stations); i++)
		if ((priv->stations[i].used) &&
		    (!memcmp
		     (priv->stations[i].sta.sta.MACAddr, bssid, ETH_ALEN)))
			return i;

	IPW_DEBUG_ASSOC("can not find STA " MAC_FMT "total %d\n",
			MAC_ARG(bssid), priv->num_stations);

	return IPW_INVALID_STATION;
}

static u8 ipw_sync_station(struct ipw_priv *priv, int sta_id, u8 tx_rate,
			   u8 flags)
{

	if (sta_id != IPW_INVALID_STATION) {
		priv->stations[sta_id].sta.sta.modify_mask =
		    STA_CONTROL_MODIFY_MSK;
		priv->stations[sta_id].sta.tx_rate = tx_rate;
		priv->stations[sta_id].current_rate = tx_rate;
		priv->stations[sta_id].sta.ctrlAddModify =
		    STA_CONTROL_MODIFY_MSK;
		ipw_send_add_station(priv, &priv->stations[sta_id].sta, flags);
		IPW_DEBUG_RATE("SCALE sync station %d to rate %d\n",
			       sta_id, tx_rate);
		return sta_id;
	}

	return IPW_INVALID_STATION;
}

static void ipw_send_disassociate(struct ipw_priv *priv, int quiet)
{
	int err;

	if (!(priv->status & (STATUS_ASSOCIATING | STATUS_ASSOCIATED))) {
		IPW_DEBUG_ASSOC("Disassociating while not associated.\n");
		return;
	}

	IPW_DEBUG_ASSOC("Disassociation attempt from " MAC_FMT " "
			"on channel %d.\n",
			MAC_ARG(priv->assoc_request.bssid),
			priv->assoc_request.channel);

	priv->status &= ~(STATUS_ASSOCIATING | STATUS_ASSOCIATED);
	priv->status |= STATUS_DISASSOCIATING;

	if (quiet)
		priv->assoc_request.assoc_type = HC_DISASSOC_QUIET;
	else
		priv->assoc_request.assoc_type = HC_DISASSOCIATE;

	priv->rxon.filter_flags &= ~RXON_FILTER_ASSOC_MSK;

	priv->rxon.flags &= ~(RXON_FLG_TGG_PROTECT_MSK |
			      RXON_FLG_SHORT_PREAMBLE_MSK);
	if (priv->rxon.flags & RXON_FLG_BAND_24G_MSK)
		priv->rxon.flags &= ~RXON_FLG_SHORT_SLOT_MSK;

	err = ipw_rxon_call(priv, 0);

	priv->status &= ~STATUS_DISASSOCIATING;

	priv->status &= ~(STATUS_DISASSOCIATING | STATUS_ASSOCIATING |
			  STATUS_ASSOCIATED | STATUS_AUTH);

	if (priv->assoc_request.capability & WLAN_CAPABILITY_IBSS)
		ipw_remove_current_network(priv);

	priv->num_stations = 0;
	memset(priv->stations, 0,
	       NUM_OF_STATIONS * sizeof(struct ipw_station_entry));

	priv->auth_state = CMAS_INIT;
	if (!(priv->status & STATUS_EXIT_PENDING))
		queue_work(priv->workqueue, &priv->link_down);

	if (err) {
		IPW_DEBUG_HC("Attempt to send [dis]associate command "
			     "failed.\n");
		return;
	}
}

static int ipw_disassociate(void *data)
{
	struct ipw_priv *priv = data;

	cancel_delayed_work(&priv->associate_timeout);

	if (!(priv->status & (STATUS_ASSOCIATED | STATUS_ASSOCIATING)))
		return 0;

	priv->auth_state = CMAS_INIT;
	ipw_send_disassociate(data, 0);

	if (priv->ieee->iw_mode == IW_MODE_INFRA)
		del_timer_sync(&priv->roaming_wdt);

	del_timer_sync(&priv->disassociate_wdt);

	/* re-add the broadcast STA so we can do active scans successfully */
	ipw_rxon_add_station(priv, BROADCAST_ADDR, 0);

	return 1;
}

static void ipw_bg_disassociate(void *data)
{
	struct ipw_priv *priv = data;

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	ipw_disassociate(data);
	mutex_unlock(&priv->mutex);
}

struct ipw_status_code {
	u16 status;
	const char *reason;
};
static const struct ipw_status_code ipw_status_codes[] = {
	{
	 0x00, "Successful"}, {
			       0x01, "Unspecified failure"}, {
							      0x0A,
							      "Cannot support all requested capabilities in the "
							      "Capability information field"},
	{
	 0x0B,
	 "Reassociation denied due to inability to confirm that "
	 "association exists"}, {
				 0x0C,
				 "Association denied due to reason outside the scope of this "
				 "standard"}, {
					       0x0D,
					       "Responding station does not support the specified authentication "
					       "algorithm"}, {
							      0x0E,
							      "Received an Authentication frame with authentication sequence "
							      "transaction sequence number out of expected sequence"},
	{
	 0x0F, "Authentication rejected because of challenge failure"}, {
									 0x10,
									 "Authentication rejected due to timeout waiting for next "
									 "frame in sequence"},
	{
	 0x11,
	 "Association denied because AP is unable to handle additional "
	 "associated stations"}, {
				  0x12,
				  "Association denied due to requesting station not supporting all "
				  "of the datarates in the BSSBasicServiceSet Parameter"},
	{
	 0x13,
	 "Association denied due to requesting station not supporting "
	 "short preamble operation"}, {
				       0x14,
				       "Association denied due to requesting station not supporting "
				       "PBCC encoding"}, {
							  0x15,
							  "Association denied due to requesting station not supporting "
							  "channel agility"}, {
									       0x19,
									       "Association denied due to requesting station not supporting "
									       "short slot operation"},
	{
	 0x1A,
	 "Association denied due to requesting station not supporting "
	 "DSSS-OFDM operation"}, {
				  0x28, "Invalid Information Element"}, {
									 0x29,
									 "Group Cipher is not valid"},
	{
	 0x2A, "Pairwise Cipher is not valid"}, {
						 0x2B, "AKMP is not valid"}, {
									      0x2C,
									      "Unsupported RSN IE version"},
	{
	 0x2D, "Invalid RSN IE Capabilities"}, {
						0x2E,
						"Cipher suite is rejected per security policy"},
};

#ifdef CONFIG_IPW3945_DEBUG
static const char *ipw_get_status_code(u16 status)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(ipw_status_codes); i++)
		if (ipw_status_codes[i].status == (status & 0xff))
			return ipw_status_codes[i].reason;
	return "Unknown status value.";
}
#endif

static void inline average_init(struct average *avg)
{
	memset(avg, 0, sizeof(*avg));
}

static void inline average_add(struct average *avg, s16 val)
{
	avg->sum -= avg->entries[avg->pos];
	avg->sum += val;
	avg->entries[avg->pos++] = val;
	if (unlikely(avg->pos == AVG_ENTRIES)) {
		avg->init = 1;
		avg->pos = 0;
	}
}

static s16 inline average_value(struct average *avg)
{
	if (!unlikely(avg->init)) {
		if (avg->pos)
			return avg->sum / avg->pos;
		return 0;
	}

	return avg->sum / AVG_ENTRIES;
}

static void ipw_reset_stats(struct ipw_priv *priv)
{
	priv->quality = 0;
	average_init(&priv->average_missed_beacons);
	average_init(&priv->average_rssi);
	average_init(&priv->average_noise);
	priv->last_rate = 0;
	priv->last_missed_beacons = 0;
	priv->last_rx_packets = 0;
	priv->last_tx_packets = 0;
	priv->last_tx_failures = 0;
	/* Firmware managed, reset only when NIC is restarted, so we have to
	 * normalize on the current value */

	/* TODO -- obtain the RX error and TX failure stats from HW */

	/* Driver managed, reset with each association */
	priv->missed_adhoc_beacons = 0;
	priv->missed_beacons = 0;
	priv->tx_packets = 0;
	priv->rx_packets = 0;
}

static inline u32 ipw_get_max_rate(struct ipw_priv *priv)
{
	u32 i = 0x80000000;
	u32 mask = priv->rates_mask;
	/* If currently associated in B mode, restrict the maximum
	 * rate match to B rates */
	if (priv->assoc_request.ieee_mode == IPW_B_MODE)
		mask &= IEEE80211_CCK_RATES_MASK;
	/* TODO: Verify that the rate is supported by the current rates
	 * list. */
	while (i && !(mask & i))
		i >>= 1;
	switch (i) {
	case IEEE80211_CCK_RATE_1MB_MASK:
		return 1000000;
	case IEEE80211_CCK_RATE_2MB_MASK:
		return 2000000;
	case IEEE80211_CCK_RATE_5MB_MASK:
		return 5500000;
	case IEEE80211_OFDM_RATE_6MB_MASK:
		return 6000000;
	case IEEE80211_OFDM_RATE_9MB_MASK:
		return 9000000;
	case IEEE80211_CCK_RATE_11MB_MASK:
		return 11000000;
	case IEEE80211_OFDM_RATE_12MB_MASK:
		return 12000000;
	case IEEE80211_OFDM_RATE_18MB_MASK:
		return 18000000;
	case IEEE80211_OFDM_RATE_24MB_MASK:
		return 24000000;
	case IEEE80211_OFDM_RATE_36MB_MASK:
		return 36000000;
	case IEEE80211_OFDM_RATE_48MB_MASK:
		return 48000000;
	case IEEE80211_OFDM_RATE_54MB_MASK:
		return 54000000;
	}

	if (priv->ieee->mode == IEEE_B)
		return 11000000;
	else
		return 54000000;
}

static u32 ipw_get_current_rate(struct ipw_priv *priv)
{
	u32 rate;

	if (!(priv->status & STATUS_ASSOCIATED))
		return 0;
	if (priv->ieee->iw_mode == IW_MODE_ADHOC)
		rate = priv->stations[STA_ID].current_rate;
	else
		rate = priv->stations[AP_ID].current_rate;

	switch (rate) {
	case IPW_TX_RATE_1MB:
		return 1000000;
	case IPW_TX_RATE_2MB:
		return 2000000;
	case IPW_TX_RATE_5MB:
		return 5500000;
	case IPW_TX_RATE_6MB:
		return 6000000;
	case IPW_TX_RATE_9MB:
		return 9000000;
	case IPW_TX_RATE_11MB:
		return 11000000;
	case IPW_TX_RATE_12MB:
		return 12000000;
	case IPW_TX_RATE_18MB:
		return 18000000;
	case IPW_TX_RATE_24MB:
		return 24000000;
	case IPW_TX_RATE_36MB:
		return 36000000;
	case IPW_TX_RATE_48MB:
		return 48000000;
	case IPW_TX_RATE_54MB:
		return 54000000;
	}

	return 0;
}

#define IPW_STATS_INTERVAL (2 * HZ)
static void ipw_gather_stats(struct ipw_priv *priv)
{
	u32 rx_err = 0, rx_err_delta, rx_packets_delta;
	u32 tx_failures = 0, tx_failures_delta, tx_packets_delta;
	u32 missed_beacons_percent, missed_beacons_delta;
	u32 quality = 0;
	s16 rssi;
	u32 beacon_quality, signal_quality, tx_quality, rx_quality,
	    rate_quality;
	u32 max_rate;

	if (!(priv->status & STATUS_ASSOCIATED)) {
		priv->quality = 0;
		return;
	}

	/* TODO -- gather missed beacon stats */
	missed_beacons_delta = priv->missed_beacons - priv->last_missed_beacons;
	priv->last_missed_beacons = priv->missed_beacons;
	if (priv->assoc_request.beacon_interval) {
		missed_beacons_percent =
		    missed_beacons_delta * (HZ *
					    priv->assoc_request.
					    beacon_interval) /
		    (IPW_STATS_INTERVAL * 10);
	} else {
		missed_beacons_percent = 0;
	}
	average_add(&priv->average_missed_beacons, missed_beacons_percent);

	/* TODO -- gather RX ERROR stats */

	rx_err_delta = rx_err - priv->last_rx_err;
	priv->last_rx_err = rx_err;

	/* TODO -- gather TX failure stats */

	tx_failures_delta = tx_failures - priv->last_tx_failures;
	priv->last_tx_failures = tx_failures;
	rx_packets_delta = priv->rx_packets - priv->last_rx_packets;
	priv->last_rx_packets = priv->rx_packets;
	tx_packets_delta = priv->tx_packets - priv->last_tx_packets;
	priv->last_tx_packets = priv->tx_packets;
	/* Calculate quality based on the following:
	 *
	 * Missed beacon: 100% = 0, 0% = 70% missed
	 * Rate: 60% = 1Mbs, 100% = Max
	 * Rx and Tx errors represent a straight % of total Rx/Tx
	 * RSSI: 100% = > -50,  0% = < -80
	 * Rx errors: 100% = 0, 0% = 50% missed
	 *
	 * The lowest computed quality is used.
	 *
	 */
#define BEACON_THRESHOLD 5
	beacon_quality = 100 - missed_beacons_percent;
	if (beacon_quality < BEACON_THRESHOLD)
		beacon_quality = 0;
	else
		beacon_quality = (beacon_quality - BEACON_THRESHOLD) * 100 /
		    (100 - BEACON_THRESHOLD);
	IPW_DEBUG_STATS("Missed beacon: %3d%% (%d%%)\n",
			beacon_quality, missed_beacons_percent);
	priv->last_rate = ipw_get_current_rate(priv);
	max_rate = ipw_get_max_rate(priv);
	rate_quality = priv->last_rate * 40 / max_rate + 60;
	IPW_DEBUG_STATS("Rate quality : %3d%% (%dMbs)\n",
			rate_quality, priv->last_rate / 1000000);
	if (rx_packets_delta > 100 && rx_packets_delta + rx_err_delta)
		rx_quality = 100 - (rx_err_delta * 100) /
		    (rx_packets_delta + rx_err_delta);
	else
		rx_quality = 100;
	IPW_DEBUG_STATS("Rx quality   : %3d%% (%u errors, %u packets)\n",
			rx_quality, rx_err_delta, rx_packets_delta);
	if (tx_packets_delta > 100 && tx_packets_delta + tx_failures_delta)
		tx_quality = 100 - (tx_failures_delta * 100) /
		    (tx_packets_delta + tx_failures_delta);
	else
		tx_quality = 100;
	IPW_DEBUG_STATS("Tx quality   : %3d%% (%u errors, %u packets)\n",
			tx_quality, tx_failures_delta, tx_packets_delta);
	rssi = average_value(&priv->average_rssi);
	signal_quality =
	    (100 *
	     (priv->ieee->perfect_rssi - priv->ieee->worst_rssi) *
	     (priv->ieee->perfect_rssi - priv->ieee->worst_rssi) -
	     (priv->ieee->perfect_rssi - rssi) *
	     (15 * (priv->ieee->perfect_rssi - priv->ieee->worst_rssi) +
	      62 * (priv->ieee->perfect_rssi - rssi))) /
	    ((priv->ieee->perfect_rssi - priv->ieee->worst_rssi) *
	     (priv->ieee->perfect_rssi - priv->ieee->worst_rssi));
	if (signal_quality > 100)
		signal_quality = 100;
	else if (signal_quality < 1)
		signal_quality = 0;
	IPW_DEBUG_STATS("Signal level : %3d%% (%d dBm)\n",
			signal_quality, rssi);
	quality = min(beacon_quality,
		      min(rate_quality,
			  min(tx_quality, min(rx_quality, signal_quality))));
	if (quality == beacon_quality)
		IPW_DEBUG_STATS("Quality (%d%%): Clamped to missed beacons.\n",
				quality);
	if (quality == rate_quality)
		IPW_DEBUG_STATS("Quality (%d%%): Clamped to rate quality.\n",
				quality);
	if (quality == tx_quality)
		IPW_DEBUG_STATS("Quality (%d%%): Clamped to Tx quality.\n",
				quality);
	if (quality == rx_quality)
		IPW_DEBUG_STATS("Quality (%d%%): Clamped to Rx quality.\n",
				quality);
	if (quality == signal_quality)
		IPW_DEBUG_STATS("Quality (%d%%): Clamped to signal quality.\n",
				quality);
	priv->quality = quality;
	queue_delayed_work(priv->workqueue, &priv->gather_stats,
			   IPW_STATS_INTERVAL);
}

static void ipw_bg_gather_stats(void *data)
{
	struct ipw_priv *priv = data;

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	ipw_gather_stats(data);
	mutex_unlock(&priv->mutex);
}

static void ipw_kickoff_roaming(unsigned long data)
{
	struct ipw_priv *priv = (struct ipw_priv *)data;

	/* If the user changed the roaming enable/disable through
	 * sysfs while the roaming timer was configured, we just
	 * return when roaming disengages */
	if (priv->status & CFG_NO_ROAMING)
		return;

	if (priv->status & STATUS_ROAMING) {
		/* If we are currently roaming, then just
		 * print a debug statement... */
		IPW_DEBUG(IPW_DL_NOTIF | IPW_DL_STATE, "roam in progress\n");
		return;
	}

	/* If we are not already roaming, set the ROAM
	 * bit in the status and kick off a scan */
	IPW_DEBUG(IPW_DL_NOTIF | IPW_DL_STATE, "initiate roaming\n");
	if (!(priv->status & STATUS_ROAMING)) {
		priv->status |= STATUS_ROAMING;
		if (!(priv->status & STATUS_SCANNING)) {
			/* We only roam on the same band we are currently
			 * on */
			ipw_scan_initiate(priv,
					  priv->assoc_network->stats.freq, 0);
		}
	}
	return;
}

static void ipw_kickoff_disassociate(unsigned long data)
{
	struct ipw_priv *priv = (struct ipw_priv *)data;

	if ((priv->ieee->iw_mode == IW_MODE_ADHOC) &&
	    (priv->config & CFG_ADHOC_PERSIST))
		return;

	if (priv->status & STATUS_ASSOCIATED) {
		/* If associated and we've hit the missed
		 * beacon threshold, disassociate, turn
		 * off roaming, and abort any active scans */
		IPW_DEBUG(IPW_DL_INFO | IPW_DL_NOTIF | IPW_DL_STATE,
			  "Missed beacon hits disassociate threshold\n");
		priv->status &= ~STATUS_ROAMING;
		if (priv->status & STATUS_SCANNING) {
			IPW_DEBUG(IPW_DL_INFO | IPW_DL_NOTIF | IPW_DL_STATE,
				  "Aborting scan with missed beacon.\n");
			queue_work(priv->workqueue, &priv->abort_scan);
		}

		queue_work(priv->workqueue, &priv->disassociate);
		return;
	}

	if (priv->status & STATUS_SCANNING) {
		/* Stop scan to keep fw from getting
		 * stuck (only if we aren't roaming --
		 * otherwise we'll never scan more than 2 or 3
		 * channels..) */
		IPW_DEBUG(IPW_DL_INFO | IPW_DL_NOTIF | IPW_DL_STATE,
			  "Aborting scan with missed beacon.\n");
		queue_work(priv->workqueue, &priv->abort_scan);
	}
	return;
}

static int ipw_queue_tx_hcmd(struct ipw_priv *priv, struct ipw_host_cmd *cmd)
{
	struct ipw_tx_queue *txq = &priv->txq[CMD_QUEUE_NUM];
	struct ipw_queue *q = &txq->q;
	struct tfd_frame *tfd;
	struct ipw_cmd *out_cmd;
	u32 idx = 0;
	u16 fix_size = (u16) (cmd->meta.len + sizeof(out_cmd->hdr));
	int pad;
	dma_addr_t phys_addr;
	u8 fifo = CMD_QUEUE_NUM;
	u16 count;
	int rc;

	/* If any of the command structures end up being larger than
	 * the TFD_MAX_PAYLOAD_SIZE, and it sent as a 'small' command then
	 * we will need to increase the size of the TFD entries */
	BUG_ON((fix_size > TFD_MAX_PAYLOAD_SIZE) && is_cmd_small(cmd));
	if (ipw_queue_space(q) < (is_cmd_sync(cmd) ? 1 : 2)) {
		IPW_ERROR("No space for Tx\n");
		return -ENOSPC;
	}

	tfd = &txq->bd[q->first_empty];
	memset(tfd, 0, sizeof(*tfd));

	txq->txb[q->first_empty] = NULL;

	idx = get_next_cmd_index(q, q->first_empty,
				 cmd->meta.flags & CMD_SIZE_HUGE);
	out_cmd = &txq->cmd[idx];

	out_cmd->hdr.cmd = cmd->id;
	memcpy(&out_cmd->meta, &cmd->meta, sizeof(cmd->meta));
	memcpy(&out_cmd->cmd.payload, cmd->data, cmd->meta.len);

	/* At this point, the out_cmd now has all of the incoming cmd
	 * information */

	out_cmd->hdr.flags = 0;
	out_cmd->hdr.sequence = FIFO_TO_SEQ(fifo) |
	    INDEX_TO_SEQ(q->first_empty);
	if (out_cmd->meta.flags & CMD_SIZE_HUGE)
		out_cmd->hdr.sequence |= SEQ_HUGE_FRAME;

	phys_addr = txq->dma_addr_cmd + sizeof(txq->cmd[0]) * idx +
	    offsetof(struct ipw_cmd, hdr);
	attach_buffer_to_tfd_frame(tfd, phys_addr, fix_size);

	pad = U32_PAD(out_cmd->meta.len);
	count = TFD_CTL_COUNT_GET(tfd->control_flags);
	tfd->control_flags = TFD_CTL_COUNT_SET(count) | TFD_CTL_PAD_SET(pad);

/*	IPW_DEBUG_TX("TFD %d - %d chunks for %d bytes (%d pad) at 0x%08X:\n",
		     q->first_empty, TFD_CTL_COUNT_GET(tfd->control_flags),
		     tfd->pa[0].len, TFD_CTL_PAD_GET(tfd->control_flags),
		     tfd->pa[0].addr);
		     printk_buf(IPW_DL_TX, (u8 *) tfd, sizeof(*tfd));*/

	if (out_cmd->hdr.cmd != 0x23 &&
	    out_cmd->hdr.cmd != 0x24 && out_cmd->hdr.cmd != 0x22) {
		IPW_DEBUG_HC("Sending command %s (#%x), seq: 0x%04X, "
			     "%d bytes%s at %d[%d]:%d\n",
			     get_cmd_string(out_cmd->hdr.cmd),
			     out_cmd->hdr.cmd, out_cmd->hdr.sequence,
			     fix_size,
			     (out_cmd->meta.flags & CMD_INDIRECT) ? "*" : "",
			     q->first_empty, idx, fifo);
	}

	q->first_empty = ipw_queue_inc_wrap(q->first_empty, q->n_bd);

	txq->need_update = 1;
	rc = ipw_tx_queue_update_write_ptr(priv, txq, CMD_QUEUE_NUM);
	if (rc)
		return rc;

	return 0;
}

static int ipw_is_rate_in_mask(struct ipw_priv *priv, int ieee_mode, u8 rate)
{
	rate &= ~IEEE80211_BASIC_RATE_MASK;
	if (ieee_mode == IEEE_A) {
		switch (rate) {
		case IEEE80211_OFDM_RATE_6MB:
			return priv->
			    rates_mask & IEEE80211_OFDM_RATE_6MB_MASK ? 1 : 0;
		case IEEE80211_OFDM_RATE_9MB:
			return priv->
			    rates_mask & IEEE80211_OFDM_RATE_9MB_MASK ? 1 : 0;
		case IEEE80211_OFDM_RATE_12MB:
			return priv->
			    rates_mask & IEEE80211_OFDM_RATE_12MB_MASK ? 1 : 0;
		case IEEE80211_OFDM_RATE_18MB:
			return priv->
			    rates_mask & IEEE80211_OFDM_RATE_18MB_MASK ? 1 : 0;
		case IEEE80211_OFDM_RATE_24MB:
			return priv->
			    rates_mask & IEEE80211_OFDM_RATE_24MB_MASK ? 1 : 0;
		case IEEE80211_OFDM_RATE_36MB:
			return priv->
			    rates_mask & IEEE80211_OFDM_RATE_36MB_MASK ? 1 : 0;
		case IEEE80211_OFDM_RATE_48MB:
			return priv->
			    rates_mask & IEEE80211_OFDM_RATE_48MB_MASK ? 1 : 0;
		case IEEE80211_OFDM_RATE_54MB:
			return priv->
			    rates_mask & IEEE80211_OFDM_RATE_54MB_MASK ? 1 : 0;
		default:
			return 0;
		}
	}

	/* B and G mixed */
	switch (rate) {
	case IEEE80211_CCK_RATE_1MB:
		return priv->rates_mask & IEEE80211_CCK_RATE_1MB_MASK ? 1 : 0;
	case IEEE80211_CCK_RATE_2MB:
		return priv->rates_mask & IEEE80211_CCK_RATE_2MB_MASK ? 1 : 0;
	case IEEE80211_CCK_RATE_5MB:
		return priv->rates_mask & IEEE80211_CCK_RATE_5MB_MASK ? 1 : 0;
	case IEEE80211_CCK_RATE_11MB:
		return priv->rates_mask & IEEE80211_CCK_RATE_11MB_MASK ? 1 : 0;
	}

	/* If we are limited to B modulations, bail at this point */
	if (ieee_mode == IEEE_B)
		return 0;
	/* G */
	switch (rate) {
	case IEEE80211_OFDM_RATE_6MB:
		return priv->rates_mask & IEEE80211_OFDM_RATE_6MB_MASK ? 1 : 0;
	case IEEE80211_OFDM_RATE_9MB:
		return priv->rates_mask & IEEE80211_OFDM_RATE_9MB_MASK ? 1 : 0;
	case IEEE80211_OFDM_RATE_12MB:
		return priv->rates_mask & IEEE80211_OFDM_RATE_12MB_MASK ? 1 : 0;
	case IEEE80211_OFDM_RATE_18MB:
		return priv->rates_mask & IEEE80211_OFDM_RATE_18MB_MASK ? 1 : 0;
	case IEEE80211_OFDM_RATE_24MB:
		return priv->rates_mask & IEEE80211_OFDM_RATE_24MB_MASK ? 1 : 0;
	case IEEE80211_OFDM_RATE_36MB:
		return priv->rates_mask & IEEE80211_OFDM_RATE_36MB_MASK ? 1 : 0;
	case IEEE80211_OFDM_RATE_48MB:
		return priv->rates_mask & IEEE80211_OFDM_RATE_48MB_MASK ? 1 : 0;
	case IEEE80211_OFDM_RATE_54MB:
		return priv->rates_mask & IEEE80211_OFDM_RATE_54MB_MASK ? 1 : 0;
	}

	return 0;
}

static int ipw_compatible_rates(struct ipw_priv *priv, const struct ieee80211_network
				*network, struct ipw_supported_rates *rates)
{
	int num_rates, i;
	memset(rates, 0, sizeof(*rates));
	num_rates = min(network->rates_len, (u8) IPW_MAX_RATES);
	rates->num_rates = 0;
	for (i = 0; i < num_rates; i++) {
		if (!ipw_is_rate_in_mask
		    (priv, network->mode, network->rates[i])) {

			if (network->rates[i] & IEEE80211_BASIC_RATE_MASK) {
				IPW_DEBUG_SCAN("Adding masked mandatory "
					       "rate %02X\n",
					       network->rates[i]);
				rates->supported_rates[rates->num_rates++] =
				    network->rates[i];
				continue;
			}

			IPW_DEBUG_SCAN("Rate %02X masked : 0x%08X\n",
				       network->rates[i], priv->rates_mask);
			continue;
		}

		rates->supported_rates[rates->num_rates++] = network->rates[i];
	}

	num_rates = min(network->rates_ex_len,
			(u8) (IPW_MAX_RATES - num_rates));
	for (i = 0; i < num_rates; i++) {
		if (!ipw_is_rate_in_mask
		    (priv, network->mode, network->rates_ex[i])) {
			if (network->rates_ex[i] & IEEE80211_BASIC_RATE_MASK) {
				IPW_DEBUG_SCAN("Adding masked mandatory "
					       "rate %02X\n",
					       network->rates_ex[i]);
				rates->supported_rates[rates->num_rates++] =
				    network->rates[i];
				continue;
			}

			IPW_DEBUG_SCAN("Rate %02X masked : 0x%08X\n",
				       network->rates_ex[i], priv->rates_mask);
			continue;
		}

		rates->supported_rates[rates->num_rates++] =
		    network->rates_ex[i];
	}

	return 1;
}

static inline void ipw_copy_rates(struct ipw_supported_rates *dest, const struct ipw_supported_rates
				  *src)
{
	u8 i;
	for (i = 0; i < src->num_rates; i++)
		dest->supported_rates[i] = src->supported_rates[i];
	dest->num_rates = src->num_rates;
}

/* TODO: Look at sniffed packets in the air to determine if the basic rate
 * mask should ever be used -- right now all callers to add the scan rates are
 * set with the modulation = CCK, so BASIC_RATE_MASK is never set... */
static void ipw_add_cck_scan_rates(struct ipw_supported_rates
				   *rates, u8 modulation, u32 rate_mask)
{
	u8 basic_mask =
	    (IEEE80211_OFDM_MODULATION ==
	     modulation) ? IEEE80211_BASIC_RATE_MASK : 0;
	if (rate_mask & IEEE80211_CCK_RATE_1MB_MASK)
		rates->supported_rates[rates->num_rates++] =
		    IEEE80211_BASIC_RATE_MASK | IEEE80211_CCK_RATE_1MB;
	if (rate_mask & IEEE80211_CCK_RATE_2MB_MASK)
		rates->supported_rates[rates->num_rates++] =
		    IEEE80211_BASIC_RATE_MASK | IEEE80211_CCK_RATE_2MB;
	if (rate_mask & IEEE80211_CCK_RATE_5MB_MASK)
		rates->supported_rates[rates->num_rates++] =
		    basic_mask | IEEE80211_CCK_RATE_5MB;
	if (rate_mask & IEEE80211_CCK_RATE_11MB_MASK)
		rates->supported_rates[rates->num_rates++] =
		    basic_mask | IEEE80211_CCK_RATE_11MB;
}

static void ipw_add_ofdm_scan_rates(struct ipw_supported_rates
				    *rates, u8 modulation, u32 rate_mask)
{
	u8 basic_mask =
	    (IEEE80211_OFDM_MODULATION ==
	     modulation) ? IEEE80211_BASIC_RATE_MASK : 0;
	if (rate_mask & IEEE80211_OFDM_RATE_6MB_MASK)
		rates->supported_rates[rates->num_rates++] =
		    basic_mask | IEEE80211_OFDM_RATE_6MB;
	if (rate_mask & IEEE80211_OFDM_RATE_9MB_MASK)
		rates->supported_rates[rates->num_rates++] =
		    IEEE80211_OFDM_RATE_9MB;
	if (rate_mask & IEEE80211_OFDM_RATE_12MB_MASK)
		rates->supported_rates[rates->num_rates++] =
		    basic_mask | IEEE80211_OFDM_RATE_12MB;
	if (rate_mask & IEEE80211_OFDM_RATE_18MB_MASK)
		rates->supported_rates[rates->num_rates++] =
		    IEEE80211_OFDM_RATE_18MB;
	if (rate_mask & IEEE80211_OFDM_RATE_24MB_MASK)
		rates->supported_rates[rates->num_rates++] =
		    basic_mask | IEEE80211_OFDM_RATE_24MB;
	if (rate_mask & IEEE80211_OFDM_RATE_36MB_MASK)
		rates->supported_rates[rates->num_rates++] =
		    IEEE80211_OFDM_RATE_36MB;
	if (rate_mask & IEEE80211_OFDM_RATE_48MB_MASK)
		rates->supported_rates[rates->num_rates++] =
		    IEEE80211_OFDM_RATE_48MB;
	if (rate_mask & IEEE80211_OFDM_RATE_54MB_MASK)
		rates->supported_rates[rates->num_rates++] =
		    IEEE80211_OFDM_RATE_54MB;
}

struct ipw_network_match {
	struct ieee80211_network *network;
	struct ipw_supported_rates rates;
};
static int ipw_find_adhoc_network(struct ipw_priv *priv,
				  struct ipw_network_match *match,
				  struct ieee80211_network *network,
				  int roaming)
{
	struct ipw_supported_rates rates;
	/* Verify that this network's capability is compatible with the
	 * current mode (AdHoc or Infrastructure) */
	if ((priv->ieee->iw_mode == IW_MODE_ADHOC &&
	     !(network->capability & WLAN_CAPABILITY_IBSS))) {
		IPW_DEBUG_MERGE("Network '%s (" MAC_FMT ")' excluded due to "
				"capability mismatch.\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid));
		return 0;
	}

	/* If we do not have an ESSID for this AP, we can not associate with
	 * it */
	if (network->flags & NETWORK_EMPTY_ESSID) {
		IPW_DEBUG_MERGE("Network '%s (" MAC_FMT ")' excluded "
				"because of hidden ESSID.\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid));
		return 0;
	}

	if (unlikely(roaming)) {
		/* If we are roaming, then ensure check if this is a valid
		 * network to try and roam to */
		if ((network->ssid_len != match->network->ssid_len) ||
		    memcmp(network->ssid, match->network->ssid,
			   network->ssid_len)) {
			IPW_DEBUG_MERGE("Netowrk '%s (" MAC_FMT ")' excluded "
					"because of non-network ESSID.\n",
					escape_essid(network->ssid,
						     network->ssid_len),
					MAC_ARG(network->bssid));
			return 0;
		}
	} else {
		/* If an ESSID has been configured then compare the broadcast
		 * ESSID to ours */
		if ((priv->config & CFG_STATIC_ESSID) &&
		    ((network->ssid_len != priv->essid_len) ||
		     memcmp(network->ssid, priv->essid,
			    min(network->ssid_len, priv->essid_len)))) {
			char escaped[IW_ESSID_MAX_SIZE * 2 + 1];
			strncpy(escaped,
				escape_essid(network->ssid, network->ssid_len),
				sizeof(escaped));
			IPW_DEBUG_MERGE("Network '%s (" MAC_FMT ")' excluded "
					"because of ESSID mismatch: '%s'.\n",
					escaped, MAC_ARG(network->bssid),
					escape_essid(priv->essid,
						     priv->essid_len));
			return 0;
		}
	}

	/* If the old network rate is better than this one, don't bother
	 * testing everything else. */

	if (network->time_stamp[0] < match->network->time_stamp[0]) {
		IPW_DEBUG_MERGE
		    ("Network '%s excluded because newer than current network.\n",
		     escape_essid(match->network->ssid,
				  match->network->ssid_len));
		return 0;
	} else if (network->time_stamp[1] < match->network->time_stamp[1]) {
		IPW_DEBUG_MERGE
		    ("Network '%s excluded because newer than current network.\n",
		     escape_essid(match->network->ssid,
				  match->network->ssid_len));
		return 0;
	}

	/* Now go through and see if the requested network is valid... */
	if (priv->ieee->scan_age != 0 &&
	    time_after(jiffies, network->last_scanned + priv->ieee->scan_age)) {
		IPW_DEBUG_MERGE("Network '%s (" MAC_FMT ")' excluded "
				"because of age: %ums (limit=%ums)\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid),
				jiffies_to_msecs(elapsed_jiffies
						 (network->last_scanned,
						  jiffies)),
				jiffies_to_msecs(priv->ieee->scan_age));
		return 0;
	}

	if ((priv->config & CFG_STATIC_CHANNEL) &&
	    (network->channel != priv->channel)) {
		IPW_DEBUG_MERGE("Network '%s (" MAC_FMT ")' excluded "
				"because of channel mismatch: %d != %d.\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid),
				network->channel, priv->channel);
		return 0;
	}

	/* Verify privacy compatability */
	if (((priv->capability & CAP_PRIVACY_ON) ? 1 : 0) !=
	    ((network->capability & WLAN_CAPABILITY_PRIVACY) ? 1 : 0)) {
		IPW_DEBUG_MERGE("Network '%s (" MAC_FMT ")' excluded "
				"because of privacy mismatch: %s != %s.\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid),
				priv->capability & CAP_PRIVACY_ON ? "on" :
				"off",
				network->capability &
				WLAN_CAPABILITY_PRIVACY ? "on" : "off");
		return 0;
	}

	if (!memcmp(network->bssid, priv->bssid, ETH_ALEN)) {
		IPW_DEBUG_MERGE("Network '%s (" MAC_FMT ")' excluded "
				"because of the same BSSID match: " MAC_FMT
				".\n", escape_essid(network->ssid,
						    network->ssid_len),
				MAC_ARG(network->bssid), MAC_ARG(priv->bssid));
		return 0;
	}

	/* Filter out any incompatible freq / mode combinations */
	if (!ieee80211_is_valid_mode(priv->ieee, network->mode)) {
		IPW_DEBUG_MERGE("Network '%s (" MAC_FMT ")' excluded "
				"because of invalid frequency/mode "
				"combination.\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid));
		return 0;
	}

	/* Ensure that the rates supported by the driver are compatible with
	 * this AP, including verification of basic rates (mandatory) */
	if (!ipw_compatible_rates(priv, network, &rates)) {
		IPW_DEBUG_MERGE("Network '%s (" MAC_FMT ")' excluded "
				"because configured rate mask excludes "
				"AP mandatory rate.\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid));
		return 0;
	}

	if (rates.num_rates == 0) {
		IPW_DEBUG_MERGE("Network '%s (" MAC_FMT ")' excluded "
				"because of no compatible rates.\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid));
		return 0;
	}

	/* TODO: Perform any further minimal comparititive tests.  We do not
	 * want to put too much policy logic here; intelligent scan selection
	 * should occur within a generic IEEE 802.11 user space tool.  */

	/* Set up 'new' AP to this network */
	ipw_copy_rates(&match->rates, &rates);
	match->network = network;
	IPW_DEBUG_MERGE("Network '%s (" MAC_FMT ")' is a viable match.\n",
			escape_essid(network->ssid, network->ssid_len),
			MAC_ARG(network->bssid));
	return 1;
}

static void ipw_merge_adhoc_network(void *data)
{
	struct ipw_priv *priv = data;
	struct ieee80211_network *network = NULL;
	struct ipw_network_match match = {
		.network = priv->assoc_network
	};
	if ((priv->status & STATUS_ASSOCIATED)
	    && (priv->ieee->iw_mode == IW_MODE_ADHOC)) {
		/* First pass through ROAM process -- look for a better
		 * network */
		unsigned long flags;
		spin_lock_irqsave(&priv->ieee->lock, flags);
		list_for_each_entry(network, &priv->ieee->network_list, list) {
			if (network != priv->assoc_network)
				ipw_find_adhoc_network(priv, &match, network,
						       1);
		}
		spin_unlock_irqrestore(&priv->ieee->lock, flags);
		if (match.network == priv->assoc_network) {
			IPW_DEBUG_MERGE("No better ADHOC in this network to "
					"merge to.\n");
			return;
		}

		mutex_lock(&priv->mutex);
		if ((priv->ieee->iw_mode == IW_MODE_ADHOC)) {
			IPW_DEBUG_MERGE("remove network %s\n",
					escape_essid(priv->essid,
						     priv->essid_len));
			ipw_remove_current_network(priv);
		}

		ipw_disassociate(priv);
		priv->assoc_network = match.network;
		mutex_unlock(&priv->mutex);
		return;
	}

}

static int ipw_best_network(struct ipw_priv *priv,
			    struct ipw_network_match *match,
			    struct ieee80211_network *network, int roaming)
{
	struct ipw_supported_rates rates;
	/* Verify that this network's capability is compatible with the
	 * current mode (AdHoc or Infrastructure) */
	if ((priv->ieee->iw_mode == IW_MODE_INFRA &&
	     !(network->capability & WLAN_CAPABILITY_ESS)) ||
	    (priv->ieee->iw_mode == IW_MODE_ADHOC &&
	     !(network->capability & WLAN_CAPABILITY_IBSS))) {
		IPW_DEBUG_ASSOC("Network '%s (" MAC_FMT ")' excluded due to "
				"capability mismatch.\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid));
		return 0;
	}

	if (!ieee80211_is_valid_channel(priv->ieee, network->channel)) {
		IPW_DEBUG_SCAN("Invalid channel for  '%s (" MAC_FMT ")'.\n",
			       escape_essid(network->ssid,
					    network->ssid_len),
			       MAC_ARG(network->bssid));
		return 0;
	}

	/* 802.11h Sanity Checks */
	if ((ieee80211_get_channel_flags(priv->ieee, network->channel) &
	     IEEE80211_CH_RADAR_DETECT) &&
	    !(network->capability & WLAN_CAPABILITY_SPECTRUM_MGMT)) {
		IPW_DEBUG_SCAN("Network '%s (" MAC_FMT ")' "
			       "is invalid - Spectrum Management mismatch.\n",
			       escape_essid(network->ssid, network->ssid_len),
			       MAC_ARG(network->bssid));
		return 0;
	}

	if (network->capability & WLAN_CAPABILITY_SPECTRUM_MGMT) {
		if ((network->capability & WLAN_CAPABILITY_IBSS) &&
		    !(network->flags & NETWORK_HAS_IBSS_DFS)) {
			IPW_DEBUG_SCAN("Network '%s (" MAC_FMT ")' "
				       "is invalid - IBSS DFS mismatch.\n",
				       escape_essid(network->ssid,
						    network->ssid_len),
				       MAC_ARG(network->bssid));
			return 0;
		}

		if (!(network->flags & NETWORK_HAS_POWER_CONSTRAINT)) {
			IPW_DEBUG_SCAN("Spectrum managed network '%s (" MAC_FMT
				       ")' "
				       "but is missing Power Constraint.\n",
				       escape_essid(network->ssid,
						    network->ssid_len),
				       MAC_ARG(network->bssid));
		}

		if (!(network->flags & NETWORK_HAS_TPC_REPORT)) {
			IPW_DEBUG_SCAN("Spectrum managed network '%s (" MAC_FMT
				       ")' " "but is missing TPC.\n",
				       escape_essid(network->ssid,
						    network->ssid_len),
				       MAC_ARG(network->bssid));
		}
	}

	/* If we do not have an ESSID for this AP, we can not associate with
	 * it */
	if (network->flags & NETWORK_EMPTY_ESSID) {
		IPW_DEBUG_ASSOC("Network '%s (" MAC_FMT ")' excluded "
				"because of hidden ESSID.\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid));
		return 0;
	}

	if (unlikely(roaming)) {
		/* If we are roaming, then ensure check if this is a valid
		 * network to try and roam to */
		if ((network->ssid_len != match->network->ssid_len) ||
		    memcmp(network->ssid, match->network->ssid,
			   network->ssid_len)) {
			IPW_DEBUG_ASSOC("Netowrk '%s (" MAC_FMT ")' excluded "
					"because of non-network ESSID.\n",
					escape_essid(network->ssid,
						     network->ssid_len),
					MAC_ARG(network->bssid));
			return 0;
		}
	} else {
		/* If an ESSID has been configured then compare the broadcast
		 * ESSID to ours */
		if ((priv->config & CFG_STATIC_ESSID) &&
		    ((network->ssid_len != priv->essid_len) ||
		     memcmp(network->ssid, priv->essid,
			    min(network->ssid_len, priv->essid_len)))) {
			char escaped[IW_ESSID_MAX_SIZE * 2 + 1];
			strncpy(escaped,
				escape_essid(network->ssid, network->ssid_len),
				sizeof(escaped));
			IPW_DEBUG_ASSOC("Network '%s (" MAC_FMT ")' excluded "
					"because of ESSID mismatch: '%s'.\n",
					escaped, MAC_ARG(network->bssid),
					escape_essid(priv->essid,
						     priv->essid_len));
			return 0;
		}
	}

	/* If the old network rate is better than this one, don't bother
	 * testing everything else. */
	if (match->network && match->network->stats.rssi > network->stats.rssi) {
		char escaped[IW_ESSID_MAX_SIZE * 2 + 1];
		strncpy(escaped, escape_essid(network->ssid, network->ssid_len),
			sizeof(escaped));
		IPW_DEBUG_ASSOC("Network '%s (" MAC_FMT ")' excluded because "
				"'%s (" MAC_FMT ")' has a stronger signal.\n",
				escaped, MAC_ARG(network->bssid),
				escape_essid(match->network->ssid,
					     match->network->ssid_len),
				MAC_ARG(match->network->bssid));
		return 0;
	}

	/* If this network has already had an association attempt within the
	 * last 3 seconds, do not try and associate again... */
	if (network->last_associate &&
	    time_after(network->last_associate + (HZ * 3UL), jiffies)) {
		IPW_DEBUG_ASSOC("Network '%s (" MAC_FMT ")' excluded "
				"because of storming (%lu since last "
				"assoc attempt).\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid),
				(jiffies - network->last_associate) / HZ);
		return 0;
	}

	/* Now go through and see if the requested network is valid... */
	if (priv->ieee->scan_age != 0 &&
	    time_after(jiffies, network->last_scanned + priv->ieee->scan_age)) {
		IPW_DEBUG_ASSOC("Network '%s (" MAC_FMT ")' excluded "
				"because of age: %ums (limit=%ums)\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid),
				jiffies_to_msecs(elapsed_jiffies
						 (network->last_scanned,
						  jiffies)),
				jiffies_to_msecs(priv->ieee->scan_age));
		return 0;
	}

	if ((priv->config & CFG_STATIC_CHANNEL) &&
	    (network->channel != priv->channel)) {
		IPW_DEBUG_ASSOC("Network '%s (" MAC_FMT ")' excluded "
				"because of channel mismatch: %d != %d.\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid),
				network->channel, priv->channel);
		return 0;
	}

	/* Verify privacy compatability */
	if (((priv->capability & CAP_PRIVACY_ON) ? 1 : 0) !=
	    ((network->capability & WLAN_CAPABILITY_PRIVACY) ? 1 : 0)) {
		IPW_DEBUG_ASSOC("Network '%s (" MAC_FMT ")' excluded "
				"because of privacy mismatch: %s != %s.\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid),
				priv->capability & CAP_PRIVACY_ON ? "on" :
				"off",
				network->capability &
				WLAN_CAPABILITY_PRIVACY ? "on" : "off");
		return 0;
	}

	if ((priv->config & CFG_STATIC_BSSID) &&
	    memcmp(network->bssid, priv->bssid, ETH_ALEN)) {
		IPW_DEBUG_ASSOC("Network '%s (" MAC_FMT ")' excluded "
				"because of BSSID mismatch: " MAC_FMT ".\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid), MAC_ARG(priv->bssid));
		return 0;
	}

	/* Filter out any incompatible freq / mode combinations */
	if (!ieee80211_is_valid_mode(priv->ieee, network->mode)) {
		IPW_DEBUG_ASSOC("Network '%s (" MAC_FMT ")' excluded "
				"because of invalid frequency/mode "
				"combination.\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid));
		return 0;
	}

	/* Ensure that the rates supported by the driver are compatible with
	 * this AP, including verification of basic rates (mandatory) */
	if (!ipw_compatible_rates(priv, network, &rates)) {
		IPW_DEBUG_ASSOC("Network '%s (" MAC_FMT ")' excluded "
				"because configured rate mask excludes "
				"AP mandatory rate.\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid));
		return 0;
	}

	if (rates.num_rates == 0) {
		IPW_DEBUG_ASSOC("Network '%s (" MAC_FMT ")' excluded "
				"because of no compatible rates.\n",
				escape_essid(network->ssid, network->ssid_len),
				MAC_ARG(network->bssid));
		return 0;
	}

	/* TODO: Perform any further minimal comparititive tests.  We do not
	 * want to put too much policy logic here; intelligent scan selection
	 * should occur within a generic IEEE 802.11 user space tool.  */

	/* Set up 'new' AP to this network */
	ipw_copy_rates(&match->rates, &rates);
	match->network = network;
	IPW_DEBUG_ASSOC("Network '%s (" MAC_FMT ")' is a viable match.\n",
			escape_essid(network->ssid, network->ssid_len),
			MAC_ARG(network->bssid));
	return 1;
}

static void ipw_adhoc_create(struct ipw_priv *priv,
			     struct ieee80211_network *network)
{
	const struct ieee80211_geo *geo = ieee80211_get_geo(priv->ieee);
	int i;

	if (!ipw_is_ready(priv)) {
		IPW_WARNING("adhoc create called when driver not ready.\n");
		return;
	}

	/*
	 * For the purposes of scanning, we can set our wireless mode
	 * to trigger scans across combinations of bands, but when it
	 * comes to creating a new ad-hoc network, we have tell the FW
	 * exactly which band to use.
	 *
	 * We also have the possibility of an invalid channel for the
	 * chossen band.  Attempting to create a new ad-hoc network
	 * with an invalid channel for wireless mode will trigger a
	 * FW fatal error.
	 */
	switch (ieee80211_is_valid_channel(priv->ieee, priv->channel)) {
	case IEEE80211_52GHZ_BAND:
		network->mode = IEEE_A;
		i = ieee80211_channel_to_index(priv->ieee, priv->channel);
		if (i == -1)
			BUG();
		/*
		   if (geo->a[i].passive_only) {
		   IPW_WARNING("Overriding invalid channel\n");
		   priv->channel = geo->a[0].channel;
		   }
		 */
		break;

	case IEEE80211_24GHZ_BAND:
		if (priv->ieee->mode & IEEE_G)
			network->mode = IEEE_G;
		else
			network->mode = IEEE_B;
		break;

	default:
		IPW_WARNING("Overriding invalid channel\n");
		if (priv->ieee->mode & IEEE_A) {
			network->mode = IEEE_A;
			priv->channel = geo->a[0].channel;
		} else if (priv->ieee->mode & IEEE_G) {
			network->mode = IEEE_G;
			priv->channel = geo->bg[0].channel;
		} else {
			network->mode = IEEE_B;
			priv->channel = geo->bg[0].channel;
		}
		break;
	}

	network->channel = priv->channel;
	priv->config |= CFG_ADHOC_PERSIST;
	ipw_create_bssid(priv, network->bssid);
	network->ssid_len = priv->essid_len;
	memcpy(network->ssid, priv->essid, priv->essid_len);
	memset(&network->stats, 0, sizeof(network->stats));
	network->capability = WLAN_CAPABILITY_IBSS;
	if (!(priv->config & CFG_PREAMBLE_LONG))
		network->capability |= WLAN_CAPABILITY_SHORT_PREAMBLE;
	if (priv->capability & CAP_PRIVACY_ON)
		network->capability |= WLAN_CAPABILITY_PRIVACY;
	network->rates_len = min(priv->rates.num_rates, MAX_RATES_LENGTH);
	memcpy(network->rates, priv->rates.supported_rates, network->rates_len);
	network->rates_ex_len = priv->rates.num_rates - network->rates_len;
	memcpy(network->rates_ex,
	       &priv->rates.supported_rates[network->rates_len],
	       network->rates_ex_len);
	network->last_scanned = 0;
	network->flags = 0;
	network->last_associate = 0;
	network->time_stamp[0] = 0;
	network->time_stamp[1] = 0;
	network->beacon_interval = 100;	/* Default */
	network->listen_interval = 10;	/* Default */
	network->atim_window = 0;	/* Default */
	network->wpa_ie_len = 0;
	network->rsn_ie_len = 0;
	network->erp_value = 0;
}

#ifdef CONFIG_IPW3945_DEBUG
static void ipw_debug_config(struct ipw_priv *priv)
{
	IPW_DEBUG_INFO("Scan completed, no valid APs matched "
		       "[CFG 0x%08X]\n", priv->config);
	if (priv->config & CFG_STATIC_CHANNEL)
		IPW_DEBUG_INFO("Channel locked to %d\n", priv->channel);
	else
		IPW_DEBUG_INFO("Channel unlocked.\n");
	if (priv->config & CFG_STATIC_ESSID)
		IPW_DEBUG_INFO("ESSID locked to '%s'\n",
			       escape_essid(priv->essid, priv->essid_len));
	else
		IPW_DEBUG_INFO("ESSID unlocked.\n");
	if (priv->config & CFG_STATIC_BSSID)
		IPW_DEBUG_INFO("BSSID locked to " MAC_FMT "\n",
			       MAC_ARG(priv->bssid));
	else
		IPW_DEBUG_INFO("BSSID unlocked.\n");
	if (priv->capability & CAP_PRIVACY_ON)
		IPW_DEBUG_INFO("PRIVACY on\n");
	else
		IPW_DEBUG_INFO("PRIVACY off\n");
	IPW_DEBUG_INFO("RATE MASK: 0x%08X\n", priv->rates_mask);
}
#else
#define ipw_debug_config(x) do {} while (0);
#endif

/* For active scan, listen ACTIVE_DWELL_TIME (msec) on each channel after
 * sending probe req.  This should be set long enough to hear probe responses
 * from more than one AP.  */
#define IPW_ACTIVE_DWELL_TIME_24    (20)	/* all times in msec */
#define IPW_ACTIVE_DWELL_TIME_52    (10)

/* For faster active scanning, scan will move to the next channel if fewer than
 * PLCP_QUIET_THRESH packets are heard on this channel within
 * ACTIVE_QUIET_TIME after sending probe request.  This shortens the dwell
 * time if it's a quiet channel (nothing responded to our probe, and there's
 * no other traffic).
 * Disable "quiet" feature by setting PLCP_QUIET_THRESH to 0. */
#define IPW_PLCP_QUIET_THRESH       (1)	/* packets */
#define IPW_ACTIVE_QUIET_TIME       (5)	/* msec */

/* For passive scan, listen PASSIVE_DWELL_TIME (msec) on each channel.
 * Must be set longer than active dwell time.
 * For the most reliable scan, set > AP beacon interval (typically 100msec). */
#define IPW_PASSIVE_DWELL_TIME_24   (20)	/* all times in msec */
#define IPW_PASSIVE_DWELL_TIME_52   (10)
#define IPW_PASSIVE_DWELL_BASE      (100)
#define IPW_CHANNEL_TUNE_TIME       5
static void ipw_abort_scan(struct ipw_priv *priv)
{
	priv->status &= ~STATUS_SCAN_PENDING;
	priv->status |= STATUS_SCAN_ABORTING;
	ipw_send_scan_abort(priv);
}

static inline u16 ipw_get_active_dwell_time(struct ipw_priv *priv, u8 band)
{
	if (band == IEEE80211_24GHZ_BAND)
		return IPW_ACTIVE_DWELL_TIME_24;
	else
		return IPW_ACTIVE_DWELL_TIME_52;
}

static inline u16 ipw_get_passive_dwell_time(struct ipw_priv *priv, u8 band)
{
	u16 active = ipw_get_active_dwell_time(priv, band);
	u16 passive = (band == IEEE80211_24GHZ_BAND) ?
	    IPW_PASSIVE_DWELL_BASE + IPW_PASSIVE_DWELL_TIME_24 :
	    IPW_PASSIVE_DWELL_BASE + IPW_PASSIVE_DWELL_TIME_52;

	if (priv->status & STATUS_ASSOCIATED) {
		/* If we're associated, we clamp the maximum passive
		 * dwell time to be 98% of the beacon interval (minus
		 * 2 * channel tune time) */
		passive = priv->assoc_request.beacon_interval;
		if (passive > IPW_PASSIVE_DWELL_BASE)
			passive = IPW_PASSIVE_DWELL_BASE;
		passive = (passive * 98) / 100 - IPW_CHANNEL_TUNE_TIME * 2;
	}

	if (passive <= active)
		passive = active + 1;

	return passive;
}

static int ipw_get_channels_for_scan(struct ipw_priv *priv, u8 band,
				     u8 is_active,
				     struct daemon_scan_channel *scan_list,
				     int max_count)
{
	const struct ieee80211_geo *geo = ieee80211_get_geo(priv->ieee);
	u16 passive_dwell = 0;
	u16 active_dwell = 0;
	int indx, i;
	const struct ieee80211_channel *channels = NULL;
	int count = 0;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	if (band == IEEE80211_52GHZ_BAND) {
		channels = geo->a;
		count = geo->a_channels;
	} else if (band == IEEE80211_24GHZ_BAND) {
		channels = geo->bg;
		count = geo->bg_channels;
	} else {
		/* Band is a bogus value -- it must be a specific band to
		 * scan, not a full list of supported bands */
		BUG();
	}

	active_dwell = ipw_get_active_dwell_time(priv, band);
	passive_dwell = ipw_get_passive_dwell_time(priv, band);

	for (i = 0, indx = 0; (indx < max_count) && (i < count); i++) {
		/* If we're roaming, we only want to grab the current channel
		 * so skip the rest. */
		if (priv->status & STATUS_ROAMING) {
			if (channels[i].channel != priv->channel)
				continue;
		} else if (channels[i].channel == priv->channel) {
			if (priv->status & STATUS_ASSOCIATED) {
				IPW_DEBUG_SCAN("Skipping current channel %d\n",
					       priv->channel);
				continue;
			}
		}

		scan_list[indx].channel = channels[i].channel;
		/* If this is configured as a passive only channel
		 * then force the flags to passive, otherwise honor the
		 * user request for active / passive */
		scan_list[indx].request_active =
		    (channels[i].flags & IEEE80211_CH_PASSIVE_ONLY) ?
		    0 : is_active;
		scan_list[indx].active_dwell = active_dwell;
		scan_list[indx].passive_dwell = passive_dwell;
		IPW_DEBUG_SCAN("Scanning %d [%s %d]\n",
			       scan_list[indx].channel,
			       scan_list[indx].request_active ? "ACTIVE" :
			       "PASSIVE",
			       scan_list[indx].
			       request_active ? active_dwell :
			       scan_list[indx].passive_dwell);

		indx++;
	}

	IPW_DEBUG_SCAN("total channel to scan %d \n", indx);
	return indx;
}

static void ipw_bg_request_scan(void *data)
{
	struct ipw_daemon_cmd_list *element;
	struct ipw_priv *priv = data;
	int rc = 0;
	struct daemon_scan_request *scan;
	const struct ieee80211_geo *geo = ieee80211_get_geo(priv->ieee);
	u32 band = 0;

	if (!ipw_is_ready(priv)) {
		IPW_WARNING("request scan called when driver not ready.\n");
		return;
	}

	mutex_lock(&priv->mutex);

	/* This should never be called or scheduled if there is currently
	 * a scan active in the hardware. */
	if (priv->status & STATUS_SCAN_HW) {
		IPW_DEBUG_INFO("Multiple concurrent scan requests in parallel. "
			       "Ignoring second request.\n");
		rc = -EIO;
		goto done;
	}

	if (priv->status & STATUS_EXIT_PENDING) {
		IPW_DEBUG_SCAN("Aborting scan due to device shutdown\n");
		priv->status |= STATUS_SCAN_PENDING;
		goto done;
	}

	if (priv->status & STATUS_SCAN_ABORTING) {
		IPW_DEBUG_HC("Scan request while abort pending.  Queuing.\n");
		priv->status |= STATUS_SCAN_PENDING;
		goto done;
	}

	if (priv->status & STATUS_RF_KILL_MASK) {
		IPW_DEBUG_HC("Aborting scan due to RF Kill activation\n");
		priv->status |= STATUS_SCAN_PENDING;
		goto done;
	}

	if (!(priv->status & STATUS_READY)) {
		IPW_DEBUG_HC("Scan request while uninitialized.  Queuing.\n");
		priv->status |= STATUS_SCAN_PENDING;
		goto done;
	}

	if (!priv->scan_bands_remaining) {
		IPW_DEBUG_HC("Aborting scan due to no requested bands\n");
		goto done;
	}

	element = kmalloc(sizeof(*element) + DAEMON_MAX_SCAN_SIZE, GFP_ATOMIC);
	if (!element) {
		rc = -ENOMEM;
		goto done;
	}

	memset(element, 0, sizeof(struct daemon_scan_request));
	element->cmd.cmd = DAEMON_SCAN_REQUEST;

	scan = (struct daemon_scan_request *)element->cmd.data;

	scan->quiet_plcp_th = IPW_PLCP_QUIET_THRESH;
	scan->quiet_time = IPW_ACTIVE_QUIET_TIME;

	if (priv->status & STATUS_ASSOCIATED) {
		u16 interval = priv->assoc_request.beacon_interval;
		u32 extra;

		IPW_DEBUG_INFO("Scanning while associated...\n");
		scan->suspend_time = 100;
		scan->max_out_time = 600 * 1024;
		if (interval) {
			/*
			 * suspend time format:
			 *  0-19: beacon interval in usec (time before exec.)
			 * 20-23: 0
			 * 24-31: number of beacons (suspend between channels)
			 */

			extra = (scan->suspend_time / interval) << 24;
			scan->suspend_time = 0xFF0FFFFF & (extra |
							   ((scan->
							     suspend_time %
							     interval) * 1024));
		}
	}

	/* We should add the ability for user to lock to PASSIVE ONLY */
	scan->flags |= DAEMON_SCAN_FLAG_ACTIVE;

	if (scan->flags & DAEMON_SCAN_FLAG_ACTIVE) {
		if ((priv->config & CFG_STATIC_ESSID) &&
		    !(priv->status & STATUS_ASSOCIATED)) {
			scan->flags |= DAEMON_SCAN_FLAG_DIRECT;
			scan->direct_scan.id = MFIE_TYPE_SSID;
			scan->direct_scan.len = priv->essid_len;
			memcpy(scan->direct_scan.ssid,
			       priv->essid, priv->essid_len);
		}

		scan->probe_request_len =
		    ipw_fill_probe_req(priv,
				       (struct ieee80211_probe_request *)scan->
				       data,
				       DAEMON_MAX_SCAN_SIZE - sizeof(scan), 0);
	} else
		scan->probe_request_len = 0;

	/* flags + rate selection */

	if (priv->scan_bands_remaining & IEEE80211_24GHZ_BAND) {
		band = IEEE80211_24GHZ_BAND;
		scan->flags |= DAEMON_SCAN_FLAG_24GHZ;
		scan->rxon_flags =
		    RXON_FLG_BAND_24G_MSK | RXON_FLG_AUTO_DETECT_MSK;
		scan->probe_request_rate = R_1M;

	} else if (priv->scan_bands_remaining & IEEE80211_52GHZ_BAND) {
		band = IEEE80211_52GHZ_BAND;
		scan->flags |= DAEMON_SCAN_FLAG_52GHZ;
		scan->probe_request_rate = R_6M;
	} else {
		printk(KERN_ERR "Invalid scan_bands_remaining bitmask.\n");
		BUG();
	}

	scan->rxon_flags |= ipw_get_antenna_flags(priv);

#ifdef CONFIG_IPW3945_MONITOR
	if (priv->ieee->iw_mode == IW_MODE_MONITOR)
		scan->filter_flags = RXON_FILTER_PROMISC_MSK;
#endif

	if (scan->flags & DAEMON_SCAN_FLAG_ACTIVE) {
		if ((priv->config & CFG_STATIC_ESSID) &&
		    !(priv->status & STATUS_ASSOCIATED))
			IPW_DEBUG_SCAN("Initiating direct scan for %s.\n",
				       escape_essid(priv->essid,
						    priv->essid_len));
		else
			IPW_DEBUG_SCAN("Initiating indirect scan.\n");
	} else
		IPW_DEBUG_SCAN("Initiating passive scan.\n");

	scan->channel_count = ipw_get_channels_for_scan(priv, band,
							scan->
							flags &
							DAEMON_SCAN_FLAG_ACTIVE,
							(struct
							 daemon_scan_channel *)
							&scan->data[scan->
								    probe_request_len],
							geo->a_channels +
							geo->bg_channels);

	element->cmd.data_len = sizeof(*scan) +
	    scan->probe_request_len +
	    scan->channel_count * sizeof(struct daemon_scan_channel);

	priv->scan_flags = scan->flags;

	priv->status |= STATUS_SCAN_HW;
	rc = ipw_send_daemon_cmd(priv, element);
	if (rc)
		goto done;

	queue_delayed_work(priv->workqueue, &priv->scan_check,
			   IPW_SCAN_CHECK_WATCHDOG);

	priv->status &= ~STATUS_SCAN_PENDING;

	goto done;

      done:
	if (!rc)
		ipw_update_link_led(priv);
	mutex_unlock(&priv->mutex);
}

static void ipw_bg_abort_scan(void *data)
{
	struct ipw_priv *priv = data;

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	if (priv->status & STATUS_READY)
		ipw_abort_scan(data);
	mutex_unlock(&priv->mutex);
}

enum {
#if WIRELESS_EXT < 18
	IPW_IOCTL_WPA_SUPPLICANT = SIOCIWFIRSTPRIV + 30,
#endif
	IPW_IOCTL_CMD = SIOCDEVPRIVATE + 1,
};

#if WIRELESS_EXT < 18
/* Support for wpa_supplicant. Will be replaced with WEXT once
 * they get WPA support. */

/* following definitions must match definitions in driver_ipw.c */

#define IPW_CMD_SET_WPA_PARAM			1
#define	IPW_CMD_SET_WPA_IE			2
#define IPW_CMD_SET_ENCRYPTION			3
#define IPW_CMD_MLME				4

#define IPW_PARAM_WPA_ENABLED			1
#define IPW_PARAM_TKIP_COUNTERMEASURES		2
#define IPW_PARAM_DROP_UNENCRYPTED		3
#define IPW_PARAM_PRIVACY_INVOKED		4
#define IPW_PARAM_AUTH_ALGS			5
#define IPW_PARAM_IEEE_802_1X			6

#define IPW_MLME_STA_DEAUTH			1
#define IPW_MLME_STA_DISASSOC			2

#define IPW_CRYPT_ERR_UNKNOWN_ALG		2
#define IPW_CRYPT_ERR_UNKNOWN_ADDR		3
#define IPW_CRYPT_ERR_CRYPT_INIT_FAILED		4
#define IPW_CRYPT_ERR_KEY_SET_FAILED		5
#define IPW_CRYPT_ERR_TX_KEY_SET_FAILED		6
#define IPW_CRYPT_ERR_CARD_CONF_FAILED		7

#define	IPW_CRYPT_ALG_NAME_LEN			16

struct ipw_param {
	u32 cmd;
	u8 sta_addr[ETH_ALEN];
	union {
		struct {
			u8 name;
			u32 value;
		} wpa_param;
		struct {
			u32 len;
			u8 reserved[32];
			u8 data[0];
		} wpa_ie;
		struct {
			int command;
			int reason;
		} mlme;
		struct {
			u8 alg[IPW_CRYPT_ALG_NAME_LEN];
			u8 set_tx;
			u32 err;
			u8 idx;
			u8 seq[8];	/* sequence counter (set: RX, get: TX) */
			u16 key_len;
			u8 key[0];
		} crypt;
	} u;
};

/* end of driver_ipw.c code */
#endif

static int ipw_wpa_enable(struct ipw_priv *priv, int value)
{
	/* This is called when wpa_supplicant loads and closes the driver
	 * interface. */
	return 0;
}

#if WIRELESS_EXT < 18
#define IW_AUTH_ALG_OPEN_SYSTEM			0x1
#define IW_AUTH_ALG_SHARED_KEY			0x2
#endif

static int ipw_wpa_set_auth_algs(struct ipw_priv *priv, int value)
{
	struct ieee80211_device *ieee = priv->ieee;
	struct ieee80211_security sec = {
		.flags = SEC_AUTH_MODE,
	};
	int ret = 0;
	if (value & IW_AUTH_ALG_SHARED_KEY) {
		sec.auth_mode = WLAN_AUTH_SHARED_KEY;
		ieee->open_wep = 0;
	} else {
		sec.auth_mode = WLAN_AUTH_OPEN;
		ieee->open_wep = 1;
	}

	if (ieee->set_security)
		ieee->set_security(ieee->dev, &sec);
	else
		ret = -EOPNOTSUPP;
	return ret;
}

//todoG is this still needed
static int ipw_set_rsn_capa(struct ipw_priv *priv,
			    char *capabilities, int length)
{
	return 0;
}

static void ipw_wpa_assoc_frame(struct ipw_priv *priv, char *wpa_ie,
				int wpa_ie_len)
{
	/* make sure WPA is enabled */
	ipw_wpa_enable(priv, 1);
	ipw_disassociate(priv);
}

#if WIRELESS_EXT < 18
static int ipw_wpa_set_param(struct net_device *dev, u8 name, u32 value)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	struct ieee80211_crypt_data *crypt;
	unsigned long flags;
	int ret = 0;

	switch (name) {
	case IPW_PARAM_WPA_ENABLED:
		ret = ipw_wpa_enable(priv, value);
		break;
	case IPW_PARAM_TKIP_COUNTERMEASURES:
		crypt = priv->ieee->crypt[priv->ieee->tx_keyidx];
		if (!crypt || !crypt->ops->set_flags || !crypt->ops->get_flags) {
			IPW_WARNING("Can't set TKIP countermeasures: "
				    "crypt not set!\n");
			break;
		}

		flags = crypt->ops->get_flags(crypt->priv);

		if (value)
			flags |= IEEE80211_CRYPTO_TKIP_COUNTERMEASURES;
		else
			flags &= ~IEEE80211_CRYPTO_TKIP_COUNTERMEASURES;

		crypt->ops->set_flags(flags, crypt->priv);

		break;
	case IPW_PARAM_DROP_UNENCRYPTED:{
			/* HACK:
			 *
			 * wpa_supplicant calls set_wpa_enabled when the driver
			 * is loaded and unloaded, regardless of if WPA is being
			 * used.  No other calls are made which can be used to
			 * determine if encryption will be used or not prior to
			 * association being expected.  If encryption is not being
			 * used, drop_unencrypted is set to false, else true -- we
			 * can use this to determine if the CAP_PRIVACY_ON bit should
			 * be set.
			 */
			struct ieee80211_security sec = {
				.flags = SEC_LEVEL | SEC_ENABLED,.enabled =
				    value,.level =
				    value ? SEC_LEVEL_1 : SEC_LEVEL_0,
			};
			priv->ieee->drop_unencrypted = value;
			if (priv->ieee->set_security)
				priv->ieee->set_security(priv->ieee->dev, &sec);
			break;
		}

	case IPW_PARAM_PRIVACY_INVOKED:
		priv->ieee->privacy_invoked = value;
		break;
	case IPW_PARAM_AUTH_ALGS:
		ret = ipw_wpa_set_auth_algs(priv, value);
		break;
	case IPW_PARAM_IEEE_802_1X:
		priv->ieee->ieee802_1x = value;
		break;
	default:
		IPW_ERROR("%s: Unknown WPA param: %d\n", dev->name, name);
		ret = -EOPNOTSUPP;
	}

	return ret;
}

static int ipw_wpa_mlme(struct net_device *dev, int command, int reason)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	int ret = 0;
	switch (command) {
	case IPW_MLME_STA_DEAUTH:
		ipw_disassociate(priv);
		break;
	case IPW_MLME_STA_DISASSOC:
		ipw_disassociate(priv);
		break;
	default:
		IPW_ERROR("%s: Unknown MLME request: %d\n", dev->name, command);
		ret = -EOPNOTSUPP;
	}

	return ret;
}

static int ipw_wpa_set_wpa_ie(struct net_device *dev,
			      struct ipw_param *param, int plen)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee;
	u8 *buf;
	if (param->u.wpa_ie.len > MAX_WPA_IE_LEN ||
	    (param->u.wpa_ie.len && param->u.wpa_ie.data == NULL))
		return -EINVAL;
	if (param->u.wpa_ie.len) {
		buf = kmalloc(param->u.wpa_ie.len, GFP_KERNEL);
		if (buf == NULL)
			return -ENOMEM;
		memcpy(buf, param->u.wpa_ie.data, param->u.wpa_ie.len);
		kfree(ieee->wpa_ie);
		ieee->wpa_ie = buf;
		ieee->wpa_ie_len = param->u.wpa_ie.len;
	} else {
		kfree(ieee->wpa_ie);
		ieee->wpa_ie = NULL;
		ieee->wpa_ie_len = 0;
	}

	ipw_wpa_assoc_frame(priv, ieee->wpa_ie, ieee->wpa_ie_len);
	return 0;
}

/* implementation borrowed from hostap driver */

static int ipw_wpa_set_encryption(struct net_device *dev,
				  struct ipw_param *param, int param_len)
{
	int ret = 0;
	struct ipw_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee;
	struct ieee80211_crypto_ops *ops;
	struct ieee80211_crypt_data **crypt;
	struct ieee80211_security sec = {
		.flags = 0,
	};
	param->u.crypt.err = 0;
	param->u.crypt.alg[IPW_CRYPT_ALG_NAME_LEN - 1] = '\0';
	if (param_len !=
	    (int)((char *)param->u.crypt.key - (char *)param) +
	    param->u.crypt.key_len) {
		IPW_DEBUG_INFO("Len mismatch %d, %d\n", param_len,
			       param->u.crypt.key_len);
		return -EINVAL;
	}
	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff
	    && param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff
	    && param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff) {
		if (param->u.crypt.idx >= WEP_KEYS)
			return -EINVAL;
		crypt = &ieee->crypt[param->u.crypt.idx];
	} else {
		return -EINVAL;
	}

	sec.flags |= SEC_ENABLED | SEC_ENCRYPT;
	if (strcmp(param->u.crypt.alg, "none") == 0) {
		if (crypt) {
			sec.enabled = 0;
			sec.level = SEC_LEVEL_0;
			sec.flags |= SEC_ENABLED | SEC_LEVEL;
			ieee80211_crypt_delayed_deinit(ieee, crypt);
		}
		goto done;
	}
	sec.enabled = 1;
	sec.encrypt = 1;

	if (strcmp(param->u.crypt.alg, "TKIP") == 0)
		ieee->host_encrypt_msdu = 1;

	sec.flags |= SEC_ENABLED;
	ops = ieee80211_get_crypto_ops(param->u.crypt.alg);
	if (ops == NULL && strcmp(param->u.crypt.alg, "WEP") == 0) {
		request_module("ieee80211_crypt_wep");
		ops = ieee80211_get_crypto_ops(param->u.crypt.alg);
	} else if (ops == NULL && strcmp(param->u.crypt.alg, "TKIP") == 0) {
		request_module("ieee80211_crypt_tkip");
		ops = ieee80211_get_crypto_ops(param->u.crypt.alg);
	} else if (ops == NULL && strcmp(param->u.crypt.alg, "CCMP") == 0) {
		request_module("ieee80211_crypt_ccmp");
		ops = ieee80211_get_crypto_ops(param->u.crypt.alg);
	}
	if (ops == NULL) {
		IPW_DEBUG_INFO("%s: unknown crypto alg '%s'\n",
			       dev->name, param->u.crypt.alg);
		param->u.crypt.err = IPW_CRYPT_ERR_UNKNOWN_ALG;
		ret = -EINVAL;
		goto done;
	}

	if (*crypt == NULL || (*crypt)->ops != ops) {
		struct ieee80211_crypt_data *new_crypt;
		ieee80211_crypt_delayed_deinit(ieee, crypt);
		new_crypt = (struct ieee80211_crypt_data *)
		    kmalloc(sizeof(*new_crypt), GFP_KERNEL);
		if (new_crypt == NULL) {
			ret = -ENOMEM;
			goto done;
		}
		memset(new_crypt, 0, sizeof(struct ieee80211_crypt_data));
		new_crypt->ops = ops;
		if (new_crypt->ops && try_module_get(new_crypt->ops->owner))
			new_crypt->priv =
			    new_crypt->ops->init(param->u.crypt.idx);
		if (new_crypt->priv == NULL) {
			kfree(new_crypt);
			param->u.crypt.err = IPW_CRYPT_ERR_CRYPT_INIT_FAILED;
			ret = -EINVAL;
			goto done;
		}

		*crypt = new_crypt;
	}

	if (param->u.crypt.key_len > 0 && (*crypt)->ops->set_key &&
	    (*crypt)->ops->set_key(param->u.crypt.key,
				   param->u.crypt.key_len,
				   param->u.crypt.seq, (*crypt)->priv) < 0) {
		IPW_DEBUG_INFO("%s: key setting failed\n", dev->name);
		param->u.crypt.err = IPW_CRYPT_ERR_KEY_SET_FAILED;
		ret = -EINVAL;
		goto done;
	}

	if (param->u.crypt.set_tx) {
		ieee->tx_keyidx = param->u.crypt.idx;
		sec.active_key = param->u.crypt.idx;
		sec.flags |= SEC_ACTIVE_KEY;
	}

	if (ops->name != NULL) {
		if (strcmp(ops->name, "WEP") == 0) {
			memcpy(sec.keys[param->u.crypt.idx],
			       param->u.crypt.key, param->u.crypt.key_len);
			sec.key_sizes[param->u.crypt.idx] =
			    param->u.crypt.key_len;
			sec.flags |= (1 << param->u.crypt.idx);
			sec.flags |= SEC_LEVEL;
			sec.level = SEC_LEVEL_1;
		} else if (strcmp(ops->name, "TKIP") == 0) {
			sec.flags |= SEC_LEVEL;
			sec.level = SEC_LEVEL_2;
		} else if (strcmp(ops->name, "CCMP") == 0) {
			sec.flags |= SEC_LEVEL;
			sec.level = SEC_LEVEL_3;
		}
	}
      done:
	if (ieee->set_security)
		ieee->set_security(ieee->dev, &sec);
	/* Do not reset port if card is in Managed mode since resetting will
	 * generate new IEEE 802.11 authentication which may end up in looping
	 * with IEEE 802.1X.  If your hardware requires a reset after WEP
	 * configuration (for example... Prism2), implement the reset_port in
	 * the callbacks structures used to initialize the 802.11 stack. */
	if (ieee->reset_on_keychange &&
	    ieee->iw_mode != IW_MODE_INFRA &&
	    ieee->reset_port && ieee->reset_port(dev)) {
		IPW_DEBUG_INFO("%s: reset_port failed\n", dev->name);
		param->u.crypt.err = IPW_CRYPT_ERR_CARD_CONF_FAILED;
		return -EINVAL;
	}

	return ret;
}

static int ipw_wpa_supplicant(struct net_device *dev, struct iw_point *p)
{
	struct ipw_param *param;
	int ret = 0;
	IPW_DEBUG_INFO("wpa_supplicant: len=%d\n", p->length);
	if (p->length < sizeof(struct ipw_param) || !p->pointer)
		return -EINVAL;
	param = (struct ipw_param *)kmalloc(p->length, GFP_KERNEL);
	if (param == NULL)
		return -ENOMEM;
	if (copy_from_user(param, p->pointer, p->length)) {
		kfree(param);
		return -EFAULT;
	}

	switch (param->cmd) {

	case IPW_CMD_SET_WPA_PARAM:
		ret =
		    ipw_wpa_set_param(dev, param->u.wpa_param.name,
				      param->u.wpa_param.value);
		break;
	case IPW_CMD_SET_WPA_IE:
		ret = ipw_wpa_set_wpa_ie(dev, param, p->length);
		break;
	case IPW_CMD_SET_ENCRYPTION:
		ret = ipw_wpa_set_encryption(dev, param, p->length);
		break;
	case IPW_CMD_MLME:
		ret =
		    ipw_wpa_mlme(dev, param->u.mlme.command,
				 param->u.mlme.reason);
		break;
	default:
		IPW_ERROR("%s: Unknown WPA supplicant request: %d\n",
			  dev->name, param->cmd);
		ret = -EOPNOTSUPP;
	}

	if (ret == 0 && copy_to_user(p->pointer, param, p->length))
		ret = -EFAULT;
	kfree(param);
	return ret;
}
#endif

/* QoS -- untested and not fully plumbed */
//#define CONFIG_IPW3945_QOS
#ifdef CONFIG_IPW3945_QOS

/*
* background support to run QoS activate functionality
*/
static int qos_enable = 0;
static int qos_burst_enable = 0;
static int qos_no_ack_mask = 0;
static int qos_burst_CCK = 0;
static int qos_burst_OFDM = 0;

static struct ieee80211_qos_parameters def_qos_parameters_OFDM = {
	{QOS_TX0_CW_MIN_OFDM, QOS_TX1_CW_MIN_OFDM, QOS_TX2_CW_MIN_OFDM,
	 QOS_TX3_CW_MIN_OFDM},
	{QOS_TX0_CW_MAX_OFDM, QOS_TX1_CW_MAX_OFDM, QOS_TX2_CW_MAX_OFDM,
	 QOS_TX3_CW_MAX_OFDM},
	{QOS_TX0_AIFS, QOS_TX1_AIFS, QOS_TX2_AIFS, QOS_TX3_AIFS},
	{QOS_TX0_ACM, QOS_TX1_ACM, QOS_TX2_ACM, QOS_TX3_ACM},
	{QOS_TX0_TXOP_LIMIT_OFDM, QOS_TX1_TXOP_LIMIT_OFDM,
	 QOS_TX2_TXOP_LIMIT_OFDM, QOS_TX3_TXOP_LIMIT_OFDM}
};

static struct ieee80211_qos_parameters def_qos_parameters_CCK = {
	{QOS_TX0_CW_MIN_CCK, QOS_TX1_CW_MIN_CCK, QOS_TX2_CW_MIN_CCK,
	 QOS_TX3_CW_MIN_CCK},
	{QOS_TX0_CW_MAX_CCK, QOS_TX1_CW_MAX_CCK, QOS_TX2_CW_MAX_CCK,
	 QOS_TX3_CW_MAX_CCK},
	{QOS_TX0_AIFS, QOS_TX1_AIFS, QOS_TX2_AIFS, QOS_TX3_AIFS},
	{QOS_TX0_ACM, QOS_TX1_ACM, QOS_TX2_ACM, QOS_TX3_ACM},
	{QOS_TX0_TXOP_LIMIT_CCK, QOS_TX1_TXOP_LIMIT_CCK, QOS_TX2_TXOP_LIMIT_CCK,
	 QOS_TX3_TXOP_LIMIT_CCK}
};

static struct ieee80211_qos_parameters def_parameters_OFDM = {
	{DEF_TX0_CW_MIN_OFDM, DEF_TX1_CW_MIN_OFDM, DEF_TX2_CW_MIN_OFDM,
	 DEF_TX3_CW_MIN_OFDM},
	{DEF_TX0_CW_MAX_OFDM, DEF_TX1_CW_MAX_OFDM, DEF_TX2_CW_MAX_OFDM,
	 DEF_TX3_CW_MAX_OFDM},
	{DEF_TX0_AIFS, DEF_TX1_AIFS, DEF_TX2_AIFS, DEF_TX3_AIFS},
	{DEF_TX0_ACM, DEF_TX1_ACM, DEF_TX2_ACM, DEF_TX3_ACM},
	{DEF_TX0_TXOP_LIMIT_OFDM, DEF_TX1_TXOP_LIMIT_OFDM,
	 DEF_TX2_TXOP_LIMIT_OFDM, DEF_TX3_TXOP_LIMIT_OFDM}
};

static struct ieee80211_qos_parameters def_parameters_CCK = {
	{DEF_TX0_CW_MIN_CCK, DEF_TX1_CW_MIN_CCK, DEF_TX2_CW_MIN_CCK,
	 DEF_TX3_CW_MIN_CCK},
	{DEF_TX0_CW_MAX_CCK, DEF_TX1_CW_MAX_CCK, DEF_TX2_CW_MAX_CCK,
	 DEF_TX3_CW_MAX_CCK},
	{DEF_TX0_AIFS, DEF_TX1_AIFS, DEF_TX2_AIFS, DEF_TX3_AIFS},
	{DEF_TX0_ACM, DEF_TX1_ACM, DEF_TX2_ACM, DEF_TX3_ACM},
	{DEF_TX0_TXOP_LIMIT_CCK, DEF_TX1_TXOP_LIMIT_CCK, DEF_TX2_TXOP_LIMIT_CCK,
	 DEF_TX3_TXOP_LIMIT_CCK}
};

static int ipw_send_qos_params_command(struct ipw_priv *priv, struct ieee80211_qos_parameters
				       *qos_param)
{
	struct ipw_cmd cmd = {
		.hdr.cmd = REPLY_QOS_PARAM,.meta.len =
		    sizeof(struct ipw_qosparam_cmd),
	};
	int i;
	for (i = 0; i < IPW_TX_QUEUE_4; i++) {
		cmd.cmd.qosparam.ac[i].dot11CWmin =
		    qos_param[QOS_PARAM_SET_ACTIVE].cw_min[i];
		cmd.cmd.qosparam.ac[i].dot11CWmax =
		    qos_param[QOS_PARAM_SET_ACTIVE].cw_max[i];
		cmd.cmd.qosparam.ac[i].dot11AIFSN =
		    qos_param[QOS_PARAM_SET_ACTIVE].aifs[i];
		cmd.cmd.qosparam.ac[i].edca_txop =
		    qos_param[QOS_PARAM_SET_ACTIVE].tx_op_limit[i];
		IPW_DEBUG_HC
		    ("Qos for Queue %d  min %d max %d, aifs %d txop %d\n", i,
		     cmd.cmd.qosparam.ac[i].dot11CWmin,
		     cmd.cmd.qosparam.ac[i].dot11CWmax,
		     cmd.cmd.qosparam.ac[i].dot11AIFSN,
		     cmd.cmd.qosparam.ac[i].edca_txop);
	}

	return ipw_send_cmd(priv, &cmd);
}

/*
* This function set up the firmware to support QoS. It sends
* IPW_CMD_QOS_PARAMETERS and IPW_CMD_WME_INFO
*/
static int ipw_qos_activate(struct ipw_priv *priv, struct ieee80211_qos_data
			    *qos_network_data)
{
	int ret = 0;
	struct ieee80211_qos_parameters qos_parameters[QOS_QOS_SETS];
	struct ieee80211_qos_parameters *active_one = NULL;
	u32 size = sizeof(struct ieee80211_qos_parameters);
	u32 burst_duration;
	int i;
	u8 type;
	if ((priv == NULL))
		return -1;

	/* determine modulation type of the current network or
	 * the card current mode */
	type = (priv->status & STATUS_ASSOCIATED) ?
	    priv->assoc_request.ieee_mode : priv->ieee->mode;

	active_one = &(qos_parameters[QOS_PARAM_SET_DEF_CCK]);
	memcpy(active_one, priv->qos_data.def_qos_parm_CCK, size);
	active_one = &(qos_parameters[QOS_PARAM_SET_DEF_OFDM]);
	memcpy(active_one, priv->qos_data.def_qos_parm_OFDM, size);

	burst_duration = (priv->ieee->modulation & IEEE80211_OFDM_MODULATION) ?
	    priv->qos_data.burst_duration_OFDM :
	    priv->qos_data.burst_duration_CCK;

	if (!qos_network_data) {
		if ((type == IEEE_B)
		    || ((priv->rxon.flags & RXON_FLG_SHORT_SLOT_MSK) == 0)) {
			IPW_DEBUG_QOS("QoS activate network mode %d\n", type);
			active_one = &def_parameters_CCK;
		} else {
			active_one = &def_parameters_OFDM;
		}

		memcpy(&(qos_parameters[QOS_PARAM_SET_ACTIVE]), active_one,
		       size);
		for (i = 0; i < QOS_QUEUE_NUM; ++i) {
			qos_parameters[QOS_PARAM_SET_ACTIVE].tx_op_limit[i] =
			    (u16) burst_duration;
		}
	} else if ((priv->ieee->iw_mode != IW_MODE_INFRA)) {
		if (type == IEEE_B) {
			IPW_DEBUG_QOS("QoS activate IBSS nework mode %d\n",
				      type);
			if (priv->qos_data.qos_enable == 0)
				active_one = &def_parameters_CCK;
			else
				active_one = priv->qos_data.def_qos_parm_CCK;
		} else {
			if (priv->qos_data.qos_enable == 0)
				active_one = &def_parameters_OFDM;
			else
				active_one = priv->qos_data.def_qos_parm_OFDM;
		}
		memcpy(&(qos_parameters[QOS_PARAM_SET_ACTIVE]), active_one,
		       size);
	} else {
		qos_network_data->old_param_count =
		    qos_network_data->param_count;
		if (priv->qos_data.qos_enable == 0) {
			if ((type == IEEE_B) ||
			    ((priv->rxon.flags & RXON_FLG_SHORT_SLOT_MSK) ==
			     0)) {
				active_one = &def_parameters_CCK;
			} else {
				active_one = &def_parameters_OFDM;
			}
			memcpy(&(qos_parameters[QOS_PARAM_SET_ACTIVE]),
			       active_one, size);
		} else {
			unsigned long flags;
			int active;
			spin_lock_irqsave(&priv->ieee->lock, flags);
			active_one = &(qos_network_data->parameters);
			qos_network_data->old_param_count =
			    qos_network_data->param_count;
			memcpy(&(qos_parameters[QOS_PARAM_SET_ACTIVE]),
			       active_one, size);
			active = qos_network_data->supported;
			spin_unlock_irqrestore(&priv->ieee->lock, flags);
			if (active == 0) {
				for (i = 0; i < QOS_QUEUE_NUM; ++i) {
					qos_parameters[QOS_PARAM_SET_ACTIVE].
					    tx_op_limit[i] =
					    (u16) burst_duration;
				}
			}
		}
	}

	IPW_DEBUG_QOS("QoS sending IPW_CMD_QOS_PARAMETERS\n");
	ret = ipw_send_qos_params_command(priv,
					  (struct ieee80211_qos_parameters *)
					  &(qos_parameters[0]));
	if (ret)
		IPW_DEBUG_QOS("QoS IPW_CMD_QOS_PARAMETERS failed: %d\n", ret);

	return ret;
}

static void ipw_bg_qos_activate(void *data)
{
	struct ipw_priv *priv = data;

	if (priv == NULL)
		return;
	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	if (priv->status & STATUS_ASSOCIATED) {
		ipw_qos_activate(priv, &(priv->assoc_network->qos_data));
	}
	mutex_unlock(&priv->mutex);
}

/*
* Handle management frame beacon and probe response
*/
static int ipw_qos_handle_probe_response(struct ipw_priv *priv,
					 int active_network,
					 struct ieee80211_network
					 *network)
{
	u32 size = sizeof(struct ieee80211_qos_parameters);
	if ((network->capability & WLAN_CAPABILITY_IBSS))
		network->qos_data.active = network->qos_data.supported;
	if (network->flags & NETWORK_HAS_QOS_MASK) {
		if (active_network
		    && (network->flags & NETWORK_HAS_QOS_PARAMETERS))
			network->qos_data.active = network->qos_data.supported;
		if ((network->qos_data.active == 1) && (active_network) &&
		    (network->flags & NETWORK_HAS_QOS_PARAMETERS) &&
		    (network->qos_data.old_param_count !=
		     network->qos_data.param_count)) {
			network->qos_data.old_param_count =
			    network->qos_data.param_count;
			queue_work(priv->workqueue, &priv->qos_activate);
			IPW_DEBUG_QOS
			    ("QoS parameters change call qos_activate\n");
		}
	} else {
		if ((priv->ieee->mode == IEEE_B) ||
		    (network->mode == IEEE_B) ||
		    ((priv->rxon.flags & RXON_FLG_SHORT_SLOT_MSK) == 0)) {
			memcpy(&(network->qos_data.parameters),
			       &def_parameters_CCK, size);
		} else {
			memcpy(&(network->qos_data.parameters),
			       &def_parameters_OFDM, size);
		}
		if ((network->qos_data.active == 1) && (active_network)) {
			IPW_DEBUG_QOS("QoS was disabled call qos_activate \n");
			queue_work(priv->workqueue, &priv->qos_activate);
		}

		network->qos_data.active = 0;
		network->qos_data.supported = 0;
	}
	return 0;
}

static int ipw_send_qos_info_command(struct ipw_priv *priv, struct ieee80211_qos_information_element
				     *qos_param)
{
	return 0;
}

/*
* send IPW_CMD_WME_INFO to the firmware
*/
static u8 qos_oui[QOS_OUI_LEN] = { 0x00, 0x50, 0xF2 };

static int ipw_qos_set_info_element(struct ipw_priv *priv)
{
	int ret = 0;
	struct ieee80211_qos_information_element qos_info;
	if (priv == NULL)
		return -1;
	qos_info.elementID = QOS_ELEMENT_ID;
	qos_info.length = sizeof(struct ieee80211_qos_information_element) - 2;
	qos_info.version = QOS_VERSION_1;
	qos_info.ac_info = 0;
	memcpy(qos_info.qui, qos_oui, QOS_OUI_LEN);
	qos_info.qui_type = QOS_OUI_TYPE;
	qos_info.qui_subtype = QOS_OUI_INFO_SUB_TYPE;
	ret = ipw_send_qos_info_command(priv, &qos_info);
	if (ret != 0) {
		IPW_DEBUG_QOS("QoS error calling ipw_send_qos_info_command\n");
	}
	return ret;
}

/*
* Set the QoS parameter with the association request structure
*/
static int ipw_qos_association(struct ipw_priv *priv,
			       struct ieee80211_network *network)
{
	int ret = 0;
	struct ieee80211_qos_data *qos_data = NULL;
	if ((priv == NULL) || (network == NULL))
		return -1;
	qos_data = &(network->qos_data);
	if ((priv->ieee->iw_mode != IW_MODE_INFRA)) {
		struct ieee80211_qos_data ibss_data;
		network->capability |= WLAN_CAPABILITY_IBSS;
		qos_data = &ibss_data;
		qos_data->supported = 1;
		qos_data->active = 1;
	}

	ret = ipw_qos_activate(priv, qos_data);
	if ((priv->qos_data.qos_enable == 1)
	    && (qos_data->supported == 1)) {
		IPW_DEBUG_QOS("QoS will be enabled for this association\n");
		priv->assoc_request.policy_support |= HC_QOS_SUPPORT_ASSOC;
		ret = ipw_qos_set_info_element(priv);
	} else {
		priv->assoc_request.policy_support &= ~HC_QOS_SUPPORT_ASSOC;
	}

	return ret;
}

/*
* handling the beacon responses. if QoS setting of the network is different
* from the the associated setting, adjust the QoS setting
*/
static int ipw_qos_association_resp(struct ipw_priv *priv,
				    struct ieee80211_network *network)
{
	int ret = 0;
	unsigned long flags;
	u32 size = sizeof(struct ieee80211_qos_parameters);
	int set_qos_param = 0;

	//todoG add support for QoS
	return ret;

	if ((priv == NULL) || (network == NULL)
	    || (priv->assoc_network == NULL))

		return ret;
	if (!(priv->status & STATUS_ASSOCIATED))
		return ret;
	if ((priv->ieee->iw_mode != IW_MODE_INFRA)) {
		return ret;
	}

	spin_lock_irqsave(&priv->ieee->lock, flags);
	if (network->flags & NETWORK_HAS_QOS_PARAMETERS) {
		memcpy(&(priv->assoc_network->qos_data), &(network->qos_data),
		       sizeof(struct ieee80211_qos_data));
		priv->assoc_network->qos_data.active = 1;
		if ((network->qos_data.old_param_count !=
		     network->qos_data.param_count)) {
			set_qos_param = 1;
			network->qos_data.old_param_count =
			    network->qos_data.param_count;
		}

	} else {
		if ((network->mode == IEEE_B) || (priv->ieee->mode == IEEE_B) ||
		    ((priv->rxon.flags & RXON_FLG_SHORT_SLOT_MSK) == 0)) {
			memcpy(&(priv->assoc_network->qos_data.parameters),
			       &def_parameters_CCK, size);
		} else {
			memcpy(&(priv->assoc_network->qos_data.parameters),
			       &def_parameters_OFDM, size);
		}
		priv->assoc_network->qos_data.active = 0;
		priv->assoc_network->qos_data.supported = 0;
		set_qos_param = 1;
	}

	spin_unlock_irqrestore(&priv->ieee->lock, flags);
	if (set_qos_param == 1)
		queue_work(priv->workqueue, &priv->qos_activate);
	return ret;
}

/*
* Initialize the setting of QoS global
*/
static int ipw_qos_init(struct ipw_priv *priv, int enable,
			int burst_enable, u32 burst_duration_CCK,
			u32 burst_duration_OFDM)
{
	int ret = 0;
	if (priv == NULL)
		return -1;
	priv->qos_data.qos_enable = enable;
	if (priv->qos_data.qos_enable) {
		priv->qos_data.def_qos_parm_CCK = &def_qos_parameters_CCK;
		priv->qos_data.def_qos_parm_OFDM = &def_qos_parameters_OFDM;
		IPW_DEBUG_QOS("QoS is enabled\n");
	} else {
		priv->qos_data.def_qos_parm_CCK = &def_parameters_CCK;
		priv->qos_data.def_qos_parm_OFDM = &def_parameters_OFDM;
		IPW_DEBUG_QOS("QoS is not enabled\n");
	}

	priv->qos_data.burst_enable = burst_enable;
	if (burst_enable) {
		priv->qos_data.burst_duration_CCK = burst_duration_CCK;
		priv->qos_data.burst_duration_OFDM = burst_duration_OFDM;
	} else {
		priv->qos_data.burst_duration_CCK = 0;
		priv->qos_data.burst_duration_OFDM = 0;
	}

	return ret;
}
#endif				/* CONFIG_IPW3945_QOS */

/*
* map the packet priority to the right TX Queue
*/
static int ipw_get_tx_queue_number(struct ipw_priv *priv, u16 priority)
{
#ifdef CONFIG_IPW3945_QOS
	if ((priority > 7) || !priv->qos_data.qos_enable)
		priority = 0;
#else
	if (priority > 7)
		priority = 0;
#endif

	return from_priority_to_tx_queue[priority] - 1;
}

#ifdef CONFIG_IPW3945_QOS
/*
* add QoS parameter to the TX command
*/
static int ipw_qos_set_tx_queue_command(struct ipw_priv *priv,
					u16 priority,
					struct ipw_tx_cmd *tfd, u8 unicast)
{
	int ret = 0;
	int tx_queue_id = 0;
	struct ieee80211_qos_data *qos_data = NULL;
	int active, supported;
	unsigned long flags;
	if ((priv == NULL) || (tfd == NULL)
	    || !(priv->status & STATUS_ASSOCIATED))
		return 0;
	if (priv->assoc_network == NULL)
		return 0;
	qos_data = &(priv->assoc_network->qos_data);
	spin_lock_irqsave(&priv->ieee->lock, flags);
	if (priv->ieee->iw_mode != IW_MODE_INFRA) {
		if (unicast == 0) {
			qos_data->active = 0;
		} else {
			qos_data->active = qos_data->supported;
		}
	}

	active = qos_data->active;
	supported = qos_data->supported;
	spin_unlock_irqrestore(&priv->ieee->lock, flags);
	IPW_DEBUG_QOS
	    ("QoS  %d network is QoS active %d  supported %d  unicast %d\n",
	     priv->qos_data.qos_enable, active, supported, unicast);
	if ((active == 1) && (priv->qos_data.qos_enable == 1)) {
		ret = from_priority_to_tx_queue[priority];
		tx_queue_id = ret - 1;
		IPW_DEBUG_QOS("QoS packet priority is %d \n", priority);
		if ((priority <= 7)) {
//todoG move to 3945 format
/*
			tfd->tx_flags_ext |= DCT_FLAG_EXT_QOS_ENABLED;
			tfd->tfd.tfd_26.mchdr.qos_ctl = priority;
                        tfd->tfd.tfd_26.mchdr.frame_ctl |= IEEE80211_STYPE_QOS_DATA;

                        if( (priv->qos_data.qos_no_ack_mask & (1UL<<tx_queue_id) )) {
                            tfd->tx_flags &= ~DCT_FLAG_ACK_REQD;
                            tfd->tfd.tfd_26.mchdr.qos_ctl |= CTRL_QOS_NO_ACK;

                        }
*/
		}

	}
	return ret;
}
#endif

#define STATUS_SUCCESS	0
#define STATUS_FAIL	1

static void ipw_auth_work(void *data)
{
	struct ipw_priv *priv = data;
	enum connection_manager_assoc_states *state = &priv->auth_state;
	struct ieee80211_auth *auth = priv->auth_frame;
	unsigned long flags;
	int status;

	mutex_lock(&priv->mutex);
	spin_lock_irqsave(&priv->lock, flags);
	if (*state >= CMAS_INIT && *state <= CMAS_RX_AUTH_SEQ_4) {
		*state = CMAS_RX_AUTH_SEQ_2;

		if (auth->algorithm == WLAN_AUTH_OPEN &&
		    *state <= CMAS_RX_AUTH_SEQ_2) {
			BUG_ON(!priv->auth_frame);
			status = auth->status;
			kfree(priv->auth_frame);
			priv->auth_frame = NULL;

			if (status == STATUS_SUCCESS) {
				goto fill_assoc;
			}
			*state = CMAS_INIT;
			spin_unlock_irqrestore(&priv->lock, flags);
			mutex_unlock(&priv->mutex);
			IPW_DEBUG_ASSOC("AUTH_OPEN fail\n");
			return;
		}

		if (auth->algorithm == WLAN_AUTH_SHARED_KEY &&
		    *state <= CMAS_RX_AUTH_SEQ_4) {
			if (auth->transaction == 2) {
				struct sk_buff *skb_auth =
				    dev_alloc_skb(sizeof(struct ieee80211_auth)
						  + 138);
				struct ieee80211_auth *auth2;
				struct ieee80211_crypt_data *crypt =
				    priv->ieee->crypt[0];

				skb_reserve(skb_auth, 4);
				skb_put(skb_auth, sizeof(*auth) + 130);
				auth2 = (struct ieee80211_auth *)skb_auth->data;
				auth2->header.frame_ctl = IEEE80211_FTYPE_MGMT |
				    IEEE80211_STYPE_AUTH |
				    IEEE80211_FCTL_PROTECTED;
				auth2->header.seq_ctl = 0;
				BUG_ON(!priv->auth_frame);

				memcpy(auth2->header.addr1, priv->bssid, 6);
				memcpy(auth2->header.addr2, priv->mac_addr, 6);
				memcpy(auth2->header.addr3, priv->ieee->bssid,
				       6);
				auth2->algorithm = WLAN_AUTH_SHARED_KEY;
				auth2->transaction = 3;
				auth2->status = 0;

				memcpy(auth2->info_element,
				       priv->auth_frame->info_element, 130);

				atomic_inc(&crypt->refcnt);
				if (crypt->ops->encrypt_mpdu)
					crypt->ops->encrypt_mpdu(skb_auth,
								 IEEE80211_3ADDR_LEN,
								 crypt->priv);
				atomic_dec(&crypt->refcnt);

				memcpy(priv->auth_frame, skb_auth->data,
				       skb_auth->len);
				ieee80211_tx_frame(priv->ieee,
						   (struct ieee80211_hdr *)
						   priv->auth_frame,
						   skb_auth->len);
				spin_unlock_irqrestore(&priv->lock, flags);
				kfree_skb(skb_auth);
				mutex_unlock(&priv->mutex);
				return;
			} else if (auth->transaction == 4) {
				BUG_ON(!priv->auth_frame);
				status = auth->status;
				kfree(priv->auth_frame);
				priv->auth_frame = NULL;
				if (status == STATUS_SUCCESS) {
					goto fill_assoc;
				}

				*state = CMAS_INIT;
				spin_unlock_irqrestore(&priv->lock, flags);
				mutex_unlock(&priv->mutex);
				IPW_DEBUG_ASSOC("AUTH_SHARED_KEY fail\n");
				return;
			}
		}
	}
	spin_unlock_irqrestore(&priv->lock, flags);
	mutex_unlock(&priv->mutex);
	return;

      fill_assoc:
	priv->auth_state = CMAS_AUTHENTICATED;
	spin_unlock_irqrestore(&priv->lock, flags);
	mutex_unlock(&priv->mutex);
	if (!(priv->status & STATUS_ASSOCIATED)) {
		struct ipw_frame *frame = ipw_get_free_frame(priv);
		int len;

		if (!frame) {
			IPW_ERROR("Could not allocate frame for auth work.\n");
			return;
		}

		len = ipw_fill_association_req(priv, &frame->u.frame,
					       sizeof(frame->u));
		if (len) {
			IPW_DEBUG_11H("Sending %d bytes.\n", len);
			spin_lock_irqsave(&priv->lock, flags);
			ieee80211_tx_frame(priv->ieee, &frame->u.frame, len);
			spin_unlock_irqrestore(&priv->lock, flags);
		}

		ipw_free_frame(priv, frame);
	}
}

static int ipw_handle_probe_request(struct net_device *dev,
				    struct ieee80211_probe_request *frame,
				    struct ieee80211_rx_stats *stats)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	int rc = 0;
	u16 left;
	struct ieee80211_info_element *info_element;
	struct ipw_frame *out_frame;
	int len;

	if (priv->status & STATUS_EXIT_PENDING)
		return 0;

	if (((priv->status & STATUS_ASSOCIATED) == 0)
	    || (priv->ieee->iw_mode != IW_MODE_ADHOC))
		return rc;

	/* ignore proble recieved during scan period on diff channels */
	if (stats->received_channel != priv->channel)
		return rc;
	/* if direct scan make sure ssid are the same */
	info_element = frame->info_element;
	left = stats->len - sizeof(*frame);
	while (left >= sizeof(*info_element)) {
		if (sizeof(*info_element) + info_element->len > left) {
			break;
		}

		if (info_element->id == MFIE_TYPE_SSID) {
			if (info_element->len == 0) {
				break;
			}

			if (priv->essid_len != info_element->len)
				return rc;
			if (memcmp
			    (priv->essid, info_element->data,
			     info_element->len) != 0)
				return rc;
			break;
		}
		left -= sizeof(struct ieee80211_info_element) +
		    info_element->len;
		info_element = (struct ieee80211_info_element *)
		    &info_element->data[info_element->len];
	}

	out_frame = ipw_get_free_frame(priv);

	if (!out_frame) {
		IPW_ERROR("Could not allocate frame for auth work.\n");
		return -ENOMEM;
	}

	len = ipw_fill_beacon_frame(priv, &out_frame->u.frame,
				    frame->header.addr2, sizeof(out_frame->u));

	if (len) {
		IPW_DEBUG_11H("Sending %d bytes.\n", len);
		out_frame->u.frame.frame_ctl = IEEE80211_FTYPE_MGMT |
		    IEEE80211_STYPE_PROBE_RESP;

		ieee80211_tx_frame(priv->ieee, &out_frame->u.frame, len);
	}

	ipw_free_frame(priv, out_frame);

	return 0;
}

static int ipw_handle_auth(struct net_device *dev, struct ieee80211_auth *auth)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	int size = sizeof(struct ieee80211_auth);

	IPW_DEBUG_INFO
	    ("auth frame algorithm %d  transaction %d  status %d \n",
	     auth->algorithm, auth->transaction, auth->status);

	if (priv->status & STATUS_EXIT_PENDING)
		return 0;

	if (!(priv->status & STATUS_ASSOCIATED) &&
	    !(priv->status & STATUS_ASSOCIATING))
		return 0;

	if ((priv->auth_state <= CMAS_INIT) && (priv->auth_frame == NULL)) {
		priv->auth_frame = kmalloc(size + 138, GFP_ATOMIC);
		if (priv->auth_frame == NULL) {
			IPW_DEBUG_ASSOC("can't alloc auth frame\n");
			return 0;
		}
	}

	if (priv->auth_state != CMAS_AUTHENTICATED) {
		BUG_ON(!priv->auth_frame);
		if (auth->transaction == 2
		    && auth->algorithm == WLAN_AUTH_SHARED_KEY)
			size += 138;
		memcpy(priv->auth_frame, auth, size);

		queue_work(priv->workqueue, &priv->auth_work);
	}

	return 0;
}

static int ipw_handle_deauth(struct net_device *dev,
			     struct ieee80211_deauth *deauth)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	IPW_DEBUG_INFO("deauthentication reasoncode %x\n", deauth->reason);
	queue_work(priv->workqueue, &priv->disassociate);
	return 0;
	/*
	   struct ieee80211_auth frame = {
	   .header.frame_ctl = IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_AUTH,
	   .transaction = 1,
	   };

	   if (!(priv->status & STATUS_ASSOCIATED))
	   return 0;

	   frame.algorithm = (priv->assoc_request.auth_type == AUTH_SHARED_KEY) ?
	   WLAN_AUTH_SHARED_KEY : WLAN_AUTH_OPEN;

	   memcpy(frame.header.addr1, priv->bssid, ETH_ALEN);
	   memcpy(frame.header.addr2, priv->mac_addr, ETH_ALEN);
	   memcpy(frame.header.addr3, priv->ieee->bssid, ETH_ALEN);

	   priv->auth_state = CMAS_INIT;

	   ieee80211_tx_frame(priv->ieee,
	   (struct ieee80211_hdr *)&frame, sizeof(frame));

	   return 0;
	 */
}

static int ipw_try_merge_network(struct ipw_priv *priv, int active_network, struct ieee80211_network
				 *network)
{
	if ((priv->status & STATUS_ASSOCIATED) &&
	    (priv->ieee->iw_mode == IW_MODE_ADHOC) && (!active_network)) {
		if (memcmp(network->bssid, priv->bssid, ETH_ALEN)) {
			if ((network->capability & WLAN_CAPABILITY_IBSS) &&
			    !(network->flags & NETWORK_EMPTY_ESSID)) {
				if ((network->ssid_len ==
				     priv->assoc_network->ssid_len)
				    && !memcmp(network->ssid,
					       priv->assoc_network->ssid,
					       network->ssid_len)) {
					queue_work(priv->workqueue,
						   &priv->merge_networks);
				}

			}
		}
	}

	return 0;
}

static int ipw_handle_probe_response(struct net_device *dev,
				     struct ieee80211_probe_response *probe,
				     struct ieee80211_network *network)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	int active_network = 0;

	if (priv->status & STATUS_EXIT_PENDING)
		return 0;

	if (priv->status & STATUS_ASSOCIATED) {
		if (!memcmp
		    (network->bssid, priv->assoc_request.bssid, ETH_ALEN))
			active_network = 1;
	}
#ifdef CONFIG_IPW3945_QOS
	ipw_qos_handle_probe_response(priv, active_network, network);
#endif

	if (active_network) {
		priv->missed_adhoc_beacons = 0;

		ipw_card_bss_active_changed_notify(priv, network);

		if (priv->ieee->iw_mode == IW_MODE_ADHOC)
			ipw_add_station(priv, probe->header.addr2, 0,
					CMD_ASYNC | CMD_NO_LOCK);
	} else
		ipw_try_merge_network(priv, active_network, network);

	return 0;
}

static inline int is_same_network(struct ieee80211_network *src,
				  struct ieee80211_network *dst)
{
	/* A network is a match only if the channel, BSSID, and ESSID all match.
	 * To support <hidden> networks (beacons don't contain SSID),
	 * if either of the networks being compared has ssid_length == 0,
	 * we don't bother comparing the ESSID itself. */
	if ((src->channel != dst->channel) ||
	    memcmp(src->bssid, dst->bssid, ETH_ALEN))
		return 0;

	if ((src->ssid_len == 0) || (dst->ssid_len == 0))
		return 1;

	return ((src->ssid_len == dst->ssid_len) &&
		!memcmp(src->ssid, dst->ssid, src->ssid_len));
}

static int ipw_handle_beacon(struct net_device *dev,
			     struct ieee80211_beacon *beacon,
			     struct ieee80211_network *network)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	if (priv->status & STATUS_EXIT_PENDING)
		return 0;

	if ((priv->status & STATUS_ASSOCIATED) &&
	    is_same_network(priv->assoc_network, network)) {

		priv->missed_adhoc_beacons = 0;

		ipw_card_bss_active_changed_notify(priv, network);

		if (priv->ieee->iw_mode == IW_MODE_ADHOC)
			ipw_add_station(priv, beacon->header.addr2, 0,
					CMD_ASYNC | CMD_NO_LOCK);
		else if ((priv->ieee->iw_mode == IW_MODE_INFRA) &&
			 !(priv->config & CFG_NO_ROAMING))
			/* roaming is only in managed mode */
			mod_timer(&priv->roaming_wdt, jiffies +
				  priv->roaming_threshold *
				  BEACON_JIFFIES(priv));

		mod_timer(&priv->disassociate_wdt, jiffies +
			  priv->missed_beacon_threshold * BEACON_JIFFIES(priv));
	} else
		ipw_try_merge_network(priv, 0, network);

	return 0;
}

static int ipw_handle_assoc_response(struct net_device *dev,
				     struct ieee80211_assoc_response *resp,
				     struct ieee80211_network *network)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	if (priv->status & STATUS_EXIT_PENDING)
		return 0;

	if (resp->status != 0) {
		IPW_DEBUG(IPW_DL_NOTIF | IPW_DL_STATE | IPW_DL_ASSOC,
			  "association failed: '%s' " MAC_FMT
			  " Reason: %s (%d)\n", escape_essid(priv->essid,
							     priv->essid_len),
			  MAC_ARG(priv->bssid),
			  ipw_get_status_code(resp->status), resp->status);
		return 0;
	}

	if (priv->status & STATUS_ASSOCIATED) {
		if (is_same_network(priv->assoc_network, network))
			IPW_DEBUG_INFO("Association response received for "
				       "current network ('%s').  Ignoring.\n",
				       escape_essid(network->ssid,
						    network->ssid_len));
		else
			IPW_DEBUG_INFO("Association response received for "
				       "invalid network ('%s').  Ignoring.\n",
				       escape_essid(network->ssid,
						    network->ssid_len));
		return 0;
	}

	IPW_DEBUG(IPW_DL_NOTIF | IPW_DL_STATE | IPW_DL_ASSOC,
		  "associated: '%s' " MAC_FMT "\n",
		  escape_essid(priv->essid, priv->essid_len),
		  MAC_ARG(priv->bssid));

	cancel_delayed_work(&priv->associate_timeout);

	priv->assoc_request.assoc_id = resp->aid & 0x3fff;
	queue_work(priv->workqueue, &priv->post_associate);
#ifdef CONFIG_IPW3945_QOS
	ipw_qos_association_resp(priv, network);
#endif
	return 0;
}

static void ipw_bg_report_work(void *data)
{
	struct ipw_priv *priv = data;

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	ipw_send_daemon_sync(priv, DAEMON_SYNC_MEASURE_REPORT,
			     sizeof(priv->measure_report),
			     &priv->measure_report);
	mutex_unlock(&priv->mutex);
}

static int ipw_handle_disassoc(struct net_device *dev,
			       struct ieee80211_disassoc *disassoc)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	IPW_DEBUG_INFO("disassociation reasoncode %d: %s\n",
		       disassoc->reason, ipw_get_status_code(disassoc->reason));
	queue_work(priv->workqueue, &priv->disassociate);
	return 0;
}

/***************************************************************/

static int ipw_associate_network(struct ipw_priv *priv,
				 struct ieee80211_network *network,
				 struct ipw_supported_rates *rates, int roaming)
{
	int err;
	unsigned long flags;

	spin_lock_irqsave(&priv->ieee->lock, flags);

	priv->assoc_network = NULL;

	if (!(priv->config & CFG_STATIC_ESSID)) {
		priv->essid_len = min(network->ssid_len,
				      (u8) IW_ESSID_MAX_SIZE);
		memcpy(priv->essid, network->ssid, priv->essid_len);
	}

	network->last_associate = jiffies;
	memset(&priv->assoc_request, 0, sizeof(priv->assoc_request));
	priv->assoc_request.channel = network->channel;
	if ((priv->capability & CAP_PRIVACY_ON) &&
	    (priv->capability & CAP_SHARED_KEY)) {
		priv->assoc_request.auth_type = AUTH_SHARED_KEY;
		priv->assoc_request.auth_key = priv->ieee->sec.active_key;
	} else {
		priv->assoc_request.auth_type = AUTH_OPEN;
		priv->assoc_request.auth_key = 0;
	}

	if (priv->ieee->wpa_ie_len) {
		priv->assoc_request.policy_support = 0x02;	/* RSN active */
		ipw_set_rsn_capa(priv, priv->ieee->wpa_ie,
				 priv->ieee->wpa_ie_len);
	}

	/*
	 * It is valid for our ieee device to support multiple modes, but
	 * when it comes to associating to a given network we have to choose
	 * just one mode.
	 */
	if (network->mode & priv->ieee->mode & IEEE_A)
		priv->assoc_request.ieee_mode = IPW_A_MODE;
	else if (network->mode & priv->ieee->mode & IEEE_G)
		priv->assoc_request.ieee_mode = IPW_G_MODE;
	else if (network->mode & priv->ieee->mode & IEEE_B)
		priv->assoc_request.ieee_mode = IPW_B_MODE;

	priv->assoc_request.capability = network->capability;
	if ((network->capability & WLAN_CAPABILITY_SHORT_PREAMBLE)
	    && !(priv->config & CFG_PREAMBLE_LONG)) {
		priv->assoc_request.preamble_length = DCT_FLAG_SHORT_PREAMBLE;
	} else {
		priv->assoc_request.preamble_length = DCT_FLAG_LONG_PREAMBLE;
		/* Clear the short preamble if we won't be supporting it */
		priv->assoc_request.capability &=
		    ~WLAN_CAPABILITY_SHORT_PREAMBLE;
	}

	IPW_DEBUG_ASSOC("%sssociation attempt [%s]: '%s', channel %d, "
			"802.11%c [%d], %s[:%s], enc=%s%s%s%c%c\n",
			roaming ? "Rea" : "A",
			(priv->capability & WLAN_CAPABILITY_IBSS) ?
			"IBSS" : "BSS",
			escape_essid(priv->essid, priv->essid_len),
			network->channel,
			ipw_modes[priv->assoc_request.ieee_mode],
			rates->num_rates,
			(priv->assoc_request.preamble_length ==
			 DCT_FLAG_LONG_PREAMBLE) ? "long" : "short",
			network->capability &
			WLAN_CAPABILITY_SHORT_PREAMBLE ? "short" : "long",
			priv->capability & CAP_PRIVACY_ON ? "on " : "off",
			priv->capability & CAP_PRIVACY_ON ?
			(priv->capability & CAP_SHARED_KEY ? "(shared)" :
			 "(open)") : "",
			priv->capability & CAP_PRIVACY_ON ? " key=" : "",
			priv->capability & CAP_PRIVACY_ON ?
			'1' + priv->assoc_request.auth_key : '.',
			priv->capability & CAP_PRIVACY_ON ? '.' : ' ');
	priv->assoc_request.beacon_interval = network->beacon_interval;
	IPW_DEBUG_ASSOC("Beacon interval for the network is %d\n",
			priv->assoc_request.beacon_interval);

	if ((priv->ieee->iw_mode == IW_MODE_ADHOC) &&
	    (network->time_stamp[0] == 0)
	    && (network->time_stamp[1] == 0)) {
		priv->assoc_request.assoc_type = HC_IBSS_START;
		priv->assoc_request.assoc_tsf_msw = 0;
		priv->assoc_request.assoc_tsf_lsw = 0;
	} else {
		if (unlikely(roaming))
			priv->assoc_request.assoc_type = HC_REASSOCIATE;
		else
			priv->assoc_request.assoc_type = HC_ASSOCIATE;
		priv->assoc_request.assoc_tsf_msw = network->time_stamp[1];
		priv->assoc_request.assoc_tsf_lsw = network->time_stamp[0];
	}

	memcpy(&priv->assoc_request.bssid, network->bssid, ETH_ALEN);
	if (priv->ieee->iw_mode == IW_MODE_ADHOC) {
		memset(&priv->assoc_request.dest, 0xFF, ETH_ALEN);
		priv->assoc_request.atim_window = network->atim_window;
	} else {
		memcpy(&priv->assoc_request.dest, network->bssid, ETH_ALEN);
		priv->assoc_request.atim_window = 0;
	}

	priv->assoc_request.listen_interval = network->listen_interval;
	rates->ieee_mode = priv->assoc_request.ieee_mode;
	rates->purpose = IPW_RATE_CONNECT;
	ipw_send_supported_rates(priv, rates);

	/*
	 * If preemption is enabled, it is possible for the association
	 * to complete before we return from ipw_send_associate.  Therefore
	 * we have to be sure and update our priviate data first.
	 */
	priv->channel = network->channel;
	memcpy(priv->bssid, network->bssid, ETH_ALEN);

	priv->assoc_request.erp_value = network->erp_value;

	priv->status |= STATUS_ASSOCIATING;
	priv->status &= ~STATUS_SECURITY_UPDATED;

	priv->assoc_network = network;

	spin_unlock_irqrestore(&priv->ieee->lock, flags);

#ifdef CONFIG_IPW3945_QOS
	ipw_qos_association(priv, network);
#endif

	err = ipw_send_associate(priv, &priv->assoc_request);
	if (err) {
		IPW_DEBUG_HC("Attempt to send associate command failed.\n");
		return err;
	}

	IPW_DEBUG(IPW_DL_STATE, "associating: '%s' " MAC_FMT " \n",
		  escape_essid(priv->essid, priv->essid_len),
		  MAC_ARG(priv->bssid));

	queue_delayed_work(priv->workqueue, &priv->associate_timeout,
			   ASSOCIATE_TIMEOUT);

	return 0;
}

static void ipw_roam(void *data)
{
	struct ipw_priv *priv = data;
	struct ieee80211_network *network = NULL;
	struct ipw_network_match match = {
		.network = priv->assoc_network
	};
	/* The roaming process is as follows:
	 *
	 * 1.  Missed beacon threshold triggers the roaming process by
	 *     setting the status ROAM bit and requesting a scan.
	 * 2.  When the scan completes, it schedules the ROAM work
	 * 3.  The ROAM work looks at all of the known networks for one that
	 *     is a better network than the currently associated.  If none
	 *     found, the ROAM process is over (ROAM bit cleared)
	 * 4.  If a better network is found, a disassociation request is
	 *     sent.
	 * 5.  When the disassociation completes, the roam work is again
	 *     scheduled.  The second time through, the driver is no longer
	 *     associated, and the newly selected network is sent an
	 *     association request.
	 * 6.  At this point ,the roaming process is complete and the ROAM
	 *     status bit is cleared.
	 */
	/* If we are no longer associated, and the roaming bit is no longer
	 * set, then we are not actively roaming, so just return */
	if (!(priv->status & (STATUS_ASSOCIATED | STATUS_ROAMING)))
		return;
	if (priv->status & STATUS_ASSOCIATED) {
		/* First pass through ROAM process -- look for a better
		 * network */
		unsigned long flags;
		u8 rssi = priv->assoc_network->stats.rssi;
		priv->assoc_network->stats.rssi = -128;
		spin_lock_irqsave(&priv->ieee->lock, flags);
		list_for_each_entry(network, &priv->ieee->network_list, list) {
			if (network != priv->assoc_network)
				ipw_best_network(priv, &match, network, 1);
		}
		spin_unlock_irqrestore(&priv->ieee->lock, flags);
		priv->assoc_network->stats.rssi = rssi;
		if (match.network == priv->assoc_network) {
			IPW_DEBUG_ASSOC("No better APs in this network to "
					"roam to.\n");
			priv->status &= ~STATUS_ROAMING;
			ipw_debug_config(priv);
			return;
		}

		ipw_send_disassociate(priv, 1);
		priv->assoc_network = match.network;
		return;
	}

	/* Second pass through ROAM process -- request association */
	ipw_compatible_rates(priv, priv->assoc_network, &match.rates);
	ipw_associate_network(priv, priv->assoc_network, &match.rates, 1);
	priv->status &= ~STATUS_ROAMING;
}

static void ipw_bg_roam(void *data)
{
	struct ipw_priv *priv = data;

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	ipw_roam(data);
	mutex_unlock(&priv->mutex);
}

static int ipw_associate(void *data)
{
	struct ipw_priv *priv = data;
	struct ieee80211_network *network = NULL;
	struct ipw_network_match match = {
		.network = NULL
	};
	struct ipw_supported_rates *rates;
	struct list_head *element;
	unsigned long flags;

	ipw_update_link_led(priv);

	if (priv->status & (STATUS_ASSOCIATED | STATUS_ASSOCIATING)) {
		IPW_DEBUG_ASSOC
		    ("Not attempting association (already in progress)\n");
		return 0;
	}

	if (!ipw_is_alive(priv)) {
		IPW_DEBUG_ASSOC("Not attempting association (not "
				"initialized)\n");
		return 0;
	}

	if (!ipw_is_calibrated(priv)) {
		IPW_DEBUG_ASSOC("Not attempting association (not "
				"calibrated)\n");
		return 0;
	}

	if (priv->status & STATUS_SCANNING) {
		IPW_DEBUG_ASSOC("Not attempting association (scanning)\n");
		return 0;
	}

	if (!(priv->config & CFG_ASSOCIATE) &&
	    !(priv->config & (CFG_STATIC_ESSID |
			      CFG_STATIC_CHANNEL | CFG_STATIC_BSSID))) {
		IPW_DEBUG_ASSOC("Not attempting association (associate=0)\n");
		return 0;
	}

	if (priv->status & STATUS_EXIT_PENDING) {
		IPW_DEBUG_ASSOC
		    ("Not attempting association driver unloading\n");
		return 0;
	}

	ipw_scan_cancel(priv);

	/* Protect our use of the network_list */
	spin_lock_irqsave(&priv->ieee->lock, flags);
	list_for_each_entry(network, &priv->ieee->network_list, list)
	    ipw_best_network(priv, &match, network, 0);
	network = match.network;
	rates = &match.rates;
	if (network == NULL &&
	    priv->ieee->iw_mode == IW_MODE_ADHOC &&
	    priv->config & CFG_ADHOC_CREATE &&
	    priv->config & CFG_STATIC_ESSID &&
	    priv->config & CFG_STATIC_CHANNEL &&
	    !list_empty(&priv->ieee->network_free_list)) {
		element = priv->ieee->network_free_list.next;
		network = list_entry(element, struct ieee80211_network, list);
		ipw_adhoc_create(priv, network);
		rates = &priv->rates;
		list_del(element);
		list_add_tail(&network->list, &priv->ieee->network_list);
	}
	spin_unlock_irqrestore(&priv->ieee->lock, flags);

	/* If we reached the end of the list, then we don't have any valid
	 * matching APs */
	if (!network) {
		ipw_debug_config(priv);
		ipw_scan_initiate(priv, priv->ieee->freq_band, SCAN_INTERVAL);
		return 0;
	}

	ipw_associate_network(priv, network, rates, 0);
	return 1;
}

static void ipw_bg_daemon_tx_status_sync(void *data)
{
	struct ipw_priv *priv = data;

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	IPW_DEBUG_DAEMON("Sending daemon sync tx status: 0x%08X\n",
			 priv->daemon_tx_status);
	ipw_send_daemon_sync(priv, DAEMON_SYNC_TX_STATUS,
			     sizeof(priv->daemon_tx_status),
			     &priv->daemon_tx_status);
}

#define IPW_FORCE_TXPOWR_CALIB  (180 * HZ)

static void ipw_bg_associate(void *data)
{
	struct ipw_priv *priv = data;

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	ipw_associate(data);
	mutex_unlock(&priv->mutex);
}

#define IPW_RX_HDR(x) ((struct ipw_rx_frame_hdr *)(\
                       x->u.rx_frame.stats.payload + \
                       x->u.rx_frame.stats.mib_count))
#define IPW_RX_END(x) ((struct ipw_rx_frame_end *)(\
                       IPW_RX_HDR(x)->payload + \
                       le16_to_cpu(IPW_RX_HDR(x)->len)))
#define IPW_RX_STATS(x) (&x->u.rx_frame.stats)
#define IPW_RX_DATA(x) (IPW_RX_HDR(x)->payload)

static inline void ipw_handle_data_packet(struct ipw_priv *priv, struct ipw_rx_mem_buffer
					  *rxb, struct ieee80211_rx_stats
					  *stats)
{
	struct ipw_rx_packet *pkt = (struct ipw_rx_packet *)rxb->skb->data;
	struct ipw_rx_frame_hdr *rx_hdr = IPW_RX_HDR(pkt);
	/* We received data from the HW, so stop the watchdog */
	priv->net_dev->trans_start = jiffies;
	if (unlikely((le16_to_cpu(rx_hdr->len) + IPW_RX_FRAME_SIZE) >
		     skb_tailroom(rxb->skb))) {
		priv->ieee->stats.rx_errors++;
		priv->wstats.discard.misc++;
		IPW_DEBUG_DROP("Corruption detected! Oh no!\n");
		return;
	}

	/* We only process data packets if the interface is open */
	if (unlikely(!priv->netdev_registered || !netif_running(priv->net_dev))) {
		priv->ieee->stats.rx_dropped++;
		priv->wstats.discard.misc++;
		IPW_DEBUG_DROP("Dropping packet while interface is not up.\n");
		return;
	}

	IPW_DEBUG_RX("Advancing %zd bytes (channel %d)...\n",
		     (void *)rx_hdr->payload - (void *)pkt, rx_hdr->channel);
	skb_reserve(rxb->skb, (void *)rx_hdr->payload - (void *)pkt);
	/* Set the size of the skb to the size of the frame */
	skb_put(rxb->skb, le16_to_cpu(rx_hdr->len));
	IPW_DEBUG_RX("Rx packet of %d bytes.\n", rxb->skb->len);
	printk_buf(IPW_DL_RX, rxb->skb->data, rx_hdr->len);
	if (!ieee80211_rx(priv->ieee, rxb->skb, stats))
		priv->ieee->stats.rx_errors++;
	else {
		/* ieee80211_rx succeeded, so it now owns the SKB */
		rxb->skb = NULL;
		priv->led_packets += rx_hdr->len;
		ipw_setup_activity_timer(priv);
	}
}

#define IPW_PACKET_RETRY_TIME HZ

static inline int is_duplicate_packet(struct ipw_priv *priv,
				      struct ieee80211_hdr_4addr *header)
{
	u16 fc = le16_to_cpu(header->frame_ctl);
	u16 sc = le16_to_cpu(header->seq_ctl);
	u16 seq = WLAN_GET_SEQ_SEQ(sc);
	u16 frag = WLAN_GET_SEQ_FRAG(sc);
	u16 *last_seq, *last_frag;
	unsigned long *last_time;

	switch (priv->ieee->iw_mode) {
	case IW_MODE_ADHOC:
		{
			struct list_head *p;
			struct ipw_ibss_seq *entry = NULL;
			u8 *mac = header->addr2;
			int index = mac[5] % IPW_IBSS_MAC_HASH_SIZE;

			__list_for_each(p, &priv->ibss_mac_hash[index]) {
				entry =
				    list_entry(p, struct ipw_ibss_seq, list);
				if (!memcmp(entry->mac, mac, ETH_ALEN))
					break;
			}
			if (p == &priv->ibss_mac_hash[index]) {
				entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
				if (!entry) {
					IPW_ERROR
					    ("Cannot malloc new mac entry\n");
					return 0;
				}
				memcpy(entry->mac, mac, ETH_ALEN);
				entry->seq_num = seq;
				entry->frag_num = frag;
				entry->packet_time = jiffies;
				list_add(&entry->list,
					 &priv->ibss_mac_hash[index]);
				return 0;
			}
			last_seq = &entry->seq_num;
			last_frag = &entry->frag_num;
			last_time = &entry->packet_time;
			break;
		}
	case IW_MODE_INFRA:
		last_seq = &priv->last_seq_num;
		last_frag = &priv->last_frag_num;
		last_time = &priv->last_packet_time;
		break;
	default:
		return 0;
	}
	if ((*last_seq == seq) &&
	    time_after(*last_time + IPW_PACKET_RETRY_TIME, jiffies)) {
		if (*last_frag == frag)
			goto drop;
		if (*last_frag + 1 != frag)
			/* out-of-order fragment */
			goto drop;
	} else
		*last_seq = seq;

	*last_frag = frag;
	*last_time = jiffies;
	return 0;

      drop:
	BUG_ON(!(fc & IEEE80211_FCTL_RETRY));
	return 1;
}

static inline int is_network_packet(struct ipw_priv *priv, struct ieee80211_hdr_4addr
				    *header)
{
	/* Filter incoming packets to determine if they are targetted toward
	 * this network, discarding packets coming from ourselves */
	switch (priv->ieee->iw_mode) {
	case IW_MODE_ADHOC:	/* Header: Dest. | Source    | BSSID */
		/* packets from our adapter are dropped (echo) */
		if (!memcmp(header->addr2, priv->net_dev->dev_addr, ETH_ALEN))
			return 0;
		/* {broad,multi}cast packets to our IBSS go through */
		if (ipw_is_broadcast_ether_addr(header->addr1) ||
		    is_multicast_ether_addr(header->addr1))
			return !memcmp(header->addr3, priv->bssid, ETH_ALEN);
		/* packets to our adapter go through */
		return !memcmp(header->addr1, priv->net_dev->dev_addr,
			       ETH_ALEN);
	case IW_MODE_INFRA:	/* Header: Dest. | AP{BSSID} | Source */
		/* packets from our adapter are dropped (echo) */
		if (!memcmp(header->addr3, priv->net_dev->dev_addr, ETH_ALEN))
			return 0;
		/* {broad,multi}cast packets to our BSS go through */
		if (ipw_is_broadcast_ether_addr(header->addr1) ||
		    is_multicast_ether_addr(header->addr1))
			return !memcmp(header->addr2, priv->bssid, ETH_ALEN);
		/* packets to our adapter go through */
		return !memcmp(header->addr1, priv->net_dev->dev_addr,
			       ETH_ALEN);
	}

	return 1;
}

/*
 * Reclaim Tx queue entries no more used by NIC.
 *
 * When FW adwances 'R' index, all entries between old and
 * new 'R' index need to be reclaimed. As result, some free space
 * forms. If there is enough free space (> low mark), wake Tx queue.
 */
static int ipw_queue_tx_reclaim(struct ipw_priv *priv, int fifo, int index)
{
	struct ipw_tx_queue *txq = &priv->txq[fifo];
	struct ipw_queue *q = &txq->q;
	u8 is_next = 0;
	int used;
	if ((index >= q->n_bd) || (x2_queue_used(q, index) == 0)) {
		IPW_ERROR
		    ("Read index for DMA queue (%d) is out of range [0-%d) %d %d\n",
		     index, q->n_bd, q->first_empty, q->last_used);
		goto done;
	}
	index = ipw_queue_inc_wrap(index, q->n_bd);
	for (; q->last_used != index;
	     q->last_used = ipw_queue_inc_wrap(q->last_used, q->n_bd)) {
		if (is_next) {
			IPW_WARNING("XXXL we have skipped command\n");
			queue_work(priv->workqueue, &priv->down);
		}
		if (fifo != CMD_QUEUE_NUM) {
			ipw_queue_tx_free_tfd(priv, txq);
			priv->tx_packets++;
		}

		is_next = 1;
	}
      done:
	if (ipw_queue_space(q) > q->low_mark && (fifo >= 0)
	    && (fifo != CMD_QUEUE_NUM)
	    && (priv->status & STATUS_ASSOCIATED)
	    && priv->netdev_registered && netif_running(priv->net_dev))
		netif_wake_queue(priv->net_dev);
	used = q->first_empty - q->last_used;
	if (used < 0)
		used += q->n_window;
	return used;
}

static void ipw_tx_complete(struct ipw_priv *priv,
			    struct ipw_rx_mem_buffer *rxb)
{
	struct ipw_rx_packet *pkt = (struct ipw_rx_packet *)rxb->skb->data;
	int fifo = SEQ_TO_FIFO(pkt->hdr.sequence);
	int index = SEQ_TO_INDEX(pkt->hdr.sequence);
	int is_huge = (pkt->hdr.sequence & SEQ_HUGE_FRAME);
	int cmd_index;
	struct ipw_cmd *cmd;
	if (fifo > MAX_REAL_TX_QUEUE_NUM)
		return;
	if (fifo != CMD_QUEUE_NUM) {
		ipw_queue_tx_reclaim(priv, fifo, index);
		return;
	}

	cmd_index = get_next_cmd_index(&priv->txq[CMD_QUEUE_NUM].q,
				       index, is_huge);
	cmd = &priv->txq[CMD_QUEUE_NUM].cmd[cmd_index];
	/* Input error checking is done when commands are added to queue. */
	if (cmd->meta.flags & CMD_WANT_SKB) {
		cmd->meta.u.source->u.skb = rxb->skb;
		rxb->skb = NULL;
	} else if (cmd->meta.u.callback &&
		   !cmd->meta.u.callback(priv, cmd, rxb->skb))
		rxb->skb = NULL;

	ipw_queue_tx_reclaim(priv, fifo, index);

	/* is_cmd_sync(cmd) works with ipw_host_cmd... here we only have ipw_cmd */
	if (!(cmd->meta.flags & CMD_ASYNC)) {
		priv->status &= ~STATUS_HCMD_ACTIVE;
		wake_up_interruptible(&priv->wait_command_queue);
	}
}

#ifdef CONFIG_IPW3945_DEBUG

/* hack this function to show different aspects of received frames */
static void ipw_report_frame(struct ipw_priv *priv,
			     struct ipw_rx_packet *pkt,
			     struct ieee80211_hdr_4addr *header, int group100)
{
	u32 to_us = 0;
	u32 print_it = 0;
	u32 hundred = 0;
	u32 dataframe = 0;

	/* these are declared without "=" to avoid compiler warnings if we
	 *   don't use them in the debug messages below */
	u16 frame_ctl;
	u16 seq_ctl;
	u16 channel;
	u16 phy_flags;
	u8 rate_sym;
	u16 length;
	u16 status;
	u16 bcn_tmr;
	u32 tsf_low;
	u64 tsf;
	u8 rssi;
	u8 agc;
	u16 sig_avg;
	u16 noise_diff;

	struct ipw_rx_frame_stats *rx_stats = IPW_RX_STATS(pkt);
	struct ipw_rx_frame_hdr *rx_hdr = IPW_RX_HDR(pkt);
	struct ipw_rx_frame_end *rx_end = IPW_RX_END(pkt);

	/* MAC header */
	frame_ctl = le16_to_cpu(header->frame_ctl);
	seq_ctl = le16_to_cpu(header->seq_ctl);

	/* metadata */
	channel = le16_to_cpu(rx_hdr->channel);
	phy_flags = le16_to_cpu(rx_hdr->phy_flags);
	rate_sym = rx_hdr->rate;
	length = le16_to_cpu(rx_hdr->len);

	/* end-of-frame status and timestamp */
	status = le16_to_cpu(rx_end->status);
	bcn_tmr = le32_to_cpu(rx_end->beaconTimeStamp);
	tsf_low = (u32) (rx_end->timestamp & 0x0ffffffff);
	tsf = le64_to_cpu(rx_end->timestamp);

	/* signal statistics */
	rssi = rx_stats->rssi;
	agc = rx_stats->agc;
	sig_avg = le16_to_cpu(rx_stats->sig_avg);
	noise_diff = le16_to_cpu(rx_stats->noise_diff);

	to_us = !memcmp(header->addr1, priv->net_dev->dev_addr, ETH_ALEN);

	/* if data frame is to us and all is good, print only every 100 */
	if (to_us && (frame_ctl & ~0x4000) == 0x0208) {
		dataframe = 1;
		if (!group100) {
			print_it = 1;	/* print each frame */
		} else if (priv->framecnt_to_us < 100) {
			priv->framecnt_to_us++;
			print_it = 0;
		} else {
			priv->framecnt_to_us = 0;
			print_it = 1;
			hundred = 1;
		}
	} else {
		print_it = 1;
	}

	if (print_it) {
		char *title;
		u32 rate;

		if (hundred)
			title = "100Frames";
		else if (frame_ctl & 0x0800)
			title = "Retry";
		else if ((frame_ctl & 0x00FF) == 0x10)
			title = "AscRsp";
		else if ((frame_ctl & 0x00FF) == 0x30)
			title = "RasRsp";
		else if ((frame_ctl & 0x00FF) == 0x50)
			title = "PrbRsp";
		else if ((frame_ctl & 0x00FF) == 0x80)
			title = "Beacon";
		else if ((frame_ctl & 0x00FF) == 0x90)
			title = "ATIM";
		else if ((frame_ctl & 0x00FF) == 0xB0)
			title = "Auth";
		else
			title = "Frame";

		switch (rate_sym) {
		case IPW_TX_RATE_1MB:
			rate = 1;
			break;
		case IPW_TX_RATE_2MB:
			rate = 2;
			break;
		case IPW_TX_RATE_5MB:
			rate = 5;
			break;
		case IPW_TX_RATE_6MB:
			rate = 6;
			break;
		case IPW_TX_RATE_9MB:
			rate = 9;
			break;
		case IPW_TX_RATE_11MB:
			rate = 11;
			break;
		case IPW_TX_RATE_12MB:
			rate = 12;
			break;
		case IPW_TX_RATE_18MB:
			rate = 18;
			break;
		case IPW_TX_RATE_24MB:
			rate = 24;
			break;
		case IPW_TX_RATE_36MB:
			rate = 36;
			break;
		case IPW_TX_RATE_48MB:
			rate = 48;
			break;
		case IPW_TX_RATE_54MB:
			rate = 54;
			break;
		default:
			rate = 0;
		}

		if (dataframe) {
			IPW_DEBUG_RX
			    ("%s: mhd=0x%04x, dst=0x%02x, len=%u, rssi=%d, chnl=%d, rate=%u, \n",
			     title, frame_ctl, header->addr1[5], length, rssi,
			     channel, rate);
		} else {
			/* src/dst addresses assume managed mode */
			IPW_DEBUG_RX
			    ("%s: 0x%04x, dst=0x%02x, src=0x%02x, rssi=%u, tim=%lu usec, phy=0x%02x, chnl=%d\n",
			     title, frame_ctl, header->addr1[5],
			     header->addr3[5], rssi,
			     tsf_low - priv->scan_start_tsf, phy_flags,
			     channel);
		}
	}
}

#endif				/* CONFIG_IPW3945_DEBUG */

static void ipw_handle_reply_rx(struct ipw_priv *priv,
				struct ipw_rx_mem_buffer *rxb)
{
	struct ipw_rx_packet *pkt = pkt = (struct ipw_rx_packet *)
	    rxb->skb->data;
	struct ipw_rx_frame_stats *rx_stats = IPW_RX_STATS(pkt);
	struct ipw_rx_frame_hdr *rx_hdr = IPW_RX_HDR(pkt);
	struct ipw_rx_frame_end *rx_end = IPW_RX_END(pkt);
	struct ieee80211_hdr_4addr *header;
	struct ieee80211_rx_stats stats = {
		.rssi = le16_to_cpu(rx_stats->rssi) - IPW_RSSI_OFFSET,
		.signal = le16_to_cpu(rx_stats->sig_avg),
		.noise = le16_to_cpu(rx_stats->noise_diff),
		.mac_time = rx_end->beaconTimeStamp,
		.rate = rx_hdr->rate,
		.received_channel = rx_hdr->channel,
		.len = le16_to_cpu(rx_hdr->len),
		.freq = (rx_hdr->phy_flags & RX_RES_PHY_FLAGS_BAND_24_MSK) ?
		    IEEE80211_24GHZ_BAND : IEEE80211_52GHZ_BAND,
		.tsf = rx_end->timestamp,
		.beacon_time = rx_end->beaconTimeStamp,
	};
	u8 network_packet;
	unsigned long flags;

	if (unlikely(rx_stats->mib_count > 20)) {
		IPW_DEBUG_DROP
		    ("dsp size out of range [0,20]: "
		     "%d", rx_stats->mib_count);
		priv->ieee->stats.rx_errors++;
		priv->wstats.discard.misc++;
		return;
	}

	if (stats.rssi != 0)
		stats.mask |= IEEE80211_STATMASK_RSSI;
//      if (stats.signal == 0)
	stats.signal = stats.rssi;
	if (stats.signal != 0)
		stats.mask |= IEEE80211_STATMASK_SIGNAL;
	if (stats.noise != 0)
		stats.mask |= IEEE80211_STATMASK_NOISE;
	if (stats.rate != 0)
		stats.mask |= IEEE80211_STATMASK_RATE;
#ifdef CONFIG_IPW3945_MONITOR
	if (priv->ieee->iw_mode == IW_MODE_MONITOR) {
		ipw_handle_data_packet(priv, rxb, &stats);
		return;
	}
#endif

	if (!(rx_end->status & RX_RES_STATUS_NO_CRC32_ERROR)
	    || !(rx_end->status & RX_RES_STATUS_NO_RXE_OVERFLOW)) {
		IPW_DEBUG_RX("Bad CRC or FIFO: 0x%08X.\n", rx_end->status);
		priv->ieee->stats.rx_dropped++;
		priv->wstats.discard.misc++;
		return;
	}

	/* can be covered by ipw_report_frames() in most cases */
//      IPW_DEBUG_RX("RX status: 0x%08X\n", rx_end->status);

	priv->rx_packets++;

	header = (struct ieee80211_hdr_4addr *)IPW_RX_DATA(pkt);

	network_packet = is_network_packet(priv, header);

	if (ipw_debug_level & IPW_DL_STATS && net_ratelimit())
		IPW_DEBUG_STATS
		    ("[%c] %d RSSI: %d Signal: %u, Noise: %u, Rate: %u\n",
		     network_packet ? '*' : ' ', stats.received_channel,
		     stats.rssi, stats.signal, stats.noise, stats.rate);

	spin_lock_irqsave(&priv->ieee->lock, flags);
	if (network_packet && priv->assoc_network) {
		if (stats.noise) {
			average_add(&priv->average_noise, stats.noise);
			priv->assoc_network->stats.noise = stats.noise;
		}

		priv->assoc_network->stats.rssi = stats.rssi;
		average_add(&priv->average_rssi, stats.rssi);

		priv->last_rx_rssi = stats.rssi;
	}
	spin_unlock_irqrestore(&priv->ieee->lock, flags);

#ifdef CONFIG_IPW3945_DEBUG
	if (ipw_debug_level & (IPW_DL_RX))
		/* Set "1" to report good data frames in groups of 100 */
		ipw_report_frame(priv, pkt, header, 1);
#endif

	switch (WLAN_FC_GET_TYPE(le16_to_cpu(header->frame_ctl))) {
	case IEEE80211_FTYPE_MGMT:
		switch (WLAN_FC_GET_STYPE(le16_to_cpu(header->frame_ctl))) {
		case IEEE80211_STYPE_PROBE_RESP:
		case IEEE80211_STYPE_BEACON:
		case IEEE80211_STYPE_ACTION:
			if (network_packet)
				ipw_send_daemon_frame(priv,
						      rx_hdr->channel,
						      rx_stats->rssi,
						      rx_end->timestamp,
						      rx_end->beaconTimeStamp,
						      (u8 *) header,
						      rx_hdr->len);
			break;
		}

		ieee80211_rx_mgt(priv->ieee, header, &stats);
		break;

	case IEEE80211_FTYPE_CTL:
		break;

	case IEEE80211_FTYPE_DATA:
		if (unlikely(!network_packet))
			IPW_DEBUG_DROP("Dropping (non network): " MAC_FMT ", "
				       MAC_FMT ", " MAC_FMT "\n",
				       MAC_ARG(header->addr1),
				       MAC_ARG(header->addr2),
				       MAC_ARG(header->addr3));
		else if (unlikely(is_duplicate_packet(priv, header)))
			IPW_DEBUG_DROP("Dropping (dup): " MAC_FMT ", "
				       MAC_FMT ", " MAC_FMT "\n",
				       MAC_ARG(header->addr1),
				       MAC_ARG(header->addr2),
				       MAC_ARG(header->addr3));
		else
			ipw_handle_data_packet(priv, rxb, &stats);
		break;
	}
}

static void ipw_handle_reply_tx(struct ipw_priv *priv,
				struct ipw_tx_resp *resp, u16 sequence)
{
	int fifo = SEQ_TO_FIFO(sequence);

	IPW_DEBUG(IPW_DL_RATE | IPW_DL_TX,
		  "Tx fifo %d Status 0x%08x plcp rate %d retries %d\n",
		  fifo, resp->status, resp->rate, resp->failure_frame);

	/* We don't do rate scaling or measurements with microcode commands */
	if (fifo == CMD_QUEUE_NUM)
		return;

	ipw_rate_scale_tx_resp_handle(priv, resp);

	/* If we're not actively looking for a packet to measure or if this
	 * packet isn't the one we're measuring, then bail... */
	if (!(priv->status & STATUS_TX_MEASURE) ||
	    (sequence != priv->daemon_tx_sequence))
		return;

	/* We found our packet, so we're not measuring anymore */
	priv->status &= ~STATUS_TX_MEASURE;

	/* Record when we attempted to gather status even if the transmit
	 * failed */
	priv->daemon_last_status = jiffies;

	/* If the packet had any retries or status does not indicate success,
	 * then we don't do anything with this status info. */
	if (resp->failure_frame || resp->failure_rts || resp->bt_kill_count ||
	    ((resp->status & 0xff) != 1))
		return;

	/* For daemon tx status, upper 24-bits are taken from the Tx reply
	 * status, and the low 8-bits are filled with the rate used for the
	 * packet */
	priv->daemon_tx_status = resp->status & ~0xFF;
	priv->daemon_tx_status |= priv->daemon_tx_rate;
	queue_work(priv->workqueue, &priv->daemon_tx_status_sync);
}

/* Checks if param1 is the same network as param2 but with a different
 * channel number */
static int is_same_network_channel_switch(struct ieee80211_network *src,
					  struct ieee80211_network *dst,
					  u8 channel)
{
	return ((src->ssid_len == dst->ssid_len) &&
		(src->channel == channel) &&
		!compare_ether_addr(src->bssid, dst->bssid) &&
		!memcmp(src->ssid, dst->ssid, src->ssid_len));
}

static struct ieee80211_network *ieee80211_move_network_channel(struct
								ieee80211_device
								*ieee,
								struct
								ieee80211_network
								*network,
								u8 channel)
{
	struct ieee80211_network *target;
	unsigned long flags;

	spin_lock_irqsave(&ieee->lock, flags);

	list_for_each_entry(target, &ieee->network_list, list) {
		/* Look to see if we have already received a beacon from
		 * the new network and created a new entry */
		if (!is_same_network_channel_switch(target, network, channel))
			continue;

		/* If we found the network, then return it so the caller
		 * can switch to it. */
		goto exit;
	}

	/* If we reach here, then the new network has not appeared yet.
	 * We can simply update the channel information for this network. */
	network->channel = channel;
	target = network;

      exit:
	spin_unlock_irqrestore(&ieee->lock, flags);

	return target;
}

//todoG fix this
/*
 * Main entry function for recieving a packet with 80211 headers.  This
 * should be called when ever the FW has notified us that there is a new
 * skb in the recieve queue.
 */
static void ipw_rx_handle(struct ipw_priv *priv)
{
	struct ipw_rx_mem_buffer *rxb;
	struct ipw_rx_packet *pkt;
	u32 r, i;

	r = priv->shared_virt->rx_read_ptr[0];
	i = priv->rxq->read;
	while (i != r) {
		rxb = priv->rxq->queue[i];
		BUG_ON(rxb == NULL);
		priv->rxq->queue[i] = NULL;
		pci_dma_sync_single_for_cpu(priv->pci_dev, rxb->dma_addr,
					    IPW_RX_BUF_SIZE,
					    PCI_DMA_FROMDEVICE);
		pkt = (struct ipw_rx_packet *)rxb->skb->data;

		/* Don't report replies covered by debug messages below ...
		 * switch statement for readability ... compiler may optimize.
		 * Hack at will to see/not-see what you want in logs. */
		switch (pkt->hdr.cmd) {
		case REPLY_DAEMON_1:
		case REPLY_DAEMON_2:
		case REPLY_DAEMON_3:
		case REPLY_DAEMON_4:
		case SCAN_START_NOTIFICATION:
		case SCAN_RESULTS_NOTIFICATION:
		case SCAN_COMPLETE_NOTIFICATION:
		case REPLY_STATISTICS_CMD:
		case STATISTICS_NOTIFICATION:
		case REPLY_RX:
		case REPLY_ALIVE:
		case REPLY_ADD_STA:
		case REPLY_ERROR:
			break;
		default:
			IPW_DEBUG_RX("Received %s command (#%x), seq:0x%04X, "
				     "flags=0x%02X, len = %d\n",
				     get_cmd_string(pkt->hdr.cmd),
				     pkt->hdr.cmd,
				     pkt->hdr.sequence,
				     pkt->hdr.flags, le16_to_cpu(pkt->len));
		}

		switch (pkt->hdr.cmd) {
		case REPLY_RX:	/* 802.11 frame */
			ipw_handle_reply_rx(priv, rxb);
			break;

		case REPLY_ALIVE:{
				memcpy(&priv->card_alive,
				       &pkt->u.alive_frame,
				       sizeof(struct ipw_alive_resp));

				IPW_DEBUG_INFO
				    ("Alive ucode status 0x%08X revision "
				     "0x%01X 0x%01X\n",
				     priv->card_alive.is_valid,
				     priv->card_alive.ver_type,
				     priv->card_alive.ver_subtype);
				if (priv->card_alive.is_valid == UCODE_VALID_OK)
					queue_work(priv->workqueue,
						   &priv->alive_start);
				else
					IPW_WARNING
					    ("uCode did not respond OK.\n");
				break;
			}

		case REPLY_ADD_STA:{
				IPW_DEBUG_RX("Received REPLY_ADD_STA: 0x%02X\n",
					     pkt->u.status);
				break;
			}

		case REPLY_ERROR:{
				u32 err_type = pkt->u.err_resp.enumErrorType;
				u8 cmd_id = pkt->u.err_resp.currentCmdID;
				u16 seq = pkt->u.err_resp.erroneousCmdSeqNum;
				u32 ser = pkt->u.err_resp.errorService;
				IPW_ERROR("Error Reply type 0x%08X "
					  "cmd %s (0x%02X) "
					  "seq 0x%04X ser 0x%08X\n",
					  err_type,
					  get_cmd_string(cmd_id),
					  cmd_id, seq, ser);
				break;
			}
		case REPLY_TX:
			ipw_handle_reply_tx(priv, &pkt->u.tx_resp,
					    pkt->hdr.sequence);
			break;

		case CHANNEL_SWITCH_NOTIFICATION:{
				struct ipw_csa_notification *csa =
				    &(pkt->u.csa_notif);
				IPW_DEBUG_11H
				    ("CSA notif: channel %d, status %d\n",
				     csa->channel, csa->status);
				priv->channel = csa->channel;
				priv->assoc_network =
				    ieee80211_move_network_channel(priv->ieee,
								   priv->
								   assoc_network,
								   csa->
								   channel);
				break;
			}

		case SPECTRUM_MEASURE_NOTIFICATION:{
				struct ipw_spectrum_notification *report =
				    &(pkt->u.spectrum_notif);

				if (!report->state) {
					IPW_DEBUG(IPW_DL_11H | IPW_DL_INFO,
						  "Spectrum Measure Notification: "
						  "Start\n");
					break;
				}

				memcpy(&priv->measure_report, report,
				       sizeof(*report));
				queue_work(priv->workqueue, &priv->report_work);
				break;
			}

		case QUIET_NOTIFICATION:
			IPW_DEBUG_INFO("UNHANDLED - Quiet Notification.\n");
			break;

		case MEASURE_ABORT_NOTIFICATION:
			IPW_DEBUG_INFO
			    ("UNHANDLED - Measure Abort Notification.\n");
			break;

		case RADAR_NOTIFICATION:
			IPW_DEBUG_INFO("UNHANDLED - Radar Notification.\n");
			break;

		case PM_SLEEP_NOTIFICATION:{
#ifdef CONFIG_IPW3945_DEBUG
				struct ipw_sleep_notification *sleep =
				    &(pkt->u.sleep_notif);
				IPW_DEBUG_RX("sleep mode: %d, src: %d\n",
					     sleep->pm_sleep_mode,
					     sleep->pm_wakeup_src);
#endif
				break;
			}

		case PM_DEBUG_STATISTIC_NOTIFIC:
			IPW_DEBUG_RADIO("Dumping %d bytes of unhandled "
					"notification for %s:\n",
					le16_to_cpu(pkt->len),
					get_cmd_string(pkt->hdr.cmd));
			printk_buf(IPW_DL_RADIO, pkt->u.raw,
				   le16_to_cpu(pkt->len));
			break;

		case BEACON_NOTIFICATION:{
#ifdef CONFIG_IPW3945_DEBUG
				struct BeaconNtfSpecifics *beacon =
				    &(pkt->u.beacon_status);
				IPW_DEBUG_INFO
				    ("beacon status %x retries %d iss %d "
				     "tsf %d %d rate %d\n",
				     beacon->bconNotifHdr.status,
				     beacon->bconNotifHdr.failure_frame,
				     beacon->ibssMgrStatus, beacon->highTSF,
				     beacon->lowTSF, beacon->bconNotifHdr.rate);
#endif
				break;
			}
		case REPLY_STATISTICS_CMD:
		case STATISTICS_NOTIFICATION:
			IPW_DEBUG_RX
			    ("Statistics notification received (%zd vs %d).\n",
			     sizeof(priv->statistics), pkt->len);
			memcpy(&priv->statistics, pkt->u.raw,
			       sizeof(priv->statistics));
			break;

		case WHO_IS_AWAKE_NOTIFICATION:
			IPW_DEBUG_RX("Notification from the card \n");
			break;

		case SCAN_REQUEST_NOTIFICATION:{
#ifdef CONFIG_IPW3945_DEBUG
				struct ipw_scanreq_notification *notif =
				    (struct ipw_scanreq_notification *)pkt->u.
				    raw;
				IPW_DEBUG_RX("Scan request status = 0x%x\n",
					     notif->status);
#endif
				break;
			}

		case SCAN_START_NOTIFICATION:{
				struct ipw_scanstart_notification *notif =
				    (struct ipw_scanstart_notification *)pkt->u.
				    raw;
				priv->scan_start_tsf = notif->tsf_low;
				IPW_DEBUG_SCAN("Scan start: "
					       "%d [802.11%s] "
					       "(TSF: 0x%08X:%08X) - %d (beacon timer %u)\n",
					       notif->channel,
					       notif->band ? "bg" : "a",
					       notif->tsf_high, notif->tsf_low,
					       notif->status,
					       notif->beacon_timer);
				break;
			}

		case SCAN_RESULTS_NOTIFICATION:{
#ifdef CONFIG_IPW3945_DEBUG
				struct ipw_scanresults_notification *notif =
				    (struct ipw_scanresults_notification *)pkt->
				    u.raw;

				IPW_DEBUG_SCAN("Scan ch.res: "
					       "%d [802.11%s] "
					       "(TSF: 0x%08X:%08X) - %d "
					       "elapsed=%lu usec (%dms since last)\n",
					       notif->channel,
					       notif->band ? "bg" : "a",
					       notif->tsf_high, notif->tsf_low,
					       notif->statistics[0],
					       notif->tsf_low -
					       priv->scan_start_tsf,
					       jiffies_to_msecs(elapsed_jiffies
								(priv->
								 last_scan_jiffies,
								 jiffies)));
#endif
				priv->last_scan_jiffies = jiffies;
				break;
			}

		case SCAN_COMPLETE_NOTIFICATION:{
				struct ipw_scancomplete_notification *scan_notif
				    = (struct ipw_scancomplete_notification *)
				    pkt->u.raw;
				IPW_DEBUG_SCAN("Scan complete: %d channels "
					       "(TSF 0x%08X:%08X) - %d\n",
					       scan_notif->scanned_channels,
					       scan_notif->tsf_low,
					       scan_notif->tsf_high,
					       scan_notif->status);

				ipw_scan_completed(priv,
						   scan_notif->status == 1);
				break;
			}

		case CARD_STATE_NOTIFICATION:{
				u32 flags =
				    le32_to_cpu(pkt->u.card_state_notif.flags);
				u32 status = priv->status;
				IPW_DEBUG_RF_KILL("Card state received: "
						  "HW:%s SW:%s\n",
						  (flags & HW_CARD_DISABLED) ?
						  "Off" : "On",
						  (flags & SW_CARD_DISABLED) ?
						  "Off" : "On");

				if (flags & HW_CARD_DISABLED) {
					ipw_write32(priv, CSR_UCODE_DRV_GP1_SET,
						    CSR_UCODE_DRV_GP1_BIT_CMD_BLOCKED);

					priv->status |= STATUS_RF_KILL_HW;
				} else
					priv->status &= ~STATUS_RF_KILL_HW;

				if (flags & SW_CARD_DISABLED)
					priv->status |= STATUS_RF_KILL_SW;
				else
					priv->status &= ~STATUS_RF_KILL_SW;

				priv->status &=
				    ~(STATUS_ASSOCIATED | STATUS_ASSOCIATING);

				ipw_scan_cancel(priv);

				if (((status & STATUS_RF_KILL_HW) !=
				     (priv->status & STATUS_RF_KILL_HW)) ||
				    ((status & STATUS_RF_KILL_SW) !=
				     (priv->status & STATUS_RF_KILL_SW))) {

					queue_work(priv->workqueue,
						   &priv->rf_kill);
				} else
					wake_up_interruptible(&priv->
							      wait_command_queue);

				break;
			}
		default:
			break;
		}

		/* MSB on sequence indicates if this is a Tx response
		 * or a frame initiated by the HW */
		if (!(pkt->hdr.sequence & SEQ_RX_FRAME)) {
			/* Invoke any callbacks, transfer the skb to
			 * caller, and fire off the (possibly) blocking
			 * ipw_send_cmd() via as we reclaim the queue... */
			ipw_tx_complete(priv, rxb);
		}

		/* For now we just don't re-use anything.  We can tweak this
		 * later to try and re-use notification packets and SKBs that
		 * fail to Rx correctly */
		if (rxb->skb != NULL) {
			dev_kfree_skb_any(rxb->skb);
			rxb->skb = NULL;
		}

		pci_unmap_single(priv->pci_dev, rxb->dma_addr,
				 IPW_RX_BUF_SIZE, PCI_DMA_FROMDEVICE);
		list_add_tail(&rxb->list, &priv->rxq->rx_used);
		i = (i + 1) % RX_QUEUE_SIZE;
	}

	/* Backtrack one entry */
	priv->rxq->read = i;
	ipw_rx_queue_restock(priv);
}

/*
 * This file defines the Wireless Extension handlers.  It does not
 * define any methods of hardware manipulation and relies on the
 * functions defined in ipw_main to provide the HW interaction.
 *
 */

static int ipw_wx_get_name(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	mutex_lock(&priv->mutex);
	if (priv->status & STATUS_RF_KILL_MASK)
		strcpy(wrqu->name, "radio off");
	else if (!(priv->status & STATUS_ASSOCIATED))
		strcpy(wrqu->name, "unassociated");
	else
		snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11%c",
			 ipw_modes[priv->assoc_request.ieee_mode]);
	IPW_DEBUG_WX("Name: %s\n", wrqu->name);
	mutex_unlock(&priv->mutex);
	return 0;
}

static int ipw_set_channel(struct ipw_priv *priv, u8 channel)
{
	if (channel == 0) {
		IPW_DEBUG_INFO("Setting channel to ANY (0)\n");
		priv->config &= ~CFG_STATIC_CHANNEL;
		IPW_DEBUG_ASSOC("Attempting to associate with new "
				"parameters.\n");
		ipw_associate(priv);
		return 0;
	}

	priv->config |= CFG_STATIC_CHANNEL;
	if (priv->channel == channel) {
		IPW_DEBUG_INFO("Request to set channel to current value (%d)\n",
			       channel);
		return 0;
	}

	IPW_DEBUG_INFO("Setting channel to %i\n", (int)channel);
	priv->channel = channel;
	/* Network configuration changed -- force [re]association */
	IPW_DEBUG_ASSOC("[re]association triggered due to channel change.\n");
	if (!ipw_disassociate(priv))
		ipw_associate(priv);
	return 0;
}

static int ipw_wx_set_freq(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	struct iw_freq *fwrq = &wrqu->freq;
	int ret = 0;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	if (fwrq->m == 0) {
		IPW_DEBUG_WX("SET Freq/Channel -> any\n");
		mutex_lock(&priv->mutex);
		ret = ipw_set_channel(priv, 0);
		mutex_unlock(&priv->mutex);
		return ret;
	}

	/* if setting by freq convert to channel */
	if (fwrq->e == 1) {
		channel = ieee80211_freq_to_channel(priv->ieee, fwrq->m);
		if (channel == 0)
			return -EINVAL;
	} else
		channel = fwrq->m;
	if (!ieee80211_is_valid_channel(priv->ieee, channel))
		return -EINVAL;
	IPW_DEBUG_WX("SET Freq/Channel -> %d \n", fwrq->m);
	mutex_lock(&priv->mutex);
	ret = ipw_set_channel(priv, channel);
	mutex_unlock(&priv->mutex);
	return ret;
}

static int ipw_wx_get_freq(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	wrqu->freq.e = 0;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	/* If we are associated, trying to associate, or have a statically
	 * configured CHANNEL then return that; otherwise return ANY */
	mutex_lock(&priv->mutex);
	if (priv->config & CFG_STATIC_CHANNEL ||
	    priv->status & (STATUS_ASSOCIATING | STATUS_ASSOCIATED))
		wrqu->freq.m = priv->channel;
	else
		wrqu->freq.m = 0;
	mutex_unlock(&priv->mutex);
	IPW_DEBUG_WX("GET Freq/Channel -> %d \n", priv->channel);
	return 0;
}

static int ipw_wx_set_mode(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	int err = 0;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	IPW_DEBUG_WX("Set MODE: %d\n", wrqu->mode);
	mutex_lock(&priv->mutex);
	if (wrqu->mode == priv->ieee->iw_mode) {
		mutex_unlock(&priv->mutex);
		return 0;
	}

	cancel_delayed_work(&priv->scan_check);
	cancel_delayed_work(&priv->gather_stats);
	priv->status &= ~STATUS_SCAN_PENDING;
	ipw_scan_cancel(priv);

	err = ipw_disassociate(priv);

	switch (wrqu->mode) {
#ifdef CONFIG_IPW3945_MONITOR
	case IW_MODE_MONITOR:
#endif
	case IW_MODE_ADHOC:
	case IW_MODE_INFRA:
		break;
	case IW_MODE_AUTO:
		wrqu->mode = IW_MODE_INFRA;
		break;
	default:
		mutex_unlock(&priv->mutex);
		return -EINVAL;
	}

#ifdef CONFIG_IPW3945_MONITOR
	if (priv->ieee->iw_mode == IW_MODE_MONITOR)
		priv->net_dev->type = ARPHRD_ETHER;
	if (wrqu->mode == IW_MODE_MONITOR)
		priv->net_dev->type = ARPHRD_IEEE80211;
#endif				/* CONFIG_IPW3945_MONITOR */
	priv->ieee->iw_mode = wrqu->mode;

	ipw_connection_init_rx_config(priv);

	memcpy(priv->rxon.node_addr, priv->net_dev->dev_addr, ETH_ALEN);

	ipw_rxon_call(priv, 0);
	ipw_rxon_add_station(priv, BROADCAST_ADDR, 0);

	if (!err)
		ipw_associate(priv);

	err = 0;
	mutex_unlock(&priv->mutex);
	return err;
}

static int ipw_wx_get_mode(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	mutex_lock(&priv->mutex);
	wrqu->mode = priv->ieee->iw_mode;
	IPW_DEBUG_WX("Get MODE -> %d\n", wrqu->mode);
	mutex_unlock(&priv->mutex);
	return 0;
}

#define DEFAULT_RTS_THRESHOLD     2304U
#define MIN_RTS_THRESHOLD         1U
#define MAX_RTS_THRESHOLD         2304U
#define DEFAULT_BEACON_INTERVAL   100U
#define	DEFAULT_SHORT_RETRY_LIMIT 7U
#define	DEFAULT_LONG_RETRY_LIMIT  4U

/* Values are in microsecond */
static const s32 timeout_duration[] = {
	350000,
	250000,
	75000, 37000, 25000,
};
static const s32 period_duration[] = {
	400000,
	700000,
	1000000,
	1000000,
	1000000
};
static int ipw_wx_get_range(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	struct iw_range *range = (struct iw_range *)extra;
	const struct ieee80211_geo *geo = ieee80211_get_geo(priv->ieee);
	int i = 0, j;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	wrqu->data.length = sizeof(*range);
	memset(range, 0, sizeof(*range));
	/* 54Mbs == ~27 Mb/s real (802.11g) */
	range->throughput = 27 * 1000 * 1000;
	range->max_qual.qual = 100;
	/* TODO: Find real max RSSI and stick here */
	range->max_qual.level = 0;
	range->max_qual.noise = priv->ieee->worst_rssi + 0x100;
	range->max_qual.updated = 7;	/* Updated all three */
	range->avg_qual.qual = 70;
	/* TODO: Find real 'good' to 'bad' threshol value for RSSI */
	range->avg_qual.level = 0;	/* FIXME to real average level */
	range->avg_qual.noise = 0;
	range->avg_qual.updated = 7;	/* Updated all three */
	mutex_lock(&priv->mutex);
	range->num_bitrates = min(priv->rates.num_rates, (u8) IW_MAX_BITRATES);
	for (i = 0; i < range->num_bitrates; i++)
		range->bitrate[i] =
		    (priv->rates.supported_rates[i] & 0x7F) * 500000;
	range->max_rts = DEFAULT_RTS_THRESHOLD;
	range->min_frag = MIN_FRAG_THRESHOLD;
	range->max_frag = MAX_FRAG_THRESHOLD;
	range->encoding_size[0] = 5;
	range->encoding_size[1] = 13;
	range->num_encoding_sizes = 2;
	range->max_encoding_tokens = WEP_KEYS;
	/* Set the Wireless Extension versions */
	range->we_version_compiled = WIRELESS_EXT;
	range->we_version_source = 16;
	i = 0;
	if (priv->ieee->mode & (IEEE_B | IEEE_G)) {
		for (j = 0; j < geo->bg_channels && i < IW_MAX_FREQUENCIES;
		     i++, j++) {
			range->freq[i].i = geo->bg[j].channel;
			range->freq[i].m = geo->bg[j].freq * 100000;
			range->freq[i].e = 1;
		}
	}

	if (priv->ieee->mode & IEEE_A) {
		for (j = 0; j < geo->a_channels && i < IW_MAX_FREQUENCIES;
		     i++, j++) {
			range->freq[i].i = geo->a[j].channel;
			range->freq[i].m = geo->a[j].freq * 100000;
			range->freq[i].e = 1;
		}
	}

	range->num_channels = i;
	range->num_frequency = i;

#if WIRELESS_EXT > 16
	/* Event capability (kernel + driver) */
	range->event_capa[0] = (IW_EVENT_CAPA_K_0 |
				IW_EVENT_CAPA_MASK(SIOCGIWTHRSPY) |
				IW_EVENT_CAPA_MASK(SIOCGIWAP));
	range->event_capa[1] = IW_EVENT_CAPA_K_1;
#endif				/* WIRELESS_EXT > 16 */

	mutex_unlock(&priv->mutex);
	IPW_DEBUG_WX("GET Range\n");
	return 0;
}

static int ipw_wx_set_wap(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	static const unsigned char any[] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	};
	static const unsigned char off[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	if (wrqu->ap_addr.sa_family != ARPHRD_ETHER)
		return -EINVAL;

	mutex_lock(&priv->mutex);
	if (!memcmp(any, wrqu->ap_addr.sa_data, ETH_ALEN) ||
	    !memcmp(off, wrqu->ap_addr.sa_data, ETH_ALEN)) {
		/* we disable mandatory BSSID association */
		IPW_DEBUG_WX("Setting AP BSSID to ANY\n");
		priv->config &= ~CFG_STATIC_BSSID;
		IPW_DEBUG_ASSOC("Attempting to associate with new "
				"parameters.\n");
		ipw_associate(priv);
		mutex_unlock(&priv->mutex);
		return 0;
	}

	priv->config |= CFG_STATIC_BSSID;
	if (!memcmp(priv->bssid, wrqu->ap_addr.sa_data, ETH_ALEN)) {
		IPW_DEBUG_WX("BSSID set to current BSSID.\n");
		mutex_unlock(&priv->mutex);
		return 0;
	}

	IPW_DEBUG_WX("Setting mandatory BSSID to " MAC_FMT "\n",
		     MAC_ARG(wrqu->ap_addr.sa_data));
	memcpy(priv->bssid, wrqu->ap_addr.sa_data, ETH_ALEN);
	/* Network configuration changed -- force [re]association */
	IPW_DEBUG_ASSOC("[re]association triggered due to BSSID change.\n");
	if (!ipw_disassociate(priv))
		ipw_associate(priv);
	mutex_unlock(&priv->mutex);
	return 0;
}

static int ipw_wx_get_wap(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	/* If we are associated, trying to associate, or have a statically
	 * configured BSSID then return that; otherwise return ANY */

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	mutex_lock(&priv->mutex);
	if (priv->config & CFG_STATIC_BSSID ||
	    priv->status & (STATUS_ASSOCIATED | STATUS_ASSOCIATING)) {
		wrqu->ap_addr.sa_family = ARPHRD_ETHER;
		memcpy(wrqu->ap_addr.sa_data, &priv->bssid, ETH_ALEN);
	} else
		memset(wrqu->ap_addr.sa_data, 0, ETH_ALEN);
	IPW_DEBUG_WX("Getting WAP BSSID: " MAC_FMT "\n",
		     MAC_ARG(wrqu->ap_addr.sa_data));
	mutex_unlock(&priv->mutex);
	return 0;
}

static int ipw_wx_set_essid(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	char *essid = "";	/* ANY */
	int length = 0;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	mutex_lock(&priv->mutex);
	if (wrqu->essid.flags && wrqu->essid.length) {
		length = wrqu->essid.length - 1;
		essid = extra;
	}

	if (length == 0) {
		IPW_DEBUG_WX("Setting ESSID to ANY\n");
		priv->config &= ~CFG_STATIC_ESSID;
		if (!(priv->status & (STATUS_ASSOCIATED | STATUS_ASSOCIATING))) {
			IPW_DEBUG_ASSOC("Attempting to associate with new "
					"parameters.\n");
			ipw_associate(priv);
		}
		mutex_unlock(&priv->mutex);
		return 0;
	}

	length = min(length, IW_ESSID_MAX_SIZE);
	priv->config |= CFG_STATIC_ESSID;
	if (priv->essid_len == length && !memcmp(priv->essid, extra, length)) {
		IPW_DEBUG_WX("ESSID set to current ESSID.\n");
		mutex_unlock(&priv->mutex);
		return 0;
	}

	IPW_DEBUG_WX("Setting ESSID: '%s' (%d)\n",
		     escape_essid(essid, length), length);
	priv->essid_len = length;
	memcpy(priv->essid, essid, priv->essid_len);
	/* Network configuration changed -- force [re]association */
	IPW_DEBUG_ASSOC("[re]association triggered due to ESSID change.\n");
	if (!ipw_disassociate(priv))
		ipw_associate(priv);

	mutex_unlock(&priv->mutex);
	return 0;
}

static int ipw_wx_get_essid(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	/* If we are associated, trying to associate, or have a statically
	 * configured ESSID then return that; otherwise return ANY */
	mutex_lock(&priv->mutex);
	if (priv->config & CFG_STATIC_ESSID ||
	    priv->status & (STATUS_ASSOCIATED | STATUS_ASSOCIATING)) {
		IPW_DEBUG_WX("Getting essid: '%s'\n",
			     escape_essid(priv->essid, priv->essid_len));
		memcpy(extra, priv->essid, priv->essid_len);
		wrqu->essid.length = priv->essid_len;
		wrqu->essid.flags = 1;	/* active */
	} else {
		IPW_DEBUG_WX("Getting essid: ANY\n");
		wrqu->essid.length = 0;
		wrqu->essid.flags = 0;	/* active */
	}
	mutex_unlock(&priv->mutex);
	return 0;
}

static int ipw_wx_set_nick(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	IPW_DEBUG_WX("Setting nick to '%s'\n", extra);

	if (wrqu->data.length > IW_ESSID_MAX_SIZE)
		return -E2BIG;

	mutex_lock(&priv->mutex);
	wrqu->data.length = min((size_t) wrqu->data.length, sizeof(priv->nick));
	memset(priv->nick, 0, sizeof(priv->nick));
	memcpy(priv->nick, extra, wrqu->data.length);
	mutex_unlock(&priv->mutex);
	return 0;
}

static int ipw_wx_get_nick(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	IPW_DEBUG_WX("Getting nick\n");
	mutex_lock(&priv->mutex);
	wrqu->data.length = strlen(priv->nick) + 1;
	memcpy(extra, priv->nick, wrqu->data.length);
	wrqu->data.flags = 1;	/* active */
	mutex_unlock(&priv->mutex);
	return 0;
}

static int ipw_wx_set_rate(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	/* TODO: We should use semaphores or locks for access to priv */
	struct ipw_priv *priv = ieee80211_priv(dev);
	u32 target_rate = wrqu->bitrate.value;
	u32 fixed, mask;
	int ret = 0, rc = -1;
	struct ipw_supported_rates rates;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	/* value = -1, fixed = 0 means auto only, so we should use
	 * all rates offered by AP
	 * value = X, fixed = 1 means only rate X
	 * value = X, fixed = 0 means all rates lower equal X */
	if (target_rate == -1) {
		fixed = 0;
		mask = IEEE80211_DEFAULT_RATES_MASK;
		/* Now we should reassociate */
		goto apply;
	}

	mask = 0;
	fixed = wrqu->bitrate.fixed;
	if (target_rate == 1000000 || !fixed)
		mask |= IEEE80211_CCK_RATE_1MB_MASK;
	if (target_rate == 1000000)
		goto apply;
	if (target_rate == 2000000 || !fixed)
		mask |= IEEE80211_CCK_RATE_2MB_MASK;
	if (target_rate == 2000000)
		goto apply;
	if (target_rate == 5500000 || !fixed)
		mask |= IEEE80211_CCK_RATE_5MB_MASK;
	if (target_rate == 5500000)
		goto apply;
	if (target_rate == 6000000 || !fixed)
		mask |= IEEE80211_OFDM_RATE_6MB_MASK;
	if (target_rate == 6000000)
		goto apply;
	if (target_rate == 9000000 || !fixed)
		mask |= IEEE80211_OFDM_RATE_9MB_MASK;
	if (target_rate == 9000000)
		goto apply;
	if (target_rate == 11000000 || !fixed)
		mask |= IEEE80211_CCK_RATE_11MB_MASK;
	if (target_rate == 11000000)
		goto apply;
	if (target_rate == 12000000 || !fixed)
		mask |= IEEE80211_OFDM_RATE_12MB_MASK;
	if (target_rate == 12000000)
		goto apply;
	if (target_rate == 18000000 || !fixed)
		mask |= IEEE80211_OFDM_RATE_18MB_MASK;
	if (target_rate == 18000000)
		goto apply;
	if (target_rate == 24000000 || !fixed)
		mask |= IEEE80211_OFDM_RATE_24MB_MASK;
	if (target_rate == 24000000)
		goto apply;
	if (target_rate == 36000000 || !fixed)
		mask |= IEEE80211_OFDM_RATE_36MB_MASK;
	if (target_rate == 36000000)
		goto apply;
	if (target_rate == 48000000 || !fixed)
		mask |= IEEE80211_OFDM_RATE_48MB_MASK;
	if (target_rate == 48000000)
		goto apply;
	if (target_rate == 54000000 || !fixed)
		mask |= IEEE80211_OFDM_RATE_54MB_MASK;
	if (target_rate == 54000000)
		goto apply;
	IPW_DEBUG_WX("invalid rate specified, returning error\n");
	return -EINVAL;
      apply:
	IPW_DEBUG_WX("Setting rate mask to 0x%08X [%s]\n",
		     mask, fixed ? "fixed" : "sub-rates");
	mutex_lock(&priv->mutex);
	if (mask == IEEE80211_DEFAULT_RATES_MASK)
		priv->config &= ~CFG_FIXED_RATE;
	else
		priv->config |= CFG_FIXED_RATE;
	if (priv->rates_mask == mask) {
		IPW_DEBUG_WX("Mask set to current mask.\n");
		mutex_unlock(&priv->mutex);
		return 0;
	}

	priv->rates_mask = mask;

	if (priv->status & STATUS_ASSOCIATED) {
		unsigned long flags;

		spin_lock_irqsave(&priv->ieee->lock, flags);
		ret = ipw_compatible_rates(priv, priv->assoc_network, &rates);
		spin_unlock_irqrestore(&priv->ieee->lock, flags);
	}
	if ((ret) && (rates.num_rates != 0)) {
		ipw_send_supported_rates(priv, &rates);
		priv->rxon.cck_basic_rates =
		    ((priv->active_rate_basic & 0xF) | R_1M_MSK);
		priv->rxon.ofdm_basic_rates =
		    ((priv->active_rate_basic >> 4) | R_6M_MSK);

		if ((priv->active_rate_basic & 0xF) == 0)
			priv->rxon.cck_basic_rates =
			    R_1M_MSK | R_2M_MSK | R_5_5M_MSK | R_11M_MSK;
		if (priv->active_rate_basic >> 4 == 0)
			priv->rxon.ofdm_basic_rates =
			    R_6M_MSK | R_12M_MSK | R_24M_MSK;

		rc = ipw_rxon_call(priv, 1);

	}
	if (rc) {
		/* Network configuration changed -- force [re]association */
		IPW_DEBUG_ASSOC
		    ("[re]association triggered due to rates change.\n");
		if (!ipw_disassociate(priv))
			ipw_associate(priv);
	}
	mutex_unlock(&priv->mutex);
	return 0;
}

static int ipw_wx_get_rate(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	u32 max_rate;

	mutex_lock(&priv->mutex);
	/* If we are not actively transmitting data then the
	 * priv->last_rate may be set to a data rate higher
	 * than has actually been configured via ipw_wx_set_rate. */
	max_rate = ipw_get_max_rate(priv);
	if (max_rate >= priv->last_rate)
		wrqu->bitrate.value = priv->last_rate;
	else
		wrqu->bitrate.value = max_rate;
	mutex_unlock(&priv->mutex);
	IPW_DEBUG_WX("GET Rate -> %d \n", wrqu->bitrate.value);
	return 0;
}

static int ipw_wx_set_rts(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	mutex_lock(&priv->mutex);
	if (wrqu->rts.disabled)
		priv->rts_threshold = DEFAULT_RTS_THRESHOLD;
	else {
		if (wrqu->rts.value < MIN_RTS_THRESHOLD ||
		    wrqu->rts.value > MAX_RTS_THRESHOLD) {
			mutex_unlock(&priv->mutex);
			return -EINVAL;
		}
		priv->rts_threshold = wrqu->rts.value;
	}

	mutex_unlock(&priv->mutex);
	IPW_DEBUG_WX("SET RTS Threshold -> %d \n", priv->rts_threshold);
	return 0;
}

static int ipw_wx_get_rts(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	mutex_lock(&priv->mutex);
	wrqu->rts.value = priv->rts_threshold;
	wrqu->rts.fixed = 0;	/* no auto select */
	wrqu->rts.disabled = (wrqu->rts.value == DEFAULT_RTS_THRESHOLD);
	mutex_unlock(&priv->mutex);
	IPW_DEBUG_WX("GET RTS Threshold -> %d \n", wrqu->rts.value);
	return 0;
}

static int ipw_wx_set_txpow(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	struct daemon_sync_txpower_limit sync_power;
	int rc = 0;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	mutex_lock(&priv->mutex);
	ipw_radio_kill_sw(priv, wrqu->txpower.disabled);

	if (wrqu->txpower.disabled) {
		IPW_DEBUG_WX("Turning Tx power OFF\n");
		goto out;
	}

	IPW_DEBUG_WX("Turning Tx power ON\n");

	if (wrqu->txpower.flags != IW_TXPOW_DBM) {
		IPW_DEBUG_WX("Tx Power must be in dBm\n");
		rc = -EINVAL;
		goto out;
	}

	if (!wrqu->txpower.fixed) {
		IPW_DEBUG_WX("Setting maximum Tx Power to SKU limit: %ddBm\n",
			     priv->max_channel_txpower_limit);
		wrqu->txpower.value = priv->max_channel_txpower_limit;
		priv->config &= ~CFG_TXPOWER_LIMIT;
	} else if (wrqu->txpower.value > priv->max_channel_txpower_limit) {
		wrqu->txpower.value = priv->max_channel_txpower_limit;
		priv->config |= CFG_TXPOWER_LIMIT;
	} else if (wrqu->txpower.value < -12) {
		IPW_DEBUG_WX("Tx Power out of range (-12 to %d)\n",
			     priv->max_channel_txpower_limit);
		rc = -EINVAL;
		goto out;
	} else
		priv->config |= CFG_TXPOWER_LIMIT;

	if (priv->config & CFG_TXPOWER_LIMIT)
		priv->user_txpower_limit = wrqu->txpower.value;

	sync_power.channel = priv->channel;
	sync_power.power = wrqu->txpower.value;

	IPW_DEBUG_WX("Tx Power set to %ddBm\n", sync_power.power);

	ipw_send_daemon_sync(priv, DAEMON_SYNC_TXPOWER_LIMIT,
			     sizeof(sync_power), &sync_power);

      out:
	mutex_unlock(&priv->mutex);
	return rc;
}

static int ipw_wx_get_txpow(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	mutex_lock(&priv->mutex);
	if (priv->status & STATUS_ASSOCIATED)
		wrqu->txpower.value = priv->actual_txpower_limit;
	else
		wrqu->txpower.value = priv->max_channel_txpower_limit;

	/* If the user specified a power level, report the lower of that
	 * value or what was just calculated. */
	if (priv->config & CFG_TXPOWER_LIMIT) {
		wrqu->txpower.fixed = 1;
		wrqu->txpower.value = min((int)priv->user_txpower_limit,
					  (int)wrqu->txpower.value);
	} else
		wrqu->txpower.fixed = 0;

	wrqu->txpower.flags = IW_TXPOW_DBM;
	wrqu->txpower.disabled = (priv->status & STATUS_RF_KILL_MASK) ? 1 : 0;
	mutex_unlock(&priv->mutex);
	IPW_DEBUG_WX("GET TX Power -> %s %d \n",
		     wrqu->txpower.disabled ? "OFF" : "ON", wrqu->power.value);
	return 0;
}

//todoG fix it
static int ipw_wx_set_frag(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	mutex_lock(&priv->mutex);
	if (wrqu->frag.disabled)
		priv->ieee->fts = DEFAULT_FTS;
	else {
		if (wrqu->frag.value < MIN_FRAG_THRESHOLD ||
		    wrqu->frag.value > MAX_FRAG_THRESHOLD) {
			mutex_unlock(&priv->mutex);
			return -EINVAL;
		}

		priv->ieee->fts = wrqu->frag.value & ~0x1;
	}

	mutex_unlock(&priv->mutex);
	IPW_DEBUG_WX("SET Frag Threshold -> %d \n", wrqu->frag.value);
	return 0;
}

static int ipw_wx_get_frag(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	mutex_lock(&priv->mutex);
	wrqu->frag.value = priv->ieee->fts;
	wrqu->frag.fixed = 0;	/* no auto select */
	wrqu->frag.disabled = (wrqu->frag.value == DEFAULT_FTS);
	mutex_unlock(&priv->mutex);
	IPW_DEBUG_WX("GET Frag Threshold -> %d \n", wrqu->frag.value);
	return 0;
}

static int ipw_wx_set_retry(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	IPW_DEBUG_WX("0x%p, 0x%p, 0x%p\n", dev, info, wrqu);

	if (!(wrqu->retry.flags & IW_RETRY_LIMIT))
		return 0;

	if ((wrqu->retry.value < 1) || (wrqu->retry.value > IPW_MAX_TX_RETRY))
		return -EINVAL;

	mutex_lock(&priv->mutex);
	priv->data_retry_limit = wrqu->retry.value;
	mutex_unlock(&priv->mutex);

	return 0;
}

static int ipw_wx_get_retry(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	IPW_DEBUG_WX("0x%p, 0x%p, 0x%p\n", dev, info, wrqu);

	mutex_lock(&priv->mutex);
	wrqu->retry.disabled = 0;
	if (priv->data_retry_limit == -1)
		wrqu->retry.value = IPW_DEFAULT_TX_RETRY;
	else
		wrqu->retry.value = priv->data_retry_limit;

	wrqu->retry.flags = IW_RETRY_LIMIT;

	mutex_unlock(&priv->mutex);

	return 0;
}

static int ipw_wx_set_scan(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	int rc = 0;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	IPW_DEBUG_WX("Start scan\n");

	mutex_lock(&priv->mutex);

	if (!(priv->status & STATUS_SCANNING))
		rc = ipw_scan_initiate(priv, priv->ieee->freq_band, 0);

	mutex_unlock(&priv->mutex);

	return rc;
}

static int ipw_wx_get_scan(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	return ieee80211_wx_get_scan(priv->ieee, info, wrqu, extra);
}

static int ipw_wx_set_encode(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *key)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	int rc;
	u32 cap = priv->capability;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	rc = ieee80211_wx_set_encode(priv->ieee, info, wrqu, key);

	if (cap != priv->capability &&
	    priv->ieee->iw_mode == IW_MODE_ADHOC &&
	    priv->status & STATUS_ASSOCIATED) {

		mutex_lock(&priv->mutex);
		if (priv->config & CFG_ADHOC_CREATE &&
		    priv->config & CFG_STATIC_ESSID &&
		    priv->config & CFG_STATIC_CHANNEL &&
		    priv->status & STATUS_ASSOCIATED) {

			if ((priv->capability & CAP_PRIVACY_ON) &&
			    (priv->capability & CAP_SHARED_KEY)) {
				priv->assoc_request.auth_type = AUTH_SHARED_KEY;
				priv->assoc_request.auth_key =
				    priv->ieee->sec.active_key;
			} else {
				priv->assoc_request.auth_type = AUTH_OPEN;
				priv->assoc_request.auth_key = 0;
			}

			ipw_send_beacon_cmd(priv);
		} else
			queue_work(priv->workqueue, &priv->disassociate);

		mutex_unlock(&priv->mutex);
	}

	return rc;

}

static int ipw_wx_get_encode(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *key)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	return ieee80211_wx_get_encode(priv->ieee, info, wrqu, key);
}

static int ipw_wx_set_power(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	int err;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	mutex_lock(&priv->mutex);
	if (wrqu->power.disabled) {
		priv->power_mode = IPW_POWER_LEVEL(priv->power_mode);
		err = ipw_send_power_mode(priv, IPW_POWER_MODE_CAM);
		if (err) {
			IPW_DEBUG_WX("failed setting power mode.\n");
			mutex_unlock(&priv->mutex);
			return err;
		}
		IPW_DEBUG_WX("SET Power Management Mode -> off\n");
		mutex_unlock(&priv->mutex);
		return 0;
	}

	switch (wrqu->power.flags & IW_POWER_MODE) {
	case IW_POWER_ON:	/* If not specified */
	case IW_POWER_MODE:	/* If set all mask */
	case IW_POWER_ALL_R:	/* If explicitely state all */
		break;
	default:		/* Otherwise we don't support it */
		IPW_DEBUG_WX("SET PM Mode: %X not supported.\n",
			     wrqu->power.flags);
		mutex_unlock(&priv->mutex);
		return -EOPNOTSUPP;
	}

	/* If the user hasn't specified a power management mode yet, default
	 * to BATTERY */
	if ((IPW_POWER_LEVEL(priv->power_mode) == IPW_POWER_AC) ||
	    (IPW_POWER_LEVEL(priv->power_mode) == IPW_POWER_MODE_CAM))
		priv->power_mode = IPW_POWER_MODE_CAM;
	else
		priv->power_mode = IPW_POWER_ENABLED | priv->power_mode;
	err = ipw_send_power_mode(priv, IPW_POWER_LEVEL(priv->power_mode));
	if (err) {
		IPW_DEBUG_WX("failed setting power mode.\n");
		mutex_unlock(&priv->mutex);
		return err;
	}

	IPW_DEBUG_WX("SET Power Management Mode -> 0x%02X\n", priv->power_mode);
	mutex_unlock(&priv->mutex);
	return 0;
}

static int ipw_wx_get_power(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	mutex_lock(&priv->mutex);
	if (!(priv->power_mode & IPW_POWER_ENABLED))
		wrqu->power.disabled = 1;
	else
		wrqu->power.disabled = 0;
	mutex_unlock(&priv->mutex);
	IPW_DEBUG_WX("GET Power Management Mode -> %02X\n", priv->power_mode);
	return 0;
}

#if WIRELESS_EXT > 17
/*
 * WE-18 Support
 */
/* SIOCSIWGENIE */
static int ipw_wx_set_genie(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee;
	u8 *buf;
	int err = 0;
	struct ieee80211_info_element *info_element;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	if (wrqu->data.length && extra == NULL)
		return -EINVAL;

	info_element = (struct ieee80211_info_element *)extra;

	//mutex_lock(&priv->mutex);

	//if (!ieee->wpa_enabled) {
	//      err = -EOPNOTSUPP;
	//      goto out;
	//}

	if ((info_element->id == MFIE_TYPE_GENERIC &&
	     info_element->len >= 4 &&
	     info_element->data[0] == 0x00 &&
	     info_element->data[1] == 0x50 &&
	     info_element->data[2] == 0xf2 &&
	     info_element->data[3] == 0x01) ||
	    info_element->id == MFIE_TYPE_RSN) {
		if (wrqu->data.length) {
			if (wrqu->data.length > MAX_WPA_IE_LEN)
				return -EINVAL;

			buf = kmalloc(wrqu->data.length, GFP_KERNEL);
			if (buf == NULL) {
				err = -ENOMEM;
				goto out;
			}

			memcpy(buf, extra, wrqu->data.length);
			kfree(ieee->wpa_ie);
			ieee->wpa_ie = buf;
			ieee->wpa_ie_len = wrqu->data.length;
		} else {
			kfree(ieee->wpa_ie);
			ieee->wpa_ie = NULL;
			ieee->wpa_ie_len = 0;
		}

		ipw_wpa_assoc_frame(priv, ieee->wpa_ie, ieee->wpa_ie_len);
	}
      out:
	//mutex_unlock(&priv->mutex);
	return err;
}

/* SIOCGIWGENIE */
static int ipw_wx_get_genie(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee;
	int err = 0;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	//mutex_lock(&priv->mutex);

	//if (!ieee->wpa_enabled) {
	//      err = -EOPNOTSUPP;
	//      goto out;
	//}

	if (ieee->wpa_ie_len == 0 || ieee->wpa_ie == NULL) {
		wrqu->data.length = 0;
		goto out;
	}

	if (wrqu->data.length < ieee->wpa_ie_len) {
		err = -E2BIG;
		goto out;
	}

	wrqu->data.length = ieee->wpa_ie_len;
	memcpy(extra, ieee->wpa_ie, ieee->wpa_ie_len);

      out:
	//mutex_unlock(&priv->mutex);
	return err;
}

/* SIOCSIWAUTH */
static int ipw_wx_set_auth(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee;
	struct iw_param *param = &wrqu->param;
	struct ieee80211_crypt_data *crypt;
	unsigned long flags;
	int ret = 0;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	switch (param->flags & IW_AUTH_INDEX) {
	case IW_AUTH_WPA_VERSION:
	case IW_AUTH_CIPHER_PAIRWISE:
	case IW_AUTH_CIPHER_GROUP:
	case IW_AUTH_KEY_MGMT:
		/*
		 * ipw2200 does not use these parameters
		 */
		break;

	case IW_AUTH_TKIP_COUNTERMEASURES:
		crypt = priv->ieee->crypt[priv->ieee->tx_keyidx];
		if (!crypt || !crypt->ops->set_flags || !crypt->ops->get_flags)
			break;

		flags = crypt->ops->get_flags(crypt->priv);

		if (param->value)
			flags |= IEEE80211_CRYPTO_TKIP_COUNTERMEASURES;
		else
			flags &= ~IEEE80211_CRYPTO_TKIP_COUNTERMEASURES;

		crypt->ops->set_flags(flags, crypt->priv);

		break;

	case IW_AUTH_DROP_UNENCRYPTED:{
			/* HACK:
			 *
			 * wpa_supplicant calls set_wpa_enabled when the driver
			 * is loaded and unloaded, regardless of if WPA is being
			 * used.  No other calls are made which can be used to
			 * determine if encryption will be used or not prior to
			 * association being expected.  If encryption is not being
			 * used, drop_unencrypted is set to false, else true -- we
			 * can use this to determine if the CAP_PRIVACY_ON bit should
			 * be set.
			 */
			struct ieee80211_security sec = {
				.flags = SEC_ENABLED,
				.enabled = param->value,
			};
			priv->ieee->drop_unencrypted = param->value;
			/* We only change SEC_LEVEL for open mode. Others
			 * are set by ipw_wpa_set_encryption.
			 */
			if (!param->value) {
				sec.flags |= SEC_LEVEL;
				sec.level = SEC_LEVEL_0;
			} else {
				sec.flags |= SEC_LEVEL;
				sec.level = SEC_LEVEL_1;
			}
			if (priv->ieee->set_security)
				priv->ieee->set_security(priv->ieee->dev, &sec);
			break;
		}

	case IW_AUTH_80211_AUTH_ALG:
		ret = ipw_wpa_set_auth_algs(priv, param->value);
		break;

	case IW_AUTH_WPA_ENABLED:
		ret = ipw_wpa_enable(priv, param->value);
		break;

	case IW_AUTH_RX_UNENCRYPTED_EAPOL:
		ieee->ieee802_1x = param->value;
		break;

		//case IW_AUTH_ROAMING_CONTROL:
	case IW_AUTH_PRIVACY_INVOKED:
		ieee->privacy_invoked = param->value;
		break;

	default:
		return -EOPNOTSUPP;
	}
	return ret;
}

/* SIOCGIWAUTH */
static int ipw_wx_get_auth(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee;
	struct iw_param *param = &wrqu->param;
	struct ieee80211_crypt_data *crypt;
	unsigned long flags;
	int ret = 0;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	switch (param->flags & IW_AUTH_INDEX) {
	case IW_AUTH_WPA_VERSION:
	case IW_AUTH_CIPHER_PAIRWISE:
	case IW_AUTH_CIPHER_GROUP:
	case IW_AUTH_KEY_MGMT:
		/*
		 * wpa_supplicant will control these internally
		 */
		ret = -EOPNOTSUPP;
		break;

	case IW_AUTH_TKIP_COUNTERMEASURES:
		crypt = priv->ieee->crypt[priv->ieee->tx_keyidx];
		if (!crypt || !crypt->ops->set_flags || !crypt->ops->get_flags)
			break;

		flags = crypt->ops->get_flags(crypt->priv);

		if (param->value)
			flags |= IEEE80211_CRYPTO_TKIP_COUNTERMEASURES;
		else
			flags &= ~IEEE80211_CRYPTO_TKIP_COUNTERMEASURES;

		crypt->ops->set_flags(flags, crypt->priv);

		break;

	case IW_AUTH_DROP_UNENCRYPTED:
		param->value = ieee->drop_unencrypted;
		break;

	case IW_AUTH_80211_AUTH_ALG:
		param->value = ieee->sec.auth_mode;
		break;

	case IW_AUTH_WPA_ENABLED:
		param->value = ieee->wpa_enabled;
		break;

	case IW_AUTH_RX_UNENCRYPTED_EAPOL:
		param->value = ieee->ieee802_1x;
		break;

	case IW_AUTH_ROAMING_CONTROL:
	case IW_AUTH_PRIVACY_INVOKED:
		param->value = ieee->privacy_invoked;
		break;

	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

/* SIOCSIWENCODEEXT */
static int ipw_wx_set_encodeext(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	struct iw_encode_ext *ext = (struct iw_encode_ext *)extra;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	/* IPW HW can't build TKIP MIC, host decryption still needed */
	if (ext->alg == IW_ENCODE_ALG_TKIP) {
		priv->ieee->host_encrypt = 1;
		priv->ieee->host_encrypt_msdu = 1;
		priv->ieee->host_decrypt = 1;
	} else {
		priv->ieee->host_encrypt = 1;
		priv->ieee->host_encrypt_msdu = 0;
		priv->ieee->host_decrypt = 1;
	}

	return ieee80211_wx_set_encodeext(priv->ieee, info, wrqu, extra);
}

/* SIOCGIWENCODEEXT */
static int ipw_wx_get_encodeext(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	return ieee80211_wx_get_encodeext(priv->ieee, info, wrqu, extra);
}

/* SIOCSIWMLME */
static int ipw_wx_set_mlme(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	struct iw_mlme *mlme = (struct iw_mlme *)extra;
	u16 reason;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	reason = cpu_to_le16(mlme->reason_code);

	switch (mlme->cmd) {
	case IW_MLME_DEAUTH:
		ipw_disassociate(priv);
		break;

	case IW_MLME_DISASSOC:
		ipw_disassociate(priv);
		break;

	default:
		return -EOPNOTSUPP;
	}
	return 0;
}
#endif				/* WIRELESS_EXT > 17 */

static int ipw_wx_set_powermode(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	int mode = *(int *)extra;
	int err;
	int old_power_mode = priv->power_mode;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	mutex_lock(&priv->mutex);
	if ((mode < 1) || (mode > IPW_POWER_LIMIT)) {
		mode = IPW_POWER_AC;
		priv->power_mode = mode;
	} else {
		priv->power_mode = IPW_POWER_ENABLED | mode;
	}

	if (old_power_mode != priv->power_mode) {
		err = ipw_send_power_mode(priv, mode);
		if (err) {
			IPW_DEBUG_WX("failed setting power mode.\n");
			mutex_unlock(&priv->mutex);
			return err;
		}
	}
	mutex_unlock(&priv->mutex);
	return 0;
}

#define MAX_WX_STRING 80
static int ipw_wx_get_powermode(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	int level = IPW_POWER_LEVEL(priv->power_mode);
	char *p = extra;

	p += snprintf(p, MAX_WX_STRING, "Power save level: %d ", level);
	switch (level) {
	case IPW_POWER_AC:
		p += snprintf(p, MAX_WX_STRING - (p - extra), "(AC)");
		break;
	case IPW_POWER_BATTERY:
		p += snprintf(p, MAX_WX_STRING - (p - extra), "(BATTERY)");
		break;
	default:
		p += snprintf(p, MAX_WX_STRING - (p - extra),
			      "(Timeout %dms, Period %dms)",
			      timeout_duration[level - 1] / 1000,
			      period_duration[level - 1] / 1000);
	}

	if (!(priv->power_mode & IPW_POWER_ENABLED))
		p += snprintf(p, MAX_WX_STRING - (p - extra), " OFF");
	wrqu->data.length = p - extra + 1;
	return 0;
}

static int ipw_wx_set_wireless_mode(struct net_device *dev,
				    struct iw_request_info *info,
				    union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	int mode = *(int *)extra;
	u8 band = 0, modulation = 0;

	if (!ipw_is_ready(priv))
		return -EAGAIN;

	if (mode == 0 || mode & ~IEEE_MODE_MASK) {
		IPW_WARNING("Attempt to set invalid wireless mode: %d\n", mode);
		return -EINVAL;
	}

	if (mode & IEEE_A) {
		if (!priv->is_abg) {
			IPW_WARNING("Attempt to set "
				    "Intel PRO/Wireless 3945BG Network "
				    "Connection into A mode.\n");
			return -EINVAL;
		}

		band |= IEEE80211_52GHZ_BAND;
		modulation |= IEEE80211_OFDM_MODULATION;
	}

	if (mode & IEEE_B) {
		band |= IEEE80211_24GHZ_BAND;
		modulation |= IEEE80211_CCK_MODULATION;
	}

	if (mode & IEEE_G) {
		band |= IEEE80211_24GHZ_BAND;
		modulation |= IEEE80211_OFDM_MODULATION;
	}

	mutex_lock(&priv->mutex);

	priv->ieee->mode = mode;
	priv->ieee->freq_band = band;
	priv->ieee->modulation = modulation;
	init_supported_rates(priv, &priv->rates);
	/* Network configuration changed -- force [re]association */
	IPW_DEBUG_ASSOC("[re]association triggered due to mode change.\n");
	if (!ipw_disassociate(priv)) {
		ipw_send_supported_rates(priv, &priv->rates);
		ipw_associate(priv);
	}

	IPW_DEBUG_WX("PRIV SET MODE: %c%c%c\n",
		     mode & IEEE_A ? 'a' : '.',
		     mode & IEEE_B ? 'b' : '.', mode & IEEE_G ? 'g' : '.');
	mutex_unlock(&priv->mutex);
	return 0;
}

static int ipw_wx_get_wireless_mode(struct net_device *dev,
				    struct iw_request_info *info,
				    union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	if (!ipw_is_alive(priv))
		return -EAGAIN;

	if (priv->status & STATUS_EXIT_PENDING)
		return 0;

	mutex_lock(&priv->mutex);
	switch (priv->ieee->mode) {
	case IEEE_A:
		strncpy(extra, "802.11a (1)", MAX_WX_STRING);
		break;
	case IEEE_B:
		strncpy(extra, "802.11b (2)", MAX_WX_STRING);
		break;
	case IEEE_A | IEEE_B:
		strncpy(extra, "802.11ab (3)", MAX_WX_STRING);
		break;
	case IEEE_G:
		strncpy(extra, "802.11g (4)", MAX_WX_STRING);
		break;
	case IEEE_A | IEEE_G:
		strncpy(extra, "802.11ag (5)", MAX_WX_STRING);
		break;
	case IEEE_B | IEEE_G:
		strncpy(extra, "802.11bg (6)", MAX_WX_STRING);
		break;
	case IEEE_A | IEEE_B | IEEE_G:
		strncpy(extra, "802.11abg (7)", MAX_WX_STRING);
		break;
	default:
		strncpy(extra, "unknown", MAX_WX_STRING);
		break;
	}

	IPW_DEBUG_WX("PRIV GET MODE: %s\n", extra);
	wrqu->data.length = strlen(extra) + 1;
	mutex_unlock(&priv->mutex);
	return 0;
}

static int ipw_wx_set_preamble(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	int mode = *(int *)extra;

	if (!ipw_is_alive(priv))
		return -EAGAIN;

	if (priv->status & STATUS_EXIT_PENDING)
		return 0;

	mutex_lock(&priv->mutex);
	/* Switching from SHORT -> LONG requires a disassociation */
	if (mode == 1) {
		if (!(priv->config & CFG_PREAMBLE_LONG)) {
			priv->config |= CFG_PREAMBLE_LONG;
			/* Network configuration changed -- force [re]association */
			IPW_DEBUG_ASSOC
			    ("[re]association triggered due to preamble change.\n");
			if (!ipw_disassociate(priv))
				ipw_associate(priv);
		}
		goto done;
	}

	if (mode == 0) {
		priv->config &= ~CFG_PREAMBLE_LONG;
		goto done;
	}
	mutex_unlock(&priv->mutex);
	return -EINVAL;
      done:
	mutex_unlock(&priv->mutex);
	return 0;
}

static int ipw_wx_get_preamble(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	if (!ipw_is_alive(priv))
		return -EAGAIN;

	mutex_lock(&priv->mutex);
	if (priv->config & CFG_PREAMBLE_LONG)
		snprintf(wrqu->name, IFNAMSIZ, "long (1)");
	else
		snprintf(wrqu->name, IFNAMSIZ, "auto (0)");
	mutex_unlock(&priv->mutex);
	return 0;
}

#ifdef CONFIG_IPW3945_MONITOR
static int ipw_wx_set_monitor(struct net_device *dev,
			      struct iw_request_info *info,
			      union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	int *parms = (int *)extra;
	int enable = (parms[0] > 0);

	if (!ipw_is_alive(priv))
		return -EAGAIN;

	if (priv->status & STATUS_EXIT_PENDING)
		return 0;

	mutex_lock(&priv->mutex);
	if (enable) {
		if (priv->ieee->iw_mode != IW_MODE_MONITOR) {
			priv->net_dev->type = ARPHRD_IEEE80211;
			IPW_DEBUG(IPW_DL_INFO | IPW_DL_WX,
				  "Restarting adapter to MONITOR mode (ch %d).\n",
				  parms[1]);
			queue_work(priv->workqueue, &priv->down);
		}

		ipw_set_channel(priv, parms[1]);
	} else {
		if (priv->ieee->iw_mode != IW_MODE_MONITOR) {
			mutex_unlock(&priv->mutex);
			return 0;
		}
		priv->net_dev->type = ARPHRD_ETHER;
		IPW_DEBUG(IPW_DL_INFO | IPW_DL_WX,
			  "Restarting adapter to NORMAL mode.\n");
		queue_work(priv->workqueue, &priv->down);
	}
	mutex_unlock(&priv->mutex);
	return 0;
}
#endif				// CONFIG_IPW3945_MONITOR

static int ipw_wx_reset(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	if (!ipw_is_alive(priv))
		return -EAGAIN;

	if (priv->status & STATUS_EXIT_PENDING)
		return 0;

	IPW_DEBUG(IPW_DL_INFO | IPW_DL_WX, "Restarting adapter.\n");
	queue_work(priv->workqueue, &priv->down);
	return 0;
}

/* Rebase the WE IOCTLs to zero for the handler array */
#define IW_IOCTL(x) [(x)-SIOCSIWCOMMIT]
static iw_handler ipw_wx_handlers[] = {
	IW_IOCTL(SIOCGIWNAME) = ipw_wx_get_name,
	IW_IOCTL(SIOCSIWFREQ) = ipw_wx_set_freq,
	IW_IOCTL(SIOCGIWFREQ) = ipw_wx_get_freq,
	IW_IOCTL(SIOCSIWMODE) = ipw_wx_set_mode,
	IW_IOCTL(SIOCGIWMODE) = ipw_wx_get_mode,
	IW_IOCTL(SIOCGIWRANGE) = ipw_wx_get_range,
	IW_IOCTL(SIOCSIWAP) = ipw_wx_set_wap,
	IW_IOCTL(SIOCGIWAP) = ipw_wx_get_wap,
	IW_IOCTL(SIOCSIWSCAN) = ipw_wx_set_scan,
	IW_IOCTL(SIOCGIWSCAN) = ipw_wx_get_scan,
	IW_IOCTL(SIOCSIWESSID) = ipw_wx_set_essid,
	IW_IOCTL(SIOCGIWESSID) = ipw_wx_get_essid,
	IW_IOCTL(SIOCSIWNICKN) = ipw_wx_set_nick,
	IW_IOCTL(SIOCGIWNICKN) = ipw_wx_get_nick,
	IW_IOCTL(SIOCSIWRATE) = ipw_wx_set_rate,
	IW_IOCTL(SIOCGIWRATE) = ipw_wx_get_rate,
	IW_IOCTL(SIOCSIWRTS) = ipw_wx_set_rts,
	IW_IOCTL(SIOCGIWRTS) = ipw_wx_get_rts,
	IW_IOCTL(SIOCSIWFRAG) = ipw_wx_set_frag,
	IW_IOCTL(SIOCGIWFRAG) = ipw_wx_get_frag,
	IW_IOCTL(SIOCSIWTXPOW) = ipw_wx_set_txpow,
	IW_IOCTL(SIOCGIWTXPOW) = ipw_wx_get_txpow,
	IW_IOCTL(SIOCSIWRETRY) = ipw_wx_set_retry,
	IW_IOCTL(SIOCGIWRETRY) = ipw_wx_get_retry,
	IW_IOCTL(SIOCSIWENCODE) = ipw_wx_set_encode,
	IW_IOCTL(SIOCGIWENCODE) = ipw_wx_get_encode,
	IW_IOCTL(SIOCSIWPOWER) = ipw_wx_set_power,
	IW_IOCTL(SIOCGIWPOWER) = ipw_wx_get_power,
	IW_IOCTL(SIOCSIWSPY) = iw_handler_set_spy,
	IW_IOCTL(SIOCGIWSPY) = iw_handler_get_spy,
	IW_IOCTL(SIOCSIWTHRSPY) = iw_handler_set_thrspy,
	IW_IOCTL(SIOCGIWTHRSPY) = iw_handler_get_thrspy,
#if WIRELESS_EXT > 17
	IW_IOCTL(SIOCSIWGENIE) = ipw_wx_set_genie,
	IW_IOCTL(SIOCGIWGENIE) = ipw_wx_get_genie,
	IW_IOCTL(SIOCSIWMLME) = ipw_wx_set_mlme,
	IW_IOCTL(SIOCSIWAUTH) = ipw_wx_set_auth,
	IW_IOCTL(SIOCGIWAUTH) = ipw_wx_get_auth,
	IW_IOCTL(SIOCSIWENCODEEXT) = ipw_wx_set_encodeext,
	IW_IOCTL(SIOCGIWENCODEEXT) = ipw_wx_get_encodeext,
#endif
};

#define IPW_PRIV_SET_POWER	SIOCIWFIRSTPRIV
#define IPW_PRIV_GET_POWER	SIOCIWFIRSTPRIV+1
#define IPW_PRIV_SET_MODE	SIOCIWFIRSTPRIV+2
#define IPW_PRIV_GET_MODE	SIOCIWFIRSTPRIV+3
#define IPW_PRIV_SET_PREAMBLE	SIOCIWFIRSTPRIV+4
#define IPW_PRIV_GET_PREAMBLE	SIOCIWFIRSTPRIV+5
#define IPW_PRIV_SET_MONITOR	SIOCIWFIRSTPRIV+6
#define IPW_PRIV_RESET		SIOCIWFIRSTPRIV+7
/* *INDENT-OFF* */
static struct iw_priv_args ipw_priv_args[] = {
	{
		.cmd = IPW_PRIV_SET_POWER,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		.name = "set_power"
	}, {
		.cmd = IPW_PRIV_GET_POWER,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | MAX_WX_STRING,
		.name = "get_power"
	}, {
		.cmd = IPW_PRIV_SET_MODE,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		.name = "set_mode"
	}, {
		.cmd = IPW_PRIV_GET_MODE,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | MAX_WX_STRING,
		.name = "get_mode"
	}, {
		.cmd = IPW_PRIV_SET_PREAMBLE,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		.name = "set_preamble"
	}, {
		.cmd = IPW_PRIV_GET_PREAMBLE,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | IFNAMSIZ,
		.name = "get_preamble"
	}, {
		.cmd = IPW_PRIV_RESET,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED,
		.name = "reset"
	},
#ifdef CONFIG_IPW3945_MONITOR
	{
		.cmd = IPW_PRIV_SET_MONITOR,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		.name = "monitor"
	},
#endif				/* CONFIG_IPW3945_MONITOR */
};
/* *INDENT-ON* */
static iw_handler ipw_priv_handler[] = {

	ipw_wx_set_powermode,
	ipw_wx_get_powermode,
	ipw_wx_set_wireless_mode,
	ipw_wx_get_wireless_mode,
	ipw_wx_set_preamble,
	ipw_wx_get_preamble,
	ipw_wx_reset,
#ifdef CONFIG_IPW3945_MONITOR
	ipw_wx_set_monitor,
#endif
};

/*
 * Get wireless statistics.
 * Called by /proc/net/wireless
 * Also called by SIOCGIWSTATS
 */
static struct iw_statistics *ipw_get_wireless_stats(struct net_device *dev)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	struct iw_statistics *wstats;
	wstats = &priv->wstats;

	/* ipw_get_wireless_stats seems to be called before fw is
	 * initialized.  STATUS_ASSOCIATED will only be set if the hw is up
	 * and associated; if not associcated, the values are all meaningless
	 * anyway, so set them all to NULL and INVALID */
	if (!(priv->status & STATUS_ASSOCIATED)) {
		wstats->miss.beacon = 0;
		wstats->discard.retries = 0;
		wstats->qual.qual = 0;
		wstats->qual.level = 0;
		wstats->qual.noise = 0;
		wstats->qual.updated = 7;
		wstats->qual.updated |= IW_QUAL_NOISE_INVALID |
		    IW_QUAL_QUAL_INVALID | IW_QUAL_LEVEL_INVALID;
		return wstats;
	}

	wstats->qual.qual = priv->quality;
	wstats->qual.level = average_value(&priv->average_rssi);
	wstats->qual.noise = average_value(&priv->average_noise);
	wstats->qual.updated =
	    IW_QUAL_QUAL_UPDATED | IW_QUAL_LEVEL_UPDATED |
	    IW_QUAL_NOISE_UPDATED;
	if (!wstats->qual.level)
		wstats->qual.updated |= IW_QUAL_LEVEL_INVALID;
	if (!wstats->qual.qual)
		wstats->qual.updated |= IW_QUAL_QUAL_INVALID;
	if (!wstats->qual.noise)
		wstats->qual.updated |= IW_QUAL_NOISE_INVALID;
	wstats->miss.beacon = average_value(&priv->average_missed_beacons);
	wstats->discard.retries = priv->last_tx_failures;
	wstats->discard.code = priv->ieee->ieee_stats.rx_discards_undecryptable;

	/* TODO -- get the TX RETRY stats
	 *
	 * wstats->discard.retries += tx_retry;
	 */
	return wstats;
}

static struct iw_handler_def ipw_wx_handler_def = {
	.standard = ipw_wx_handlers,
	.num_standard = ARRAY_SIZE(ipw_wx_handlers),
	.num_private = ARRAY_SIZE(ipw_priv_handler),
	.num_private_args = ARRAY_SIZE(ipw_priv_args),
	.private = ipw_priv_handler,
	.private_args = ipw_priv_args,
#if WIRELESS_EXT > 16
	.get_wireless_stats = ipw_get_wireless_stats,
#endif
};

/* net device stuff */

static int ipw_net_open(struct net_device *dev)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	IPW_DEBUG_INFO("dev->open\n");
	/* we should be verifying the device is ready to be opened */
	mutex_lock(&priv->mutex);
	if (!(priv->status & STATUS_RF_KILL_MASK) &&
	    (priv->status & STATUS_ASSOCIATED))
		netif_start_queue(dev);
	mutex_unlock(&priv->mutex);
	return 0;
}

static int ipw_net_stop(struct net_device *dev)
{
	IPW_DEBUG_INFO("dev->close\n");
	netif_stop_queue(dev);
	return 0;
}

/*************** RATE-SCALING fumctions ***********************/

static s8 ipw_lower_rate_g[NUM_RATES] = {
	RATE_SCALE_5_5M_INDEX, RATE_SCALE_5_5M_INDEX,
	RATE_SCALE_11M_INDEX, RATE_SCALE_12M_INDEX,
	RATE_SCALE_18M_INDEX, RATE_SCALE_24M_INDEX,
	RATE_SCALE_36M_INDEX, RATE_SCALE_48M_INDEX,
	255, RATE_SCALE_1M_INDEX, RATE_SCALE_2M_INDEX,
	RATE_SCALE_5_5M_INDEX,

};

static s8 ipw_higher_rate_g[NUM_RATES] = {
	RATE_SCALE_11M_INDEX, RATE_SCALE_11M_INDEX, RATE_SCALE_18M_INDEX,
	RATE_SCALE_24M_INDEX, RATE_SCALE_36M_INDEX, RATE_SCALE_48M_INDEX,
	RATE_SCALE_54M_INDEX, 255, RATE_SCALE_2M_INDEX,
	RATE_SCALE_5_5M_INDEX, RATE_SCALE_11M_INDEX, RATE_SCALE_12M_INDEX
};

static s32 ipw_expected_tpt_g[NUM_RATES] = {
	0, 0, 76, 104, 130, 168, 191, 202, 7, 13, 35, 58
};

static s32 ipw_expected_tpt_g_prot[NUM_RATES] = {
	0, 0, 0, 80, 93, 113, 123, 125, 7, 13, 35, 58
};

static s32 ipw_expected_tpt_a[NUM_RATES] = {
	40, 57, 72, 98, 121, 154, 177, 186, 0, 0, 0, 0
};

static s32 ipw_expected_tpt_b[NUM_RATES] = {
	0, 0, 0, 0, 0, 0, 0, 0, 7, 13, 35, 58
};

static s8 ipw_lower_rate_g_prot[NUM_RATES] = {
	RATE_SCALE_5_5M_INDEX, RATE_SCALE_5_5M_INDEX, RATE_SCALE_11M_INDEX,
	RATE_SCALE_11M_INDEX, RATE_SCALE_18M_INDEX, RATE_SCALE_24M_INDEX,
	RATE_SCALE_36M_INDEX, RATE_SCALE_48M_INDEX, 255,
	RATE_SCALE_1M_INDEX, RATE_SCALE_2M_INDEX, RATE_SCALE_5_5M_INDEX,
};

static s8 ipw_higher_rate_g_prot[NUM_RATES] = {
	RATE_SCALE_11M_INDEX, RATE_SCALE_11M_INDEX, RATE_SCALE_18M_INDEX,
	RATE_SCALE_24M_INDEX, RATE_SCALE_36M_INDEX, RATE_SCALE_48M_INDEX,
	RATE_SCALE_54M_INDEX, 255, RATE_SCALE_2M_INDEX,
	RATE_SCALE_5_5M_INDEX, RATE_SCALE_11M_INDEX, RATE_SCALE_18M_INDEX
};

static struct ipw_tpt_entry ipw_tpt_table_a[] = {
	{-60, 22000, 0, 0, RATE_SCALE_54M_INDEX},
	{-64, 20000, 0, 0, RATE_SCALE_48M_INDEX},
	{-72, 18000, 0, 0, RATE_SCALE_36M_INDEX},
	{-80, 16000, 0, 0, RATE_SCALE_24M_INDEX},
	{-84, 12000, 0, 0, RATE_SCALE_18M_INDEX},
	{-85, 8000, 0, 0, RATE_SCALE_12M_INDEX},
	{-87, 7000, 0, 0, RATE_SCALE_9M_INDEX},
	{-89, 5000, 0, 0, RATE_SCALE_6M_INDEX}
};

static struct ipw_tpt_entry ipw_tpt_table_b[] = {
	{-86, 6000, 0, 0, RATE_SCALE_11M_INDEX},
	{-88, 3000, 0, 0, RATE_SCALE_5_5M_INDEX},
	{-90, 1000, 0, 0, RATE_SCALE_2M_INDEX},
	{-92, 800, 0, 0, RATE_SCALE_1M_INDEX}

};

static struct ipw_tpt_entry ipw_tpt_table_g[] = {
	{-60, 22000, 12000, 14000, RATE_SCALE_54M_INDEX},
	{-64, 20000, 11000, 13000, RATE_SCALE_48M_INDEX},
	{-68, 18000, 10000, 14000, RATE_SCALE_36M_INDEX},
	{-80, 16000, 9000, 11000, RATE_SCALE_24M_INDEX},
	{-84, 12000, 7000, 10000, RATE_SCALE_18M_INDEX},
	{-85, 8000, 5000, 8000, RATE_SCALE_12M_INDEX},
	{-86, 6000, 6000, 6000, RATE_SCALE_11M_INDEX},
	{-88, 3000, 3000, 3000, RATE_SCALE_5_5M_INDEX},
	{-90, 1000, 1000, 1000, RATE_SCALE_2M_INDEX},
	{-92, 800, 800, 800, RATE_SCALE_1M_INDEX}
};

static struct ipw_tpt_entry *ipw_get_tpt_by_rssi(s32 rssi, u8 mode)
{
	u32 index = 0;
	u32 table_size = 0;
	struct ipw_tpt_entry *tpt_table = NULL;

	if ((rssi < IPW_MIN_RSSI_VAL) || (rssi > IPW_MAX_RSSI_VAL))
		rssi = IPW_MIN_RSSI_VAL;

	switch (mode) {
	case IPW_G_MODE:
		tpt_table = ipw_tpt_table_g;
		table_size = ARRAY_SIZE(ipw_tpt_table_g);
		break;

	case IPW_B_MODE:
		tpt_table = ipw_tpt_table_b;
		table_size = ARRAY_SIZE(ipw_tpt_table_b);
		break;

	case IPW_A_MODE:
		tpt_table = ipw_tpt_table_a;
		table_size = ARRAY_SIZE(ipw_tpt_table_a);
		break;

	default:
		BUG();
		return NULL;
	}

	while ((index < table_size) && (rssi < tpt_table[index].min_rssi))
		index++;

	index = min(index, (table_size - 1));

	return &tpt_table[index];
}

static int ipw_update_rate_scaling(struct ipw_priv *priv, u8 mode)
{
	int rc;
	struct RateScalingCmdSpecifics *cmd;
	u8 retry_rate = 2;
	struct rate_scaling_info *table;
	unsigned long flags;

	cmd = &priv->rate_scale_mgr.scale_rate_cmd;
	table = &cmd->rate_scale_table[0];

	spin_lock_irqsave(&priv->rate_scale_mgr.lock, flags);

	if (mode == IPW_A_MODE) {
		IPW_DEBUG_RATE("Select A mode rate scale\n");

		table[RATE_SCALE_6M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_6M_INDEX].next_rate_index =
		    RATE_SCALE_6M_INDEX;
		table[RATE_SCALE_9M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_9M_INDEX].next_rate_index =
		    RATE_SCALE_6M_INDEX;
		table[RATE_SCALE_12M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_12M_INDEX].next_rate_index =
		    RATE_SCALE_9M_INDEX;
		table[RATE_SCALE_18M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_18M_INDEX].next_rate_index =
		    RATE_SCALE_12M_INDEX;
		table[RATE_SCALE_24M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_24M_INDEX].next_rate_index =
		    RATE_SCALE_18M_INDEX;
		table[RATE_SCALE_36M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_36M_INDEX].next_rate_index =
		    RATE_SCALE_24M_INDEX;
		table[RATE_SCALE_48M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_48M_INDEX].next_rate_index =
		    RATE_SCALE_36M_INDEX;
		table[RATE_SCALE_54M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_54M_INDEX].next_rate_index =
		    RATE_SCALE_48M_INDEX;

		/* If one of the following CCK rates is used,
		 * have it fall back to an above OFDM rate */
		table[RATE_SCALE_1M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_1M_INDEX].next_rate_index =
		    RATE_SCALE_6M_INDEX;
		table[RATE_SCALE_2M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_2M_INDEX].next_rate_index =
		    RATE_SCALE_6M_INDEX;
		table[RATE_SCALE_5_5M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_5_5M_INDEX].next_rate_index =
		    RATE_SCALE_6M_INDEX;
		table[RATE_SCALE_11M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_11M_INDEX].next_rate_index =
		    RATE_SCALE_6M_INDEX;
	} else if (mode == IPW_B_MODE) {
		IPW_DEBUG_RATE("Select B mode rate scale\n");

		/* If one of the following OFDM rates is used,
		 * have it fall back to the CCK rates at the end */
		table[RATE_SCALE_6M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_6M_INDEX].next_rate_index =
		    RATE_SCALE_1M_INDEX;
		table[RATE_SCALE_9M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_9M_INDEX].next_rate_index =
		    RATE_SCALE_1M_INDEX;
		table[RATE_SCALE_12M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_12M_INDEX].next_rate_index =
		    RATE_SCALE_1M_INDEX;
		table[RATE_SCALE_18M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_18M_INDEX].next_rate_index =
		    RATE_SCALE_1M_INDEX;
		table[RATE_SCALE_24M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_24M_INDEX].next_rate_index =
		    RATE_SCALE_1M_INDEX;
		table[RATE_SCALE_36M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_36M_INDEX].next_rate_index =
		    RATE_SCALE_1M_INDEX;
		table[RATE_SCALE_48M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_48M_INDEX].next_rate_index =
		    RATE_SCALE_1M_INDEX;
		table[RATE_SCALE_54M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_54M_INDEX].next_rate_index =
		    RATE_SCALE_1M_INDEX;

		/* CCK rates... */
		table[RATE_SCALE_1M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_1M_INDEX].next_rate_index =
		    RATE_SCALE_1M_INDEX;
		table[RATE_SCALE_2M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_2M_INDEX].next_rate_index =
		    RATE_SCALE_1M_INDEX;
		table[RATE_SCALE_5_5M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_5_5M_INDEX].next_rate_index =
		    RATE_SCALE_2M_INDEX;
		table[RATE_SCALE_11M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_11M_INDEX].next_rate_index =
		    RATE_SCALE_5_5M_INDEX;
	} else {
		IPW_DEBUG_RATE("Select G mode rate scale\n");

		table[RATE_SCALE_6M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_6M_INDEX].next_rate_index =
		    RATE_SCALE_2M_INDEX;
		table[RATE_SCALE_9M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_9M_INDEX].next_rate_index =
		    RATE_SCALE_6M_INDEX;
		table[RATE_SCALE_12M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_12M_INDEX].next_rate_index =
		    RATE_SCALE_9M_INDEX;
		table[RATE_SCALE_18M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_18M_INDEX].next_rate_index =
		    RATE_SCALE_12M_INDEX;
		table[RATE_SCALE_24M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_24M_INDEX].next_rate_index =
		    RATE_SCALE_18M_INDEX;
		table[RATE_SCALE_36M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_36M_INDEX].next_rate_index =
		    RATE_SCALE_24M_INDEX;
		table[RATE_SCALE_48M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_48M_INDEX].next_rate_index =
		    RATE_SCALE_36M_INDEX;
		table[RATE_SCALE_54M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_54M_INDEX].next_rate_index =
		    RATE_SCALE_48M_INDEX;
		table[RATE_SCALE_1M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_1M_INDEX].next_rate_index =
		    RATE_SCALE_1M_INDEX;
		table[RATE_SCALE_2M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_2M_INDEX].next_rate_index =
		    RATE_SCALE_1M_INDEX;
		table[RATE_SCALE_5_5M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_5_5M_INDEX].next_rate_index =
		    RATE_SCALE_2M_INDEX;
		table[RATE_SCALE_11M_INDEX].try_cnt = retry_rate;
		table[RATE_SCALE_11M_INDEX].next_rate_index =
		    RATE_SCALE_5_5M_INDEX;
	}

	/* Update the rate scaling for control frame Tx */
	cmd->table_id = 0;
	spin_unlock_irqrestore(&priv->rate_scale_mgr.lock, flags);

	rc = ipw_send_rates_scaling_cmd(priv, cmd);
	if (rc)
		return rc;

	/* Update the rate scaling for data frame Tx */
	cmd->table_id = 1;
	rc = ipw_send_rates_scaling_cmd(priv, cmd);

	return rc;
}

static int ipw_rate_scale_clear_window(struct ipw_rate_scale_data *window)
{

	window->data = 0;
	window->success_counter = 0;
	window->success_ratio = IPW_INVALID_VALUE;
	window->counter = 0;
	window->average_tpt = IPW_INVALID_VALUE;
	window->stamp = 0;
	return 0;
}

static int ipw_rate_scale_flush_old(struct ipw_priv *priv, u32 flush_interval)
{
	int rc = 0;
	s32 rate_index = 0;
	unsigned long flags;
	struct ipw_rate_scale_data *window;

	for (rate_index = 0; rate_index < NUM_RATES; rate_index++) {
		window = &priv->rate_scale_mgr.window[rate_index];

		spin_lock_irqsave(&priv->rate_scale_mgr.lock, flags);
		if (window->counter > 0) {
			if (time_after(jiffies, window->stamp + flush_interval)) {
				IPW_DEBUG_RATE
				    ("flushed samples of rate index %d\n",
				     rate_index);
				ipw_rate_scale_clear_window(window);
			}
		}
		spin_unlock_irqrestore(&priv->rate_scale_mgr.lock, flags);
	}

	return rc;
}

static int ipw_rate_scale_init_handle(struct ipw_priv *priv, s32 window_size)
{
	int rc = 0;
	int i;
	unsigned long flags;
	struct RateScalingCmdSpecifics *cmd;
	u8 retry_rate = 5;
	struct rate_scaling_info *table;

	cmd = &priv->rate_scale_mgr.scale_rate_cmd;
	memset(cmd, 0, sizeof(cmd));
	table = &cmd->rate_scale_table[0];

	IPW_DEBUG_RATE("initialize rate scale window to %d\n", window_size);

	if ((window_size > IPW_RATE_SCALE_MAX_WINDOW) || (window_size < 0))
		window_size = IPW_RATE_SCALE_MAX_WINDOW;

	spin_lock_irqsave(&priv->rate_scale_mgr.lock, flags);

	priv->rate_scale_mgr.expected_tpt = NULL;
	priv->rate_scale_mgr.next_higher_rate = NULL;
	priv->rate_scale_mgr.next_lower_rate = NULL;

	priv->rate_scale_mgr.stamp = jiffies;

	priv->rate_scale_mgr.max_window_size = window_size;

	for (i = 0; i < NUM_RATES; i++)
		ipw_rate_scale_clear_window(&priv->rate_scale_mgr.window[i]);

	table[RATE_SCALE_6M_INDEX].tx_rate = R_6M;
	table[RATE_SCALE_6M_INDEX].try_cnt = retry_rate;
	table[RATE_SCALE_6M_INDEX].next_rate_index = RATE_SCALE_11M_INDEX;

	table[RATE_SCALE_9M_INDEX].tx_rate = R_9M;
	table[RATE_SCALE_9M_INDEX].try_cnt = retry_rate;
	table[RATE_SCALE_9M_INDEX].next_rate_index = RATE_SCALE_6M_INDEX;

	table[RATE_SCALE_12M_INDEX].tx_rate = R_12M;
	table[RATE_SCALE_12M_INDEX].try_cnt = retry_rate;
	table[RATE_SCALE_12M_INDEX].next_rate_index = RATE_SCALE_9M_INDEX;

	table[RATE_SCALE_18M_INDEX].tx_rate = R_18M;
	table[RATE_SCALE_18M_INDEX].try_cnt = retry_rate;
	table[RATE_SCALE_18M_INDEX].next_rate_index = RATE_SCALE_12M_INDEX;

	table[RATE_SCALE_24M_INDEX].tx_rate = R_24M;
	table[RATE_SCALE_24M_INDEX].try_cnt = retry_rate;
	table[RATE_SCALE_24M_INDEX].next_rate_index = RATE_SCALE_18M_INDEX;

	table[RATE_SCALE_36M_INDEX].tx_rate = R_36M;
	table[RATE_SCALE_36M_INDEX].try_cnt = retry_rate;
	table[RATE_SCALE_36M_INDEX].next_rate_index = RATE_SCALE_24M_INDEX;

	table[RATE_SCALE_48M_INDEX].tx_rate = R_48M;
	table[RATE_SCALE_48M_INDEX].try_cnt = retry_rate;
	table[RATE_SCALE_48M_INDEX].next_rate_index = RATE_SCALE_36M_INDEX;

	table[RATE_SCALE_54M_INDEX].tx_rate = R_54M;
	table[RATE_SCALE_54M_INDEX].try_cnt = retry_rate;
	table[RATE_SCALE_54M_INDEX].next_rate_index = RATE_SCALE_48M_INDEX;

	table[RATE_SCALE_1M_INDEX].tx_rate = R_1M;
	table[RATE_SCALE_1M_INDEX].try_cnt = retry_rate;
	table[RATE_SCALE_1M_INDEX].next_rate_index = RATE_SCALE_1M_INDEX;

	table[RATE_SCALE_2M_INDEX].tx_rate = R_2M;
	table[RATE_SCALE_2M_INDEX].try_cnt = retry_rate;
	table[RATE_SCALE_2M_INDEX].next_rate_index = RATE_SCALE_1M_INDEX;

	table[RATE_SCALE_5_5M_INDEX].tx_rate = R_5_5M;
	table[RATE_SCALE_5_5M_INDEX].try_cnt = retry_rate;
	table[RATE_SCALE_5_5M_INDEX].next_rate_index = RATE_SCALE_2M_INDEX;

	table[RATE_SCALE_11M_INDEX].tx_rate = R_11M;
	table[RATE_SCALE_11M_INDEX].try_cnt = retry_rate;
	table[RATE_SCALE_11M_INDEX].next_rate_index = RATE_SCALE_5_5M_INDEX;

	spin_unlock_irqrestore(&priv->rate_scale_mgr.lock, flags);

	return rc;
}

static int ipw_collect_tx_data(struct ipw_priv *priv, int scale_index,
			       u32 status)
{
	int rc = 0;
	struct ipw_rate_scale_data *window = NULL;
	unsigned long flags;
	u64 old_data;

	if (scale_index < 0)
		return -1;

	if ((priv->rate_scale_mgr.max_window_size < 0) ||
	    (priv->rate_scale_mgr.max_window_size > IPW_RATE_SCALE_MAX_WINDOW))
		return -1;

	if (scale_index >= NUM_RATES)
		return -1;

	spin_lock_irqsave(&priv->rate_scale_mgr.lock, flags);

	old_data = window->data;
	window = &priv->rate_scale_mgr.window[scale_index];
	if (window->counter >= priv->rate_scale_mgr.max_window_size) {
		u64 mask;

		window->counter = priv->rate_scale_mgr.max_window_size;
		window->counter--;
		mask = (1 << (priv->rate_scale_mgr.max_window_size - 1));
		if ((window->data & mask) != 0) {
			window->data &= ~mask;
			window->success_counter--;
		}
	}

	window->counter++;
	window->data = window->data << 1;
	if (status != 0) {
		window->success_counter++;
		window->data |= 0x1;
	}

	if (window->counter > 0) {
		window->success_ratio = 128 * (100 * window->success_counter)
		    / window->counter;
	} else
		window->success_ratio = IPW_INVALID_VALUE;

	window->stamp = jiffies;

	spin_unlock_irqrestore(&priv->rate_scale_mgr.lock, flags);

	return rc;
}

static int ipw_rate_scale_rxon_handle(struct ipw_priv *priv, s32 sta_id)
{
	int rc = 0;
	int i;
	s32 rssi;
	u8 mode;
	struct ipw_tpt_entry *entry = NULL;
	unsigned long flags;

	if ((priv->rxon.filter_flags & RXON_FILTER_ASSOC_MSK) == 0)
		return rc;

	spin_lock_irqsave(&priv->rate_scale_mgr.lock, flags);

	priv->rate_scale_mgr.next_lower_rate = ipw_lower_rate_g;
	priv->rate_scale_mgr.next_higher_rate = ipw_higher_rate_g;

	if (priv->rxon.flags & RXON_FLG_BAND_24G_MSK) {

		if (priv->assoc_request.ieee_mode == IPW_G_MODE) {
			mode = IPW_G_MODE;
			if (priv->rxon.flags & RXON_FLG_TGG_PROTECT_MSK) {
				priv->rate_scale_mgr.expected_tpt =
				    ipw_expected_tpt_g_prot;
				priv->rate_scale_mgr.next_higher_rate =
				    ipw_higher_rate_g_prot;
				priv->rate_scale_mgr.next_lower_rate =
				    ipw_lower_rate_g_prot;
			} else {
				priv->rate_scale_mgr.expected_tpt =
				    ipw_expected_tpt_g;
				priv->rate_scale_mgr.next_lower_rate =
				    ipw_lower_rate_g;
				priv->rate_scale_mgr.next_higher_rate =
				    ipw_higher_rate_g;
			}
		} else {
			mode = IPW_B_MODE;
			priv->rate_scale_mgr.expected_tpt = ipw_expected_tpt_b;
		}
	} else {
		mode = IPW_A_MODE;
		priv->rate_scale_mgr.expected_tpt = ipw_expected_tpt_a;
	}

	for (i = 0; i < NUM_RATES; i++)
		ipw_rate_scale_clear_window(&priv->rate_scale_mgr.window[i]);

	rssi = priv->assoc_network->stats.rssi;

	if (rssi == 0)
		rssi = IPW_MIN_RSSI_VAL;

	IPW_DEBUG_INFO("Network RSSI: %d\n", rssi);

	entry = ipw_get_tpt_by_rssi(rssi, mode);
	if (entry != NULL) {
		i = entry->rate_scale_index;
		//todoG need to support ibss
	} else {
		if (priv->assoc_request.ieee_mode == IPW_A_MODE)
			i = RATE_SCALE_6M_INDEX;
		else
			i = RATE_SCALE_1M_INDEX;
	}

	priv->stations[sta_id].current_rate = ipw_rate_scaling2rate_plcp(i);

	ipw_sync_station(priv, sta_id, priv->stations[sta_id].current_rate,
			 CMD_ASYNC | CMD_NO_LOCK);

	IPW_DEBUG_RATE("for rssi %d assign rate scale index %d plcp %d\n",
		       rssi, i, priv->stations[sta_id].sta.tx_rate);

	spin_unlock_irqrestore(&priv->rate_scale_mgr.lock, flags);

	return rc;
}

static int ipw_rate_scale_tx_resp_handle(struct ipw_priv *priv,
					 struct ipw_tx_resp *tx_resp)
{
	int rc = 0, i;
	u8 retries, current_count;
	int scale_rate_index, first_index, last_index;
	struct rate_scaling_info *table;

	retries = tx_resp->failure_frame;
	first_index = ipw_rate_plcp2rate_scaling(tx_resp->rate);

	if ((first_index < 0) || (first_index >= NUM_RATES)) {
		return -1;
	}

	if (!(priv->status & STATUS_ASSOCIATED)) {
		return -1;
	}

	if (time_after(jiffies,
		       priv->rate_scale_mgr.stamp + IPW_RATE_SCALE_FLUSH)) {
		ipw_rate_scale_flush_old(priv, IPW_RATE_SCALE_FLUSH);
		priv->rate_scale_mgr.stamp = jiffies;
	}

	scale_rate_index = first_index;
	last_index = first_index;

	table = &priv->rate_scale_mgr.scale_rate_cmd.rate_scale_table[0];
	if (table[scale_rate_index].try_cnt == 0) {
		scale_rate_index = table[scale_rate_index].next_rate_index;
		last_index = scale_rate_index;
	}

	while (retries > 0) {
		if (table[scale_rate_index].try_cnt > retries) {
			current_count = retries;
			last_index = scale_rate_index;
		} else {
			current_count = table[scale_rate_index].try_cnt;
			last_index = table[scale_rate_index].next_rate_index;
		}

		for (i = 0; i < current_count; i++)
			rc = ipw_collect_tx_data(priv, scale_rate_index, 0);

		retries -= current_count;

		scale_rate_index = table[scale_rate_index].next_rate_index;
	}

	if ((tx_resp->status & 0xFF) == 0x1)
		rc = ipw_collect_tx_data(priv, last_index, 1);
	else
		rc = ipw_collect_tx_data(priv, last_index, 0);

	return rc;
}

/*
todo:

modify to send one tfd per fragment instead of using chunking.  otherwise
we need to heavily modify the ieee80211_skb_to_txb.
*/
//todoG need to support frag

#define IPW_TX_MEASURE_LEN 100
#define M80211_IS_PRB_RESP(pHdr802_11) \
            ((IEEE80211_FCTL_FTYPE & pHdr802_11->frame_ctl) == IEEE80211_FTYPE_MGMT && \
             (IEEE80211_FCTL_STYPE & pHdr802_11->frame_ctl) == IEEE80211_STYPE_PROBE_RESP )

/*
  handle build REPLY_TX command notification. it is responsible for CALIBRATION fields.
*/
static void ipw_build_tx_cmd_calib(struct ipw_priv *priv,
				   struct ipw_tx_cmd *tx_cmd, u16 sequence)
{
	int found_frame = 0;

	/* If already have a pending Tx measurement or if it has been
	 * less than 3s since the last measurement, then don't try and
	 * start a new measurement. */
	if ((priv->status & STATUS_TX_MEASURE) ||
	    time_after(priv->daemon_last_status + (HZ * 3UL), jiffies))
		return;

	/* We only want to collect the status for a frame that will actually
	 * have the radio powered up for a long enough time to get a good
	 * value */

	switch (tx_cmd->rate) {
	case R_6M:
		found_frame = (tx_cmd->len > (IPW_TX_MEASURE_LEN * 6 / 8));
		break;
	case R_9M:
		found_frame = (tx_cmd->len > (IPW_TX_MEASURE_LEN * 9 / 8));
		break;
	case R_12M:
		found_frame = (tx_cmd->len > (IPW_TX_MEASURE_LEN * 12 / 8));
		break;
	case R_18M:
		found_frame = (tx_cmd->len > (IPW_TX_MEASURE_LEN * 18 / 8));
		break;
	case R_24M:
		found_frame = (tx_cmd->len > (IPW_TX_MEASURE_LEN * 24 / 8));
		break;
	case R_36M:
		found_frame = (tx_cmd->len > (IPW_TX_MEASURE_LEN * 36 / 8));
		break;
	case R_48M:
		found_frame = (tx_cmd->len > (IPW_TX_MEASURE_LEN * 48 / 8));
		break;
	case R_54M:
		found_frame = (tx_cmd->len > (IPW_TX_MEASURE_LEN * 54 / 8));
		break;
	default:
		break;
	}

	if (found_frame) {
		priv->status |= STATUS_TX_MEASURE;
		tx_cmd->tx_flags |= (TX_CMD_FLG_CALIB_MSK |
				     TX_CMD_FLG_BT_DIS_MSK);
		priv->daemon_tx_sequence = sequence;
		priv->daemon_tx_rate = tx_cmd->rate;
	}
}

/*
  TX command functions
*/
/*
 * get the available rate. if management frame or broadcast frame only return
 * basic available rates.
 */
static void ipw_get_supported_rates(struct ipw_priv *priv,
				    struct ieee80211_hdr_4addr *hdr,
				    u16 * data_rate, u16 * ctrl_rate)
{
	*data_rate = priv->active_rate;
	*ctrl_rate = priv->active_rate;
	if ((is_multicast_ether_addr(hdr->addr1)
	     || ipw_is_broadcast_ether_addr(hdr->addr1))
	    && (priv->active_rate_basic)) {
		*data_rate = priv->active_rate_basic;
		*ctrl_rate = priv->active_rate_basic;
	} else if (priv->config & CFG_FIXED_RATE) {
		u16 rate_mask = (priv->rates_mask & priv->active_rate);
		if (rate_mask == 0)
			rate_mask = priv->active_rate_basic;
		*data_rate = rate_mask;
		*ctrl_rate = rate_mask;
	}
}

static int ipw_get_adjuscent_rate(struct ipw_priv *priv, s32 scale_index,
				  u16 rate_mask, s32 * low, s32 * high)
{
	int rc = 0;
	s32 index, find_index;
	u16 msk;
	u8 is_g_mode = 0;

	if (priv->assoc_request.ieee_mode == IPW_G_MODE)
		is_g_mode = 1;

	index = ipw_rate_scale2index(scale_index);

	*low = IPW_INVALID_RATE;
	*high = IPW_INVALID_RATE;

	if (is_g_mode == 0) {
		find_index = index - 1;
		while ((find_index >= 0) && (find_index < NUM_RATES)) {
			msk = (1 << find_index);
			if (msk & rate_mask) {
				*low = ipw_rate_index2rate_scale(find_index);
				break;
			}
			find_index--;
		}

		find_index = index + 1;
		while ((find_index >= 0) && (find_index < NUM_RATES)) {
			msk = (1 << find_index);
			if (msk & rate_mask) {
				*high = ipw_rate_index2rate_scale(find_index);
				break;
			}
			find_index++;
		}
		return rc;
	}

	if (priv->rate_scale_mgr.next_lower_rate != NULL) {
		*low = scale_index;

		find_index = ipw_rate_scale2index(scale_index);

		while (find_index != IPW_INVALID_VALUE) {
			*low = priv->rate_scale_mgr.next_lower_rate[*low];

			if (*low == IPW_INVALID_RATE)
				break;

			find_index = ipw_rate_scale2index(*low);

			msk = (1 << find_index);
			if (msk & rate_mask) {
				break;
			}
		}
	}

	if (priv->rate_scale_mgr.next_higher_rate != NULL) {
		*high = scale_index;

		find_index = ipw_rate_scale2index(scale_index);

		while (find_index != IPW_INVALID_VALUE) {
			*high = priv->rate_scale_mgr.next_higher_rate[*high];

			if (*high == IPW_INVALID_RATE)
				break;
			find_index = ipw_rate_scale2index(*high);

			msk = (1 << find_index);
			if (msk & rate_mask) {
				break;
			}
		}
	}

	return rc;
}

/*
  find the highest availabed rate for REPLY_TX command.
*/
static int ipw_get_tx_rate(struct ipw_priv *priv, u16 rate_mask, u8 rate,
			   int sta_id)
{
	int low = IPW_INVALID_RATE;
	int high = IPW_INVALID_RATE;
	int index, rc = 0;
	int i = ipw_rate_plcp2index(rate);
	struct ipw_rate_scale_data *window = NULL;
	int current_tpt = IPW_INVALID_VALUE;
	int low_tpt = IPW_INVALID_VALUE;
	int high_tpt = IPW_INVALID_VALUE;
	u32 fail_count;
	s8 scale_action;
	unsigned long flags;

	if ((i >= 0) && ((1 << i) & rate_mask))
		goto done;

	/* get the lowest availabe rate */
	rate = IPW_INVALID_RATE;
	for (i = 0; i < 12; i++) {
		if ((1 << i) & rate_mask) {
			rate = ipw_rate_index2plcp(i);
			if (rate != IPW_INVALID_RATE)
				break;
		}
	}
      done:
	if (rate == IPW_INVALID_RATE)
		return rate;

	//todoG for now we do auto scale only to bss and unicast frame
	if ((sta_id == BROADCAST_ID) || (sta_id == MULTICAST_ID)) {
		return rate;
	}

	if (!(priv->status & STATUS_ASSOCIATED)) {
		return rate;
	}

	spin_lock_irqsave(&priv->rate_scale_mgr.lock, flags);

	index = ipw_rate_plcp2rate_scaling(rate);

	window = &priv->rate_scale_mgr.window[index];

	fail_count = window->counter - window->success_counter;

	if (((fail_count <= IPW_RATE_SCALE_MIN_FAILURE_TH) &&
	     (window->success_counter < IPW_RATE_SCALE_MIN_SUCCESS_TH)) ||
	    (priv->rate_scale_mgr.expected_tpt == NULL)) {

		spin_unlock_irqrestore(&priv->rate_scale_mgr.lock, flags);
		window->average_tpt = IPW_INVALID_VALUE;
		return rate;

	} else {
		window->average_tpt = ((window->success_ratio *
					priv->rate_scale_mgr.
					expected_tpt[index] + 64) / 128);
	}

	rc = ipw_get_adjuscent_rate(priv, index, rate_mask, &low, &high);

	if (rc) {
		spin_unlock_irqrestore(&priv->rate_scale_mgr.lock, flags);
		return rate;

	}

	current_tpt = window->average_tpt;

	if (low != IPW_INVALID_RATE) {
		low_tpt = priv->rate_scale_mgr.window[low].average_tpt;
	}

	if (high != IPW_INVALID_RATE)
		high_tpt = priv->rate_scale_mgr.window[high].average_tpt;

	spin_unlock_irqrestore(&priv->rate_scale_mgr.lock, flags);

	scale_action = 1;

	if ((window->success_ratio < IPW_RATE_SCALE_DECREASE_TH) ||
	    (current_tpt == 0)) {
		IPW_DEBUG_RATE("decrease rate because of low success_ratio\n");
		scale_action = -1;
	} else if ((low_tpt == IPW_INVALID_VALUE) &&
		   (high_tpt == IPW_INVALID_VALUE)) {
		scale_action = 1;
	} else if ((low_tpt != IPW_INVALID_VALUE) &&
		   (high_tpt != IPW_INVALID_VALUE) &&
		   (low_tpt < current_tpt) && (high_tpt < current_tpt)) {

		scale_action = 0;
	} else {
		if (high_tpt != IPW_INVALID_VALUE) {
			if (high_tpt > current_tpt)
				scale_action = 1;
			else {
				IPW_DEBUG_RATE
				    ("decrease rate because of high tpt\n");
				scale_action = -1;
			}
		} else if (low_tpt != IPW_INVALID_VALUE) {
			if (low_tpt > current_tpt) {
				IPW_DEBUG_RATE
				    ("decrease rate because of low tpt\n");
				scale_action = -1;
			} else
				scale_action = 1;
		}
	}

	if ((window->success_ratio > IPW_RATE_SCALE_HIGH_TH) ||
	    (current_tpt > (128 * window->average_tpt))) {
		scale_action = 0;
	}

	switch (scale_action) {
	case -1:
		if (low != IPW_INVALID_RATE)
			index = low;
		break;

	case 1:
		if (high != IPW_INVALID_RATE)
			index = high;

		break;

	case 0:
	default:
		break;
	}

	IPW_DEBUG_RATE("choose rate scale index %d action %d low %d high %d\n",
		       index, scale_action, low, high);
	rate = ipw_rate_scaling2rate_plcp(index);
	/*  TODO: adjust tx rate according to card values and traffic */
	return rate;
}

/*
  handle build REPLY_TX command notification. it is responsible for rates fields.
*/
static int ipw_build_tx_cmd_rate(struct ipw_priv *priv,
				 struct ipw_tx_cmd *tx_cmd,
				 struct ieee80211_hdr_4addr *hdr,
				 int sta_id, int tx_id)
{
	u16 rate_mask, ctrl_rate;

	ipw_get_supported_rates(priv, hdr, &rate_mask, &ctrl_rate);
	tx_cmd->rate = ipw_get_tx_rate(priv, rate_mask,
				       priv->stations[sta_id].current_rate,
				       sta_id);
	if (tx_cmd->rate == IPW_INVALID_RATE) {
		IPW_ERROR("ERROR: No TX rate available.\n");
		return -1;
	}

	IPW_DEBUG_RATE("Tx sta %d ew plcp rate %d old %d rate mask %x\n",
		       sta_id, tx_cmd->rate,
		       priv->stations[sta_id].current_rate, rate_mask);

	priv->stations[sta_id].current_rate = tx_cmd->rate;

	if ((priv->ieee->iw_mode == IW_MODE_ADHOC) &&
	    (sta_id != BROADCAST_ID) && (sta_id != MULTICAST_ID))
		priv->stations[STA_ID].current_rate = tx_cmd->rate;

	if (tx_id >= CMD_QUEUE_NUM)
		tx_cmd->rts_retry_limit = 3;
	else
		tx_cmd->rts_retry_limit = 7;

	if (M80211_IS_PRB_RESP(hdr)) {
		tx_cmd->data_retry_limit = 3;
		if (tx_cmd->data_retry_limit < tx_cmd->rts_retry_limit)
			tx_cmd->rts_retry_limit = tx_cmd->data_retry_limit;
	} else
		tx_cmd->data_retry_limit = IPW_DEFAULT_TX_RETRY;

	if (priv->data_retry_limit != -1)
		tx_cmd->data_retry_limit = priv->data_retry_limit;

	if ((hdr->frame_ctl & IEEE80211_FCTL_FTYPE) == IEEE80211_FTYPE_MGMT) {
		switch (hdr->frame_ctl & IEEE80211_FCTL_STYPE) {
		case IEEE80211_STYPE_AUTH:
		case IEEE80211_STYPE_DEAUTH:
		case IEEE80211_STYPE_ASSOC_REQ:
		case IEEE80211_STYPE_REASSOC_REQ:
			if (tx_cmd->tx_flags & TX_CMD_FLG_RTS_MSK) {
				tx_cmd->tx_flags &= ~TX_CMD_FLG_RTS_MSK;
				tx_cmd->tx_flags |= TX_CMD_FLG_CTS_MSK;
			}
			break;
		default:
			break;
		}
	}

	/* OFDM */
	tx_cmd->supp_rates[0] = rate_mask >> 4;

	/* CCK */
	tx_cmd->supp_rates[1] = rate_mask & 0xF;

	tx_cmd->tx_flags &= ~TX_CMD_FLG_ANT_A_MSK;
	tx_cmd->tx_flags &= ~TX_CMD_FLG_ANT_B_MSK;
	return 0;
}

/*
  handle build REPLY_TX command notification.
*/
static int ipw_build_tx_cmd_basic(struct ipw_priv *priv,
				  struct ipw_tx_cmd *tx_cmd,
				  struct ieee80211_hdr_4addr *hdr,
				  int is_unicast, u8 std_id, int tx_id)
{
	ulong nominal_length;

	tx_cmd->u.life_time = 0xFFFFFFFF;
	if (is_unicast == 1) {
		tx_cmd->tx_flags |= TX_CMD_FLG_ACK_MSK;
		if ((hdr->frame_ctl & IEEE80211_FCTL_FTYPE) ==
		    IEEE80211_FTYPE_MGMT) {
			tx_cmd->tx_flags |= TX_CMD_FLG_SEQ_CTL_MSK;
		}
		if (M80211_IS_PRB_RESP(hdr)) {
			if ((hdr->seq_ctl & 0x000F) == 0) {
				tx_cmd->tx_flags |= TX_CMD_FLG_TSF_MSK;
			}
		}
	} else {
		tx_cmd->tx_flags &= (~TX_CMD_FLG_ACK_MSK);
		tx_cmd->tx_flags |= TX_CMD_FLG_SEQ_CTL_MSK;
	}

	if (std_id == IPW_INVALID_STATION)
		return -1;

	tx_cmd->sta_id = std_id;
	if (hdr->frame_ctl & IEEE80211_FCTL_MOREFRAGS)
		tx_cmd->tx_flags |= TX_CMD_FLG_MORE_FRAG_MSK;
	if (ieee80211_get_hdrlen(hdr->frame_ctl) == 26) {
		struct ieee80211_hdr_3addrqos *qos_hdr =
		    (struct ieee80211_hdr_3addrqos *)hdr;
		tx_cmd->tid_tspec = (u8) (qos_hdr->qos_ctl & 0x000F);
	} else
		tx_cmd->tx_flags |= TX_CMD_FLG_SEQ_CTL_MSK;
	tx_cmd->next_frame_len = 0;
	nominal_length = tx_cmd->len;
	nominal_length += IEEE80211_FCS_LEN;
	//todoG need to count security over head;
	/*
	   nominal_length += usMpduSecOverhead + IEEE80211_FCS_LEN;
	   if( (hdr->frame_ctl & IEEE80211_FCTL_MOREFRAGS) == 0)
	   nominal_length += usMsduSecOverhead;
	 */
	if (hdr->frame_ctl & IEEE80211_FCTL_PROTECTED)
		nominal_length += 4;
	if (priv->rts_threshold <= nominal_length) {
		tx_cmd->tx_flags |= TX_CMD_FLG_RTS_MSK;
		tx_cmd->tx_flags &= ~TX_CMD_FLG_CTS_MSK;
	}

	if ((tx_cmd->tx_flags & TX_CMD_FLG_RTS_MSK) ||
	    (tx_cmd->tx_flags & TX_CMD_FLG_CTS_MSK))
		tx_cmd->tx_flags |= TX_CMD_FLG_FULL_TXOP_PROT_MSK;
	tx_cmd->tx_flags &= ~(TX_CMD_FLG_ANT_SEL_MSK);
	if ((hdr->frame_ctl & IEEE80211_FCTL_FTYPE) == IEEE80211_FTYPE_MGMT) {
		if (((hdr->frame_ctl & IEEE80211_FCTL_STYPE) ==
		     IEEE80211_STYPE_ASSOC_REQ) ||
		    ((hdr->frame_ctl & IEEE80211_FCTL_STYPE) ==
		     IEEE80211_STYPE_REASSOC_REQ)) {
			tx_cmd->u2.pm_frame_timeout = 3;
		} else {
			tx_cmd->u2.pm_frame_timeout = 2;
		}
	} else
		tx_cmd->u2.pm_frame_timeout = 0;
	tx_cmd->driver_txop = 0;

	return 0;
}

//todoG need to move this to ieee code
#define IEEE80211_STYPE_BA_REQ  0x0080

#define IS_PLCP_RATE_CCK(rate) (((rate) & 0xf0) || (!((rate) & 0x1)))

#define IS_PLCP_RATE_OFDM(rate) (((rate) & 0x1) && (!((rate) & 0xf0)))

static inline void ipw_build_tx_cmd_hwcrypto(struct ipw_priv *priv,
					     struct ipw_tx_cmd *tx_cmd,
					     struct sk_buff *skb_frag,
					     int hdr_len, int last_frag)
{
	struct ieee80211_crypt_data *crypt = priv->ieee->crypt[0];

	atomic_inc(&crypt->refcnt);
	if (crypt->ops->build_iv)
		crypt->ops->build_iv(skb_frag, hdr_len,
				     tx_cmd->key, 16, crypt->priv);
	atomic_dec(&crypt->refcnt);

	switch (priv->ieee->sec.level) {
	case SEC_LEVEL_3:
		tx_cmd->sec_ctl = TX_CMD_SEC_CCM;

		tx_cmd->hdr->frame_ctl |= IEEE80211_FCTL_PROTECTED;
		/* XXX: ACK flag must be set for CCMP even if it
		 * is a multicast/broadcast packet, because CCMP
		 * group communication encrypted by GTK is
		 * actually done by the AP. */
		tx_cmd->tx_flags |= TX_CMD_FLG_ACK_MSK;
		break;
	case SEC_LEVEL_2:
		tx_cmd->sec_ctl = TX_CMD_SEC_TKIP;

		if (last_frag)
			memcpy(tx_cmd->tkip_mic.byte, skb_frag->tail - 8, 8);
		else
			memset(tx_cmd->tkip_mic.byte, 0, 8);

		tx_cmd->hdr->frame_ctl |= IEEE80211_FCTL_PROTECTED;
		/* XXX: ACK flag must be set for CCMP even if it
		 * is a multicast/broadcast packet, because CCMP
		 * group communication encrypted by GTK is
		 * actually done by the AP. */
		tx_cmd->tx_flags |= TX_CMD_FLG_ACK_MSK;
		break;
	case SEC_LEVEL_1:
		tx_cmd->sec_ctl = 1 |	/* WEP */
		    (priv->ieee->tx_keyidx & 0x3) << 6;

		if (priv->ieee->sec.key_sizes[priv->ieee->tx_keyidx] == 13)
			tx_cmd->sec_ctl |= (1 << 3);	/* 128-bit */

		memcpy(&tx_cmd->key[3],
		       priv->ieee->sec.keys[priv->ieee->tx_keyidx],
		       priv->ieee->sec.key_sizes[priv->ieee->tx_keyidx]);

		tx_cmd->hdr->frame_ctl |= IEEE80211_FCTL_PROTECTED;

		IPW_DEBUG_TX("Configuring packet for WEP encryption "
			     "with key %d\n", priv->ieee->tx_keyidx);
		break;

	case SEC_LEVEL_0:
		IPW_DEBUG_TX("Tx packet in the clear "
			     "(encrypt requested).\n");
		break;

	default:
		printk(KERN_ERR "Unknow security level %d\n",
		       priv->ieee->sec.level);
		break;
	}
}

/*
  start REPLY_TX command process
*/
static inline int ipw_tx_skb(struct ipw_priv *priv,
			     struct ieee80211_txb *txb, int pri)
{
	struct ieee80211_hdr_4addr *hdr = (struct ieee80211_hdr_4addr *)
	    txb->fragments[0]->data;
	int i = 0;
	struct tfd_frame *tfd;
	int tx_id = ipw_get_tx_queue_number(priv, pri);
	struct ipw_tx_queue *txq = &priv->txq[tx_id];
	struct ipw_queue *q = &txq->q;
	dma_addr_t phys_addr;
	struct ipw_cmd *out_cmd = NULL;
	u16 len, idx;
	u8 id, hdr_len, unicast;
//      u16 remaining_bytes;
	u8 sta_id;
	u16 seq_number;
	int rc;

	if (priv->status & STATUS_RF_KILL_MASK)
		goto drop;

	switch (priv->ieee->iw_mode) {
	case IW_MODE_ADHOC:
		hdr_len = IEEE80211_3ADDR_LEN;
		unicast = !ipw_is_broadcast_ether_addr(hdr->addr1) &&
		    !is_multicast_ether_addr(hdr->addr1);
		id = 0;
		/*id = ipw_find_station(priv, hdr->addr1);
		   if (id == IPW_INVALID_STATION) {
		   //todoG we dont need to add station from here. we use beacon
		   IPW_DEBUG_ASSOC("need Adding STA ID: " MAC_FMT "\n", MAC_ARG(hdr->addr1));
		   id = ipw_add_station(priv, hdr->addr1, 0,
		   (CMD_ASYNC|CMD_NO_LOCK));

		   if (id == IPW_INVALID_STATION) {
		   IPW_WARNING("Attempt to send data to "
		   "invalid cell: " MAC_FMT "\n",
		   MAC_ARG(hdr->addr1));
		   goto drop;
		   }
		   goto drop;
		   } */
		break;
	case IW_MODE_INFRA:
	default:
		unicast = !ipw_is_broadcast_ether_addr(hdr->addr1) &&
		    !is_multicast_ether_addr(hdr->addr1);
		hdr_len = IEEE80211_3ADDR_LEN;
		id = 0;
		break;
	}

//todoG call filter packets
//      rc = ipw_filter_packet_notify(priv, hdr);
//      if (rc)
//              goto drop;

//todoG make sure you get the header right
//      hdr_len = ieee80211_get_hdrlen(le16_to_cpu(hdr->frame_ctl));
//todoG make sure we capture all tx command parameters

	if (unicast == 1) {
		if (M80211_IS_PRB_RESP(hdr))
			sta_id = ipw_find_station(priv, BROADCAST_ADDR);
		else
			sta_id = ipw_find_station(priv, hdr->addr1);
	} else
		sta_id = ipw_find_station(priv, BROADCAST_ADDR);

	if (sta_id == IPW_INVALID_STATION) {
		IPW_DEBUG_DROP("Station " MAC_FMT " not in station map. "
			       "Defaulting to broadcast...\n",
			       MAC_ARG(hdr->addr1));
		printk_buf(IPW_DL_DROP, (u8 *) hdr, sizeof(*hdr));
		sta_id = ipw_find_station(priv, BROADCAST_ADDR);
		if (sta_id == IPW_INVALID_STATION)
			goto drop;
	}

	seq_number = priv->stations[sta_id].tid[tx_id].seq_number;
	priv->stations[sta_id].tid[tx_id].seq_number++;

	for (i = 0; i < txb->nr_frags; i++) {
		hdr = (struct ieee80211_hdr_4addr *)
		    txb->fragments[i]->data;

		tfd = &txq->bd[q->first_empty];
		memset(tfd, 0, sizeof(*tfd));
		idx = get_next_cmd_index(q, q->first_empty, 0);

		txq->txb[q->first_empty] = (i == (txb->nr_frags - 1)) ?
		    txb : NULL;

		hdr->seq_ctl |= i;

		out_cmd = &txq->cmd[idx];
		memset(&out_cmd->hdr, 0, sizeof(out_cmd->hdr));
		memset(&out_cmd->cmd.tx, 0, sizeof(out_cmd->cmd.tx));
		out_cmd->hdr.cmd = REPLY_TX;
		out_cmd->hdr.sequence = FIFO_TO_SEQ(tx_id) |
		    INDEX_TO_SEQ(q->first_empty);
		/* copy frags header */
		memcpy(out_cmd->cmd.tx.hdr, hdr, hdr_len);

		hdr = (struct ieee80211_hdr_4addr *)out_cmd->cmd.tx.hdr;
		len = sizeof(struct ipw_tx_cmd) + sizeof(struct ipw_cmd_header)
		    + hdr_len;
		len = (len + 3) & ~3;

		phys_addr = txq->dma_addr_cmd + sizeof(struct ipw_cmd) * idx +
		    offsetof(struct ipw_cmd, hdr);
		attach_buffer_to_tfd_frame(tfd, phys_addr, cpu_to_le16(len));

		if (txb->encrypted && !priv->ieee->host_encrypt)
			ipw_build_tx_cmd_hwcrypto(priv, &(out_cmd->cmd.tx),
						  txb->fragments[i], hdr_len,
						  i == (txb->nr_frags - 1));
		else {
			IPW_DEBUG_TX("Tx packet in the clear, [%c] unicast.\n",
				     unicast ? '*' : ' ');
		}

		IPW_DEBUG_FRAG("Adding fragment %i of %i (%d bytes).\n",
			       i, txb->nr_frags,
			       txb->fragments[i]->len - hdr_len);
		IPW_DEBUG_TX("Dumping TX packet frag %i of %i (%d bytes):\n",
			     i, txb->nr_frags,
			     txb->fragments[i]->len - hdr_len);
		printk_buf(IPW_DL_TX, txb->fragments[i]->data + hdr_len,
			   txb->fragments[i]->len - hdr_len);

		phys_addr = cpu_to_le32(pci_map_single(priv->pci_dev,
						       txb->fragments[i]->data +
						       hdr_len,
						       txb->fragments[i]->len -
						       hdr_len,
						       PCI_DMA_TODEVICE));
		len = txb->fragments[i]->len - hdr_len;
		attach_buffer_to_tfd_frame(tfd, phys_addr, cpu_to_le16(len));
		out_cmd->cmd.tx.len = txb->fragments[i]->len;

		tfd->control_flags = TFD_CTL_COUNT_SET(2) |
		    TFD_CTL_PAD_SET(U32_PAD(len));

		//todoG need this for burst mode later on
		if (ipw_build_tx_cmd_basic(priv, &(out_cmd->cmd.tx),
					   hdr, unicast, sta_id, tx_id) != 0) {
			IPW_ERROR("tx build cmd basic failed.\n");
			goto drop;
		}

		if (ipw_build_tx_cmd_rate(priv, &(out_cmd->cmd.tx),
					  hdr, sta_id, tx_id)) {
			IPW_ERROR("tx cmd rate scale  failed.\n");
			goto drop;

		}

		IPW_DEBUG_TX("Tx rate %d (%02X:%02X)\n",
			     out_cmd->cmd.tx.rate,
			     out_cmd->cmd.tx.supp_rates[0],
			     out_cmd->cmd.tx.supp_rates[1]);

		ipw_build_tx_cmd_calib(priv, &out_cmd->cmd.tx,
				       out_cmd->hdr.sequence);
		out_cmd->cmd.tx.tx_flags |= TX_CMD_FLG_SEQ_CTL_MSK;

		printk_buf(IPW_DL_TX, out_cmd->cmd.payload,
			   sizeof(out_cmd->cmd.tx));

		printk_buf(IPW_DL_TX, (u8 *) out_cmd->cmd.tx.hdr,
			   ieee80211_get_hdrlen(out_cmd->cmd.tx.hdr->
						frame_ctl));

		IPW_DEBUG_FRAG("%i fragments being sent as %i chunks.\n",
			       txb->nr_frags,
			       TFD_CTL_COUNT_GET(tfd->control_flags));

		q->first_empty = ipw_queue_inc_wrap(q->first_empty, q->n_bd);
	}
	/* kick DMA */

	txq->need_update = 1;
	rc = ipw_tx_queue_update_write_ptr(priv, txq, tx_id);

	if (rc)
		return rc;
	if ((ipw_queue_space(q) < q->high_mark) && priv->netdev_registered)
		netif_stop_queue(priv->net_dev);

	return 0;

      drop:
	IPW_DEBUG_DROP("Silently dropping Tx packet.\n");
	ieee80211_txb_free(txb);
	return 0;
}

static int ipw_net_hard_start_xmit(struct ieee80211_txb *txb,
				   struct net_device *dev, int pri)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	unsigned long flags = 0;
	int is_locked = 0;
	if (pri < 0) {
		pri = 0;
		is_locked = 1;
	}

	IPW_DEBUG_TX("dev->xmit(%d bytes)\n", txb->payload_size);
	if (is_locked == 0)
		spin_lock_irqsave(&priv->lock, flags);
	if (!(priv->status & (STATUS_ASSOCIATED | STATUS_ASSOCIATING))) {
		IPW_DEBUG_INFO("Tx attempt while not associated.\n");
		priv->ieee->stats.tx_carrier_errors++;
		goto fail_unlock;
	}
	ipw_tx_skb(priv, txb, pri);
	priv->led_packets += txb->payload_size;
	ipw_setup_activity_timer(priv);
	if (is_locked == 0)
		spin_unlock_irqrestore(&priv->lock, flags);
	return 0;
      fail_unlock:
	if (is_locked == 0)
		spin_unlock_irqrestore(&priv->lock, flags);
	return -1;
}

static struct net_device_stats *ipw_net_get_stats(struct net_device *dev)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	priv->ieee->stats.tx_packets = priv->tx_packets;
	priv->ieee->stats.rx_packets = priv->rx_packets;
	return &priv->ieee->stats;
}

static int ipw_net_set_mac_address(struct net_device *dev, void *p)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	struct sockaddr *addr = p;
	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;
	mutex_lock(&priv->mutex);
	priv->config |= CFG_CUSTOM_MAC;
	memcpy(priv->mac_addr, addr->sa_data, ETH_ALEN);
	printk(KERN_INFO "%s: Setting MAC to " MAC_FMT "\n",
	       priv->net_dev->name, MAC_ARG(priv->mac_addr));
	IPW_DEBUG_INFO("Restarting adapter to set new MAC.\n");
	queue_work(priv->workqueue, &priv->down);
	mutex_unlock(&priv->mutex);
	return 0;
}

static void ipw_ethtool_get_drvinfo(struct net_device *dev,
				    struct ethtool_drvinfo *info)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	char ver_string[9];

	strcpy(info->driver, DRV_NAME);
	strcpy(info->version, DRV_VERSION);

	memcpy(ver_string, priv->card_alive.sw_rev, 8);
	ver_string[8] = '\0';

	snprintf(info->fw_version, sizeof(info->fw_version),
		 "%s%d.%d %d:%d (%s)",
		 priv->card_alive.ucode_major & 0x80 ? "P" : "",
		 priv->card_alive.ucode_major & 0x7f,
		 priv->card_alive.ucode_minor,
		 priv->card_alive.ver_type,
		 priv->card_alive.ver_subtype, ver_string);

	strcpy(info->bus_info, pci_name(priv->pci_dev));
	info->eedump_len = EEPROM_IMAGE_SIZE;
}

static u32 ipw_ethtool_get_link(struct net_device *dev)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	return (priv->status & STATUS_ASSOCIATED) != 0;
}

static int ipw_ethtool_get_eeprom_len(struct net_device *dev)
{
	return EEPROM_IMAGE_SIZE;
}

static int ipw_ethtool_get_eeprom(struct net_device *dev,
				  struct ethtool_eeprom *eeprom, u8 * bytes)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	if (eeprom->offset + eeprom->len > EEPROM_IMAGE_SIZE)
		return -EINVAL;

	mutex_lock(&priv->mutex);
	memcpy(bytes, &((u8 *) priv->eeprom)[eeprom->offset], eeprom->len);
	mutex_unlock(&priv->mutex);
	return 0;
}

static struct ethtool_ops ipw_ethtool_ops = {
	.get_link = ipw_ethtool_get_link,
	.get_drvinfo = ipw_ethtool_get_drvinfo,
	.get_eeprom_len = ipw_ethtool_get_eeprom_len,
	.get_eeprom = ipw_ethtool_get_eeprom,
};

static irqreturn_t ipw_isr(int irq, void *data, struct pt_regs *regs)
{
	struct ipw_priv *priv = data;
	u32 inta, inta_mask;
	if (!priv)
		return IRQ_NONE;

	spin_lock(&priv->lock);
	if (!(priv->status & STATUS_INT_ENABLED)) {
		/* Shared IRQ */
		goto none;
	}

	inta = ipw_read32(priv, CSR_INT);
	inta_mask = ipw_read32(priv, CSR_INT_MASK);
	if (inta == 0xFFFFFFFF) {
		/* Hardware disappeared */
		IPW_WARNING("IRQ INTA == 0xFFFFFFFF\n");
		goto none;
	}

	if (!(inta & (CSR_INI_SET_MASK & inta_mask))) {
		if (inta)
			ipw_write32(priv, CSR_INT, inta);
		/* Shared interrupt */
		goto none;
	}

	/* tell the device to stop sending interrupts */

	IPW_DEBUG_ISR
	    ("interrupt recieved 0x%08x masked 0x%08x card mask 0x%08x\n", inta,
	     inta_mask, CSR_INI_SET_MASK);

	priv->status &= ~STATUS_INT_ENABLED;
	ipw_write32(priv, CSR_INT_MASK, 0x00000000);
	/* ack current interrupts */
	ipw_write32(priv, CSR_INT, inta);
	inta &= (CSR_INI_SET_MASK & inta_mask);
	/* Cache INTA value for our tasklet */
	priv->isr_inta = inta;
	tasklet_schedule(&priv->irq_tasklet);
	spin_unlock(&priv->lock);

	return IRQ_HANDLED;

      none:
	spin_unlock(&priv->lock);
	return IRQ_NONE;
}

static void ipw_bg_rf_kill(void *data)
{
	struct ipw_priv *priv = data;

	wake_up_interruptible(&priv->wait_command_queue);

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);

	if (!(priv->status & STATUS_RF_KILL_MASK)) {
		IPW_DEBUG(IPW_DL_INFO | IPW_DL_RF_KILL,
			  "HW RF Kill no longer active, restarting "
			  "device\n");
		if (priv->status & STATUS_EXIT_PENDING)
			ipw_down(priv);
	} else {
		priv->led_state = IPW_LED_LINK_RADIOOFF;

		if (!(priv->status & STATUS_RF_KILL_HW))
			IPW_DEBUG_RF_KILL("Can not turn radio back on - "
					  "disabled by SW switch\n");
	}
	mutex_unlock(&priv->mutex);
}

static void ipw_link_up(struct ipw_priv *priv)
{
	BUG_ON(!priv->netdev_registered);

	/* Reset ieee stats */
	memset(&priv->ieee->ieee_stats, 0, sizeof(struct ieee80211_stats));

	/* We don't reset the net_device_stats (ieee->stats) on
	 * re-association */

	priv->last_seq_num = -1;
	priv->last_frag_num = -1;
	priv->last_packet_time = 0;

	netif_carrier_on(priv->net_dev);
	if (netif_queue_stopped(priv->net_dev)) {
		IPW_DEBUG_NOTIF("waking queue\n");
		netif_wake_queue(priv->net_dev);
	} else {
		IPW_DEBUG_NOTIF("starting queue\n");
		netif_start_queue(priv->net_dev);
	}

	ipw_scan_cancel(priv);
	ipw_reset_stats(priv);

	/* Ensure the rate is updated immediately */
	priv->last_rate = ipw_get_current_rate(priv);
	ipw_gather_stats(priv);

	ipw_update_link_led(priv);

	notify_wx_assoc_event(priv);
	if (priv->config & CFG_BACKGROUND_SCAN)
		ipw_scan_initiate(priv, priv->ieee->freq_band, 1000);
}

static void ipw_bg_update_link_led(void *data)
{
	struct ipw_priv *priv = data;
	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	ipw_update_link_led(priv);
	mutex_unlock(&priv->mutex);
}

static void ipw_bg_link_up(void *data)
{
	struct ipw_priv *priv = data;
	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	ipw_link_up(priv);
	mutex_unlock(&priv->mutex);
}

static void ipw_link_down(struct ipw_priv *priv)
{
	/* ipw_link_down should never be called if we aren't
	 * registered with netdev. */
	BUG_ON(!priv->netdev_registered);

	priv->led_packets = 0;
	netif_carrier_off(priv->net_dev);
	netif_stop_queue(priv->net_dev);
	notify_wx_assoc_event(priv);
	/* Cancel any queued work ... */
	cancel_delayed_work(&priv->scan_check);
	cancel_delayed_work(&priv->gather_stats);
	ipw_scan_cancel(priv);
	ipw_reset_stats(priv);
	/* Queue up another scan... */

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	ipw_scan_initiate(priv, priv->ieee->freq_band, 0);
}

static void ipw_bg_link_down(void *data)
{
	struct ipw_priv *priv = data;
	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	ipw_link_down(priv);
	mutex_unlock(&priv->mutex);
}

static void ipw_bg_resume_work(void *data)
{
	struct ipw_priv *priv = data;
	unsigned long flags;

	mutex_lock(&priv->mutex);

	/* The following it a temporary work around due to the
	 * suspend / resume not fully initializing the NIC correctly.
	 * Without all of the following, resume will not attempt to take
	 * down the NIC (it shouldn't really need to) and will just try
	 * and bring the NIC back up.  However that fails during the
	 * ucode verification process.  This then causes ipw_down to be
	 * called *after* ipw_nic_init() has succeedded -- which
	 * then lets the next init sequence succeed.  So, we've
	 * replicated all of that NIC init code here... */

	ipw_write32(priv, CSR_INT, 0xFFFFFFFF);

	ipw_nic_init(priv);

	ipw_write32(priv, CSR_UCODE_DRV_GP1_CLR, CSR_UCODE_SW_BIT_RFKILL);
	ipw_write32(priv, CSR_UCODE_DRV_GP1_CLR,
		    CSR_UCODE_DRV_GP1_BIT_CMD_BLOCKED);
	ipw_write32(priv, CSR_INT, 0xFFFFFFFF);
	ipw_write32(priv, CSR_UCODE_DRV_GP1_CLR, CSR_UCODE_SW_BIT_RFKILL);
	ipw_write32(priv, CSR_UCODE_DRV_GP1_CLR, CSR_UCODE_SW_BIT_RFKILL);

	/* tell the device to stop sending interrupts */
	ipw_disable_interrupts(priv);

	spin_lock_irqsave(&priv->lock, flags);
	ipw_clear_bit(priv, CSR_GP_CNTRL, CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
	spin_unlock_irqrestore(&priv->lock, flags);

	spin_lock_irqsave(&priv->lock, flags);
	if (!ipw_grab_restricted_access(priv)) {
		ipw_write_restricted_reg(priv, ALM_APMG_CLK_DIS,
					 APMG_CLK_REG_VAL_DMA_CLK_RQT);
		ipw_release_restricted_access(priv);
	}
	spin_unlock_irqrestore(&priv->lock, flags);

	udelay(5);

	ipw_nic_reset(priv);

	/* Bring the device back up */
	priv->status &= ~STATUS_IN_SUSPEND;

	/* Send the RESUME state to the daemon */
	ipw_send_daemon_sync(priv, DAEMON_SYNC_RESUME, 0, NULL);

	mutex_unlock(&priv->mutex);
}

static void ipw_associate_timeout(void *data)
{
	struct ipw_priv *priv = data;

	mutex_lock(&priv->mutex);
	if (priv->status & STATUS_ASSOCIATED ||
	    !(priv->status & STATUS_ASSOCIATING)) {
		mutex_unlock(&priv->mutex);
		return;
	}

	IPW_DEBUG_ASSOC("Association attempt timed out.\n");

	ipw_send_disassociate(data, 1);
	mutex_unlock(&priv->mutex);
}

static int ipw_setup_deferred_work(struct ipw_priv *priv)
{
	int ret = 0;
#ifdef CONFIG_SOFTWARE_SUSPEND2_BUILTIN
	priv->workqueue = create_workqueue(DRV_NAME, 0);
	priv->daemonqueue = create_workqueue(DRV_NAME, 0);
#else
	priv->workqueue = create_workqueue(DRV_NAME);
	priv->daemonqueue = create_workqueue(DRV_NAME);
#endif

	init_waitqueue_head(&priv->wait_daemon_out_queue);
	init_waitqueue_head(&priv->wait_command_queue);
	init_waitqueue_head(&priv->wait_daemon_cmd_done);

	INIT_WORK(&priv->associate, ipw_bg_associate, priv);
	INIT_WORK(&priv->calibrated_work, ipw_bg_calibrated_work, priv);
	INIT_WORK(&priv->disassociate, ipw_bg_disassociate, priv);
	INIT_WORK(&priv->rx_replenish, ipw_bg_rx_queue_replenish, priv);
	INIT_WORK(&priv->rf_kill, ipw_bg_rf_kill, priv);
	INIT_WORK(&priv->up, ipw_bg_up, priv);
	INIT_WORK(&priv->down, ipw_bg_down, priv);
	INIT_WORK(&priv->request_scan, ipw_bg_request_scan, priv);
	INIT_WORK(&priv->gather_stats, ipw_bg_gather_stats, priv);
	INIT_WORK(&priv->abort_scan, ipw_bg_abort_scan, priv);
	INIT_WORK(&priv->roam, ipw_bg_roam, priv);
	INIT_WORK(&priv->scan_check, ipw_bg_scan_check, priv);
	INIT_WORK(&priv->link_up, ipw_bg_link_up, priv);
	INIT_WORK(&priv->update_link_led, ipw_bg_update_link_led, priv);
	INIT_WORK(&priv->link_down, ipw_bg_link_down, priv);
	INIT_WORK(&priv->auth_work, ipw_auth_work, priv);
	INIT_WORK(&priv->merge_networks, ipw_merge_adhoc_network, priv);
	INIT_WORK(&priv->associate_timeout, ipw_associate_timeout, priv);

	INIT_WORK(&priv->alive_start, ipw_bg_alive_start, priv);
	INIT_WORK(&priv->resume_work, ipw_bg_resume_work, priv);

#ifdef CONFIG_IPW3945_QOS
	/* QoS */
	INIT_WORK(&priv->qos_activate, ipw_bg_qos_activate, priv);
#endif

	/* 802.11h */
	INIT_WORK(&priv->report_work, ipw_bg_report_work, priv);

	INIT_WORK(&priv->post_associate, ipw_bg_post_associate, priv);

	INIT_WORK(&priv->activity_timer, ipw_bg_activity_timer, priv);

	INIT_WORK(&priv->daemon_cmd_work, ipw_bg_daemon_cmd, priv);
	INIT_WORK(&priv->daemon_tx_status_sync, ipw_bg_daemon_tx_status_sync,
		  priv);

	tasklet_init(&priv->irq_tasklet, (void (*)(unsigned long))
		     ipw_irq_tasklet, (unsigned long)priv);

	init_timer(&priv->roaming_wdt);
	init_timer(&priv->disassociate_wdt);
	priv->roaming_wdt.function = ipw_kickoff_roaming;
	priv->roaming_wdt.data = (unsigned long)priv;
	priv->disassociate_wdt.function = ipw_kickoff_disassociate;
	priv->disassociate_wdt.data = (unsigned long)priv;

	return ret;
}

static void shim__set_security(struct net_device *dev,
			       struct ieee80211_security *sec)
{
	struct ipw_priv *priv = ieee80211_priv(dev);
	int i;

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	for (i = 0; i < 4; i++) {
		if (sec->flags & (1 << i)) {
			priv->ieee->sec.key_sizes[i] = sec->key_sizes[i];
			if (sec->key_sizes[i] == 0) {
				IPW_DEBUG_WEP("Disabling key %d.\n", i);
				priv->ieee->sec.flags &= ~(1 << i);
			} else {
				IPW_DEBUG_WEP("Setting key %d.\n", i);
				priv->ieee->sec.flags &= ~(1 << i);
				memcpy(priv->ieee->sec.keys[i], sec->keys[i],
				       sec->key_sizes[i]);
			}
			priv->ieee->sec.flags |= (1 << i);
			priv->status |= STATUS_SECURITY_UPDATED;
		}
	}

	if ((sec->flags & SEC_ACTIVE_KEY) &&
	    priv->ieee->sec.active_key != sec->active_key) {
		if (sec->active_key <= 3) {
			IPW_DEBUG_WEP("Setting active Tx key to %d.\n",
				      sec->active_key);
			priv->ieee->sec.active_key = sec->active_key;
			priv->ieee->sec.flags |= SEC_ACTIVE_KEY;
		} else
			priv->ieee->sec.flags &= ~SEC_ACTIVE_KEY;
		priv->status |= STATUS_SECURITY_UPDATED;
	}

	if ((sec->flags & SEC_AUTH_MODE) &&
	    (priv->ieee->sec.auth_mode != sec->auth_mode)) {
		priv->ieee->sec.auth_mode = sec->auth_mode;
		priv->ieee->sec.flags |= SEC_AUTH_MODE;
		if (sec->auth_mode == WLAN_AUTH_SHARED_KEY) {
			IPW_DEBUG_WEP("Setting auth mode to SHARED.\n");
			priv->capability |= CAP_SHARED_KEY;
		} else {
			IPW_DEBUG_WEP("Setting auth mode to OPEN.\n");
			priv->capability &= ~CAP_SHARED_KEY;
		}
		priv->status |= STATUS_SECURITY_UPDATED;
	}

	if (sec->flags & SEC_ENABLED && priv->ieee->sec.enabled != sec->enabled) {
		priv->ieee->sec.flags |= SEC_ENABLED;
		priv->ieee->sec.enabled = sec->enabled;
		priv->status |= STATUS_SECURITY_UPDATED;
		if (sec->enabled) {
			IPW_DEBUG_WEP("Enabling security.\n");
			priv->capability |= CAP_PRIVACY_ON;
		} else {
			IPW_DEBUG_WEP("Disabling security.\n");
			priv->capability &= ~CAP_PRIVACY_ON;
		}
	}

	if (sec->flags & SEC_ENCRYPT)
		priv->ieee->sec.encrypt = sec->encrypt;

	if (sec->flags & SEC_LEVEL && priv->ieee->sec.level != sec->level) {
		IPW_DEBUG_WEP("Setting security level to %d.\n", sec->level);
		priv->ieee->sec.level = sec->level;
		priv->ieee->sec.flags |= SEC_LEVEL;
		priv->status |= STATUS_SECURITY_UPDATED;
	}

	/* To match current functionality of ipw2100 (which works well w/
	 * various supplicants, we don't force a disassociate if the
	 * privacy capability changes ... */
#if 0
	if ((priv->status & (STATUS_ASSOCIATED | STATUS_ASSOCIATING)) &&
	    (((priv->assoc_request.capability &
	       WLAN_CAPABILITY_PRIVACY) && !sec->enabled) ||
	     (!(priv->assoc_request.capability &
		WLAN_CAPABILITY_PRIVACY) && sec->enabled))) {
		IPW_DEBUG_ASSOC("Disassociating due to capability "
				"change.\n");
		ipw_disassociate(priv);
	}
#endif
	mutex_unlock(&priv->mutex);
}

static int init_supported_rates(struct ipw_priv *priv,
				struct ipw_supported_rates *rates)
{
	/* TODO: Mask out rates based on priv->rates_mask */

	memset(rates, 0, sizeof(*rates));
	/* configure supported rates */
	switch (priv->ieee->freq_band) {
	case IEEE80211_52GHZ_BAND:
		rates->ieee_mode = IPW_A_MODE;
		rates->purpose = IPW_RATE_CAPABILITIES;
		ipw_add_ofdm_scan_rates(rates, IEEE80211_OFDM_MODULATION,
					IEEE80211_OFDM_DEFAULT_RATES_MASK);
		break;
	default:		/* Mixed or 2.4Ghz */
		rates->ieee_mode = IPW_G_MODE;
		rates->purpose = IPW_RATE_CAPABILITIES;
		ipw_add_cck_scan_rates(rates, IEEE80211_CCK_MODULATION,
				       IEEE80211_CCK_DEFAULT_RATES_MASK);
		if (priv->ieee->modulation & IEEE80211_OFDM_MODULATION) {
			u8 modulation = IEEE80211_CCK_MODULATION;

			if (!
			    (priv->ieee->modulation & IEEE80211_CCK_MODULATION))
				modulation = IEEE80211_OFDM_MODULATION;

			ipw_add_ofdm_scan_rates(rates, modulation,
						IEEE80211_OFDM_DEFAULT_RATES_MASK);
		}
		break;
	}

	return 0;
}

/*
  Power management (not Tx power!) functions
*/
#define PCI_LINK_CTRL      0x0F0

static int ipw_power_init_handle(struct ipw_priv *priv)
{
	int rc = 0, i;
	struct ipw_power_mgr *pow_data;
	int size = sizeof(struct ipw_power_vec_entry) * IPW_POWER_AC;
	u16 pci_pm;

	IPW_DEBUG_POWER("Intialize power \n");

	pow_data = &(priv->power_data);

	memset(pow_data, 0, sizeof(*pow_data));

	pow_data->active_index = IPW_POWER_RANGE_0;
	pow_data->dtim_val = 0xffff;

	memcpy(&pow_data->pwr_range_0[0], &range_0[0], size);
	memcpy(&pow_data->pwr_range_1[0], &range_1[0], size);

	rc = pci_read_config_word(priv->pci_dev, PCI_LINK_CTRL, &pci_pm);
	if (rc != 0)
		return 0;
	else {
		struct ipw_powertable_cmd *cmd;

		IPW_DEBUG_POWER("adjust power command flags\n");

		for (i = 0; i < IPW_POWER_AC; i++) {
			cmd = &pow_data->pwr_range_0[i].cmd;

			if (pci_pm & 0x1)
				cmd->flags &= ~0x8;
			else
				cmd->flags |= 0x8;
		}
	}

	return rc;
}

int ipw_update_power_cmd(struct ipw_priv *priv,
			 struct ipw_powertable_cmd *cmd, u32 mode)
{
	int rc = 0, i;
	u8 skip = 0;
	u32 max_sleep = 0;
	struct ipw_power_vec_entry *range;
	u8 period = 0;
	struct ipw_power_mgr *pow_data;

	if ((mode < IPW_POWER_MODE_CAM) || (mode > IPW_POWER_INDEX_5)) {
		IPW_DEBUG_POWER("Error invalid power mode \n");
		return -1;
	}
	pow_data = &(priv->power_data);

	if (pow_data->active_index == IPW_POWER_RANGE_0)
		range = &pow_data->pwr_range_0[0];
	else
		range = &pow_data->pwr_range_1[1];

	memcpy(cmd, &range[mode].cmd, sizeof(struct ipw_powertable_cmd));

	if (priv->assoc_network != NULL) {
		unsigned long flags;

		spin_lock_irqsave(&priv->ieee->lock, flags);
		period = priv->assoc_network->tim.tim_period;
		spin_unlock_irqrestore(&priv->ieee->lock, flags);
	}

	skip = range[mode].no_dtim;

	if (period == 0) {
		period = 1;
		skip = 0;
	}

	if (skip == 0) {
		max_sleep = period;
		cmd->flags &= ~PMC_TCMD_FLAG_SLEEP_OVER_DTIM_MSK;
	} else {
		max_sleep =
		    (cmd->SleepInterval[PMC_TCMD_SLEEP_INTRVL_TABLE_SIZE - 1] /
		     period) * period;
		cmd->flags |= PMC_TCMD_FLAG_SLEEP_OVER_DTIM_MSK;
	}

	for (i = 0; i < PMC_TCMD_SLEEP_INTRVL_TABLE_SIZE; i++) {
		if (cmd->SleepInterval[i] > max_sleep)
			cmd->SleepInterval[i] = max_sleep;
	}

	IPW_DEBUG_POWER("Flags value = 0x%08X\n", cmd->flags);
	IPW_DEBUG_POWER("Tx timeout = %u\n", cmd->TxDataTimeout);
	IPW_DEBUG_POWER("Rx timeout = %u\n", cmd->RxDataTimeout);
	IPW_DEBUG_POWER("Sleep interval vector = { %d , %d , %d , %d , %d }\n",
			cmd->SleepInterval[0], cmd->SleepInterval[1],
			cmd->SleepInterval[2], cmd->SleepInterval[3],
			cmd->SleepInterval[4]);

	return rc;
}

/************* CARD-FUNCTION ************/
/* Print card information like version number other eeprom values */
int ipw_card_show_info(struct ipw_priv *priv)
{
	u8 strPbaNumber[32] = {
		0
	};
	u16 hwVersion = 0;
	int rc = 0;
	u8 val;
	memset(strPbaNumber, 0, 32);
	rc = ipw_query_eeprom(priv, EEPROM_BOARD_REVISION, sizeof(u16),
			      (u8 *) & hwVersion);
	if (rc) {
		IPW_WARNING("failed to read EEPROM_BOARD_REVISION\n");
		return rc;
	}
	IPW_DEBUG_INFO("3945ABG HW Version %u.%u.%u\n",
		       ((hwVersion >> 8) & 0x0F), ((hwVersion >> 8) >> 4),
		       (hwVersion & 0x00FF));
	rc = ipw_query_eeprom(priv,
			      EEPROM_BOARD_PBA_NUMBER,
			      EEPROM_BOARD_PBA_NUMBER_LENTGH, strPbaNumber);
	if (rc) {
		IPW_WARNING("failed to read EEPROM_BOARD_PBA_NUMBER\n");
		return rc;
	}
	IPW_DEBUG_INFO("3945ABG PBA Number %.16s\n", strPbaNumber);
	rc = ipw_query_eeprom(priv, 0x94, sizeof(u8), (u8 *) & val);
	if (rc) {
		IPW_WARNING("failed to read EEPROM 0x94\n");
		return rc;
	}

	IPW_DEBUG_INFO("eeprom value at byte 0x94 is 0x%02X\n", val);

	rc = ipw_query_eeprom(priv, EEPROM_ANTENNA_SWITCH_TYPE, sizeof(u8),
			      (u8 *) & val);
	if (rc) {
		IPW_WARNING("failed to read EEPROM_ANTENNA_SWITCH_TYPE\n");
		return rc;
	}
	IPW_DEBUG_INFO("EEPROM_ANTENNA_SWITCH_TYPE is 0x%02X\n", val);

	return 0;
}

/*
  Call all registered function for card RXON status
  this function called after we call REPLY_RXON command.
*/
/*
  called at ipw_pci_remove to clean the driver data
*/
static int ipw_card_remove_notify(struct ipw_priv *priv)
{
	int rc = 0;
	return rc;
}

/*
  call registered function about change in current network
  beacon
*/
static int ipw_card_bss_active_changed_notify(struct ipw_priv *priv,
					      struct ieee80211_network *network)
{
	int rc = 0;
	u8 period = 0;

	if (network != NULL)
		period = network->tim.tim_period;

	if (priv->power_data.dtim_val != period) {
		//todoG need to call reg_send_power command
		priv->power_data.dtim_val = period;
		if (period <= 10)
			priv->power_data.active_index = IPW_POWER_RANGE_0;
		else
			priv->power_data.active_index = IPW_POWER_RANGE_1;
	}
	return rc;
}

/*
  called at start up to initialize the NIC
*/

/*
  called after we recieved REPLY_ALIVE notification.
  this function starts the calibration then start the process
  of transfering the card to recieving state
*/
static void ipw_alive_start(struct ipw_priv *priv)
{
	int thermal_spin = 0;

	if (priv->card_alive.is_valid != 1) {
		/* We had an error bringing up the hardware, so take it
		 * all the way back down so we can try again */
		IPW_DEBUG_INFO("Alive failed.\n");
		ipw_down(priv);
		return;
	}

	/* After the ALIVE response, we can processed host commands */
	priv->status |= STATUS_ALIVE;

	IPW_DEBUG_INFO("Alive received.\n");

	/* Wait for thermal sensor in adapter to kick in */
	while (ipw_read32(priv, CSR_UCODE_DRV_GP2) == 0) {
		thermal_spin++;
		udelay(10);
	}

	if (thermal_spin)
		IPW_DEBUG_INFO("Thermal calibration took %dus\n",
			       thermal_spin * 10);

	ipw_send_daemon_sync(priv, DAEMON_SYNC_INIT, 0, NULL);
}

static void ipw_bg_alive_start(void *data)
{
	struct ipw_priv *priv = data;

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	ipw_alive_start(data);
	mutex_unlock(&priv->mutex);
}

#define MAX_HW_RESTARTS 5
static int ipw_up(struct ipw_priv *priv)
{
	int rc, i;

	if (priv->status & STATUS_EXIT_PENDING) {
		IPW_WARNING("Exit pending will not bring the NIC up\n");
		return -EIO;
	}

	if (priv->status & STATUS_RF_KILL_SW) {
		IPW_WARNING("Radio disabled by module parameter.\n");
		return 0;
	}

	else if (priv->status & STATUS_RF_KILL_HW) {
		IPW_WARNING("Radio Frequency Kill Switch is On:\n"
			    "Kill switch must be turned off for "
			    "wireless networking to work.\n");
		return 0;
	}

	ipw_write32(priv, CSR_INT, 0xFFFFFFFF);

	rc = ipw_nic_init(priv);
	if (rc) {
		IPW_ERROR("Unable to int nic\n");
		return rc;
	}

	ipw_write32(priv, CSR_UCODE_DRV_GP1_CLR, CSR_UCODE_SW_BIT_RFKILL);
	ipw_write32(priv, CSR_UCODE_DRV_GP1_CLR,
		    CSR_UCODE_DRV_GP1_BIT_CMD_BLOCKED);
	ipw_write32(priv, CSR_INT, 0xFFFFFFFF);

	ipw_enable_interrupts(priv);

	ipw_write32(priv, CSR_UCODE_DRV_GP1_CLR, CSR_UCODE_SW_BIT_RFKILL);
	ipw_write32(priv, CSR_UCODE_DRV_GP1_CLR, CSR_UCODE_SW_BIT_RFKILL);

	for (i = 0; i < MAX_HW_RESTARTS; i++) {
		/* Load the microcode, firmware, and eeprom.
		 * Also start the clocks. */
		rc = ipw_load(priv);
		if (rc) {
			IPW_ERROR("Unable to load firmware: %d\n", rc);
			continue;
		}

		ipw_verify_ucode(priv);
		ipw_nic_start(priv);
		ipw_card_show_info(priv);

		if (!(priv->config & CFG_CUSTOM_MAC)) {
			eeprom_parse_mac(priv, priv->mac_addr);
			IPW_DEBUG_INFO("MAC address: " MAC_FMT "\n",
				       MAC_ARG(priv->mac_addr));
		}
		memcpy(priv->net_dev->dev_addr, priv->mac_addr, ETH_ALEN);
		return 0;
	}

	priv->status |= STATUS_EXIT_PENDING;
	ipw_down(priv);

	/* tried to restart and config the device for as long as our
	 * patience could withstand */
	IPW_ERROR("Unable to initialize device after %d attempts.\n", i);
	return -EIO;
}

static void ipw_bg_up(void *data)
{
	struct ipw_priv *priv = data;

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	ipw_up(data);
	mutex_unlock(&priv->mutex);
}

static void ipw_down(struct ipw_priv *priv)
{
	unsigned long flags;
	int exit_pending = priv->status & STATUS_EXIT_PENDING;

	priv->status |= STATUS_EXIT_PENDING;

	/* Nuke any running timers */
	del_timer_sync(&priv->roaming_wdt);
	del_timer_sync(&priv->disassociate_wdt);

	/* If we are coming down due to a microcode error, then
	 * don't bother trying to do anything that resutls in sending
	 * host commands... */
	if (!(priv->status & STATUS_FW_ERROR) && ipw_is_alive(priv)) {
		if ((priv->assoc_request.capability & WLAN_CAPABILITY_IBSS))
			ipw_remove_current_network(priv);

		ipw_update_link_led(priv);
		ipw_update_activity_led(priv);
		ipw_update_tech_led(priv);
	}

	/* Unblock any waiting calls */
	wake_up_interruptible_all(&priv->wait_command_queue);
	wake_up_interruptible_all(&priv->wait_daemon_out_queue);
	wake_up_interruptible_all(&priv->wait_daemon_cmd_done);

	cancel_delayed_work(&priv->resume_work);
	cancel_delayed_work(&priv->up);
	cancel_delayed_work(&priv->down);
	cancel_delayed_work(&priv->daemon_cmd_work);
	cancel_delayed_work(&priv->activity_timer);
	cancel_delayed_work(&priv->request_scan);
	cancel_delayed_work(&priv->gather_stats);
	cancel_delayed_work(&priv->scan_check);
	cancel_delayed_work(&priv->associate_timeout);
	cancel_delayed_work(&priv->rf_kill);
	cancel_delayed_work(&priv->report_work);

	/* Wipe out the EXIT_PENDING status bit if we are not actually
	 * exiting the module */
	if (!exit_pending)
		priv->status &= ~STATUS_EXIT_PENDING;

	/* tell the device to stop sending interrupts */
	ipw_disable_interrupts(priv);

	if (priv->netdev_registered) {
		netif_carrier_off(priv->net_dev);
		netif_stop_queue(priv->net_dev);
	}

	/* If we have not previously called ipw_init() then
	 * clear all bits but the RF Kill and SUSPEND bits and return */
	if (!ipw_is_init(priv)) {
		priv->status &= (STATUS_RF_KILL_MASK | STATUS_IN_SUSPEND);
		goto exit;
	}

	/* ...otherwise clear out all the status bits but the RF Kill and
	 * SUSPEND bits and continue taking the NIC down. */
	priv->status &= (STATUS_RF_KILL_MASK | STATUS_IN_SUSPEND);

	spin_lock_irqsave(&priv->lock, flags);
	ipw_clear_bit(priv, CSR_GP_CNTRL, CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
	spin_unlock_irqrestore(&priv->lock, flags);

	ipw_stop_tx_queue(priv);
	ipw_rxq_stop(priv);

	spin_lock_irqsave(&priv->lock, flags);
	if (!ipw_grab_restricted_access(priv)) {
		ipw_write_restricted_reg(priv, ALM_APMG_CLK_DIS,
					 APMG_CLK_REG_VAL_DMA_CLK_RQT);
		ipw_release_restricted_access(priv);
	}
	spin_unlock_irqrestore(&priv->lock, flags);

	udelay(5);

	ipw_nic_stop_master(priv);

	spin_lock_irqsave(&priv->lock, flags);
	ipw_set_bit(priv, CSR_RESET, CSR_RESET_REG_FLAG_SW_RESET);
	spin_unlock_irqrestore(&priv->lock, flags);

	ipw_nic_reset(priv);

      exit:
	memset(&priv->card_alive, 0, sizeof(struct ipw_alive_resp));

	spin_lock_irqsave(&priv->daemon_lock, flags);
	ipw_clear_daemon_lists(priv);
	spin_unlock_irqrestore(&priv->daemon_lock, flags);

	/* clear out any free frames */
	ipw_clear_free_frames(priv);

	/* If we aren't in a suspend cycle then send the UNIT state to the
	 * daemon */
	if (!(priv->status & STATUS_IN_SUSPEND))
		ipw_send_daemon_sync(priv, DAEMON_SYNC_UNINIT, 0, NULL);
}

static void ipw_bg_down(void *data)
{
	struct ipw_priv *priv = data;

	if (priv->status & STATUS_EXIT_PENDING)
		return;

	mutex_lock(&priv->mutex);
	ipw_down(data);
	mutex_unlock(&priv->mutex);
}

static int ipw_ioctl_cmd(struct ipw_priv *priv, struct ifreq *req)
{
	IPW_DEBUG_INFO("Calling ioctl...\n");
	return 0;
}

static int ipw_ioctl(struct net_device *dev, struct ifreq *req, int cmd)
{
	struct ipw_priv *priv = ieee80211_priv(dev);

	if (priv->status & STATUS_EXIT_PENDING)
		return -EOPNOTSUPP;

	switch (cmd) {
	case IPW_IOCTL_CMD:
		return ipw_ioctl_cmd(priv, req);

#if WIRELESS_EXT < 18
	case IPW_IOCTL_WPA_SUPPLICANT:
		return ipw_wpa_supplicant(dev, &((struct iwreq *)req)->u.data);
#endif

	default:
		return -EOPNOTSUPP;
	}

	return -EOPNOTSUPP;
}

static struct pci_device_id card_ids[] __devinitdata = {
	{0x8086, 0x4222, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0x8086, 0x4227, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0}
};

MODULE_DEVICE_TABLE(pci, card_ids);
static struct attribute *ipw_sysfs_entries[] = {
	&dev_attr_rf_kill.attr,
	&dev_attr_status.attr,
	&dev_attr_cfg.attr,
	&dev_attr_dump_errors.attr,
	&dev_attr_dump_events.attr,
	&dev_attr_roam.attr,
	&dev_attr_scan_age.attr,
	&dev_attr_led.attr,
	&dev_attr_antenna.attr,
	&dev_attr_statistics.attr,
	&dev_attr_cmd.attr,
	&dev_attr_temperature.attr,
	&dev_attr_eeprom.attr,
	&dev_attr_channels.attr,
	NULL
};

/*
static struct attribute *ipw_sysfs_entries[] = {
	NULL
};
*/
static struct attribute_group ipw_attribute_group = {
	.name = NULL,		/* put in device directory */
	.attrs = ipw_sysfs_entries,
};

static int ipw_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int err = 0;
	struct net_device *net_dev;
	void __iomem *base;
	u32 length;
	u32 pci_id;
	struct ipw_priv *priv;
	int i;

	net_dev = alloc_ieee80211(sizeof(struct ipw_priv));
	if (net_dev == NULL) {
		err = -ENOMEM;
		goto out;
	}

	IPW_DEBUG_INFO("*** LOAD DRIVER ***\n");
	priv = ieee80211_priv(net_dev);
	priv->ieee = netdev_priv(net_dev);
	priv->ieee->host_encrypt = 1;
	priv->ieee->host_decrypt = 1;
	priv->ieee->host_build_iv = 1;
	priv->net_dev = net_dev;
	priv->pci_dev = pdev;
	priv->rxq = NULL;
	priv->antenna = antenna;
#ifdef CONFIG_IPW3945_DEBUG
	ipw_debug_level = debug;
#endif

	spin_lock_init(&priv->daemon_lock);
	spin_lock_init(&priv->lock);
	spin_lock_init(&priv->power_data.lock);
	spin_lock_init(&priv->rate_scale_mgr.lock);

	for (i = 0; i < IPW_IBSS_MAC_HASH_SIZE; i++)
		INIT_LIST_HEAD(&priv->ibss_mac_hash[i]);

	INIT_LIST_HEAD(&priv->free_frames);

	INIT_LIST_HEAD(&priv->daemon_in_list);
	INIT_LIST_HEAD(&priv->daemon_out_list);
	INIT_LIST_HEAD(&priv->daemon_free_list);

	mutex_init(&priv->mutex);
	if (pci_enable_device(pdev)) {
		err = -ENODEV;
		goto out_free_ieee80211;
	}

	pci_set_master(pdev);
	priv->num_stations = 0;
	memset(priv->stations, 0,
	       NUM_OF_STATIONS * sizeof(struct ipw_station_entry));
	memset(&(priv->txq[0]), 0, sizeof(struct ipw_tx_queue) * 6);
	memset(&priv->card_alive, 0, sizeof(struct ipw_alive_resp));
	priv->data_retry_limit = -1;
	priv->auth_state = CMAS_INIT;

	err = pci_set_dma_mask(pdev, DMA_32BIT_MASK);
	if (!err)
		err = pci_set_consistent_dma_mask(pdev, DMA_32BIT_MASK);
	if (err) {
		printk(KERN_WARNING DRV_NAME ": No suitable DMA available.\n");
		goto out_pci_disable_device;
	}

	pci_set_drvdata(pdev, priv);
	err = pci_request_regions(pdev, DRV_NAME);
	if (err)
		goto out_pci_disable_device;
	/* We disable the RETRY_TIMEOUT register (0x41) to keep
	 * PCI Tx retries from interfering with C3 CPU state */
	pci_write_config_byte(pdev, 0x41, 0x00);

	length = pci_resource_len(pdev, 0);
	priv->hw_len = length;
	base = ioremap_nocache(pci_resource_start(pdev, 0), length);
	if (!base) {
		err = -ENODEV;
		goto out_pci_release_regions;
	}

	priv->hw_base = base;
	IPW_DEBUG_INFO("pci_resource_len = 0x%08x\n", length);
	IPW_DEBUG_INFO("pci_resource_base = %p\n", base);
	err = ipw_setup_deferred_work(priv);
	if (err) {
		IPW_ERROR("Unable to setup deferred work\n");
		goto out_iounmap;
	}

	/* Initialize module parameter values here */

	if (!led)
		priv->config |= CFG_NO_LED;
	if (associate)
		priv->config |= CFG_ASSOCIATE;
	else
		IPW_DEBUG_INFO("Auto associate disabled.\n");
	if (auto_create)
		priv->config |= CFG_ADHOC_CREATE;
	else
		IPW_DEBUG_INFO("Auto adhoc creation disabled.\n");
	if (disable) {
		priv->status |= STATUS_RF_KILL_SW;
		IPW_DEBUG_INFO("Radio disabled.\n");
	}
#ifdef CONFIG_IPW3945_QOS
	/* QoS */
	ipw_qos_init(priv, qos_enable, qos_burst_enable,
		     qos_burst_CCK, qos_burst_OFDM);
#endif

	/*************************************/
	switch (mode) {
	case 1:
		priv->ieee->iw_mode = IW_MODE_ADHOC;
		break;
#ifdef CONFIG_IPW3945_MONITOR
	case 2:
		priv->ieee->iw_mode = IW_MODE_MONITOR;
		break;
#endif
	default:
	case 0:
		priv->ieee->iw_mode = IW_MODE_INFRA;
		break;
	}

	priv->ieee->mode = IEEE_G | IEEE_B;
	priv->ieee->freq_band = IEEE80211_24GHZ_BAND;
	priv->ieee->modulation = IEEE80211_OFDM_MODULATION |
	    IEEE80211_CCK_MODULATION;

	pci_id =
	    (priv->pci_dev->device << 16) | priv->pci_dev->subsystem_device;

	switch (pci_id) {
	case 0x42221005:	/* 0x4222 0x8086 0x1005 is BG SKU */
	case 0x42221034:	/* 0x4222 0x8086 0x1034 is BG SKU */
	case 0x42271014:	/* 0x4227 0x8086 0x1014 is BG SKU */
	case 0x42221044:	/* 0x4222 0x8086 0x1044 is BG SKU */
		priv->is_abg = 0;
		break;

	default:		/* Rest are assumed ABG SKU -- if this is not the
				 * case then the card will get the wrong 'Detected'
				 * line in the kernel log however the code that
				 * initializes the GEO table will detect no A-band
				 * channels and remove the is_abg mask. */
		priv->ieee->mode |= IEEE_A;
		priv->ieee->freq_band |= IEEE80211_52GHZ_BAND;
		priv->is_abg = 1;
		break;
	}

	printk(KERN_INFO DRV_NAME
	       ": Detected Intel PRO/Wireless 3945%s Network Connection\n",
	       priv->is_abg ? "ABG" : "BG");

	if (channel != 0) {
		priv->config |= CFG_STATIC_CHANNEL;
		priv->channel = channel;
		IPW_DEBUG_INFO("Bind to static channel %d\n", channel);
		/* TODO: Validate that provided channel is in range */
	} else
		priv->channel = 1;

	priv->rates_mask = IEEE80211_DEFAULT_RATES_MASK;
	priv->missed_beacon_threshold = IPW_MB_DISASSOCIATE_THRESHOLD_DEFAULT;
	priv->roaming_threshold = IPW_MB_ROAMING_THRESHOLD_DEFAULT;
	priv->rts_threshold = DEFAULT_RTS_THRESHOLD;
	/* If power management is turned on, default to AC mode */
	priv->power_mode = IPW_POWER_AC;
	priv->actual_txpower_limit = IPW_DEFAULT_TX_POWER;
	err = request_irq(pdev->irq, ipw_isr, SA_SHIRQ, DRV_NAME, priv);
	if (err) {
		IPW_ERROR("Error allocating IRQ %d\n", pdev->irq);
		goto out_destroy_workqueue;
	}

	SET_MODULE_OWNER(net_dev);
	SET_NETDEV_DEV(net_dev, &pdev->dev);
	mutex_lock(&priv->mutex);
	priv->ieee->hard_start_xmit = ipw_net_hard_start_xmit;
	priv->ieee->set_security = shim__set_security;
	/* QoS */
	/* Implement the STA callbacks... */
	priv->ieee->handle_auth = ipw_handle_auth;
	priv->ieee->handle_deauth = ipw_handle_deauth;
	priv->ieee->handle_disassoc = ipw_handle_disassoc;
	priv->ieee->handle_probe_response = ipw_handle_probe_response;
	priv->ieee->handle_beacon = ipw_handle_beacon;
	priv->ieee->handle_assoc_response = ipw_handle_assoc_response;
	priv->ieee->handle_probe_request = ipw_handle_probe_request;
	/**************************************************************/
	priv->ieee->perfect_rssi = -20;
	priv->ieee->worst_rssi = -85;
	net_dev->open = ipw_net_open;
	net_dev->stop = ipw_net_stop;
	net_dev->do_ioctl = ipw_ioctl;
	net_dev->get_stats = ipw_net_get_stats;
	net_dev->set_mac_address = ipw_net_set_mac_address;

#if WIRELESS_EXT > 16
	priv->wireless_data.spy_data = &priv->ieee->spy_data;
	net_dev->wireless_data = &priv->wireless_data;
#else
	net_dev->get_wireless_stats = ipw_get_wireless_stats;
#endif

	net_dev->wireless_handlers = &ipw_wx_handler_def;
	net_dev->ethtool_ops = &ipw_ethtool_ops;
	net_dev->irq = pdev->irq;
	net_dev->base_addr = (unsigned long)priv->hw_base;
	net_dev->mem_start = pci_resource_start(pdev, 0);
	net_dev->mem_end = net_dev->mem_start + pci_resource_len(pdev, 0) - 1;

	err = sysfs_create_group(&pdev->dev.kobj, &ipw_attribute_group);
	if (err) {
		IPW_ERROR("failed to create sysfs device attributes\n");
		mutex_unlock(&priv->mutex);
		goto out_release_irq;
	}
	// allocate ucode buffers
	priv->ucode_code.actual_len = ALM_RTC_INST_SIZE;
	priv->ucode_data.actual_len = ALM_RTC_DATA_SIZE;
	priv->ucode_code.v_addr =
	    pci_alloc_consistent(priv->pci_dev, priv->ucode_code.actual_len,
				 &(priv->ucode_code.p_addr));
	priv->ucode_data.v_addr =
	    pci_alloc_consistent(priv->pci_dev, priv->ucode_data.actual_len,
				 &(priv->ucode_data.p_addr));
	if (!priv->ucode_code.v_addr || !priv->ucode_data.v_addr) {
		IPW_ERROR("failed to allocate pci memory\n");
		mutex_unlock(&priv->mutex);
		goto out_remove_sysfs;
	}

	priv->shared_virt =
	    pci_alloc_consistent(priv->pci_dev, sizeof(struct ipw_shared_t),
				 &priv->shared_phys);
	if (!priv->shared_virt) {
		IPW_ERROR("failed to allocate pci memory\n");
		mutex_unlock(&priv->mutex);
		goto out_pci_alloc;
	}
	mutex_unlock(&priv->mutex);

	/* We don't register with netdev here as the NIC isn't actually to
	 * a point where we can start accepting wireless extension and
	 * network configuration commands.  If we simply return EAGAIN
	 * from all of those handlers until the adapter is calibrated,
	 * then we run the risk of user space tools bailing out erroneously
	 * and users not knowing how to make things work. */
	IPW_DEBUG_INFO("Waiting for ipw3945d to request INIT.\n");

	return 0;

      out_remove_sysfs:
	sysfs_remove_group(&pdev->dev.kobj, &ipw_attribute_group);
	if (priv->workqueue)
		cancel_delayed_work(&priv->rf_kill);

      out_pci_alloc:
	if (priv->ucode_code.v_addr != NULL)
		pci_free_consistent(priv->pci_dev, priv->ucode_code.actual_len,
				    priv->ucode_code.v_addr,
				    priv->ucode_code.p_addr);
	if (priv->ucode_data.v_addr != NULL)
		pci_free_consistent(priv->pci_dev, priv->ucode_data.actual_len,
				    priv->ucode_data.v_addr,
				    priv->ucode_data.p_addr);
	if (priv->shared_virt != NULL)
		pci_free_consistent(priv->pci_dev, sizeof(struct ipw_shared_t),
				    priv->shared_virt, priv->shared_phys);
	priv->ucode_code.v_addr = NULL;
	priv->ucode_data.v_addr = NULL;
	priv->shared_virt = NULL;
      out_release_irq:
	free_irq(pdev->irq, priv);
      out_destroy_workqueue:
	destroy_workqueue(priv->workqueue);
	destroy_workqueue(priv->daemonqueue);
	priv->workqueue = NULL;
      out_iounmap:
	iounmap(priv->hw_base);
      out_pci_release_regions:
	pci_release_regions(pdev);
      out_pci_disable_device:
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
      out_free_ieee80211:
	free_ieee80211(priv->net_dev);
      out:
	return err;
}

static void ipw_pci_remove(struct pci_dev *pdev)
{
	struct ipw_priv *priv = pci_get_drvdata(pdev);
	struct list_head *p, *q;
	int i;

	if (!priv)
		return;

	IPW_DEBUG_INFO("*** UNLOAD DRIVER ***\n");

	mutex_lock(&priv->mutex);

	priv->status |= STATUS_EXIT_PENDING;

	ipw_down(priv);

	mutex_unlock(&priv->mutex);

	destroy_workqueue(priv->workqueue);
	destroy_workqueue(priv->daemonqueue);

	priv->workqueue = NULL;

	/* Free MAC hash list for ADHOC */
	for (i = 0; i < IPW_IBSS_MAC_HASH_SIZE; i++) {
		list_for_each_safe(p, q, &priv->ibss_mac_hash[i]) {
			list_del(p);
			kfree(list_entry(p, struct ipw_ibss_seq, list));
		}
	}

	sysfs_remove_group(&pdev->dev.kobj, &ipw_attribute_group);

	if (priv->ucode_code.v_addr != NULL)
		pci_free_consistent(priv->pci_dev, priv->ucode_code.actual_len,
				    priv->ucode_code.v_addr,
				    priv->ucode_code.p_addr);
	if (priv->ucode_data.v_addr != NULL)
		pci_free_consistent(priv->pci_dev, priv->ucode_data.actual_len,
				    priv->ucode_data.v_addr,
				    priv->ucode_data.p_addr);
	if (priv->shared_virt != NULL)
		pci_free_consistent(priv->pci_dev, sizeof(*priv->shared_virt),
				    priv->shared_virt, priv->shared_phys);
	priv->ucode_code.v_addr = NULL;
	priv->ucode_data.v_addr = NULL;
	priv->shared_virt = NULL;
	if (priv->rxq)
		ipw_rx_queue_free(priv, priv->rxq);
	ipw_tx_queue_free(priv);
	ipw_card_remove_notify(priv);

	if (priv->ucode_raw) {
		release_firmware(priv->ucode_raw);
		priv->ucode_raw = NULL;
	}

	if (priv->netdev_registered)
		unregister_netdev(priv->net_dev);
	free_irq(pdev->irq, priv);
	iounmap(priv->hw_base);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
	free_ieee80211(priv->net_dev);
}

#ifdef CONFIG_PM
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
static int ipw_pci_suspend(struct pci_dev *pdev, u32 state)
#else
static int ipw_pci_suspend(struct pci_dev *pdev, pm_message_t state)
#endif
{
	struct ipw_priv *priv = pci_get_drvdata(pdev);
	struct net_device *dev = priv->net_dev;
	printk(KERN_INFO "%s: Going into suspend...\n", dev->name);

	mutex_lock(&priv->mutex);

	priv->status |= STATUS_IN_SUSPEND;

	/* Send the RESUME state to the daemon */
	ipw_send_daemon_sync(priv, DAEMON_SYNC_SUSPEND, 0, NULL);

	/* Take down the device; powers it off, etc. */
	ipw_down(priv);

	/* Remove the PRESENT state of the device */
	if (priv->netdev_registered)
		netif_device_detach(dev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
	pci_save_state(pdev, priv->pm_state);
#else
	pci_save_state(pdev);
#endif
	pci_disable_device(pdev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
	pci_set_power_state(pdev, state);
#else
	pci_set_power_state(pdev, PCI_D3hot);
#endif

	mutex_unlock(&priv->mutex);

	return 0;
}

static int ipw_pci_resume(struct pci_dev *pdev)
{
	struct ipw_priv *priv = pci_get_drvdata(pdev);
	struct net_device *dev = priv->net_dev;

	printk(KERN_INFO "%s: Coming out of suspend...\n", dev->name);

	mutex_lock(&priv->mutex);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
	pci_set_power_state(pdev, 0);
#else
	pci_set_power_state(pdev, PCI_D0);
#endif
	pci_enable_device(pdev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
	pci_restore_state(pdev, priv->pm_state);
#else
	pci_restore_state(pdev);
#endif
	/*
	 * Suspend/Resume resets the PCI configuration space, so we have to
	 * re-disable the RETRY_TIMEOUT register (0x41) to keep PCI Tx retries
	 * from interfering with C3 CPU state. pci_restore_state won't help
	 * here since it only restores the first 64 bytes pci config header.
	 */
	pci_write_config_byte(pdev, 0x41, 0x00);

	/* Set the device back into the PRESENT state; this will also wake
	 * the queue of needed */
	if (priv->netdev_registered)
		netif_device_attach(dev);

	queue_delayed_work(priv->workqueue, &priv->resume_work, 3 * HZ);

	mutex_unlock(&priv->mutex);

	return 0;
}
#endif

/* driver initialization stuff */
static struct pci_driver ipw_driver = {
	.name = DRV_NAME,.id_table = card_ids,.probe =
	    ipw_pci_probe,.remove = __devexit_p(ipw_pci_remove),
#ifdef CONFIG_PM
	.suspend = ipw_pci_suspend,.resume = ipw_pci_resume,
#endif
};
static int __init ipw_init(void)
{

	int ret;
	printk(KERN_INFO DRV_NAME ": " DRV_DESCRIPTION ", " DRV_VERSION "\n");
	printk(KERN_INFO DRV_NAME ": " DRV_COPYRIGHT "\n");
	ret = pci_module_init(&ipw_driver);
	if (ret) {
		IPW_ERROR("Unable to initialize PCI module\n");
		return ret;
	}

	ret = driver_create_file(&ipw_driver.driver, &driver_attr_debug_level);
	if (ret) {
		IPW_ERROR("Unable to create driver sysfs file\n");
		pci_unregister_driver(&ipw_driver);
		return ret;
	}

	return ret;
}

static void __exit ipw_exit(void)
{
	driver_remove_file(&ipw_driver.driver, &driver_attr_debug_level);
	pci_unregister_driver(&ipw_driver);
}

module_param(antenna, int, 0444);
MODULE_PARM_DESC(antenna, "select antenna (1=Main, 2=Aux, default 0 [both])");
module_param(disable, int, 0444);
MODULE_PARM_DESC(disable, "manually disable the radio (default 0 [radio on])");
module_param(associate, int, 0444);
MODULE_PARM_DESC(associate, "auto associate when scanning (default 0 off)");
module_param(auto_create, int, 0444);
MODULE_PARM_DESC(auto_create, "auto create adhoc network (default 1 on)");
module_param(led, int, 0444);
MODULE_PARM_DESC(led, "enable led control (default 1 on)\n");
module_param(debug, int, 0444);
MODULE_PARM_DESC(debug, "debug output mask");
module_param(channel, int, 0444);
MODULE_PARM_DESC(channel, "channel to limit associate to (default 0 [ANY])");

#ifdef CONFIG_IPW3945_QOS
/* QoS */
module_param(qos_enable, int, 0444);
MODULE_PARM_DESC(qos_enable, "enable all QoS functionalitis");
module_param(qos_burst_enable, int, 0444);
MODULE_PARM_DESC(qos_burst_enable, "enable QoS burst mode");
module_param(qos_no_ack_mask, int, 0444);
MODULE_PARM_DESC(qos_no_ack_mask, "mask Tx_Queue to no ack");
module_param(qos_burst_CCK, int, 0444);
MODULE_PARM_DESC(qos_burst_CCK, "set CCK burst value");
module_param(qos_burst_OFDM, int, 0444);
MODULE_PARM_DESC(qos_burst_OFDM, "set OFDM burst value");
#endif

/****************************************/
#ifdef CONFIG_IPW3945_MONITOR
module_param(mode, int, 0444);
MODULE_PARM_DESC(mode, "network mode (0=BSS,1=IBSS,2=Monitor)");
#else
module_param(mode, int, 0444);
MODULE_PARM_DESC(mode, "network mode (0=BSS,1=IBSS)");
#endif
module_exit(ipw_exit);
module_init(ipw_init);
