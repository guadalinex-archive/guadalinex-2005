/***************************************************************************/
/*									   */
/* common.c								   */
/*									   */
/* Implementation of user-space PPPoE redirector for Linux.		   */
/*									   */
/* Copyright (C) 2000 by Roaring Penguin Software Inc.			   */
/*									   */
/* Implementation of user-space PPPoE redirector for Linux.		   */
/* Modified and enhanced by Anoosh Naderi from ADI on May22,2002 to support*/
/* PPPoA								   */
/*									   */
/* This program may be distributed according to the terms of the GNU       */
/* General Public License, version 2 or (at your option) any later version.*/
/* 									   */
/* LIC: GPL								   */
/* 									   */
/***************************************************************************/

static char const RCSID[] =
"$Id: common.c,v 1.1 2004/02/06 22:26:38 sleeper Exp $";
/* For vsnprintf prototype */
#define _ISOC99_SOURCE 1

#include "pppoa.h"

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/***********************************************************************/
/*%FUNCTION: printErr						       */
/*%ARGUMENTS:							       */	
/* str -- error message						       */	
/*%RETURNS:							       */
/* Nothing							       */	
/*%DESCRIPTION:							       */	
/* Prints a message to stderr and syslog.			       */	
/***********************************************************************/
void
printErr(char const *str)
{
    fprintf(stderr, "pppoa: %s\n", str);
    syslog(LOG_ERR, "%s", str);
}

/***********************************************************************/		
/*%FUNCTION: strDup						       */
/*%ARGUMENTS:							       */	
/* str -- string to copy					       */	
/*%RETURNS:							       */
/* A malloc'd copy of str.  Exits if malloc fails.		       */	
/***********************************************************************/
char *
StrDup(char const *str)
{
    char *copy = malloc(strlen(str)+1);
    if (!copy)
    {
	FatalErr("strdup failed");
    }
    strcpy(copy, str);
    return copy;
}

/***********************************************************************/
/*%FUNCTION: ComputeTCPChecksum					       */	
/*%ARGUMENTS:						               */
/* ipHdr -- pointer to IP header				       */
/* tcpHdr -- pointer to TCP header				       */	
/*%RETURNS:							       */
/* The computed TCP checksum					       */	
/***********************************************************************/
uint16_t
ComputeTCPChecksum(unsigned char *ipHdr, unsigned char *tcpHdr)
{
    uint32_t sum = 0;
    uint16_t count = ipHdr[2] * 256 + ipHdr[3];
    unsigned char *addr = tcpHdr;
    unsigned char pseudoHeader[12];

    /* Count number of bytes in TCP header and data */
    count -= (ipHdr[0] & 0x0F) * 4;

    memcpy(pseudoHeader, ipHdr+12, 8);
    pseudoHeader[8] = 0;
    pseudoHeader[9] = ipHdr[9];
    pseudoHeader[10] = (count >> 8) & 0xFF;
    pseudoHeader[11] = (count & 0xFF);

    /* Checksum the pseudo-header */
    sum += * (uint16_t *) pseudoHeader;
    sum += * ((uint16_t *) (pseudoHeader+2));
    sum += * ((uint16_t *) (pseudoHeader+4));
    sum += * ((uint16_t *) (pseudoHeader+6));
    sum += * ((uint16_t *) (pseudoHeader+8));
    sum += * ((uint16_t *) (pseudoHeader+10));

    /* Checksum the TCP header and data */
    while (count > 1)
    {
	sum += * (uint16_t *) addr;
	addr += 2;
	count -= 2;
    }
    if (count > 0)
    {
	sum += *addr;
    }

    while(sum >> 16)
    {
	sum = (sum & 0xffff) + (sum >> 16);
    }
    return (uint16_t) (~sum & 0xFFFF);
}

