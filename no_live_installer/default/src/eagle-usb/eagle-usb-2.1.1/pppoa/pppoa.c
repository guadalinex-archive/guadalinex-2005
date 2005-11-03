/*
 * 
 * pppoa.c
 * 
 * Copyright (C) 2000 by Roaring Penguin Software Inc.
 * 
 * Implementation of user-space PPPoE redirector for Linux.
 * Modified and enhanced by Anoosh Naderi from ADI on May22,2002
 * This program supports PPPoA in user space using a ADI USB ADSL LAN driver
 *
 * Fix by Robert.Siemer-eagleusb@backsla.sh  22.8.2004:
 * If we check what the driver is telling us, please exactly.
 * 
 * This program may be distributed according to the terms of the GNU
 * General Public License, version 2 or (at your option) any later version.
 * 
 * LIC: GPL
 * 
 */

static char const RCSID[] =
"$Id: pppoa.c,v 1.3 2004/09/12 19:52:10 sleeper Exp $";

#include "pppoa.h"

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef USE_LINUX_PACKET
#include <sys/ioctl.h>
#include <fcntl.h>
#endif

#include <signal.h>

#ifdef HAVE_N_HDLC
#ifndef N_HDLC
#include <linux/termios.h>
#endif
#endif

/* Default interface if no -I option given */
#define DEFAULT_IF "eth0"

/* Some Global variables */
#define DEFAULT_TIMEOUT 250
int InactivityTimeout = DEFAULT_TIMEOUT;	/* Inactivity timeout */
volatile int EndSession = 0;	/* Session termination on SIGHUP */

/***********************************************************/
/*If we havea single PC connected to modem the clamp MSS   */
/*is not useful but if we have PC acting as gateway then we*/
/*should use clamp MSS and best value is 1412 and this     */
/*value is safe for either setup                           */  
int ClampMSS = 0;

int PPPSocket;
unsigned char MyEth[ETH_ALEN];
unsigned char PeerEth[ETH_ALEN];
char *InterfaceName;
int SynchronousFlag;
FILE *DebugFile;

