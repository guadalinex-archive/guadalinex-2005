/*
 * 
 * if.c
 * 
 * Functions for opening a raw socket and reading/writing raw Ethernet frames.
 * 
 * Copyright (C) 2000 by Roaring Penguin Software Inc.
 * 
 * Implementation of user-space PPPoE redirector for Linux.
 * Modified and enhanced by Anoosh Naderi from ADI on May22,2002 to support
 * PPPoA
 * 
 * Fix by Robert.Siemer-eagleusb@backsla.sh  22.8.2004:
 * Drop packets from interface which seem bigger than buffer and syslog.
 *
 * This program may be distributed according to the terms of the GNU
 * General Public License, version 2 or (at your option) any later version
 * 
 * LIC: GPL
 * 
 */

static char const RCSID[] =
"$Id: if.c,v 1.2 2004/08/26 20:47:54 sleeper Exp $";

#include "pppoa.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_NETPACKET_PACKET_H
#include <netpacket/packet.h>
#elif defined(HAVE_LINUX_IF_PACKET_H)
#include <linux/if_packet.h>
#endif

#ifdef HAVE_NET_ETHERNET_H
#include <net/ethernet.h>
#endif

#ifdef HAVE_ASM_TYPES_H
#include <asm/types.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_NET_IF_ARP_H
#include <net/if_arp.h>
#endif

#ifdef USE_DLPI
#include <limits.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/dlpi.h>
#include <sys/bufmod.h>
#include <stdio.h>
#include <signal.h>
#include <stropts.h>
/* function declarations */
void dlpromisconreq( int fd, u_long  level);
void dlinforeq(int fd);
void dlunitdatareq(int fd, u_char *addrp, int addrlen, u_long minpri, u_long maxpri, u_char *datap, int datalen);
void dlinfoack(int fd, char *bufp);
void dlbindreq(int fd, u_long sap, u_long max_conind, u_long service_mode, u_long conn_mgmt, u_long xidtest);
void dlattachreq(int fd, u_long ppa);
void dlokack(int fd, char *bufp);
void dlbindack(int fd, char *bufp);
int strioctl(int fd, int cmd, int timout, int len, char *dp);
void strgetmsg(int fd, struct strbuf *ctlp, struct strbuf *datap, int *flagsp, char *caller);
void sigalrm(int sig);
void expecting(int prim, union DL_primitives *dlp);
char *dlprim(u_long prim);
/* #define DL_DEBUG */
static    int     dl_abssaplen;
static    int     dl_saplen;
static    int     dl_addrlen;
#endif

#ifdef USE_BPF
#include <net/bpf.h>
#include <fcntl.h>
unsigned char *bpfBuffer;    /* Packet filter buffer */
int bpfLength = 0;        /* Packet filter buffer length */
int bpfSize = 0;        /* Number of unread bytes in buffer */
int bpfOffset = 0;        /* Current offset in bpfBuffer */
#endif

/*Initialize frame types for PPPoA.Since there is no UnSpec value in*/
/*if-ether.h therefore I put 0x0800 which is for ICMP. Other values */
/*do work as well                                                   */
uint16_t PPPOA_Eth_Type   = ETH_PPPOA_TYPE;

#ifdef USE_BPF
/***********************************************************************/
/*%FUNCTION: GetHWaddr                               */
/*%ARGUMENTS:                                   */    
/* ifname -- name of interface                           */
/* hwaddr -- buffer for ehthernet address                   */    
/*%RETURNS:                                   */
/* Nothing                                   */
/*%DESCRIPTION:                                   */
/* Locates the Ethernet hardware address for an interface.              */    
/***********************************************************************/
void
GetHWaddr(int sock, char const *ifname, unsigned char *hwaddr)
{
    char inbuf[8192];
    const struct sockaddr_dl *sdl;
    struct ifconf ifc;
    struct ifreq ifreq, *ifr;
    int i;
    int found = 0;

    ifc.ifc_len = sizeof(inbuf);
    ifc.ifc_buf = inbuf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) < 0)
    {
        FatalSys("SIOCGIFCONF");
    }
    ifr = ifc.ifc_req;
    ifreq.ifr_name[0] = '\0';
    for (i = 0; i < ifc.ifc_len; )
    {
        ifr = (struct ifreq *)((caddr_t)ifc.ifc_req + i);
        i += sizeof(ifr->ifr_name) +
            (ifr->ifr_addr.sa_len > sizeof(struct sockaddr)
             ? ifr->ifr_addr.sa_len
             : sizeof(struct sockaddr));
        if (ifr->ifr_addr.sa_family == AF_LINK)
        {
            sdl = (const struct sockaddr_dl *) &ifr->ifr_addr;
            if ((sdl->sdl_type == IFT_ETHER) &&
                (sdl->sdl_alen == ETH_ALEN) &&
                !strncmp(ifname, ifr->ifr_name, sizeof(ifr->ifr_name)))
            {
                if (found)
                {
                    char buffer[256];
                    sprintf(buffer, "interface %.16s has more than one ethernet address", ifname);
                    FatalErr(buffer);
                }
                else
                {
                    found = 1;
                    memcpy(hwaddr, LLADDR(sdl), ETH_ALEN);
                }
            }
        }
    }
    if (!found)
    {
        char buffer[256];
        sprintf(buffer, "interface %.16s has no ethernet address", ifname);
        FatalErr(buffer);
    }
}

