/**********************************************************************************/
/* $Id: Cmv.h,v 1.1 2004/02/06 22:01:34 sleeper Exp $                      */
/*										  */
/* Copyright (c) 2001, Analog Devices Inc., All Rights Reserved			  */
/*										  */
/* CMV.H									  */
/*										  */
/* Contains definitions for the bits of CMV Requests and Replies..		  */
/* Taken virtually as is from NDIS ADI project, some comments added.		  */
/*										  */
/*   Rewrite for MAC OS, 10/2001						  */
/*										  */
/* This file is part of the "ADI USB ADSL Driver for Linux".			  */
/*										  */
/* "ADI USB ADSL Driver for Linux" is free software; you can redistribute it      */
/* and/or modify it under the terms of the GNU General Public License as 	  */
/* published by the Free Software Foundation; either version 2 of the License,    */
/* or (at your option) any later version.					  */
/*										  */
/* "ADI USB ADSL Driver for Linux" is distributed in the hope that it will be     */
/* useful, but WITHOUT ANY WARRANTY; without even the implied warranty of	  */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the		  */
/* GNU General Public License for more details.					  */
/*										  */
/* You should have received a copy of the GNU General Public License		  */
/* along with "ADI USB ADSL Driver for Linux"; if not, write to the Free Software */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA      */
/**********************************************************************************/

#ifndef _CMV_H_
#define _CMV_H_ 

#define ON  1
#define OFF 0

/*STAT.status CMV bit definitions, field masks & shift counts*/
#define STAT_STATUS_OFFSET      0x00

#define STAT_HW_PASS            0x0
#define STAT_HW_FAIL            0x1
#define STAT_DME_PASS           0x0
#define STAT_DME_FAIL           0x1
#define STAT_DTIR_PASS          0x0
#define STAT_DTIR_FAIL          0x1

#define STAT_SW_R_ACT_REQ       0x0
#define STAT_SW_CIDLE           0x0
#define STAT_SW_INITIALIZATION  0x1
#define STAT_SW_OPERATIONAL     0x2
#define STAT_SW_FAIL            0x3
#define STAT_SW_TESTSIGNAL      0x4
#define STAT_SW_TESTHW          0x5
#define STAT_SW_FASTRETRAIN     0x7

#define STAT_CRC_NORMAL         0x0
#define STAT_CRC_DS             0x1
#define STAT_CRC_US             0x2

#define STAT_INIT_SUCCESS           0x0
#define STAT_INIT_CRC1_ERROR        0x1
#define STAT_INIT_CRC2_ERROR        0x2
#define STAT_INIT_CRC3_ERROR        0x3
#define STAT_INIT_CRC4_ERROR        0x4
#define STAT_INIT_CRC5_ERROR        0x5
#define STAT_INIT_CQUIET2_FAIL      0x6
#define STAT_INIT_RQUIET2_FAIL      0x7
#define STAT_INIT_RREVERB3_TIMEOUT  0x8
#define STAT_INIT_CREVERB4_TIMEOUT  0x9
#define STAT_INIT_CREVERB5_TIMEOUT  0xa
#define STAT_INIT_LOWSNR        0xc

#define STAT_CORT_CO            0x0
#define STAT_CORT_RT            0x1

#define STAT_HW_MASK            0x00000001
#define STAT_DME_MASK           0x00000002
#define STAT_DTIR_MASK          0x00000004
#define STAT_SW_MASK            0x00000f00
#define STAT_CRC_MASK           0x00003000
#define STAT_INIT_MASK          0x00070000
#define STAT_RACTREQDET_MASK    0x00080000
#define STAT_OPTION_MASK        0x1fe00000
#define STAT_CORT_MASK          0x80000000

#define STAT_HW_LSB             0x00000000
#define STAT_DME_LSB            0x00000001
#define STAT_DTIR_LSB           0x00000002
#define STAT_SW_LSB             0x00000008
#define STAT_CRC_LSB            0x0000000c
#define STAT_INIT_LSB           0x00000010
#define STAT_RACTREQDET_LSB     0x00000014
#define STAT_OPTION_LSB         0x00000015
#define STAT_CORT_LSB           0x0000001f

