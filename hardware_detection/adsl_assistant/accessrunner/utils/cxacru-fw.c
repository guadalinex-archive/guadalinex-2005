/*****************************************************************************
 *  cxacru-fw -	utility to extract firmware for the USB ADSL modems based on
 *		Conexant AccessRunner chipset, from the Conexant driver for
 *		Windows(R)
 *
 *  Copyright (C) 2005	Roman Kagan (rkagan % mail ! ru)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ****************************************************************************/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <error.h>
#include <argp.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <endian.h>
#include <byteswap.h>

#if __BYTE_ORDER == __BIG_ENDIAN
static inline uint16_t le16_to_cpup(uint16_t *x) {return bswap_16(*x);}
static inline uint32_t le32_to_cpup(uint32_t *x) {return bswap_32(*x);}
#else
static inline uint16_t le16_to_cpup(uint16_t *x) {return *x;}
static inline uint32_t le32_to_cpup(uint32_t *x) {return *x;}
#endif

uint8_t *findfw(uint8_t *buf, int len, int *fwlen)
{
	const static uint8_t fwstart[] = {	/*		ARM LE asm				*/
		0x1c, 0x24, 0x9f, 0xe5,		/*	e59f241c	ldr	r2, [pc, #0x41c]	*/
		0x00, 0x10, 0xa0, 0xe3,		/*	e3a01000	mov	r1, #0x0		*/
		0x00, 0x10, 0x82, 0xe5,		/*	e5821000	str	r1, [r2]		*/
		0x22, 0x00, 0x00, 0xeb,		/*	eb000022	bl	0x9c			*/
	};

	const static uint8_t bpstart[] = {
		0x78, 0x20, 0x9f, 0xe5,		/*	e59f2078	ldr	r2, [pc, #0x78]	*/
		0x01, 0x10, 0xa0, 0xe3,		/*	e3a01001	mov	r1, #0x1	*/
		0x00, 0x10, 0x82, 0xe5,		/*	e5821000	str	r1, [r2]		*/
		0x70, 0x20, 0x9f, 0xe5,		/*	e59f2070	ldr	r2, [pc, #0x70]	*/
	};

	uint8_t *fw, *bp, *nt_header, *sect_header;
	uint32_t tmp;
	uint16_t nsect;
	uint32_t dataoff, datalen;

	*fwlen = 0;

	/* IMAGE_DOS_HEADER size and .Signature */
	if (len < 0x40 || memcmp(buf, "MZ", 2)) {
		error(0, 0, "file is not a DOS executable");
		return NULL;
	}

	/* IMAGE_NT_HEADERS size and .Signature */
	tmp = le32_to_cpup((uint32_t *) (buf + 0x3c));
	nt_header = buf + tmp;
	if (len < tmp + 0x18 || memcmp(nt_header, "PE\0\0", 4)) {
		error(0, 0, "file is not a portable executable (PE)");
		return NULL;
	}

	/* IMAGE_NT_HEADERS.FileHeader.NumberOfSections */
	nsect = le16_to_cpup((uint16_t *) (nt_header + 0x04 + 0x2));

	/* IMAGE_NT_HEADERS.FileHeader.SizeOfOptionalHeader */
	tmp = le16_to_cpup((uint16_t *) (nt_header + 0x04 + 0x10));
	sect_header = nt_header + 0x18 + tmp;

	for (; nsect; nsect--, sect_header += 0x28) {
		if (sect_header + 0x28 > buf + len) {
			error(0, 0, "file header corrupted");
			return NULL;
		}

		if (!memcmp(sect_header, ".data", 5))
			break;
	}

	if (!nsect) {
		error(0, 0, "`.data' section not found");
		return NULL;
	}

	/* IMAGE_SECTION_HEADER.SizeOfRawData */
	datalen = le32_to_cpup((uint32_t *) (sect_header + 0x10));
	/* IMAGE_SECTION_HEADER.PointerToRawData */
	dataoff = le32_to_cpup((uint32_t *) (sect_header + 0x14));
	if (dataoff + datalen > len) {
		error(0, 0, "`.data' section extends beyond end of file");
		return NULL;
	}

	/* find the starting sequence of the firmware image */
	fw = (uint8_t *) memmem(buf + dataoff, datalen, fwstart, sizeof(fwstart));
	if (!fw) {
		error(0, 0, "firmware start sequence not found");
		return NULL;
	}
	*fwlen = datalen - (fw - (buf + dataoff));

	/* find the starting sequence of the boot ROM patch, if present */
	bp = (uint8_t *) memmem(fw, *fwlen, bpstart, sizeof(bpstart));
	if (bp)
		*fwlen = bp - fw;
	return fw;
}

const char * argp_program_version = "002";
const char * argp_program_bug_address =
		"<accessrunner-general at lists dot sourceforge dot net>";
const static char args_doc[] = "INFILE OUTFILE";
const static char doc[] =
		"Firmware extractor for Conexant AccessRunner ADSL USB modems\n"
		"INFILE - Windows driver file containing firmware (usually CnxEtU.sys)\n"
		"OUTFILE - firmware image (usually cxacru-fw.bin)";

struct args {
	char *infile;
	char *outfile;
};

static error_t parse_opts(int key, char *arg, struct argp_state *state)
{
	struct args *args = state->input;
	switch (key) {
	case ARGP_KEY_ARG:
		switch (state->arg_num) {
		case 0:
			args->infile = arg;
			break;
		case 1:
			args->outfile = arg;
			break;
		default:
			argp_usage (state);
		}
		break;

	case ARGP_KEY_END:
		if (state->arg_num < 2)
			argp_usage (state);
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = {0, parse_opts, args_doc, doc};

int main(int argc, char **argv)
{
	int ret, fd;
	struct args args;
	struct stat instat;
	uint8_t *buf, *fw;
	int len, fwlen;

	argp_parse(&argp, argc, argv, 0, NULL, &args);

	fd = open(args.infile, O_RDONLY);
	if (fd < 0)
		error(1, errno, "failed to open `%s' for reading", args.infile);

	ret = fstat(fd, &instat);
	if (ret)
		error(1, errno, "failed to obtain the size of `%s'", args.infile);
	len = instat.st_size;

	buf = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (!buf)
		error(1, errno, "failed to mmap `%s'", args.infile);

	fw = findfw(buf, len, &fwlen);
	if (!fw)
		error(1, 0, "can't find AccessRunner firmware in `%s'", args.infile);

	fd = open(args.outfile, O_WRONLY | O_CREAT | O_EXCL,
		  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd < 0)
		error(1, errno, "failed to open `%s' for writing", args.outfile);

	if (write(fd, fw, fwlen) != fwlen)
		error(1, errno, "failed to write firmware to `%s'", args.outfile);
	close(fd);

	munmap(buf, len);
	printf("found firmware in `%s' at offset %#x\n", args.infile, fw - buf);
}