/***********************************************************************/
/*%FUNCTION: InitFilter                               */    
/*%ARGUMENTS:                                   */    
/* fd -- file descriptor of BSD device                       */    
/* type -- Ethernet frame type (0 for watch mode)               */    
/* hwaddr -- buffer with ehthernet address                   */    
/*%RETURNS:                                   */    
/* Nothing                                   */
/*%DESCRIPTION:                                   */    
/* Initializes the packet filter rules.                       */    
/***********************************************************************/
void
InitFilter(int fd, uint16_t type, unsigned char *hwaddr)
{
    /* Packet Filter Instructions:
     * Note that the ethernet type names come from "pppoe.h" and are
     * used here to maintain consistency with the rest of this file. */
    static struct bpf_insn bpfRun[] = {         /* run PPPoE */
        BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 12),     /* ethernet type */
        BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, ETH_PPPOA_TYPE, 5, 0),
        BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 0),      /* first word of dest. addr */
#define PPPOE_BCAST_CMPW 4                     /* offset of word compare */
        BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0, 0, 2),
        BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 4),      /* next 1/2 word of dest. */
#define PPPOE_BCAST_CMPH 6                     /* offset of 1/2 word compare */
        BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0, 4, 0),
        BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 0),      /* first word of dest. addr */
#define PPPOE_FILTER_CMPW 8                     /* offset of word compare */
        BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0, 0, 3),
        BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 4),      /* next 1/2 word of dest. */
#define PPPOE_FILTER_CMPH 10                    /* offset of 1/rd compare */
        BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0, 0, 1),
        BPF_STMT(BPF_RET+BPF_K, (u_int) -1),    /* keep packet */
        BPF_STMT(BPF_RET+BPF_K, 0),             /* drop packet */
    };

    /* Fix the potentially varying parts */
    bpfRun[1].code = (u_short) BPF_JMP+BPF_JEQ+BPF_K;
    bpfRun[1].jt   = 5;
    bpfRun[1].jf   = 0;
    bpfRun[1].k    = Eth_PPPOA_Type;

    {
        struct bpf_insn bpfInsn[sizeof(bpfRun) / sizeof(bpfRun[0])];
        struct bpf_program bpfProgram;
        memcpy(bpfInsn, bpfRun, sizeof(bpfRun));
        bpfInsn[PPPOE_BCAST_CMPW].k = ((0xff << 24) | (0xff << 16) |
                                       (0xff << 8) | 0xff);
        bpfInsn[PPPOE_BCAST_CMPH].k = ((0xff << 8) | 0xff);
        bpfInsn[PPPOE_FILTER_CMPW].k = ((hwaddr[0] << 24) | (hwaddr[1] << 16) |
                                        (hwaddr[2] << 8) | hwaddr[3]);
        bpfInsn[PPPOE_FILTER_CMPH].k = ((hwaddr[4] << 8) | hwaddr[5]);
        bpfProgram.bf_len = (sizeof(bpfInsn) / sizeof(bpfInsn[0]));
        bpfProgram.bf_insns = &bpfInsn[0];
      
        /* Apply the filter */
        if (ioctl(fd, BIOCSETF, &bpfProgram) < 0)
        {
            FatalSys("ioctl(BIOCSETF)");
        }
    }
}