/*RATE.actual CMV bit field masks & shift counts*/
#define RATE_ACTUAL_OFFSET      0x00

#define RATE_DUPLXUS_MASK       0x000000ff
#define RATE_DUPLXDS_MASK       0x0000ff00
#define RATE_SIMPLXDS_MASK      0x01ff0000

#define RATE_DUPLXUS_LSB        0x00000000
#define RATE_DUPLXDS_LSB        0x00000008
#define RATE_SIMPLXDS_LSB       0x00000010

/* CNTL.control CMV bit definitions, field masks & shift counts*/
#define CNTL_CONTROL_OFFSET         0x00

/*ATU-C*/
#define CNTL_ATUC_STATE_CIDLE       0x0
#define CNTL_ATUC_STATE_CQUIET1     0x2
#define CNTL_ATUC_STATE_CTESTSIGNAL 0x4
#define CNTL_ATUC_STATE_CTESTHW     0x5

#define CNTL_ATUC_CHCONFIG_EXPLICIT 0x0
#define CNTL_ATUC_CHCONFIG_ADAPTIVE 0x1

#define CNTL_ATUC_STATE_MASK        0x00000007
#define CNTL_ATUC_CHCONFIG_MASK     0x000000e0

#define CNTL_ATUC_STATE_LSB         0x00000000
#define CNTL_ATUC_CHCONFIG_LSB      0x00000005

/*ATU-R*/
#define CNTL_ATUR_STATE_RACTREQ     0x0
#define CNTL_ATUR_STATE_CTESTSIGNAL 0x4
#define CNTL_ATUR_STATE_CTESTHW     0x5

#define CNTL_ATUR_STATE_MASK        0x00000007
#define CNTL_ATUR_STATE_LSB         0x00000000

/* ADPT.downstream CMV bit definitions, field masks & shift counts*/
#define ADPT_DOWNSTREAM_OFFSET      0x00

#define ADPT_DS_CODING_ON           0x1
#define ADPT_DS_CODING_OFF          0x0

#define ADPT_DS_MINDATARATE_MASK    0x000001ff
#define ADPT_DS_INTLVDEPTH_MASK     0x0000fe00
#define ADPT_DS_MAXDATARATE_MASK    0x01ff0000
#define ADPT_DS_CODING_MASK         0x02000000
#define ADPT_DS_MARGIN_MASK         0xf0000000

#define ADPT_DS_MINDATARATE_LSB     0x00000000
#define ADPT_DS_INTLVDEPTH_LSB      0x00000009
#define ADPT_DS_MAXDATARATE_LSB     0x00000010
#define ADPT_DS_CODING_LSB          0x00000019
#define ADPT_DS_MARGIN_LSB          0x0000001c

/*ADPT.upstream CMV bit definitions, field masks & shift counts*/
#define ADPT_UPSTREAM_OFFSET        0x01

#define ADPT_US_CODING_ON           0x1
#define ADPT_US_CODING_OFF          0x0

#define ADPT_US_MINDATARATE_MASK    0x000001ff
#define ADPT_US_INTLVDEPTH_MASK     0x0000fe00
#define ADPT_US_MAXDATARATE_MASK    0x01ff0000
#define ADPT_US_CODING_MASK         0x02000000
#define ADPT_US_MARGIN_MASK         0xf0000000

#define ADPT_US_MINDATARATE_LSB     0x00000000
#define ADPT_US_INTLVDEPTH_LSB      0x00000009
#define ADPT_US_MAXDATARATE_LSB     0x00000010
#define ADPT_US_CODING_LSB          0x00000019
#define ADPT_US_MARGIN_LSB          0x0000001c

/*INFO.RT_serial_number CMV bit definitions, field masks & shift counts*/
#define INFO_RT_SERIALNUMBER0_OFFSET    0x00
#define INFO_RT_SERIALNUMBER1_OFFSET    0x01
#define INFO_RT_SERIALNUMBER2_OFFSET    0x02
#define INFO_RT_SERIALNUMBER3_OFFSET    0x03
#define INFO_RT_SERIALNUMBER4_OFFSET    0x04
#define INFO_RT_SERIALNUMBER5_OFFSET    0x05
#define INFO_RT_SERIALNUMBER6_OFFSET    0x06
#define INFO_RT_SERIALNUMBER7_OFFSET    0x07
#define INFO_RT_SERIALNUMBER8_OFFSET    0x08

