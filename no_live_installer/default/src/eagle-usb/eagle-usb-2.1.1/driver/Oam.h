
/* $Id: Oam.h,v 1.1 2004/02/06 22:01:34 sleeper Exp $ */

//---------------------------------------------------------------------------
//
// Copyright (c) 2002, Analog Devices Inc., All Rights Reserved
//
// Oam.h
//
// Operations And Maintenance ATM support module
//
//---------------------------------------------------------------------------

void OAMTimerFunction(unsigned long ptr);

// Offsets into cell
#define OAM_TYPE_AND_FN         (ATM_CELL_HEADER_SIZE+0)  
#define OAM_MSGID_AND_DIR       (OAM_TYPE_AND_FN+1)
#define OAM_LOOPBACK_INDICATOR  (OAM_TYPE_AND_FN+1)
#define OAM_CRC10_0             (ATM_CELL_SIZE-2)
#define OAM_CRC10_1             (ATM_CELL_SIZE-1)
#define OAM_FIRST_UNUSED_BYTE   (OAM_TYPE_AND_FN+1)
#define OAM_LAST_UNUSED_BYTE    (ATM_CELL_SIZE-3)

// Function and type definitions
#define OAM_ACTIVATION_CC       0x81
#define OAM_LOOPBACK            0x18
#define OAM_AIS                 0x10
#define OAM_RDI                 0x11
#define OAM_CC                  0x14

// Masks / values for various fields
#define OAM_MASK_MSGID          0xFC
#define OAM_MASK_DIR            0x03
#define OAM_MASK_DIR_AB         0x01
#define OAM_MASK_DIR_BA         0x02
#define OAM_LOOPBACK_REPLY_MASK 0x01
#define OAM_ACTIVATE_REQUEST            0x04
#define OAM_ACTIVATE_CONFIRMED          0x08
#define OAM_ACTIVATE_REQUEST_DENIED     0x0C
#define OAM_DEACTIVATE_REQUEST          0x14
#define OAM_DEACTIVATE_CONFIRMED        0x18
#define OAM_DEACTIVATE_REQUEST_DENIED   0x1C

// OAM cell types
#define OAM_NONE                0x00
#define OAM_F4_SEGMENT          0x01
#define OAM_F4_ENDTOEND         0x02
#define OAM_F5_SEGMENT          0x03
#define OAM_F5_ENDTOEND         0x04

#define OAM_TIMER_VALUE         1000 // 1000 msecs = 1 sec

// Macros
#define CALC_AND_STORE_CRC10(CellPtr) \
    { \
        UInt16 OAMCRC10; \
        CellPtr[OAM_CRC10_0] = 0; \
        CellPtr[OAM_CRC10_1] = 0; \
        OAMCRC10 = ByteCalcCRC10((UInt8 *)(CellPtr+ATM_CELL_HEADER_SIZE), \
                                 ATM_CELL_PAYLOAD_SIZE); \
        CellPtr[OAM_CRC10_0] ^= (OAMCRC10 >> 8); \
        CellPtr[OAM_CRC10_1] ^= (OAMCRC10 & 0xFF); \
    }

/*
$Log: Oam.h,v $
Revision 1.1  2004/02/06 22:01:34  sleeper
Initial creation

Revision 1.2  2003/09/14 22:09:00  sleeper
Various changes ( I know this is not a good reason :)

Revision 1.1  2003/03/11 00:42:39  sleeper
Initial version

Revision 1.4  2002/07/25 15:54:16  chris.edgington
Fixed reversed bits for AB and BA CC activation - fixes last OAM bug.

Revision 1.3  2002/07/19 17:14:33  chris.edgington
Fixed compile errors.

Revision 1.2  2002/07/19 16:46:28  chris.edgington
Added LOOPBACK defines.

Revision 1.1  2002/07/18 20:37:11  chris.edgington
First version.


*/



