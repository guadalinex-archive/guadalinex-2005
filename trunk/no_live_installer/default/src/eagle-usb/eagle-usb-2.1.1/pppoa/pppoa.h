/*
 * 
 * pppoa.h
 * 
 * Declaration of various PPPoA constants
 * 
 * Copyright (C) 2000 by Roaring Penguin Software Inc.
 * 
 * Implementation of user-space PPPoE redirector for Linux.
 * Modifiedandenhanced by Anoosh Naderi from ADI on May22,2002 to
 *  support PPPoA
 * 
 * Fix by Robert.Siemer-eagleusb@backsla.sh  22.8.2004:
 * Larger buffer for PPPoAPacket...
 *
 * This program may be distributed according to the terms of the GNU
 * General Public License, version 2 or (at your option) any later version.
 * 
 * LIC: GPL
 * 
 * $Id: pppoa.h,v 1.2 2004/08/26 20:47:54 sleeper Exp $
 * 
 */

#ifdef __sun__
#define __EXTENSIONS__
#endif

#include "config.h"

#if defined(HAVE_NETPACKET_PACKET_H) || defined(HAVE_LINUX_IF_PACKET_H)
#define _POSIX_SOURCE 1 /* For sigaction defines */
#endif

#include <stdio.h>		/* For FILE */
#include <sys/types.h>		/* For pid_t */
#include <stdint.h>		/* For uint16_t and uint32_t */

/* How do we access raw Ethernet devices? */
#undef USE_LINUX_PACKET
#undef USE_BPF

#if defined(HAVE_NETPACKET_PACKET_H) || defined(HAVE_LINUX_IF_PACKET_H)
#define USE_LINUX_PACKET 1
#elif defined(HAVE_SYS_DLPI_H)
#define USE_DLPI
#elif defined(HAVE_NET_BPF_H)
#define USE_BPF 1
#endif

/* Sanity check */
#if !defined(USE_BPF) && !defined(USE_LINUX_PACKET) && !defined(USE_DLPI)
#error Unknown method for accessing raw Ethernet frames
#endif

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

/* Ugly header files on some Linux boxes... */
#if defined(HAVE_LINUX_IF_H)
#include <linux/if.h>
#elif defined(HAVE_NET_IF_H)
#include <net/if.h>
#endif

#ifdef HAVE_NET_IF_TYPES_H
#include <net/if_types.h>
#endif

#ifdef HAVE_NET_IF_DL_H
#include <net/if_dl.h>
#endif

/* I'm not sure why this is needed... I do not have OpenBSD */
#if defined(__OpenBSD__)
#include <net/ppp_defs.h>
#include <net/if_ppp.h>
#endif

#ifdef USE_BPF
extern int bpfSize;
struct PPPoAPacketStruct;
#define BPF_BUFFER_IS_EMPTY (bpfSize <= 0)
#define BPF_BUFFER_HAS_DATA (bpfSize > 0)
#define ethhdr ether_header
#define h_dest ether_dhost
#define h_source ether_shost
#define h_proto ether_type
#define	ETH_DATA_LEN ETHERMTU
#define	ETH_ALEN ETHER_ADDR_LEN
#else
#undef USE_BPF
#define BPF_BUFFER_IS_EMPTY 1
#define BPF_BUFFER_HAS_DATA 0
#endif

#ifdef USE_DLPI
#include <sys/ethernet.h>
#define ethhdr ether_header
#define	ETH_DATA_LEN ETHERMTU
#define	ETH_ALEN ETHERADDRL
#define h_dest ether_dhost.ether_addr_octet
#define h_source ether_shost.ether_addr_octet
#define h_proto ether_type
/* cloned from dltest.h */
#define         MAXDLBUF        8192
#define         MAXDLADDR       1024
#define         MAXWAIT         15
#define         OFFADDR(s, n)   (u_char*)((char*)(s) + (int)(n))
#define         CASERET(s)      case s:  return ("s")
#endif

#ifdef HAVE_LINUX_IF_ETHER_H
#include <linux/if_ether.h>
#endif

#include <netinet/in.h>

#ifdef HAVE_NETINET_IF_ETHER_H
#include <sys/types.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifndef HAVE_SYS_DLPI_H
#include <netinet/if_ether.h>
#endif
#endif