#define INFO_RT_SERIALNUMBER_MASK   0xffffffff
#define INFO_RT_SERIALNUMBER_LSB    0x0

/*INFO.RT_vendor_ID CMV bit definitions, field masks & shift counts*/
#define INFO_RT_VENDORID_OFFSET     0x09

#define INFO_RT_VENDORID_MASK       0x000000ff
#define INFO_RT_VENDORID_LSB        0x0

/*INFO.RT_version_number CMV bit definitions, field masks & shift counts*/
#define INFO_RT_VERSIONNUMBER_OFFSET    0x0a

#define INFO_RT_VERSIONNUMBER_MASK  0x000000ff
#define INFO_RT_VERSIONNUMBER_LSB   0x0
 
/*INFO.RT_firmware_version CMV bit definitions, field masks & shift counts*/
/*RT only*/
#define INFO_RT_FIRMWAREVERSION_OFFSET  0x0b

#define INFO_RT_FIRMWAREVERSION_MASK    0xffffffff
#define INFO_RT_FIRMWAREVERSION_LSB     0x0

/*INFO.CO_firmware_version CMV bit definitions, field masks & shift counts*/
/*CO Only*/
#define INFO_CO_FIRMWAREVERSION_OFFSET  0x0b

#define INFO_CO_FIRMWAREVERSION_MASK    0xffffffff
#define INFO_CO_FIRMWAREVERSION_LSB     0x0


/*DIAG.control CMV bit definitions, field masks & shift counts*/
#define DIAG_CONTROL_OFFSET     0x00

#define DIAG_CONTROL_DSCRCCORRUPT_ON    0x1
#define DIAG_CONTROL_DSCRCCORRUPT_OFF   0x0

#define DIAG_CONTROL_USCRCCORRUPT_ON    0x1
#define DIAG_CONTROL_USCRCCORRUPT_OFF   0x0

#define DIAG_CONTROL_CLRNECOUNTS_MASK   0x00000001
#define DIAG_CONTROL_CLRFECOUNTS_MASK   0x00000002
#define DIAG_CONTROL_CLRNELATCH_MASK    0x00000004
#define DIAG_CONTROL_CLRFELATCH_MASK    0x00000008
#define DIAG_CONTROL_DSCRCCORRUPT_MASK  0x00010000
#define DIAG_CONTROL_USCRCCORRUPT_MASK  0x00020000

#define DIAG_CONTROL_CLRNECOUNTS_LSB    0x0
#define DIAG_CONTROL_CLRFECOUNTS_LSB    0x1
#define DIAG_CONTROL_CLRNELATCH_LSB     0x2
#define DIAG_CONTROL_CLRFELATCH_LSB     0x3
#define DIAG_CONTROL_DSCRCCORRUPT_LSB   0x10
#define DIAG_CONTROL_USCRCCORRUPT_LSB   0x11

/*DIAG.flags CMV bit definitions, field masks & shift counts*/
#define DIAG_FLAGS_OFFSET               0x01

#define DIAG_FLAGS_ASSERTED             0x1
#define DIAG_FLAGS_NEGATED              0x0

#define DIAG_FLAGS_NEFASTFEC_MASK       0x00000001
#define DIAG_FLAGS_NEINTLFEC_MASK       0x00000002
#define DIAG_FLAGS_NEFASTCRC_MASK       0x00000004
#define DIAG_FLAGS_NEINTLCRC_MASK       0x00000008
#define DIAG_FLAGS_NELOSDEFECT_MASK     0x00000010
#define DIAG_FLAGS_NELOSFAILURE_MASK    0x00000020
#define DIAG_FLAGS_NESEFDEFECT_MASK     0x00000040
#define DIAG_FLAGS_NESEFFAILURE_MASK    0x00000080
#define DIAG_FLAGS_NELPRDEFECT_MASK     0x00000100
#define DIAG_FLAGS_NELPRFAILURE_MASK    0x00000200

