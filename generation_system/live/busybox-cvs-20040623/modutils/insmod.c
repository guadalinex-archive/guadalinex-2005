/*
 * Mini insmod implementation for busybox
 *
 * This version of insmod supports x86, ARM, SH3/4/5, powerpc, m68k, 
 * MIPS, and v850e.
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999-2003 by Erik Andersen <andersen@codepoet.org>
 * and Ron Alder <alder@lineo.com>
 *
 * Miles Bader <miles@gnu.org> added NEC V850E support.
 *
 * Modified by Bryan Rittmeyer <bryan@ixiacom.com> to support SH4
 * and (theoretically) SH3. I have only tested SH4 in little endian mode.
 *
 * Modified by Alcove, Julien Gaulmin <julien.gaulmin@alcove.fr> and
 * Nicolas Ferre <nicolas.ferre@alcove.fr> to support ARM7TDMI.  Only
 * very minor changes required to also work with StrongArm and presumably
 * all ARM based systems.
 *
 * Paul Mundt <lethal@linux-sh.org> 08-Aug-2003.
 *   Integrated support for sh64 (SH-5), from preliminary modutils
 *   patches from Benedict Gaster <benedict.gaster@superh.com>.
 *   Currently limited to support for 32bit ABI.
 *
 * Magnus Damm <damm@opensource.se> 22-May-2002.
 *   The plt and got code are now using the same structs.
 *   Added generic linked list code to fully support PowerPC.
 *   Replaced the mess in arch_apply_relocation() with architecture blocks.
 *   The arch_create_got() function got cleaned up with architecture blocks.
 *   These blocks should be easy maintain and sync with obj_xxx.c in modutils.
 *
 * Magnus Damm <damm@opensource.se> added PowerPC support 20-Feb-2001.
 *   PowerPC specific code stolen from modutils-2.3.16, 
 *   written by Paul Mackerras, Copyright 1996, 1997 Linux International.
 *   I've only tested the code on mpc8xx platforms in big-endian mode.
 *   Did some cleanup and added CONFIG_USE_xxx_ENTRIES...
 *
 * Quinn Jensen <jensenq@lineo.com> added MIPS support 23-Feb-2001.
 *   based on modutils-2.4.2
 *   MIPS specific support for Elf loading and relocation.
 *   Copyright 1996, 1997 Linux International.
 *   Contributed by Ralf Baechle <ralf@gnu.ai.mit.edu>
 *
 * Based almost entirely on the Linux modutils-2.3.11 implementation.
 *   Copyright 1996, 1997 Linux International.
 *   New implementation contributed by Richard Henderson <rth@tamu.edu>
 *   Based on original work by Bjorn Ekwall <bj0rn@blox.se>
 *   Restructured (and partly rewritten) by:
 *   Björn Ekwall <bj0rn@blox.se> February 1999
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include "busybox.h"

#include "modutils.h"

static char *m_filename;

/*======================================================================*/

static int check_module_name_match(const char *filename, struct stat *statbuf, void *userdata)
{
	char *fullname = (char *) userdata;

	if (fullname[0] == '\0')
		return (FALSE);
	else {
		char *tmp, *tmp1 = bb_xstrdup(filename);
		tmp = bb_get_last_path_component(tmp1);
		if (strcmp(tmp, fullname) == 0) {
			free(tmp1);
			/* Stop searching if we find a match */
			m_filename = bb_xstrdup(filename);
			return (TRUE);
		}
		free(tmp1);
	}
	return (FALSE);
}


#ifdef CONFIG_FEATURE_2_6_MODULES
static int do_2_6(int fp, int argc, char **argv);
#endif

