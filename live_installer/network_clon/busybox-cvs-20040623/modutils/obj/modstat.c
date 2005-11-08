/*
 * Get kernel symbol table(s) and other relevant module info.
 *
 * Add module_name_list and l_module_name_list.
 *   Keith Owens <kaos@ocs.com.au> November 1999.
 * Björn Ekwall <bj0rn@blox.se> in February 1999 (C)
 * Initial work contributed by Richard Henderson <rth@tamu.edu>
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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "util.h"
#include "module.h"
#include "obj.h"
#include "modstat.h"

struct module_stat *module_stat;
size_t n_module_stat;
char *module_name_list;
size_t l_module_name_list;
struct module_symbol *ksyms;
size_t nksyms;

static void *old_kernsym;

/************************************************************************/
static void drop(void)
{
	/*
	 * Clean the slate for multiple runs
	 */
	if (module_stat) {
		struct module_stat *m;
		int i;

		for (i = 0, m = module_stat; i < n_module_stat; ++i, ++m) {
			if (m->syms)
				free(m->syms);
			if (m->refs)
				free(m->refs);
		}
		free(module_stat);
		module_stat = NULL;
		n_module_stat = 0;
	}
	if (module_name_list) {
		free(module_name_list);
		module_name_list = NULL;
		l_module_name_list = 0;
	}
	if (ksyms) {
		free(ksyms);
		ksyms = NULL;
		nksyms = 0;
	}
	if (old_kernsym) {
		free(old_kernsym);
		old_kernsym = NULL;
	}
}

int new_get_kernel_info(int type)
{
	struct module_stat *modules;
	struct module_stat *m;
	struct module_symbol *syms;
	struct module_symbol *s;
	size_t ret;
	size_t bufsize;
	size_t nmod;
	size_t nsyms;
	size_t i;
	size_t j;
	char *module_names;
	char *mn;

	drop();

	/*
	 * Collect the loaded modules
	 */
	module_names = xmalloc(bufsize = 256);
	while (query_module(NULL, QM_MODULES, module_names, bufsize, &ret)) {
		if (errno != ENOSPC) {
			error("QM_MODULES: %m\n");
			return 0;
		}
		module_names = xrealloc(module_names, bufsize = ret);
	}
	module_name_list = module_names;
	l_module_name_list = bufsize;
	n_module_stat = nmod = ret;
	module_stat = modules = xmalloc(nmod * sizeof(struct module_stat));
	memset(modules, 0, nmod * sizeof(struct module_stat));

	/* Collect the info from the modules */
	for (i = 0, mn = module_names, m = modules;
	     i < nmod;
	     ++i, ++m, mn += strlen(mn) + 1) {
		struct module_info info;

		m->name = mn;
		if (query_module(mn, QM_INFO, &info, sizeof(info), &ret)) {
			if (errno == ENOENT) {
			/* The module was removed out from underneath us. */
				m->flags = NEW_MOD_DELETED;
				continue;
			}
			/* else oops */
			error("module %s: QM_INFO: %m", mn);
			return 0;
		}

		m->addr = info.addr;

		if (type & K_INFO) {
			m->size = info.size;
			m->flags = info.flags;
			m->usecount = info.usecount;
			m->modstruct = info.addr;
		}

		if (type & K_REFS) {
			int mm;
			char *mrefs;
			char *mr;

			mrefs = xmalloc(bufsize = 64);
			while (query_module(mn, QM_REFS, mrefs, bufsize, &ret)) {
				if (errno != ENOSPC) {
					error("QM_REFS: %m");
					return 1;
				}
				mrefs = xrealloc(mrefs, bufsize = ret);
			}
			for (j = 0, mr = mrefs;
			     j < ret;
			     ++j, mr += strlen(mr) + 1) {
				for (mm = 0; mm < i; ++mm) {
					if (strcmp(mr, module_stat[mm].name) == 0) {
						m->nrefs += 1;
						m->refs = xrealloc(m->refs, m->nrefs * sizeof(struct module_stat **));
						m->refs[m->nrefs - 1] = module_stat + mm;
						break;
					}
				}
			}
			free(mrefs);
		}

		if (type & K_SYMBOLS) { /* Want info about symbols */
			syms = xmalloc(bufsize = 1024);
			while (query_module(mn, QM_SYMBOLS, syms, bufsize, &ret)) {
				if (errno == ENOSPC) {
					syms = xrealloc(syms, bufsize = ret);
					continue;
				}
				if (errno == ENOENT) {
					/*
					 * The module was removed out
					 * from underneath us.
					 */
					m->flags = NEW_MOD_DELETED;
					free(syms);
					goto next;
				} else {
					error("module %s: QM_SYMBOLS: %m", mn);
					return 0;
				}
			}
			nsyms = ret;

			m->nsyms = nsyms;
			m->syms = syms;

			/* Convert string offsets to string pointers */
			for (j = 0, s = syms; j < nsyms; ++j, ++s)
				s->name += (unsigned long) syms;
		}
		next: ;
	}

	if (type & K_SYMBOLS) { /* Want info about symbols */
		/* Collect the kernel's symbols.  */
		syms = xmalloc(bufsize = 16 * 1024);
		while (query_module(NULL, QM_SYMBOLS, syms, bufsize, &ret)) {
			if (errno != ENOSPC) {
				error("kernel: QM_SYMBOLS: %m");
				return 0;
			}
			syms = xrealloc(syms, bufsize = ret);
		}
		nksyms = nsyms = ret;
		ksyms = syms;

		/* Convert string offsets to string pointers */
		for (j = 0, s = syms; j < nsyms; ++j, ++s)
			s->name += (unsigned long) syms;
	}

	return 1;
}