/*************************************************************************/
/*%FUNCTION: OpenInterface                         */
/*%ARGUMENTS:                                 */
/* ifname -- name of interface                         */
/* type -- Ethernet frame type (0 for any frame type)             */
/* hwaddr -- if non-NULL, set to the hardware address             */     
/*%RETURNS:                                 */
/* A file descriptor for talking with the Ethernet card.  Exits on error.*/
/* Note that the Linux version of this routine returns a socket instead. */
/*%DESCRIPTION:                                 */
/* Opens a BPF on an interface for all PPPoE traffic (discovery and      */
/* session).  If 'type' is 0, uses promiscuous mode to watch any PPPoE   */
/* traffic on this network.                         */
/*************************************************************************/
int
OpenInterface(char const *ifname, uint16_t type, unsigned char *hwaddr)
{
    static int fd = -1;
    char bpfName[32];
    u_int optval;
    struct bpf_version bpf_ver;
    struct ifreq ifr;
    int sock;
    int i;

    /* BSD only opens one socket for both Discovery and Session packets */
    if (fd >= 0)
    {
        return fd;
    }

    /* Find a free BPF device */
    for (i = 0; i < 256; i++)
    {
        sprintf(bpfName, "/dev/bpf%d", i);
        if (((fd = open(bpfName, O_RDWR, 0)) >= 0) ||
            (errno != EBUSY))
        {
            break;
        }
    }
    if (fd < 0)
    {
        switch (errno)
        {
            case EACCES:        /* permission denied */
            {
                char buffer[256];
                sprintf(buffer, "Cannot open %.32s -- pppoa must be run as root.", bpfName);
                FatalErr(buffer);
            }
            break;
            case EBUSY:
            case ENOENT:        /* no such file */
                if (i == 0)
                {
                    FatalErr("No /dev/bpf* devices (check your kernel configuration for BPF support)");
                }
                else
                {
                    FatalErr("All /dev/bpf* devices are in use");
                }
                break;
        }
        FatalSys(bpfName);
    }

    if ((sock = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0)
    {
        FatalSys("socket");
    }

    /* Check that the interface is up */
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0)
    {
        FatalSys("ioctl(SIOCGIFFLAGS)");
    }
    if ((ifr.ifr_flags & IFF_UP) == 0)
    {
        char buffer[256];
        sprintf(buffer, "Interface %.16s is not up\n", ifname);
        FatalErr(buffer);
    }

    /* Fill in hardware address and initialize the packet filter rules */
    if (hwaddr == NULL)
    {
        FatalErr("openInterface: no hwaddr arg.");
    }
    GetHWaddr(sock, ifname, hwaddr);
    InitFilter(fd, type, hwaddr);

    /* Sanity check on MTU -- apparently does not work on OpenBSD */
#if !defined(__OpenBSD__)
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (ioctl(sock, SIOCGIFMTU, &ifr) < 0)
    {
        FatalSys("ioctl(SIOCGIFMTU)");
    }
    if (ifr.ifr_mtu < ETH_DATA_LEN)
    {
        char buffer[256];
        sprintf(buffer, "Interface %.16s has MTU of %d -- should be %d.  You may have serious connection problems.",
                ifname, ifr.ifr_mtu, ETH_DATA_LEN);
        printErr(buffer);
    }
#endif

    /* done with the socket */
    if (close(sock) < 0)
    {
        FatalSys("close");
    }

    /* Check the BPF version number */
    if (ioctl(fd, BIOCVERSION, &bpf_ver) < 0)
    {
        FatalSys("ioctl(BIOCVERSION)");
    }
    if ((bpf_ver.bv_major != BPF_MAJOR_VERSION) ||
        (bpf_ver.bv_minor < BPF_MINOR_VERSION))
    {
        char buffer[256];
        sprintf(buffer, "Unsupported BPF version: %d.%d (kernel: %d.%d)", 
                BPF_MAJOR_VERSION, BPF_MINOR_VERSION,
                bpf_ver.bv_major, bpf_ver.bv_minor);
        FatalErr(buffer);
    }

    /* allocate a receive packet buffer */
    if (ioctl(fd, BIOCGBLEN, &bpfLength) < 0)
    {
        FatalSys("ioctl(BIOCGBLEN)");
    }
    if (!(bpfBuffer = (unsigned char *) malloc(bpfLength)))
    {
        FatalErr("malloc");
    }

    /* reads should return as soon as there is a packet available */
    optval = 1;
    if (ioctl(fd, BIOCIMMEDIATE, &optval) < 0)
    {
        FatalSys("ioctl(BIOCIMMEDIATE)");
    }

    /* Bind the interface to the filter */
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (ioctl(fd, BIOCSETIF, &ifr) < 0)
    {
        char buffer[256];
        sprintf(buffer, "ioctl(BIOCSETIF) can't select interface %.16s",
                ifname);
        FatalErr(buffer);
    }

    syslog(LOG_INFO, "Interface=%.16s HWaddr=%02X:%02X:%02X:%02X:%02X:%02X Device=%.32s Buffer size=%d",
           ifname, 
           hwaddr[0], hwaddr[1], hwaddr[2],
           hwaddr[3], hwaddr[4], hwaddr[5],
           bpfName, bpfLength);
    return fd;
}
#endif /* USE_BPF */