/* Ethernet frame types according to RFC 2516 */
/* Since there is no UnSpec type in if-ether.h*/
/* therefore I used 0x0800 which is for ICMP  */
/* packets. I can useother typesas well       */
/*#define ETH_PPPOA_TYPE   0x0800*/
#define ETH_PPPOA_TYPE   0x0003

/* But some brain-dead peers disobey the RFC, so frame types are variables */
extern uint16_t PPPOA_Eth_Type;

/* States for scanning PPP frames */
#define STATE_WAITFOR_FRAME_ADDR 0
#define STATE_DROP_PROTO         1
#define STATE_BUILDING_PACKET    2

/* Special PPP frame characters */
#define FRAME_ESC    0x7D
#define FRAME_FLAG   0x7E
#define FRAME_ADDR   0xFF
#define FRAME_CTRL   0x03
#define FRAME_ENC    0x20

#define IPV4ALEN     4
#define SMALLBUF   256

/* PPPoA packet is a misleading term. It sounds something standard like
 * but is here in fact a unframed PPP packet with an 16 Byte "header" 
 * invented by the eagle-usb hardware driver.
 * The payload here is choosen in the hope that we never get something
 * bigger (we would drop the packet).
 * The maximum packet size of PPP transported over ATM AAL5 (PPPoA) is 
 * 65535 Bytes. Even in the case PPP is transporting only IP it could reach
 * that size (unnormally large IP packet).
 * As my ADSL PPP peer is denying (conformant) mru < 1500, I need to accept
 * at least 1502 Bytes. The same is true to conform to RFC 1661 (PPP).
 *   That's the reason I started to hack; someone set it here to 1500.
 *   - Robert
*/
/* A PPPoA Packet, including Ethernet headers */
typedef struct PPPoAPacketStruct {
    struct ethhdr ethHdr;	/* Ethernet header */
    unsigned int length:16;	/* Payload length */
    unsigned char payload[4000]; /* Artificial limit */
} PPPoAPacket;

#define PPPOA_OVERHEAD 2	/* Payload length */
#define HDR_SIZE (sizeof(struct ethhdr) + PPPOA_OVERHEAD)

/* Chunk to read from stdin */
#define READ_CHUNK 4096

#define PPPINITFCS16    0xffff  /* Initial FCS value */

/* Function Prototypes */
int OpenInterface(char const *ifname, uint16_t type, unsigned char *hwaddr);
int SendPacket(int sock, PPPoAPacket *pkt, int size);
int ReceivePacket(int sock, PPPoAPacket *pkt, int *size);
void FatalSys(char const *str);
void FatalErr(char const *str);
void printErr(char const *str);
void SysErr(char const *str);
void DumpPacket(FILE *fp, PPPoAPacket *packet, char const *dir);
void DumpHex(FILE *fp, unsigned char const *buf, int len);
void SyncReadFromPPP(PPPoAPacket *packet);
void AsyncReadFromPPP(PPPoAPacket *packet);
void AsyncReadFromEth(int sock, int clampMss);
void SyncReadFromEth(int sock, int clampMss);
char *StrDup(char const *str);
void SendPPPPacket(PPPoAPacket *packet, int len);
void InitPPP(void);
void PPPDataTransfer(void);
void DoClampMSS(PPPoAPacket *packet, char const *dir, int clampMss);
uint16_t ComputeTCPChecksum(unsigned char *ipHdr, unsigned char *tcpHdr);
uint16_t pppFCS16(uint16_t fcs, unsigned char *cp, int len);

#define SET_STRING(var, val) do { if (var) free(var); var = StrDup(val); } while(0);

#define CHECK_ROOM(cursor, start, len) \
do {\
    if (((cursor)-(start))+(len) > MAX_PPPOE_PAYLOAD) { \
        syslog(LOG_ERR, "Would create too-long packet"); \
        return; \
    } \
} while(0)

/* True if Ethernet address is broadcast or multicast */
#define NOT_UNICAST(e) ((e[0] & 0x01) != 0)
#define BROADCAST(e) ((e[0] & e[1] & e[2] & e[3] & e[4] & e[5]) == 0xFF)
#define NOT_BROADCAST(e) ((e[0] & e[1] & e[2] & e[3] & e[4] & e[5]) != 0xFF)
