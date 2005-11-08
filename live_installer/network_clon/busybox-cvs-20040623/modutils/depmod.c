/*
 * depmod
 *
 * Create dependency file for modprobe, the kernel loadable modules loader.
 *
 * Copyright 1994, 1995, 1996, 1997 Jacques Gelinas <jack@solucorp.qc.ca>
 * Additional modifications: Björn Ekwall <bj0rn@blox.se> February, March 1999
 *
 * This file is part of the Linux modutils.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <limits.h>
#include <elf.h>

#include "busybox.h"
#include "modutils.h"


extern int depmod_main(int argc, char **argv)
{
	int ret = -1;
	int stdmode = 0;
	int err = 0;
	int o;
	char *file_syms = NULL;
	char *conf_file = NULL;
	char *base_dir = "";
	char *module_dir = NULL;

	while ((o = getopt(argc, argv, "aAb:C:eF:hnqsvVru")) != EOF) {
		switch (o) {
			case 'A':
			case 'a':
				flag_quick = o == 'A';
				stdmode = 1;	/* Probe standard directory */
				break;		/* using the config file */

			case 'b':
				base_dir = optarg;
				break;

			case 'C':
				conf_file = optarg;
				break;

			case 'e':
				flag_showerror = 1;
				break;

			case 'F':
				file_syms = optarg;
				break;

			case 'h':
				return 0;
				break;

			case 'n':
				flag_dry = 1;
				break;

			case 'q':
				flag_quiet = 1;
				break;

			case 'u':
				flag_unresolved_error = 1;
				break;

			default:
				err = 1;
				break;
		}
	}

	if (err) {
		bb_error_msg("Aborting");
		return EXIT_FAILURE;
	}
	argc -= optind;
	argv += optind;

        k_version = get_kernel_version(k_strversion);

	if (k_version != -1) {
		char tmdn[FILENAME_MAX];
		char real_module_dir[FILENAME_MAX];

		snprintf(tmdn, FILENAME_MAX, "%s" _PATH_MODULES "/%s/", base_dir, k_strversion);
		if (realpath (tmdn, real_module_dir) == NULL)
			module_dir = tmdn;
		else
			module_dir = real_module_dir;
	}
	else
		return EXIT_FAILURE;

	if (k_version > MODUTILS_MINIMAL_VERSION_2_6)
#ifdef CONFIG_FEATURE_2_6_MODULES
		ret = depmod_main_2_6(argc, argv, stdmode || argc == 0, file_syms, base_dir, module_dir);
#else
		return EXIT_FAILURE;
#endif
	else
	{
#ifdef CONFIG_FEATURE_2_4_MODULES
		ret = depmod_main_2_4(argc, argv, stdmode || argc == 0, file_syms, base_dir, module_dir, file_syms);
#else
		return EXIT_FAILURE;
#endif /* CONFIG_FEATURE_2_4_MODULES */
	}

	return ret;
}