#ifdef USE_LINUX_PACKET
/***********************************************************************/
/*%FUNCTION: OpenInterface                           */    
/*%ARGUMENTS:                                   */
/* ifname -- name of interface                           */    
/* type -- Ethernet frame type                           */
/* hwaddr -- if non-NULL, set to the hardware address               */    
/*%RETURNS:                                   */
/* A raw socket for talking to the Ethernet card.  Exits on error.     */    
/*%DESCRIPTION:                                   */    
/* Opens a raw Ethernet socket                           */    
/***********************************************************************/
int
OpenInterface(char const *ifname, uint16_t type, unsigned char *hwaddr)
{
    int optval=1;
    int fd;
    struct ifreq ifr;
    int domain, stype;

#ifdef HAVE_STRUCT_SOCKADDR_LL
    struct sockaddr_ll sa;
#else
    struct sockaddr sa;
#endif

    memset(&sa, 0, sizeof(sa));

#ifdef HAVE_STRUCT_SOCKADDR_LL
    domain = PF_PACKET;
    stype = SOCK_RAW;
#else
    domain = PF_INET;
    stype = SOCK_PACKET;
#endif

    if ((fd = socket(domain, stype, htons(type))) < 0)
    {
        /* Give a more helpful message for the common error case */
        if (errno == EPERM)
        {
            FatalErr("Cannot create raw socket -- pppoa must be run as root.");
        }
        FatalSys("socket");
    }

    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) < 0)
    {
        FatalSys("setsockopt");
    }

    /* Fill in hardware address */
    if (hwaddr)
    {
        strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
        if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0)
        {
            FatalSys("ioctl(SIOCGIFHWADDR)");
        }
        memcpy(hwaddr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
#ifdef ARPHRD_ETHER
        if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
        {
            char buffer[256];
            sprintf(buffer, "Interface %.16s is not Ethernet", ifname);
            FatalErr(buffer);
        }
#endif
        if (NOT_UNICAST(hwaddr))
        {
            char buffer[256];
            sprintf(buffer,
                    "Interface %.16s has broadcast/multicast MAC address??",
                    ifname);
            FatalErr(buffer);
        }
    }

    /* Sanity check on MTU */
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (ioctl(fd, SIOCGIFMTU, &ifr) < 0)
    {
        FatalSys("ioctl(SIOCGIFMTU)");
    }
    if (ifr.ifr_mtu < ETH_DATA_LEN)
    {
        char buffer[256];
        sprintf(buffer, "Interface %.16s has MTU of %d -- should be %d.  You may have serious connection problems.",
                ifname, ifr.ifr_mtu, ETH_DATA_LEN);
        printErr(buffer);
    }

#ifdef HAVE_STRUCT_SOCKADDR_LL
    /* Get interface index */
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(type);

    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0)
    {
        FatalSys("ioctl(SIOCFIGINDEX): Could not get interface index");
    }
    sa.sll_ifindex = ifr.ifr_ifindex;
#else
    strcpy(sa.sa_data, ifname);
#endif

    /* We're only interested in packets on specified interface */
    if (bind(fd, (struct sockaddr *) &sa, sizeof(sa)) < 0)
    {
        FatalSys("bind");
    }

    return fd;
}
#endif /* USE_LINUX */

/***********************************************************************/
/*%FUNCTION: SendPacket                               */    
/*%ARGUMENTS:                                   */
/* sock -- socket to send to                           */
/* pkt -- the packet to transmit                       */
/* size -- size of packet (in bytes)                       */
/*%RETURNS:                                   */    
/* 0 on success; -1 on failure                           */    
/*%DESCRIPTION:                                   */    
/* Transmits a packet                               */    
/***********************************************************************/
int
SendPacket(int sock, PPPoAPacket *pkt, int size)
{
#if defined(USE_BPF)
    if (write(sock, pkt, size) < 0)
        return -1;
#elif defined(HAVE_STRUCT_SOCKADDR_LL)
    if (send(sock, pkt, size, 0) < 0)
        return -1;
#else
#ifdef USE_DLPI
#define ABS(x)          ((x) < 0 ? -(x) : (x))

    u_char  addr[MAXDLADDR];
    u_char  phys[MAXDLADDR];
    u_char  sap[MAXDLADDR];
    u_char    xmitbuf[MAXDLBUF];
    int    data_size;
    short    tmp_sap;
    tmp_sap = htons(pkt->ethHdr.h_proto); 
    data_size = size - sizeof(struct ethhdr); 

    memcpy((char *)phys, (char *)pkt->ethHdr.h_dest, ETHERADDRL);
    memcpy((char *)sap,  (char *)&tmp_sap, sizeof(ushort_t));
    memcpy((char *)xmitbuf, (char *)pkt + sizeof(struct ethhdr), data_size); 

    if (dl_saplen > 0)
    {  /* order is sap+phys */
        (void) memcpy((char*)addr, (char*)&sap, dl_abssaplen);
        (void) memcpy((char*)addr+dl_abssaplen, (char*)phys, ETHERADDRL);
    }
    else
    {        /* order is phys+sap */
        (void) memcpy((char*)addr, (char*)phys, ETHERADDRL);
        (void) memcpy((char*)addr+ETHERADDRL, (char*)&sap, dl_abssaplen);
    }

#ifdef DL_DEBUG
    printf("%02x:%02x:%02x:%02x:%02x:%02x %02x:%02x\n", 
           addr[0],addr[1],addr[2],addr[3],addr[4],addr[5],
           addr[6],addr[7]);
#endif

    dlunitdatareq(sock, addr, dl_addrlen, 0, 0, xmitbuf, data_size);
#else
    struct sockaddr sa;
    strcpy(sa.sa_data, InterfaceName);
    if (sendto(sock, pkt, size, 0, &sa, sizeof(sa)) < 0)
        return -1;
#endif
#endif
    return 0;
}

