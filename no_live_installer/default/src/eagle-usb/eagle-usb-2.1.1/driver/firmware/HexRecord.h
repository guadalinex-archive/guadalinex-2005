/*
    Copyright (C) C. Casteyde, 2002.
    This file is in the public domain.
 */

#ifndef HEXRECORD_H
#define HEXRECORD_H

#define MAX_RECORD_LENGTH 16
typedef struct _INTEL_HEX_RECORD
{
    uint8_t	ui8Length;
    uint16_t	ui16Address;
    uint8_t	ui8Type;
    uint8_t	aData[MAX_RECORD_LENGTH];
} INTEL_HEX_RECORD, *PINTEL_HEX_RECORD;

#endif /* HEXRECORD_H */