#define DIAG_FLAGS_NEINTLLODFAIL_MASK   0x00001000
#define DIAG_FLAGS_NEFASTLODFAIL_MASK   0x00002000
#define DIAG_FLAGS_NEINTLHECFAIL_MASK   0x00004000
#define DIAG_FLAGS_NEFASTHECFAIL_MASK   0x00008000

#define DIAG_FLAGS_FEFASTFEC_MASK       0x00010000
#define DIAG_FLAGS_FEINTLFEC_MASK       0x00020000
#define DIAG_FLAGS_FEFASTCRC_MASK       0x00040000
#define DIAG_FLAGS_FEINTLCRC_MASK       0x00080000
#define DIAG_FLAGS_FELOSDEFECT_MASK     0x00100000
#define DIAG_FLAGS_FELOSFAILURE_MASK    0x00200000
#define DIAG_FLAGS_FESEFDEFECT_MASK     0x00400000
#define DIAG_FLAGS_FESEFFAILURE_MASK    0x00800000
#define DIAG_FLAGS_FELPRDEFECT_MASK     0x01000000
#define DIAG_FLAGS_FELPRFAILURE_MASK    0x02000000

#define DIAG_FLAGS_NEFASTFEC_LSB    0x00
#define DIAG_FLAGS_NEINTLFEC_LSB    0x01
#define DIAG_FLAGS_NEFASTCRC_LSB    0x02
#define DIAG_FLAGS_NEINTLCRC_LSB    0x03
#define DIAG_FLAGS_NELOSDEFECT_LSB  0x04
#define DIAG_FLAGS_NELOSFAILURE_LSB 0x05
#define DIAG_FLAGS_NESEFDEFECT_LSB  0x06
#define DIAG_FLAGS_NESEFFAILURE_LSB 0x07
#define DIAG_FLAGS_NELPRDEFECT_LSB  0x08
#define DIAG_FLAGS_NELPRFAILURE_LSB 0x09
#define DIAG_FLAGS_FEFASTFEC_LSB    0x10
#define DIAG_FLAGS_FEINTLFEC_LSB    0x11
#define DIAG_FLAGS_FEFASTCRC_LSB    0x12
#define DIAG_FLAGS_FEINTLCRC_LSb    0x13
#define DIAG_FLAGS_FELOSDEFECT_LSB  0x14
#define DIAG_FLAGS_FELOSFAILURE_LSB 0x15
#define DIAG_FLAGS_FESEFDEFECT_LSB  0x16
#define DIAG_FLAGS_FESEFFAILURE_LSB 0x17
#define DIAG_FLAGS_FELPRDEFECT_LSB  0x18
#define DIAG_FLAGS_FELPRFAILURE_LSB 0x19

/*DIAG.flags_latched CMV bit definitions, field masks & shift counts*/
#define DIAG_FLAGS_LATCHED_OFFSET   0x02

#define DIAG_FLAGS_LATCHED_ASSERTED 0x1
#define DIAG_FLAGS_LATCHED_NEGATED  0x0

#define DIAG_FLAGS_LATCHED_NEFASTFEC_MASK       0x00000001
#define DIAG_FLAGS_LATCHED_NEINTLFEC_MASK       0x00000002
#define DIAG_FLAGS_LATCHED_NEFASTCRC_MASK       0x00000004
#define DIAG_FLAGS_LATCHED_NEINTLCRC_MASK       0x00000008
#define DIAG_FLAGS_LATCHED_NELOSDEFECT_MASK     0x00000010
#define DIAG_FLAGS_LATCHED_NELOSFAILURE_MASK    0x00000020
#define DIAG_FLAGS_LATCHED_NESEFDEFECT_MASK     0x00000040
#define DIAG_FLAGS_LATCHED_NESEFFAILURE_MASK    0x00000080
#define DIAG_FLAGS_LATCHED_NELPRDEFECT_MASK     0x00000100
#define DIAG_FLAGS_LATCHED_NELPRFAILURE_MASK    0x00000200
#define DIAG_FLAGS_LATCHED_FEFASTFEC_MASK       0x00010000
#define DIAG_FLAGS_LATCHED_FEINTLFEC_MASK       0x00020000
#define DIAG_FLAGS_LATCHED_FEFASTCRC_MASK       0x00040000
#define DIAG_FLAGS_LATCHED_FEINTLCRC_MASK       0x00080000
#define DIAG_FLAGS_LATCHED_FELOSDEFECT_MASK     0x00100000
#define DIAG_FLAGS_LATCHED_FELOSFAILURE_MASK    0x00200000
#define DIAG_FLAGS_LATCHED_FESEFDEFECT_MASK     0x00400000
#define DIAG_FLAGS_LATCHED_FESEFFAILURE_MASK    0x00800000
#define DIAG_FLAGS_LATCHED_FELPRDEFECT_MASK     0x01000000
#define DIAG_FLAGS_LATCHED_FELPRFAILURE_MASK    0x02000000