#ifdef USE_BPF
/***********************************************************************/
/*%FUNCTION: ClearPacketHeader                           */    
/*%ARGUMENTS:                                   */
/* pkt -- packet that needs its head clearing                   */
/*%RETURNS:                                   */    
/* nothing                                   */
/*%DESCRIPTION:                                   */
/* Clears a PPPoE packet header after a truncated packet has been      */
/* received.  Insures that the packet will fail any integrity tests    */
/* and will be discarded by upper level routines.  Also resets the     */
/* bpfSize and bpfOffset variables to force a new read on the next     */        
/* call to receivePacket().                           */    
/***********************************************************************/
void
ClearPacketHeader(PPPoAPacket *pkt)
{
    bpfSize = bpfOffset = 0;
    memset(pkt, 0, HDR_SIZE);
}
#endif

/*
 * %FUNCTION: ReceivePacket    
 * %ARGUMENTS:    
 * sock -- socket to read from
 * pkt -- place to store the received packet    
 * size -- set to size of packet in bytes    
 * %RETURNS:
 * >= 0 if all OK; < 0 if error    
 * %DESCRIPTION:    
 * Receives a packet    
 */
int
ReceivePacket(int sock, PPPoAPacket *pkt, int *size)
{
    /* USE_BPF and USE_DLPI path look broken. First refers to PPPoEPacket,
     * second writes to pkt without caring about size... - Robert */
#ifdef USE_BPF
    struct bpf_hdr hdr;
    int seglen, copylen;

    if (bpfSize <= 0)
    {
        bpfOffset = 0;
        if ((bpfSize = read(sock, bpfBuffer, bpfLength)) < 0)
        {
            SysErr("read (ReceivePacket)");
            return -1;
        }
    }
    if (bpfSize < sizeof(hdr))
    {
        syslog(LOG_ERR, "Truncated bpf packet header: len=%d", bpfSize);
        ClearPacketHeader(pkt);        /* resets bpfSize and bpfOffset */
        return 0;
    }
    memcpy(&hdr, bpfBuffer + bpfOffset, sizeof(hdr));
    if (hdr.bh_caplen != hdr.bh_datalen)
    {
        syslog(LOG_ERR, "Truncated bpf packet: caplen=%d, datalen=%d",
               hdr.bh_caplen, hdr.bh_datalen);
        ClearPacketHeader(pkt);        /* resets bpfSize and bpfOffset */
        return 0;
    }
    seglen = hdr.bh_hdrlen + hdr.bh_caplen;
    if (seglen > bpfSize)
    {
        syslog(LOG_ERR, "Truncated bpf packet: seglen=%d, bpfSize=%d",
               seglen, bpfSize);
        ClearPacketHeader(pkt);        /* resets bpfSize and bpfOffset */
        return 0;
    }
    seglen = BPF_WORDALIGN(seglen);
    *size = copylen = ((hdr.bh_caplen < sizeof(PPPoEPacket)) ?
                       hdr.bh_caplen : sizeof(PPPoEPacket));
    memcpy(pkt, bpfBuffer + bpfOffset + hdr.bh_hdrlen, copylen);
    if (seglen >= bpfSize)
    {
        bpfSize = bpfOffset = 0;
    }
    else
    {
        bpfSize -= seglen;
        bpfOffset += seglen;
    }
#elif defined(USE_DLPI)
    
    struct strbuf data; 
    int flags = 0;     
    int retval; 

    data.buf = (char *) pkt; 
    data.maxlen = MAXDLBUF; 
    data.len = 0; 
    
    if ((retval = getmsg(sock, NULL, &data, &flags)) < 0)
    {
        SysErr("read (ReceivePacket)");
        return -1;
    }

    *size = data.len; 
#else
    if ((*size = recv(sock, pkt, sizeof(PPPoAPacket), 0)) < 0)
    {
        SysErr("recv (ReceivePacket)");
        return -1;
    }
#endif
    if (*size == sizeof(PPPoAPacket))
    {
	    syslog(LOG_ERR, "Buffer probably too small (%u); Packet from interface"
               " dropped! Lower mru of pppd or change pppoa source.",
               sizeof(PPPoAPacket));
	    return -1;
    }
    return 0;
}