static const unsigned char PPP_ETH_ADDR_SRC[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/*
 * %FUNCTION: SendPPPPacket
 * %ARGUMENTS:	
 * packet -- the packet to send	
 * len -- length of data to send	
 * %RETURNS:	
 * Nothing
 * %DESCRIPTION:	
 * Transmits a session packet to the peer.	
 */
void
SendPPPPacket(PPPoAPacket *packet, int len)
{
    packet->length = htons(len);
    if (ClampMSS)
    {
        DoClampMSS(packet, "outgoing", ClampMSS);
    }
    if (SendPacket(PPPSocket, packet, len + HDR_SIZE) < 0)
    {
        if (errno == ENOBUFS)
        {
            /* No buffer space is a transient error */
            return;
        }
        else
        {
            char errorstr[1024];
        
            sprintf(errorstr, "sendto (SendPPPPacket size: %d)", len);
            SysErr(errorstr);
            exit(EXIT_FAILURE);
        }
    }
    if (DebugFile)
    {
        DumpPacket(DebugFile, packet, "SENT");
        fprintf(DebugFile, "\n");
        fflush(DebugFile);
    }
}


/*
 *%FUNCTION: sigEndSession
 *%ARGUMENTS:
 * src -- signal received
 *%RETURNS:
 * Nothing
 *%DESCRIPTION:
 * If an established session exists marks the flag to terminate it.
 */
void sigEndSession(int src)
{
    EndSession = 1;
}


/*
 * %FUNCTION: PPPDataTransfer	
 * %ARGUMENTS:	
 * NONE		
 * %RETURNS:	
 * Nothing
 * %DESCRIPTION:	
 * Handles the sending and receiving PPP data		
 */
void
PPPDataTransfer()
{
    fd_set readable;
    PPPoAPacket packet;
    struct timeval tv;
    struct timeval *tvp = NULL;
    int maxFD = 0;
    int r;
    int EndTransfer = 0;

    /*
     * Open a session socket
     *     
     * The second parameter in OpenInterface fuction call defines the type of
     * ethernet packet that we send. It has to be matched with one in Pipe.c in
     * source code of USB driver. Aslong asthey match, it works bur it is better
     * to use something that makes sense for example 0x080 is for ICMP and
     * 0x0003 isfor every packet and ... all of them are in
     * /usr/src/linux/include/linux/if_ether.h. It isgood to have something like
     * unspecified in that filethat we do not, and also I can use some value
     * like 0xFFFF for now or use 0x0800 or 0x0003 for now.
     */
    PPPSocket = OpenInterface(InterfaceName, PPPOA_Eth_Type, MyEth);

    /* Prepare for select() */
    if (PPPSocket > maxFD)   maxFD = PPPSocket;
    maxFD++;

    PeerEth[0]=0xFF;
    PeerEth[1]=0xFF;
    PeerEth[2]=0xFF;
    PeerEth[3]=0xFF;
    PeerEth[4]=0xFF;
    PeerEth[5]=0xFF;
    memcpy(packet.ethHdr.h_dest, PeerEth, ETH_ALEN);
    memcpy(packet.ethHdr.h_source, MyEth, ETH_ALEN);
    packet.ethHdr.h_proto = htons(PPPOA_Eth_Type);

    InitPPP();

    /* Set-up signal handlers: */
    signal(SIGTERM, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGHUP, sigEndSession);

#ifdef USE_BPF
    /* check for buffered session data */
    while (BPF_BUFFER_HAS_DATA)
    {
        if (SynchronousFlag)
        {
            SyncReadFromEth(PPPSocket, ClampMSS);
        }
        else
        {
            AsyncReadFromEth(PPPSocket, ClampMSS);
        }
    }
#endif

    FD_ZERO(&readable);
    do
    {
        if (InactivityTimeout > 0)
        {
            tv.tv_sec = InactivityTimeout / 1000;
            tv.tv_usec = InactivityTimeout % 1000;
            tvp = &tv;
        }
        FD_SET(0, &readable);     /* ppp packets come from stdin */
        FD_SET(PPPSocket, &readable);
        while(1)
        {
            r = select(maxFD, &readable, NULL, NULL, tvp);
            if (r >= 0 || errno != EINTR) break;
        }
        if (r < 0)
        {
            FatalSys("select (session)");
        }
        if (r == 0)
        {
            /* Inactivity timeout, let's see if we must leave: */
            EndTransfer = EndSession;
        }

        /* Otherwise, handle ready sockets */
        if (FD_ISSET(0, &readable))
        {
            if (SynchronousFlag)
            {
                SyncReadFromPPP(&packet);
            } else {
                AsyncReadFromPPP(&packet);
            }
        }

        if (FD_ISSET(PPPSocket, &readable))
        {
            do
            {
                if (SynchronousFlag)
                {
                    SyncReadFromEth(PPPSocket, ClampMSS);
                }
                else
                {
                    AsyncReadFromEth(PPPSocket, ClampMSS);
                }
            } while (BPF_BUFFER_HAS_DATA);
        }
    }
    while (!EndTransfer);
}

/*
 * %FUNCTION: usage	
 * %ARGUMENTS:	
 * argv0 -- program name	
 * %RETURNS:	
 * Nothing				
 * %DESCRIPTION:	
 * Prints usage information and exits.	
 */
void
usage(char const *argv0)
{
    fprintf(stderr, "Usage: %s [options]\n", argv0);
    fprintf(stderr, "Options:\n");
#ifdef USE_BPF
    fprintf(stderr, "   -I if_name     -- Specify interface (REQUIRED)\n");
#else
    fprintf(stderr, "   -I if_name     -- Specify interface (default %s.)\n",DEFAULT_IF);
#endif
    fprintf(stderr, "   -T timeout     -- Specify inactivity timeout in milliseconds.\n");
    fprintf(stderr, "   -D filename    -- Log debugging information in filename.\n");
    fprintf(stderr, "   -V             -- Print version and exit.\n");
    fprintf(stderr, "   -s             -- Use synchronous PPP encapsulation.\n");
    fprintf(stderr, "   -m MSS         -- Clamp incoming and outgoing MSS options.\n");
    fprintf(stderr, "   -f sess        -- Set Ethernet frame types (hex).\n");
    fprintf(stderr, "   -h             -- Print usage information.\n\n");
    fprintf(stderr, "This is free software, and you are welcome to redistribute it under the terms\n");
    fprintf(stderr, "of the GNU General Public License, version 2 or any later version.\n");
    exit(EXIT_SUCCESS);
}

/*
 * %FUNCTION: main	
 * %ARGUMENTS:	
 * argc, argv -- count and values of command-line arguments
 * %RETURNS:
 * Nothing
 * %DESCRIPTION:	
 * Main program	
 */
int
main(int argc, char *argv[])
{
    int opt;
    unsigned int PPPEthType;

    PPPSocket = -1;
    SynchronousFlag=0;

    /* Initialize syslog */
    openlog("pppoa", LOG_PID, LOG_DAEMON);

    while((opt = getopt(argc, argv, "I:VT:D:h:sm:f:")) != -1)
    {
        switch(opt)
        {
            case 'f':
                if (sscanf(optarg, "%x", &PPPEthType) != 1)
                {
                    fprintf(stderr, "Illegal argument to -f: Should be in hex\n");
                    exit(EXIT_FAILURE);
                }
                PPPOA_Eth_Type  = (uint16_t) PPPEthType;
                break;
            case 's':
                SynchronousFlag = 1;
                break;
            case 'D':
                DebugFile = fopen(optarg, "w");
                if (!DebugFile)
                {
                    fprintf(stderr, "Could not open %s: %s\n",optarg, strerror(errno));
                    exit(EXIT_FAILURE);
                }
                fprintf(DebugFile, "pppoa-%s\n", VERSION);
                fflush(DebugFile);
                break;
            case 'T':
                InactivityTimeout = (int) strtol(optarg, NULL, 10);
                if (InactivityTimeout <= 0)
                {
                    InactivityTimeout = DEFAULT_TIMEOUT;
                }
                break;
            case 'm':
                ClampMSS = (int) strtol(optarg, NULL, 10);
                if (ClampMSS < 536)
                {
                    fprintf(stderr, "-m: %d is too low (min 536)\n", ClampMSS);
                    exit(EXIT_FAILURE);
                }
                if (ClampMSS > 1452)
                {
                    fprintf(stderr, "-m: %d is too high (max 1452)\n", ClampMSS);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'I':
                SET_STRING(InterfaceName, optarg);
                break;
            case 'V':
                printf("PPPoA Version %s\n", VERSION);
                exit(EXIT_SUCCESS);
            case 'h':
                usage(argv[0]);
                break;
            default:
                usage(argv[0]);
        }
    }

    /* Pick a default interface name */
    if (!InterfaceName)
    {
#ifdef USE_BPF
        fprintf(stderr, "No interface specified (-I option)\n");
        exit(EXIT_FAILURE);
#else
        SET_STRING(InterfaceName, DEFAULT_IF);
#endif
    }

    PPPDataTransfer();

    return 0;
}

/*
 * %FUNCTION: FatalSys
 * %ARGUMENTS:
 * str -- error message
 * %RETURNS:
 * Nothing
 * %DESCRIPTION:
 * Prints a message plus the errno value to stderr and syslog and exits.
 */
void
FatalSys(char const *str)
{
    char buf[1024];
    sprintf(buf, "%.256s: %.256s", str, strerror(errno));
    printErr(buf);
    exit(EXIT_FAILURE);
}

/*
 * %FUNCTION: SysErr	
 * %ARGUMENTS:	
 * str -- error message	
 * %RETURNS:
 * Nothing	
 * %DESCRIPTION:	
 * Prints a message plus the errno value to syslog.	
 */
void
SysErr(char const *str)
{
    char buf[1024];
    sprintf(buf, "%.256s: %.256s", str, strerror(errno));
    printErr(buf);
}

/*
 * %FUNCTION: FatalErr	
 * %ARGUMENTS:	
 * str -- error message	
 * %RETURNS:	
 * Nothing	
 * %DESCRIPTION:	
 * Prints a message to stderr and syslog and exits.	
 */
void
FatalErr(char const *str)
{
    char buf[1024];
    printErr(str);
    sprintf(buf, "PPPOA: %.256s", str);
    exit(EXIT_FAILURE);
}

/*
 * %FUNCTION: AsyncReadFromEth	
 * %ARGUMENTS:	
 * sock -- Ethernet socket	
 * clampMss -- if non-zero, do MSS-clamping	
 * %RETURNS:	
 * Nothing	
 * %DESCRIPTION:	
 * Reads a packet from the Ethernet interface and sends it to async PPP
 * device.		
 */
void
AsyncReadFromEth(int sock, int ClampMss)
{
    PPPoAPacket packet;
    int len;
    int plen;
    int i;
    unsigned char pppBuf[4096];
    unsigned char *ptr = pppBuf;
    unsigned char c;
    uint16_t fcs;
    unsigned char header[2] = {FRAME_ADDR, FRAME_CTRL};
    unsigned char tail[2];

    if (ReceivePacket(sock, &packet, &len) < 0)
    {
        return;
    }

    /*
     * Check that mac source is driver
     */
    if ( memcmp ( packet.ethHdr.h_source, PPP_ETH_ADDR_SRC, ETH_ALEN ) != 0 ) 
    {
        syslog(LOG_ERR,
               "Packet not from driver (mac: %02X:%02X:%02X:%02X:%02X:%02X)",
               packet.ethHdr.h_source[0],
               packet.ethHdr.h_source[1],
               packet.ethHdr.h_source[2],
               packet.ethHdr.h_source[3],
               packet.ethHdr.h_source[4],
               packet.ethHdr.h_source[5]);
        return;
    }
    
    
    if (DebugFile)
    {
        DumpPacket(DebugFile, &packet, "RCVD");
        fprintf(DebugFile, "\n");
        fflush(DebugFile);
    }
    plen = ntohs(packet.length);
    if (plen + HDR_SIZE != len)
    {
        syslog(LOG_ERR, "Wrong length field in incoming data %d (%d)",(int) plen, (int) len);
        return;
    }

    /* Clamp MSS */
    if (ClampMss)
    {
        DoClampMSS(&packet, "incoming", ClampMss);
    }

    /* Compute FCS */
    fcs = pppFCS16(PPPINITFCS16, header, 2);
    fcs = pppFCS16(fcs, packet.payload, plen) ^ 0xffff;
    tail[0] = fcs & 0x00ff;
    tail[1] = (fcs >> 8) & 0x00ff;

    /* Build a buffer to send to PPP */
    *ptr++ = FRAME_FLAG;
    *ptr++ = FRAME_ADDR;
    *ptr++ = FRAME_ESC;
    *ptr++ = FRAME_CTRL ^ FRAME_ENC;

    for (i=0; i<plen; i++)
    {
        c = packet.payload[i];
        if (c == FRAME_FLAG || c == FRAME_ADDR || c == FRAME_ESC || c < 0x20)
        {
            *ptr++ = FRAME_ESC;
            *ptr++ = c ^ FRAME_ENC;
        }
        else
        {
            *ptr++ = c;
        }
    }
    for (i=0; i<2; i++)
    {
        c = tail[i];
        if (c == FRAME_FLAG || c == FRAME_ADDR || c == FRAME_ESC || c < 0x20)
        {
            *ptr++ = FRAME_ESC;
            *ptr++ = c ^ FRAME_ENC;
        }
        else
        {
            *ptr++ = c;
        }
    }
    *ptr++ = FRAME_FLAG;

    /* Ship it out */
    if (write(1, pppBuf, (ptr-pppBuf)) < 0) {
        FatalSys("AsyncReadFromEth: write");
    }
}

/*
 * %FUNCTION: SyncReadFromEth	
 * %ARGUMENTS:	
 * sock -- Ethernet socket	 	
 * clampMss -- if true, clamp MSS.	
 * %RETURNS:	
 * Nothing
 * %DESCRIPTION:
 * Reads a packet from the Ethernet interface and sends it to sync PPP 
 * device.	
 */
void
SyncReadFromEth(int sock, int ClampMss)
{
    PPPoAPacket packet;
    int len;
    int plen;
    struct iovec vec[2];
    unsigned char dummy[2];

    if (ReceivePacket(sock, &packet, &len) < 0)
    {
        return;
    }

    /*
     * Check that mac source is driver
     */
    if ( memcmp ( packet.ethHdr.h_source, PPP_ETH_ADDR_SRC, ETH_ALEN ) != 0 ) 
    {
        syslog(LOG_ERR,
               "Packet not from driver (mac: %02X:%02X:%02X:%02X:%02X:%02X)",
               packet.ethHdr.h_source[0],
               packet.ethHdr.h_source[1],
               packet.ethHdr.h_source[2],
               packet.ethHdr.h_source[3],
               packet.ethHdr.h_source[4],
               packet.ethHdr.h_source[5]);
        return;
    }

    
    /* Check length */
    plen = ntohs(packet.length);
    if (plen + HDR_SIZE != len)
    {
        syslog(LOG_ERR, "Wrong length field in incoming data %d (%d)",
               (int) plen, (int) len);
        return;
    }

    if (DebugFile)
    {
        DumpPacket(DebugFile, &packet, "RCVD");
        fprintf(DebugFile, "\n");
        fflush(DebugFile);
    }

    /* Clamp MSS */
    if (ClampMss)
    {
        DoClampMSS(&packet, "incoming", ClampMss);
    }

    /* Ship it out */
    vec[0].iov_base = (void *) dummy;
    dummy[0] = FRAME_ADDR;
    dummy[1] = FRAME_CTRL;
    vec[0].iov_len = 2;
    vec[1].iov_base = (void *) packet.payload;
    vec[1].iov_len = plen;

    if (writev(1, vec, 2) < 0)
    {
        FatalSys("SyncReadFromEth: write");
    }
}