#define DIAG_FLAGS_LATCHED_NEFASTFEC_LSB    0x00
#define DIAG_FLAGS_LATCHED_NEINTLFEC_LSB    0x01
#define DIAG_FLAGS_LATCHED_NEFASTCRC_LSB    0x02
#define DIAG_FLAGS_LATCHED_NEINTLCRC_LSB    0x03
#define DIAG_FLAGS_LATCHED_NELOSDEFECT_LSB  0x04
#define DIAG_FLAGS_LATCHED_NELOSFAILURE_LSB 0x05
#define DIAG_FLAGS_LATCHED_NESEFDEFECT_LSB  0x06
#define DIAG_FLAGS_LATCHED_NESEFFAILURE_LSB 0x07
#define DIAG_FLAGS_LATCHED_NELPRDEFECT_LSB  0x08
#define DIAG_FLAGS_LATCHED_NELPRFAILURE_LSB 0x09
#define DIAG_FLAGS_LATCHED_FEFASTFEC_LSB    0x10
#define DIAG_FLAGS_LATCHED_FEINTLFEC_LSB    0x11
#define DIAG_FLAGS_LATCHED_FEFASTCRC_LSB    0x12
#define DIAG_FLAGS_LATCHED_FEINTLCRC_LSB    0x13
#define DIAG_FLAGS_LATCHED_FELOSDEFECT_LSB  0x14
#define DIAG_FLAGS_LATCHED_FELOSFAILURE_LSB 0x15
#define DIAG_FLAGS_LATCHED_FESEFDEFECT_LSB  0x16
#define DIAG_FLAGS_LATCHED_FESEFFAILURE_LSB 0x17
#define DIAG_FLAGS_LATCHED_FELPRDEFECT_LSB  0x18
#define DIAG_FLAGS_LATCHED_FELPRFAILURE_LSB 0x19

/*FLAG.mask CMV bit definitions, field masks & shift counts*/
#define FLAG_MASK_OFFSET        0x00

#define FLAG_MASK_UNMASKED      0x1
#define FLAG_MASK_MASKED        0x0

#define FLAG_MASK_NEFASTFEC_MASK        0x00000001
#define FLAG_MASK_NEINTLFEC_MASK        0x00000002
#define FLAG_MASK_NEFASTCRC_MASK        0x00000004
#define FLAG_MASK_NEINTLCRC_MASK        0x00000008
#define FLAG_MASK_NELOSDEFECT_MASK      0x00000010
#define FLAG_MASK_NELOSFAILURE_MASK     0x00000020
#define FLAG_MASK_NESEFDEFECT_MASK      0x00000040
#define FLAG_MASK_NESEFFAILURE_MASK     0x00000080
#define FLAG_MASK_NELPRDEFECT_MASK      0x00000100
#define FLAG_MASK_NELPRFAILURE_MASK     0x00000200
#define FLAG_MASK_NEMAILBOXFULL_MASK    0x00000400
#define FLAG_MASK_FEFASTFEC_MASK        0x00010000
#define FLAG_MASK_FEINTLFEC_MASK        0x00020000
#define FLAG_MASK_FEFASTCRC_MASK        0x00040000
#define FLAG_MASK_FEINTLCRC_MASK        0x00080000
#define FLAG_MASK_FELOSDEFECT_MASK      0x00100000
#define FLAG_MASK_FELOSFAILURE_MASK     0x00200000
#define FLAG_MASK_FESEFDEFECT_MASK      0x00400000
#define FLAG_MASK_FESEFFAILURE_MASK     0x00800000
#define FLAG_MASK_FELPRDEFECT_MASK      0x01000000
#define FLAG_MASK_FELPRFAILURE_MASK     0x02000000