#ifdef USE_DLPI
/**********************************************************************
 *%FUNCTION: OpenInterface
 *%ARGUMENTS:
 * ifname -- name of interface
 * type -- Ethernet frame type
 * hwaddr -- if non-NULL, set to the hardware address
 *%RETURNS:
 * A raw socket for talking to the Ethernet card.  Exits on error.
 *%DESCRIPTION:
 * Opens a raw Ethernet socket
 ***********************************************************************/
int
OpenInterface(char const *ifname, uint16_t type, unsigned char *hwaddr)
{
    int fd;
    long buf[MAXDLBUF]; 

    union   DL_primitives   *dlp;

    char base_dev[PATH_MAX]; 
    int ppa; 

    if(strlen(ifname) > PATH_MAX) {
        FatalErr("socket: Interface name too long"); 
    }

    if (strlen(ifname) < 2) {
        FatalErr("socket: Interface name too short");
    }

    ppa = atoi(&ifname[strlen(ifname)-1]);
    strncpy(base_dev, ifname, PATH_MAX); 
    base_dev[strlen(base_dev)-1] = '\0'; 

/* rearranged order of DLPI code - delphys 20010803 */
    dlp = (union DL_primitives*) buf;

    if (( fd = open(base_dev, O_RDWR)) < 0) {
        /* Give a more helpful message for the common error case */
        if (errno == EPERM) {
            FatalErr("Cannot create raw socket -- pppoa must be run as root.");
        }
        /* Common error is to omit /dev/ */
        if (errno == ENOENT) {
            char ifname[512];
            snprintf(ifname, sizeof(ifname), "/dev/%s", base_dev);
            if ((fd = open(ifname, O_RDWR)) < 0) {
                if (errno == EPERM) {
                    rp_fatal("Cannot create raw socket -- pppoe must be run as root.");
                }
            }
        }
    }
    if (fd < 0) {
        FatalSys("socket");
    }

/* rearranged order of DLPI code - delphys 20010803 */
    dlattachreq(fd, ppa); 
    dlokack(fd, (char *)buf);

    dlbindreq(fd, type, 0, DL_CLDLS, 0, 0);
    dlbindack(fd, (char *)buf);

    dlinforeq(fd);
    dlinfoack(fd, (char *)buf);

    dl_abssaplen = ABS(dlp->info_ack.dl_sap_length);
    dl_saplen = dlp->info_ack.dl_sap_length;
    if (ETHERADDRL != (dlp->info_ack.dl_addr_length - dl_abssaplen))
        FatalSys("invalid destination physical address length");
    dl_addrlen = dl_abssaplen + ETHERADDRL;

/* ethernet address retrieved as part of DL_INFO_ACK - delphys 20010803 */
    memcpy(hwaddr, (u_char*)((char*)(dlp) + (int)(dlp->info_ack.dl_addr_offset)), ETHERADDRL);

    if ( strioctl(fd, DLIOCRAW, -1, 0, NULL) < 0 ) { 
        FatalSys("DLIOCRAW"); 
    }

    if (ioctl(fd, I_FLUSH, FLUSHR) < 0) FatalSys("I_FLUSH");

    return fd;
}

/* cloned from dlcommon.c */

void dlpromisconreq(int fd, u_long level)
{
    dl_promiscon_req_t      promiscon_req;
    struct  strbuf  ctl;
    int     flags;

    promiscon_req.dl_primitive = DL_PROMISCON_REQ;
    promiscon_req.dl_level = level;

    ctl.maxlen = 0;
    ctl.len = sizeof (promiscon_req);
    ctl.buf = (char *) &promiscon_req;

    flags = 0;

    if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0)
        FatalSys("dlpromiscon:  putmsg");
}

void dlinforeq(int fd)
{
    dl_info_req_t   info_req;
    struct  strbuf  ctl;
    int     flags;

    info_req.dl_primitive = DL_INFO_REQ;

    ctl.maxlen = 0;
    ctl.len = sizeof (info_req);
    ctl.buf = (char *) &info_req;

    flags = RS_HIPRI;

    if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0)
        FatalSys("dlinforeq:  putmsg");
}