extern int insmod_main( int argc, char **argv)
{
	int opt;
	int len;
	char *tmp, *tmp1;
	struct stat st;
	char *m_name = 0;
	int fp;
#ifdef CONFIG_FEATURE_INSMOD_LOAD_MAP
	int flag_print_load_map = 0;
#endif
	char *m_fullName;

	/* Parse any options */
#ifdef CONFIG_FEATURE_INSMOD_LOAD_MAP
	while ((opt = getopt(argc, argv, "fkqsvxmLo:")) > 0)
#else
	while ((opt = getopt(argc, argv, "fkqsvxLo:")) > 0)
#endif
		{
			switch (opt) {
				case 'f':			/* force loading */
					flag_force_load = 1;
					break;
				case 'k':			/* module loaded by kerneld, auto-cleanable */
					flag_autoclean = 1;
					break;
				case 's':			/* log to syslog */
					/* log to syslog -- not supported              */
					/* but kernel needs this for request_module(), */
					/* as this calls: modprobe -k -s -- <module>   */
					/* so silently ignore this flag                */
					break;
				case 'v':			/* verbose output */
					flag_verbose = 1;
					break;
				case 'q':			/* silent */
					flag_quiet = 1;
					break;
				case 'x':			/* do not export externs */
					flag_export = 0;
					break;
				case 'o':			/* name the output module */
					free(m_name);
					m_name = bb_xstrdup(optarg);
					break;
				case 'L':			/* Stub warning */
					/* This is needed for compatibility with modprobe.
					 * In theory, this does locking, but we don't do
					 * that.  So be careful and plan your life around not
					 * loading the same module 50 times concurrently. */
					break;
#ifdef CONFIG_FEATURE_INSMOD_LOAD_MAP
				case 'm':			/* print module load map */
					flag_print_load_map = 1;
					break;
#endif
				default:
					bb_show_usage();
			}
		}

	if (argv[optind] == NULL) {
		bb_show_usage();
	}

	/* Grab the module name */
	tmp1 = bb_xstrdup(argv[optind]);
	tmp = basename(tmp1);
	len = strlen(tmp);

	k_version = get_kernel_version(k_strversion);

#if defined(CONFIG_FEATURE_2_6_MODULES)
	if (k_version > MODUTILS_MINIMAL_VERSION_2_6 && len > 3 && tmp[len - 3] == '.' &&
			tmp[len - 2] == 'k' && tmp[len - 1] == 'o') {
		len-=3;
		tmp[len] = '\0';
	}
	else
#endif
		if (len > 2 && tmp[len - 2] == '.' && tmp[len - 1] == 'o') {
			len-=2;
			tmp[len] = '\0';
		}


#if defined(CONFIG_FEATURE_2_6_MODULES)
	if (k_version > MODUTILS_MINIMAL_VERSION_2_6)
		bb_xasprintf(&m_fullName, "%s.ko", tmp);
	else
#endif
		bb_xasprintf(&m_fullName, "%s.o", tmp);

	if (!m_name) {
		m_name = tmp;
	} else {
		free(tmp1);
	}

	/* Get a filedesc for the module.  Check we we have a complete path */
	if (stat(argv[optind], &st) < 0 || !S_ISREG(st.st_mode) ||
			(fp = open(argv[optind], O_RDONLY)) < 0) {
		/* Hmm.  Could not open it.  First search under /lib/modules/`uname -r`,
		 * but do not error out yet if we fail to find it... */
		if (k_version) {	/* uname succeedd */
			char *module_dir;
			char *tmdn;
			char real_module_dir[FILENAME_MAX];

			tmdn = concat_path_file(_PATH_MODULES, k_strversion);
			/* Jump through hoops in case /lib/modules/`uname -r`
			 * is a symlink.  We do not want recursive_action to
			 * follow symlinks, but we do want to follow the
			 * /lib/modules/`uname -r` dir, So resolve it ourselves
			 * if it is a link... */
			if (realpath (tmdn, real_module_dir) == NULL)
				module_dir = tmdn;
			else
				module_dir = real_module_dir;
			recursive_action(module_dir, TRUE, FALSE, FALSE,
					check_module_name_match, 0, m_fullName);
			free(tmdn);
		}

		/* Check if we have found anything yet */
		if (m_filename == 0 || ((fp = open(m_filename, O_RDONLY)) < 0))
		{
			char module_dir[FILENAME_MAX];

			free(m_filename);
			m_filename = 0;
			if (realpath (_PATH_MODULES, module_dir) == NULL)
				strcpy(module_dir, _PATH_MODULES);
			/* No module found under /lib/modules/`uname -r`, this
			 * time cast the net a bit wider.  Search /lib/modules/ */
			if (! recursive_action(module_dir, TRUE, FALSE, FALSE,
						check_module_name_match, 0, m_fullName))
			{
				if (m_filename == 0
						|| ((fp = open(m_filename, O_RDONLY)) < 0))
				{
					bb_error_msg("%s: no module by that name found", m_fullName);
					return EXIT_FAILURE;
				}
			} else
				bb_error_msg_and_die("%s: no module by that name found", m_fullName);
		}
	} else
		m_filename = bb_xstrdup(argv[optind]);

	if (!flag_quiet)
		printf("Using %s\n", m_filename);

	if (k_version > MODUTILS_MINIMAL_VERSION_2_6)
	{
#ifdef CONFIG_FEATURE_2_6_MODULES
		return do_2_6(fp, argc - optind, argv + optind);
#endif
	}
	else if (k_version > MODUTILS_MINIMAL_VERSION_2_4)
	{
#ifdef CONFIG_FEATURE_2_4_MODULES
		return insmod_main_2_4(fp, argc - optind, argv + optind, m_name, m_filename);
#endif
	}
	return EXIT_FAILURE;
}

#ifdef CONFIG_FEATURE_2_6_MODULES
#include <sys/mman.h>
#include <sys/syscall.h>

/* We use error numbers in a loose translation... */
static const char *moderror(int err)
{
	switch (err) {
		case ENOEXEC:
			return "Invalid module format";
		case ENOENT:
			return "Unknown symbol in module";
		case ESRCH:
			return "Module has wrong symbol version";
		case EINVAL:
			return "Invalid parameters";
		default:
			return strerror(err);
	}
}

static int do_2_6(int fd, int argc, char **argv)
{
	int i;
	long int ret;
	struct stat st;
	unsigned long len;
	void *map;
	char *options = bb_xstrdup("");

	/* Rest is options */
	for (i = 2; i < argc; i++) {
		options = xrealloc(options, strlen(options) + 2 + strlen(argv[i]) + 2);
		/* Spaces handled by "" pairs, but no way of escaping quotes */
		if (strchr(argv[i], ' ')) {
			strcat(options, "\"");
			strcat(options, argv[i]);
			strcat(options, "\"");
		} else {
			strcat(options, argv[i]);
		}
		strcat(options, " ");
	}

	fstat(fd, &st);
	len = st.st_size;
	map = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		bb_perror_msg_and_die("cannot mmap `%s'", m_filename);
	}

	ret = syscall(__NR_init_module, map, len, options);
	if (ret != 0) {
		bb_perror_msg_and_die("cannot insert `%s': %s (%li)",
				m_filename, moderror(errno), ret);
	}

	return 0;
}
#endif /* CONFIG_FEATURE_2_6_MODULES */