#define FLAG_MASK_NEFASTFEC_LSB         0x00
#define FLAG_MASK_NEINTLFEC_LSB         0x01
#define FLAG_MASK_NEFASTCRC_LSB         0x02
#define FLAG_MASK_NEINTLCRC_LSB         0x03
#define FLAG_MASK_NELOSDEFECT_LSB       0x04
#define FLAG_MASK_NELOSFAILURE_LSB      0x05
#define FLAG_MASK_NESEFDEFECT_LSB       0x06
#define FLAG_MASK_NESEFFAILURE_LSB      0x07
#define FLAG_MASK_NELPRDEFECT_LSB       0x08
#define FLAG_MASK_NELPRFAILURE_LSB      0x09
#define FLAG_MASK_NEMAILBOXFULL_LSB     0x0a
#define FLAG_MASK_FEFASTFEC_LSB         0x10
#define FLAG_MASK_FEINTLFEC_LSB         0x11
#define FLAG_MASK_FEFASTCRC_LSB         0x12
#define FLAG_MASK_FEINTLCRC_LSB         0x13
#define FLAG_MASK_FELOSDEFECT_LSB       0x14
#define FLAG_MASK_FELOSFAILURE_LSB      0x15
#define FLAG_MASK_FESEFDEFECT_LSB       0x16
#define FLAG_MASK_FESEFFAILURE_LSB      0x17
#define FLAG_MASK_FELPRDEFECT_LSB       0x18
#define FLAG_MASK_FELPRFAILURE_LSB      0x19

/*TXDA.length CMV bit definitions, field masks & shift counts*/
#define TXDA_LENGTH_OFFSET      0x00
#define TXDA_DATA_OFFSET        0x01
#define TXDA_MAILBOX_OFFSET     0x0b

#define RXDA_LENGTH_OFFSET      0x00
#define RXDA_DATA_OFFSET        0x01
#define RXDA_MAILBOX_OFFSET     0x0b


/*OPTN.options CMV bit definitions, field masks & shift counts*/
#define OPTN_OPTIONS_OFFSET     0x00

#define OPTN_EC_AVAIL_MASK      0x00000001
#define OPTN_TCM_AVAIL_MASK     0x00000002
#define OPTN_FDQ_AVAIL_MASK     0x00000004
#define OPTN_EC_CONFIG_MASK     0x00000010
#define OPTN_TCM_CONFIG_MASK    0x00000020
#define OPTN_FDQ_CONFIG_MASK    0x00000040
#define OPTN_EC_INUSE_MASK      0x00000100
#define OPTN_TCM_INUSE_MASK     0x00000200
#define OPTN_FDQ_INUSe_MASK     0x00000400

#define OPTN_EC_AVAIL_LSB       0x00000001
#define OPTN_TCM_AVAIL_LSB      0x00000002
#define OPTN_FDQ_AVAIL_LSB      0x00000003
#define OPTN_EC_CONFIG_LSB      0x00000004
#define OPTN_TCM_CONFIG_LSB     0x00000005
#define OPTN_FDQ_CONFIG_LSB     0x00000006
#define OPTN_EC_INUSE_LSB       0x00000008
#define OPTN_TCM_INUSE_LSB      0x00000009
#define OPTN_FDQ_INUSE_LSB      0x0000000a

#endif /*_CMV_H_*/

/**************************************************
$Log: Cmv.h,v $
Revision 1.1  2004/02/06 22:01:34  sleeper
Initial creation

Revision 1.2  2003/09/14 22:09:00  sleeper
Various changes ( I know this is not a good reason :)

Revision 1.1.1.1  2003/02/10 23:29:49  sleeper
Imported sources

Revision 1.30  2002/05/24 21:59:30  Anoosh Naderi
Clean up the code.

Revision 1.2  2002/01/14 21:59:30  chris.edgington
Added GPL header.

Revision 1.1  2001/12/22 21:38:04  chris.edgington
Initial versions - from MacOS9 project tree.
**************************************************/

