/*
 * Mini lsmod implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Modified by Alcove, Julien Gaulmin <julien.gaulmin@alcove.fr> and
 * Nicolas Ferre <nicolas.ferre@alcove.fr> to support pre 2.1 kernels
 * (which lack the query_module() interface).
 *
 * Modified by Colin Watson <cjwatson@debian.org> based on lsmod from
 * module-init-tools (Copyright (C) 2002 Rusty Russell, IBM Corporation)
 * to support 2.6 kernels.
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
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <assert.h>
#include <getopt.h>
#include <sys/utsname.h>
#include <sys/file.h>
#include "busybox.h"

#include "modutils.h"
#include "obj/module.h"

#ifdef CONFIG_FEATURE_2_4_MODULES
#ifdef CONFIG_FEATURE_CHECK_TAINTED_MODULE
static inline void check_tainted(void)
{
	int tainted;
	FILE *f;

	tainted = 0;
	if ((f = fopen(TAINT_FILENAME, "r"))) {
		fscanf(f, "%d", &tainted);
		fclose(f);
	}
	if (f && tainted) {
		printf("    Tainted: %c%c%c\n",
				tainted & TAINT_PROPRIETORY_MODULE      ? 'P' : 'G',
				tainted & TAINT_FORCED_MODULE           ? 'F' : ' ',
				tainted & TAINT_UNSAFE_SMP              ? 'S' : ' ');
	}
	else {
		printf("    Not tainted\n");
	}
}
#else
static inline void check_tainted(void)
{
	printf("\n");
}
#endif

static inline int query_2_4_modules(void)
{
#ifdef CONFIG_FEATURE_QUERY_MODULE_INTERFACE
	struct module_info info;
	char *module_names, *mn, *deps, *dn;
	size_t bufsize, depsize, nmod, count, i, j;
#endif

	check_tainted();

#ifdef CONFIG_FEATURE_QUERY_MODULE_INTERFACE
	if (getuid() != 0)
	{
#endif
	if (bb_xprint_file_by_name("/proc/modules") < 0) {
		return 0;
	}
	return 1;
#ifdef CONFIG_FEATURE_QUERY_MODULE_INTERFACE
	}

        module_names = xmalloc(bufsize = 256);
        if (my_query_module(NULL, QM_MODULES, (void **)&module_names, &bufsize, &nmod)) {
                bb_perror_msg_and_die("QM_MODULES");
        }

        deps = xmalloc(depsize = 256);

        for (i = 0, mn = module_names; i < nmod; mn += strlen(mn) + 1, i++) {
                if (query_module(mn, QM_INFO, &info, sizeof(info), &count)) {
                        if (errno == ENOENT) {
                                /* The module was removed out from underneath us. */
                                continue;
                        }
                        /* else choke */
                        bb_perror_msg_and_die("module %s: QM_INFO", mn);
                }
                if (my_query_module(mn, QM_REFS, (void **)&deps, &depsize, &count)) {
                        if (errno == ENOENT) {
                                /* The module was removed out from underneath us. */
                                continue;
                        }
                        bb_perror_msg_and_die("module %s: QM_REFS", mn);
                }
                printf("%-20s%8lu%4ld", mn, info.size, info.usecount);
                if (info.flags & NEW_MOD_DELETED)
                        printf(" (deleted)");
                else if (info.flags & NEW_MOD_INITIALIZING)
                        printf(" (initializing)");
                else if (!(info.flags & NEW_MOD_RUNNING))
                        printf(" (uninitialized)");
                else {
                        if (info.flags & NEW_MOD_AUTOCLEAN)
                                printf(" (autoclean) ");
                        if (!(info.flags & NEW_MOD_USED_ONCE))
                                printf(" (unused)");
                }
                if (count) printf(" [");
                for (j = 0, dn = deps; j < count; dn += strlen(dn) + 1, j++) {
                        printf("%s%s", dn, (j==count-1)? "":" ");
                }
                if (count) printf("]");

                printf("\n");
        }

        return  0;
#endif
}
#endif /* CONFIG_FEATURE_2_4_MODULES */

#ifdef CONFIG_FEATURE_2_6_MODULES
static inline int query_2_6_modules(void)
{
        char line[4096];
        FILE *file;

        if ((file = fopen("/proc/modules", "r")) == NULL)
                bb_perror_msg_and_die("Can't read /proc/modules");
        while (fgets(line, sizeof(line), file)) {
                char *mn, *size, *usecount, *deps;
                mn = strtok(line, " \t");
                size = strtok(NULL, " \t");
                printf("%-20s%8s", mn, size);
                usecount = strtok(NULL, " \t");
                /* NULL if no module unloading support. */
                if (usecount) {
                        deps = strtok(NULL, "\n");
                        if (!deps)
                                deps = "";
                        /* New-style has commas, or -.  If so,
                         * truncate (other fields might follow). */
                        else if (strchr(deps, ',')) {
                                deps = strtok(deps, " \t");
                                /* Strip trailing comma. */
                                if (deps[strlen(deps) - 1] == ',')
                                        deps[strlen(deps) - 1] = '\0';
                        } else if (deps[0] == '-' &&
                                   (deps[1] == '\0' || isspace(deps[1])))
                                deps = "";
                        printf("%4s %s", usecount, deps);
                }
                putchar('\n');
        }
        fclose(file);
        return 0;
}
#endif /* CONFIG_FEATURE_2_6_MODULES */

extern int lsmod_main(int argc, char **argv)
{
	printf("Module                  Size  Used by");

	if (get_kernel_revision() > MODUTILS_MINIMAL_VERSION_2_6) {
#ifdef CONFIG_FEATURE_2_6_MODULES
		return query_2_6_modules();
#else
		return EXIT_FAILURE;
#endif
	}
	else if (get_kernel_revision() > MODUTILS_MINIMAL_VERSION_2_4) {
#ifdef CONFIG_FEATURE_2_4_MODULES
		return query_2_4_modules();
#else
		return EXIT_FAILURE;
#endif
	}
	return EXIT_FAILURE;
}