void dlunitdatareq(int fd, u_char *addrp, int addrlen, u_long minpri, u_long maxpri, u_char *datap, int datalen)
{
    long    buf[MAXDLBUF];
    union   DL_primitives   *dlp;
    struct  strbuf  data, ctl;

    dlp = (union DL_primitives*) buf;

    dlp->unitdata_req.dl_primitive = DL_UNITDATA_REQ;
    dlp->unitdata_req.dl_dest_addr_length = addrlen;
    dlp->unitdata_req.dl_dest_addr_offset = sizeof (dl_unitdata_req_t);
    dlp->unitdata_req.dl_priority.dl_min = minpri;
    dlp->unitdata_req.dl_priority.dl_max = maxpri;

    (void) memcpy(OFFADDR(dlp, sizeof (dl_unitdata_req_t)), addrp, addrlen);

    ctl.maxlen = 0;
    ctl.len = sizeof (dl_unitdata_req_t) + addrlen;
    ctl.buf = (char *) buf;

    data.maxlen = 0;
    data.len = datalen;
    data.buf = (char *) datap;

    if (putmsg(fd, &ctl, &data, 0) < 0)
        FatalSys("dlunitdatareq:  putmsg");
}

void dlinfoack(int fd, char *bufp)
{
    union   DL_primitives   *dlp;
    struct  strbuf  ctl;
    int     flags;

    ctl.maxlen = MAXDLBUF;
    ctl.len = 0;
    ctl.buf = bufp;

    strgetmsg(fd, &ctl, (struct strbuf*)NULL, &flags, "dlinfoack");

    dlp = (union DL_primitives *) ctl.buf;

    expecting(DL_INFO_ACK, dlp);

    if (ctl.len < sizeof (dl_info_ack_t)) {
        char buffer[256];
        sprintf(buffer, "dlinfoack:  response ctl.len too short:  %d", ctl.len); 
        FatalErr(buffer); 
    }

    if (flags != RS_HIPRI)
        FatalErr("dlinfoack:  DL_INFO_ACK was not M_PCPROTO");

    if (ctl.len < sizeof (dl_info_ack_t)) {
        char buffer[256];
        sprintf(buffer, "dlinfoack:  short response ctl.len:  %d", ctl.len); 
        FatalErr(buffer); 
    }
}

void dlbindreq(int fd, u_long sap, u_long max_conind, u_long service_mode, u_long conn_mgmt, u_long xidtest)
{
    dl_bind_req_t   bind_req;
    struct  strbuf  ctl;
    int     flags;

    bind_req.dl_primitive = DL_BIND_REQ;
    bind_req.dl_sap = sap;
    bind_req.dl_max_conind = max_conind;
    bind_req.dl_service_mode = service_mode;
    bind_req.dl_conn_mgmt = conn_mgmt;
    bind_req.dl_xidtest_flg = xidtest;

    ctl.maxlen = 0;
    ctl.len = sizeof (bind_req);
    ctl.buf = (char *) &bind_req;

    flags = 0;

    if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0)
        FatalSys("dlbindreq:  putmsg");
}

void dlattachreq(int fd, u_long ppa)
{
    dl_attach_req_t attach_req;
    struct  strbuf  ctl;
    int     flags;
    
    attach_req.dl_primitive = DL_ATTACH_REQ;
    attach_req.dl_ppa = ppa;
    
    ctl.maxlen = 0;
    ctl.len = sizeof (attach_req);
    ctl.buf = (char *) &attach_req;
    
    flags = 0;
    
    if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0)
        FatalSys("dlattachreq:  putmsg");
}

void dlokack(int fd, char *bufp)
{
    union   DL_primitives   *dlp;
    struct  strbuf  ctl;
    int     flags;
    
    ctl.maxlen = MAXDLBUF;
    ctl.len = 0;
    ctl.buf = bufp;
    
    strgetmsg(fd, &ctl, (struct strbuf*)NULL, &flags, "dlokack");
    
    dlp = (union DL_primitives *) ctl.buf;
    
    expecting(DL_OK_ACK, dlp);
    
    if (ctl.len < sizeof (dl_ok_ack_t)) { 
        char buffer[256];
        sprintf(buffer, "dlokack:  response ctl.len too short:  %d", ctl.len);
        FatalErr(buffer); 
    }
    
    if (flags != RS_HIPRI)
        FatalErr("dlokack:  DL_OK_ACK was not M_PCPROTO");
    
    if (ctl.len < sizeof (dl_ok_ack_t)) {
        char buffer[256]; 
        sprintf(buffer, "dlokack:  short response ctl.len:  %d", ctl.len);
        FatalErr(buffer); 
    }
}

