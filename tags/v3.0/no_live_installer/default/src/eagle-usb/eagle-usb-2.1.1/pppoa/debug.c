/***************************************************************************/
/*									   */
/* debug.c								   */
/*									   */
/* Functions for printing debugging information				   */
/*									   */
/* Copyright (C) 2000 by Roaring Penguin Software Inc.			   */
/*									   */
/* Implementation of user-space PPPoE redirector for Linux.		   */
/* Modified and enhanced by Anoosh Naderi from ADI on May 22,2002 to	   */
/* support PPPoA							   */
/*									   */
/* This program may be distributed according to the terms of the GNU       */
/* General Public License, version 2 or (at your option) any later version.*/
/*									   */
/* LIC: GPL								   */
/*									   */
/***************************************************************************/

static char const RCSID[] =
"$Id: debug.c,v 1.1 2004/02/06 22:26:38 sleeper Exp $";

#include "pppoa.h"
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

/***********************************************************************/
/*%FUNCTION: DumpHex						       */
/*%ARGUMENTS:							       */	
/* fp -- file to dump to					       */
/* buf -- buffer to dump					       */	
/* len -- length of data					       */
/*%RETURNS:							       */	
/* Nothing							       */
/*%DESCRIPTION:							       */
/* Dumps buffer to fp in an easy-to-read format			       */	
/***********************************************************************/
void
DumpHex(FILE *fp, unsigned char const *buf, int len)
{
    int i;
    int base;

    if (!fp) return;

    /* do NOT dump PAP packets */
    if (len >= 2 && buf[0] == 0xC0 && buf[1] == 0x23)
    {
	fprintf(fp, "(PAP Authentication Frame -- Contents not dumped)\n");
	return;
    }

    for (base=0; base<len; base += 16)
    {
	for (i=base; i<base+16; i++)
        {
	    if (i < len)
            {
		fprintf(fp, "%02x ", (unsigned) buf[i]);
	    }
            else
            {
		fprintf(fp, "   ");
	    }
	}
	fprintf(fp, "  ");
	for (i=base; i<base+16; i++)
        {
	    if (i < len)
            {
		if (isprint(buf[i]))
                {
		    fprintf(fp, "%c", buf[i]);
		}
                else
                {
		    fprintf(fp, ".");
		}
	    }
            else
            {
		break;
	    }
	}
	fprintf(fp, "\n");
    }
}

/***********************************************************************/
/*%FUNCTION: DumpPacket						       */	
/*%ARGUMENTS:							       */
/* fp -- file to dump to					       */
/* packet -- a PPPoA packet					       */	
/* dir -- either SENT or RCVD					       */	
/*%RETURNS:							       */
/* Nothing							       */	
/*%DESCRIPTION:							       */
/* Dumps the PPPoA packet to fp in an easy-to-read format	       */
/***********************************************************************/
void
DumpPacket(FILE *fp, PPPoAPacket *packet, char const *dir)
{
    /* Sheesh... printing times is a pain... */
    struct timeval tv;
    time_t now;
    int millisec;
    struct tm *lt;
    char timebuf[256];

    if (!fp) return;
    gettimeofday(&tv, NULL);
    now = (time_t) tv.tv_sec;
    millisec = tv.tv_usec / 1000;
    lt = localtime(&now);
    strftime(timebuf, 256, "%H:%M:%S", lt);
    fprintf(fp, "%s.%03d %s PPPoA ", timebuf, millisec, dir);

    /* Ugly... I apologize... */
    fprintf(fp,
	    "SourceAddr %02x:%02x:%02x:%02x:%02x:%02x "
	    "DestAddr %02x:%02x:%02x:%02x:%02x:%02x\n",
	    (unsigned) packet->ethHdr.h_source[0],
	    (unsigned) packet->ethHdr.h_source[1],
	    (unsigned) packet->ethHdr.h_source[2],
	    (unsigned) packet->ethHdr.h_source[3],
	    (unsigned) packet->ethHdr.h_source[4],
	    (unsigned) packet->ethHdr.h_source[5],
	    (unsigned) packet->ethHdr.h_dest[0],
	    (unsigned) packet->ethHdr.h_dest[1],
	    (unsigned) packet->ethHdr.h_dest[2],
	    (unsigned) packet->ethHdr.h_dest[3],
	    (unsigned) packet->ethHdr.h_dest[4],
	    (unsigned) packet->ethHdr.h_dest[5]);
    DumpHex(fp, packet->payload, ntohs(packet->length));
}