/***********************************************************************/
/*%FUNCTION: DoClampMSS						       */
/*%ARGUMENTS:						               */
/* packet -- PPPoA packet			 	               */
/* dir -- either "incoming" or "outgoing"			       */
/* clampMss -- clamp value					       */
/*%RETURNS:							       */
/* Nothing							       */
/*%DESCRIPTION:							       */	
/* Clamps MSS option if TCP SYN flag is set.			       */	
/***********************************************************************/
void
DoClampMSS(PPPoAPacket *packet, char const *dir, int clampMss)
{
    unsigned char *tcpHdr;
    unsigned char *ipHdr;
    unsigned char *opt;
    unsigned char *endHdr;
    unsigned char *mssopt = NULL;
    uint16_t csum;

    int len, minlen;

    /* check PPP protocol type */
    if (packet->payload[0] & 0x01)
    {
        /* 8 bit protocol type */
        /* Is it IPv4? */
        if (packet->payload[0] != 0x21) {
            /* Nope, ignore it */
            return;
        }

        ipHdr = packet->payload + 1;
	minlen = 41;
    }
    else
    {
	/* 16 bit protocol type */
	/* Is it IPv4? */
	if (packet->payload[0] != 0x00 ||
	    packet->payload[1] != 0x21)
	{
	    /* Nope, ignore it */
	    return;
	}
    
	ipHdr = packet->payload + 2;
	minlen = 42;
    }

    /* Is it too short? */
    len = (int) ntohs(packet->length);
    if (len < minlen)
    {
	/* 20 byte IP header; 20 byte TCP header; at least 1 or 2 byte PPP protocol */
	return;
    }

    /* Verify once more that it's IPv4 */
    if ((ipHdr[0] & 0xF0) != 0x40)
    {
	return;
    }

    /* Is it a fragment that's not at the beginning of the packet? */
    if ((ipHdr[6] & 0x1F) || ipHdr[7])
    {
	/* Yup, don't touch! */
	return;
    }
    /* Is it TCP? */
    if (ipHdr[9] != 0x06)
    {
	return;
    }

    /* Get start of TCP header */
    tcpHdr = ipHdr + (ipHdr[0] & 0x0F) * 4;

    /* Is SYN set? */
    if (!(tcpHdr[13] & 0x02))
    {
	return;
    }

    /* Compute and verify TCP checksum -- do not touch a packet with a bad
       checksum */
    csum = ComputeTCPChecksum(ipHdr, tcpHdr);
    if (csum)
    {
	syslog(LOG_ERR, "Bad TCP checksum %x", (unsigned int) csum);

	/* Upper layers will drop it */
	return;
    }

    /* Look for existing MSS option */
    endHdr = tcpHdr + ((tcpHdr[12] & 0xF0) >> 2);
    opt = tcpHdr + 20;
    while (opt < endHdr)
    {
	if (!*opt) break;	/* End of options */
	switch(*opt) 
        {
	case 1:
	    opt++;
	    break;
	case 2:
	    if (opt[1] != 4)
            {
		/* Something fishy about MSS option length. */
		syslog(LOG_ERR,
		       "Bogus length for MSS option (%u) from %u.%u.%u.%u",
		       (unsigned int) opt[1],
		       (unsigned int) ipHdr[12],
		       (unsigned int) ipHdr[13],
		       (unsigned int) ipHdr[14],
		       (unsigned int) ipHdr[15]);
		return;
	    }
	    mssopt = opt;
	    break;
	default:
	    if (opt[1] < 2)
            {
		/* Someone's trying to attack us? */
		syslog(LOG_ERR,
		       "Bogus TCP option length (%u) from %u.%u.%u.%u",
		       (unsigned int) opt[1],
		       (unsigned int) ipHdr[12],
		       (unsigned int) ipHdr[13],
		       (unsigned int) ipHdr[14],
		       (unsigned int) ipHdr[15]);
		return;
	    }
	    opt += (opt[1]);
	    break;
	}
	/* Found existing MSS option? */
	if (mssopt) break;
    }

    /* If MSS exists and it's low enough, do nothing */
    if (mssopt)
    {
	unsigned mss = mssopt[2] * 256 + mssopt[3];
	if (mss <= clampMss)
        {
	    return;
	}

	mssopt[2] = (((unsigned) clampMss) >> 8) & 0xFF;
	mssopt[3] = ((unsigned) clampMss) & 0xFF;
    }
    else
    {
	/* No MSS option.  Don't add one; we'll have to use 536. */
	return;
    }

    /* Recompute TCP checksum */
    tcpHdr[16] = 0;
    tcpHdr[17] = 0;
    csum = ComputeTCPChecksum(ipHdr, tcpHdr);
    (* (uint16_t *) (tcpHdr+16)) = csum;
}