void dlbindack(int fd, char *bufp)
{
    union   DL_primitives   *dlp;
    struct  strbuf  ctl;
    int     flags;
    
    ctl.maxlen = MAXDLBUF;
    ctl.len = 0;
    ctl.buf = bufp;
    
    strgetmsg(fd, &ctl, (struct strbuf*)NULL, &flags, "dlbindack");
    
    dlp = (union DL_primitives *) ctl.buf;
    
    expecting(DL_BIND_ACK, dlp);
    
    if (flags != RS_HIPRI)
        FatalErr("dlbindack:  DL_OK_ACK was not M_PCPROTO");
    
    if (ctl.len < sizeof (dl_bind_ack_t)) {
        char buffer[256];
        sprintf(buffer, "dlbindack:  short response ctl.len:  %d", ctl.len);
        FatalErr(buffer); 
    }
}

int strioctl(int fd, int cmd, int timout, int len, char *dp)
{
    struct  strioctl        sioc;
    int     rc;
    
    sioc.ic_cmd = cmd;
    sioc.ic_timout = timout;
    sioc.ic_len = len;
    sioc.ic_dp = dp;
    rc = ioctl(fd, I_STR, &sioc);
    
    if (rc < 0)
        return (rc);
    else
        return (sioc.ic_len);
}

void strgetmsg(int fd, struct strbuf *ctlp, struct strbuf *datap, int *flagsp, char *caller)
{
    int     rc;
    static  char    errmsg[80];
    
    /*
     * Start timer.
     */
    (void) signal(SIGALRM, sigalrm);
    if (alarm(MAXWAIT) < 0) {
        (void) sprintf(errmsg, "%s:  alarm", caller);
        FatalSys(errmsg);
    }
    
    /*
     * Set flags argument and issue getmsg().
     */
    *flagsp = 0;
    if ((rc = getmsg(fd, ctlp, datap, flagsp)) < 0) {
        (void) sprintf(errmsg, "%s:  getmsg", caller);
        FatalSys(errmsg);
    }
    
    /*
     * Stop timer.
     */
    if (alarm(0) < 0) {
        (void) sprintf(errmsg, "%s:  alarm", caller);
        FatalSys(errmsg);
    }
    
    /*
     * Check for MOREDATA and/or MORECTL.
     */
    if ((rc & (MORECTL | MOREDATA)) == (MORECTL | MOREDATA)) {
        char buffer[256]; 
        sprintf(buffer, "%s:  MORECTL|MOREDATA", caller);
        FatalErr(buffer);
    }
    
    if (rc & MORECTL) {
        char buffer[256];
        sprintf(buffer, "%s:  MORECTL", caller);
        FatalErr(buffer); 
    }
    
    if (rc & MOREDATA) {
        char buffer[256]; 
        sprintf(buffer, "%s:  MOREDATA", caller);
        FatalErr(buffer);
    }
    
    /*
     * Check for at least sizeof (long) control data portion.
     */
    if (ctlp->len < sizeof (long)) {
        char buffer[256]; 
        sprintf(buffer, "getmsg:  control portion length < sizeof (long):  %d", ctlp->len);
        FatalErr(buffer); 
    }
}

void sigalrm(int sig)
{
    (void) FatalErr("sigalrm:  TIMEOUT");
}

void expecting(int prim, union DL_primitives *dlp)
{
    if (dlp->dl_primitive != (u_long)prim) {
        char buffer[256]; 
        sprintf(buffer, "expected %s got %s", dlprim(prim), dlprim(dlp->dl_primitive));
        FatalErr(buffer); 
        exit(1); 
    }
}

char *dlprim(u_long prim)
{
    static  char    primbuf[80];
    
    switch ((int)prim) {
        CASERET(DL_INFO_REQ);
        CASERET(DL_INFO_ACK);
        CASERET(DL_ATTACH_REQ);
        CASERET(DL_DETACH_REQ);
        CASERET(DL_BIND_REQ);
        CASERET(DL_BIND_ACK);
        CASERET(DL_UNBIND_REQ);
        CASERET(DL_OK_ACK);
        CASERET(DL_ERROR_ACK);
        CASERET(DL_SUBS_BIND_REQ);
        CASERET(DL_SUBS_BIND_ACK);
        CASERET(DL_UNITDATA_REQ);
        CASERET(DL_UNITDATA_IND);
        CASERET(DL_UDERROR_IND);
        CASERET(DL_UDQOS_REQ);
        CASERET(DL_CONNECT_REQ);
        CASERET(DL_CONNECT_IND);
        CASERET(DL_CONNECT_RES);
        CASERET(DL_CONNECT_CON);
        CASERET(DL_TOKEN_REQ);
        CASERET(DL_TOKEN_ACK);
        CASERET(DL_DISCONNECT_REQ);
        CASERET(DL_DISCONNECT_IND);
        CASERET(DL_RESET_REQ);
        CASERET(DL_RESET_IND);
        CASERET(DL_RESET_RES);
        CASERET(DL_RESET_CON);
        default:
            (void) sprintf(primbuf, "unknown primitive 0x%lx", prim);
            return (primbuf);
    }
}

#endif /* USE_DLPI */