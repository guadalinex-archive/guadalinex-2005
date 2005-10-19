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

#include "../modutils.h"
#include "modstat.h"
#include "module.h"
#include "obj.h"
#include "kallsyms.h"
#include "util.h"

static int n_ext_modules_used;
static int m_has_modinfo;
static int gplonly_seen;

static char *m_filename;

/* Conditionally add the symbols from the given symbol set to the
   new module.  */

static int add_symbols_from(struct obj_file *f, int idx,
			    struct module_symbol *syms, size_t nsyms, int gpl)
{
	struct module_symbol *s;
	size_t i;
	int used = 0;

	for (i = 0, s = syms; i < nsyms; ++i, ++s) {
		/*
		 * Only add symbols that are already marked external.
		 * If we override locals we may cause problems for
		 * argument initialization.
		 * We will also create a false dependency on the module.
		 */
		struct obj_symbol *sym;

		/* GPL licensed modules can use symbols exported with
		 * EXPORT_SYMBOL_GPL, so ignore any GPLONLY_ prefix on the
		 * exported names.  Non-GPL modules never see any GPLONLY_
		 * symbols so they cannot fudge it by adding the prefix on
		 * their references.
		 */
		if (strncmp((char *)s->name, "GPLONLY_", 8) == 0) {
			gplonly_seen = 1;
			if (gpl)
				s->name = ((char *)s->name) + 8;
			else
				continue;
		}

		sym = obj_find_symbol(f, (char *) s->name);

		if (sym && ELFW(ST_BIND) (sym->info) != STB_LOCAL) {
			sym = obj_add_symbol(f, (char *) s->name, -1,
					ELFW(ST_INFO) (STB_GLOBAL, STT_NOTYPE),
					idx, s->value, 0);
			/*
			 * Did our symbol just get installed?
			 * If so, mark the module as "used".
			 */
			if (sym->secidx == idx)
				used = 1;
		}
	}

	return used;
}

static void add_kernel_symbols(struct obj_file *f, int gpl)
{
	struct module_stat *m;
	int i, nused = 0;

	/* Add module symbols first.  */

	for (i = 0, m = module_stat; i < n_module_stat; ++i, ++m)
		if (m->nsyms && add_symbols_from(f, SHN_HIRESERVE + 2 + i, m->syms, m->nsyms, gpl))
			m->status = 1, ++nused;

	n_ext_modules_used = nused;

	/* And finally the symbols from the kernel proper.  */
	if (nksyms)
		add_symbols_from(f, SHN_HIRESERVE + 1, ksyms, nksyms, gpl);
}

static char *get_modinfo_value(struct obj_file *f, const char *key)
{
	struct obj_section *sec;
	char *p, *v, *n, *ep;
	size_t klen = strlen(key);

	sec = obj_find_section(f, ".modinfo");
	if (sec == NULL)
		return NULL;
	p = sec->contents;
	ep = p + sec->header.sh_size;
	while (p < ep) {
		v = strchr(p, '=');
		n = strchr(p, '\0');
		if (v) {
			if (p + klen == v && strncmp(p, key, klen) == 0)
				return v + 1;
		} else {
			if (p + klen == n && strcmp(p, key) == 0)
				return n;
		}
		p = n + 1;
	}

	return NULL;
}


/* Fetch the loaded modules, and all currently exported symbols.  */

static int new_get_kernel_symbols(void)
{
	char *module_names, *mn;
	struct module_stat *modules, *m;
	struct module_symbol *syms, *s;
	size_t ret, bufsize, nmod, nsyms, i, j;

	/* Collect the loaded modules.  */

	module_names = xmalloc(bufsize = 256);
retry_modules_load:
	if (query_module(NULL, QM_MODULES, module_names, bufsize, &ret)) {
		if (errno == ENOSPC && bufsize < ret) {
			module_names = xrealloc(module_names, bufsize = ret);
			goto retry_modules_load;
		}
		bb_perror_msg("QM_MODULES");
		return 0;
	}

	n_module_stat = nmod = ret;

	/* Collect the modules' symbols.  */

	if (nmod){
		module_stat = modules = xmalloc(nmod * sizeof(*modules));
		memset(modules, 0, nmod * sizeof(*modules));
		for (i = 0, mn = module_names, m = modules;
				i < nmod; ++i, ++m, mn += strlen(mn) + 1) {
			struct module_info info;

			if (query_module(mn, QM_INFO, &info, sizeof(info), &ret)) {
				if (errno == ENOENT) {
					/* The module was removed out from underneath us.  */
					continue;
				}
				bb_perror_msg("query_module: QM_INFO: %s", mn);
				return 0;
			}

			syms = xmalloc(bufsize = 1024);
retry_mod_sym_load:
			if (query_module(mn, QM_SYMBOLS, syms, bufsize, &ret)) {
				switch (errno) {
					case ENOSPC:
						syms = xrealloc(syms, bufsize = ret);
						goto retry_mod_sym_load;
					case ENOENT:
						/* The module was removed out from underneath us.  */
						continue;
					default:
						bb_perror_msg("query_module: QM_SYMBOLS: %s", mn);
						return 0;
				}
			}
			nsyms = ret;

			m->name = mn;
			m->addr = info.addr;
			m->nsyms = nsyms;
			m->syms = syms;

			for (j = 0, s = syms; j < nsyms; ++j, ++s) {
				s->name += (unsigned long) syms;
			}
		}
	}

	/* Collect the kernel's symbols.  */

	syms = xmalloc(bufsize = 16 * 1024);
retry_kern_sym_load:
	if (query_module(NULL, QM_SYMBOLS, syms, bufsize, &ret)) {
		if (errno == ENOSPC && bufsize < ret) {
			syms = xrealloc(syms, bufsize = ret);
			goto retry_kern_sym_load;
		}
		bb_perror_msg("kernel: QM_SYMBOLS");
		return 0;
	}
	nksyms = nsyms = ret;
	ksyms = syms;

	for (j = 0, s = syms; j < nsyms; ++j, ++s) {
		s->name += (unsigned long) syms;
	}
	return 1;
}


static int new_create_this_module(struct obj_file *f, const char *m_name)
{
	struct obj_section *sec;

	sec = obj_create_alloced_section_first(f, ".this", tgt_sizeof_long,
			sizeof(struct module));
	memset(sec->contents, 0, sizeof(struct module));

	obj_add_symbol(f, "__this_module", -1,
			ELFW(ST_INFO) (STB_LOCAL, STT_OBJECT), sec->idx, 0,
			sizeof(struct module));

	obj_string_patch(f, sec->idx, offsetof(struct module, name),
			m_name);

	return 1;
}

//#ifdef CONFIG_FEATURE_INSMOD_KSYMOOPS_SYMBOLS
/* add an entry to the __ksymtab section, creating it if necessary */
static void new_add_ksymtab(struct obj_file *f, struct obj_symbol *sym)
{
	struct obj_section *sec;
	ElfW(Addr) ofs;

	/* ensure __ksymtab is allocated, EXPORT_NOSYMBOLS creates a non-alloc section.
	 * If __ksymtab is defined but not marked alloc, x out the first character
	 * (no obj_delete routine) and create a new __ksymtab with the correct
	 * characteristics.
	 */
	sec = obj_find_section(f, "__ksymtab");
	if (sec && !(sec->header.sh_flags & SHF_ALLOC)) {
		*((char *)(sec->name)) = 'x';	/* override const */
		sec = NULL;
	}
	if (!sec)
		sec = obj_create_alloced_section(f, "__ksymtab",
				tgt_sizeof_void_p, 0, 0);
	if (!sec)
		return;
	sec->header.sh_flags |= SHF_ALLOC;
	sec->header.sh_addralign = tgt_sizeof_void_p;	/* Empty section might
							   be byte-aligned */
	ofs = sec->header.sh_size;
	obj_symbol_patch(f, sec->idx, ofs, sym);
	obj_string_patch(f, sec->idx, ofs + tgt_sizeof_void_p, sym->name);
	obj_extend_section(sec, 2 * tgt_sizeof_char_p);
}
//#endif /* CONFIG_FEATURE_INSMOD_KSYMOOPS_SYMBOLS */

static int new_create_module_ksymtab(struct obj_file *f)
{
	struct obj_section *sec;
	int i;

	/* We must always add the module references.  */

	if (n_ext_modules_used) {
		struct module_ref *dep;
		struct obj_symbol *tm;

		sec = obj_create_alloced_section(f, ".kmodtab", tgt_sizeof_void_p,
				(sizeof(struct module_ref)
				 * n_ext_modules_used), 0);
		if (!sec)
			return 0;

		tm = obj_find_symbol(f, "__this_module");
		dep = (struct module_ref *) sec->contents;
		for (i = 0; i < n_module_stat; ++i)
			if (module_stat[i].status) {
				dep->dep = module_stat[i].addr;
				obj_symbol_patch(f, sec->idx,
						(char *) &dep->ref - sec->contents, tm);
				dep->next_ref = 0;
				++dep;
			}
	}

	if (flag_export && !obj_find_section(f, "__ksymtab")) {
		int *loaded;

		/* We don't want to export symbols residing in sections that
		   aren't loaded.  There are a number of these created so that
		   we make sure certain module options don't appear twice.  */

		loaded = alloca(sizeof(int) * (i = f->header.e_shnum));
		while (--i >= 0)
			loaded[i] = (f->sections[i]->header.sh_flags & SHF_ALLOC) != 0;

		for (i = 0; i < HASH_BUCKETS; ++i) {
			struct obj_symbol *sym;
			for (sym = f->symtab[i]; sym; sym = sym->next) {
				if (ELFW(ST_BIND) (sym->info) != STB_LOCAL
						&& sym->secidx <= SHN_HIRESERVE
						&& (sym->secidx >= SHN_LORESERVE
							|| loaded[sym->secidx])) {
					new_add_ksymtab(f, sym);
				}
			}
		}
	}

	return 1;
}


static int
new_init_module(const char *m_name, struct obj_file *f,
		unsigned long m_size)
{
	struct module *module;
	struct obj_section *sec;
	void *image;
	int ret;
	tgt_long m_addr;

	sec = obj_find_section(f, ".this");
	if (!sec || !sec->contents) { 
		bb_perror_msg_and_die("corrupt module %s?",m_name);
	}
	module = (struct module *) sec->contents;
	m_addr = sec->header.sh_addr;

	module->size_of_struct = sizeof(*module);
	module->size = m_size;
	module->flags = flag_autoclean ? NEW_MOD_AUTOCLEAN : 0;

	sec = obj_find_section(f, "__ksymtab");
	if (sec && sec->header.sh_size) {
		module->syms = sec->header.sh_addr;
		module->nsyms = sec->header.sh_size / (2 * tgt_sizeof_char_p);
	}

	if (n_ext_modules_used) {
		sec = obj_find_section(f, ".kmodtab");
		module->deps = sec->header.sh_addr;
		module->ndeps = n_ext_modules_used;
	}

	module->init =
		obj_symbol_final_value(f, obj_find_symbol(f, "init_module"));
	module->cleanup =
		obj_symbol_final_value(f, obj_find_symbol(f, "cleanup_module"));

	sec = obj_find_section(f, "__ex_table");
	if (sec) {
		module->ex_table_start = sec->header.sh_addr;
		module->ex_table_end = sec->header.sh_addr + sec->header.sh_size;
	}

	sec = obj_find_section(f, ".text.init");
	if (sec) {
		module->runsize = sec->header.sh_addr - m_addr;
	}
	sec = obj_find_section(f, ".data.init");
	if (sec) {
		if (!module->runsize ||
				module->runsize > sec->header.sh_addr - m_addr)
			module->runsize = sec->header.sh_addr - m_addr;
	}
	sec = obj_find_section(f, ARCHDATA_SEC_NAME);
	if (sec && sec->header.sh_size) {
		module->archdata_start = sec->header.sh_addr;
		module->archdata_end = module->archdata_start + sec->header.sh_size;
	}
	sec = obj_find_section(f, KALLSYMS_SEC_NAME);
	if (sec && sec->header.sh_size) {
		module->kallsyms_start = sec->header.sh_addr;
		module->kallsyms_end = module->kallsyms_start + sec->header.sh_size;
	}
	if (!arch_init_module(f, module))
		return 0;

	/* Whew!  All of the initialization is complete.  Collect the final
	   module image and give it to the kernel.  */

	image = xmalloc(m_size);
	obj_create_image(f, image);

	ret = sys_init_module(m_name, (struct module *) image);
	if (ret)
		bb_perror_msg("init_module: %s", m_name);

	free(image);

	return ret == 0;
}

static void hide_special_symbols(struct obj_file *f)
{
	static const char *const specials[] = {
		"cleanup_module",
		"init_module",
		"kernel_version",
		NULL
	};

	struct obj_symbol *sym;
	const char *const *p;

	for (p = specials; *p; ++p)
		if ((sym = obj_find_symbol(f, *p)) != NULL)
			sym->info =
				ELFW(ST_INFO) (STB_LOCAL, ELFW(ST_TYPE) (sym->info));
}

#ifdef CONFIG_FEATURE_CHECK_TAINTED_MODULE
static void set_tainted(struct obj_file *f, int fd, char *m_name, 
		int kernel_has_tainted, int taint, const char *text1, const char *text2)
{
	char buf[80];
	int oldval;
	if (fd < 0 && !kernel_has_tainted)
		return;		/* New modutils on old kernel */
	printf("Warning: loading %s will taint the kernel: %s%s\n",
			m_name, text1, text2);
	if (fd >= 0) {
		read(fd, buf, sizeof(buf)-1);
		buf[sizeof(buf)-1] = '\0';
		oldval = strtoul(buf, NULL, 10);
		sprintf(buf, "%d\n", oldval | taint);
		write(fd, buf, strlen(buf));
	}
}

/* Check if loading this module will taint the kernel. */
static void check_tainted_module(struct obj_file *f, char *m_name)
{
	static const char tainted_file[] = TAINT_FILENAME;
	int fd, kernel_has_tainted;
	const char *ptr;

	kernel_has_tainted = 1;
	if ((fd = open(tainted_file, O_RDWR)) < 0) {
		if (errno == ENOENT)
			kernel_has_tainted = 0;
		else if (errno == EACCES)
			kernel_has_tainted = 1;
		else {
			perror(tainted_file);
			kernel_has_tainted = 0;
		}
	}

	switch (obj_gpl_license(f, &ptr)) {
		case 0:
			break;
		case 1:
			set_tainted(f, fd, m_name, kernel_has_tainted, TAINT_PROPRIETORY_MODULE, "no license", "");
			break;
		case 2:
			/* The module has a non-GPL license so we pretend that the
			 * kernel always has a taint flag to get a warning even on
			 * kernels without the proc flag.
			 */
			set_tainted(f, fd, m_name, 1, TAINT_PROPRIETORY_MODULE, "non-GPL license - ", ptr);
			break;
		default:
			set_tainted(f, fd, m_name, 1, TAINT_PROPRIETORY_MODULE, "Unexpected return from obj_gpl_license", "");
			break;
	}

	if (flag_force_load)
		set_tainted(f, fd, m_name, 1, TAINT_FORCED_MODULE, "forced load", "");

	if (fd >= 0)
		close(fd);
}
#endif /* CONFIG_FEATURE_CHECK_TAINTED_MODULE */

#ifdef CONFIG_FEATURE_INSMOD_VERSION_CHECKING
/* Get the module's kernel version in the canonical integer form.  */
static int get_module_version(struct obj_file *f, char str[STRVERSIONLEN])
{
	int a, b, c;
	char *p, *q;

	if ((p = get_modinfo_value(f, "kernel_version")) == NULL) {
		struct obj_symbol *sym;

		m_has_modinfo = 0;
		if ((sym = obj_find_symbol(f, "kernel_version")) == NULL)
			sym = obj_find_symbol(f, "__module_kernel_version");
		if (sym == NULL)
			return -1;
		p = f->sections[sym->secidx]->contents + sym->value;
	} else
		m_has_modinfo = 1;

	strncpy(str, p, STRVERSIONLEN-1);
	str[STRVERSIONLEN-1] = '\0';
	p = str;

	a = strtoul(p, &p, 10);
	if (*p != '.')
		return -1;
	b = strtoul(p + 1, &p, 10);
	if (*p != '.')
		return -1;
	c = strtoul(p + 1, &q, 10);
	if (p + 1 == q)
		return -1;

	return a << 16 | b << 8 | c;
}
#else
static int get_module_version(struct obj_file *f, char str[STRVERSIONLEN]) __attribute__((unused));
static int get_module_version(struct obj_file *f, char str[STRVERSIONLEN])
{
	return -1;
}
#endif /* CONFIG_FEATURE_INSMOD_VERSION_CHECKING */

#ifdef CONFIG_FEATURE_INSMOD_VERSION_CHECKING
/* Return the kernel symbol checksum version, or zero if not used. */
static int is_kernel_checksummed(void)
{
	struct module_symbol *s;
	size_t i;

	/*
	 * Using_Versions might not be the first symbol,
	 * but it should be in there.
	 */
	for (i = 0, s = ksyms; i < nksyms; ++i, ++s)
		if (strcmp((char *) s->name, "Using_Versions") == 0)
			return s->value;

	return 0;
}

static int is_module_checksummed(struct obj_file *f)
{
	if (m_has_modinfo) {
		const char *p = get_modinfo_value(f, "using_checksums");
		if (p)
			return atoi(p);
		else
			return 0;
	} else
		return obj_find_symbol(f, "Using_Versions") != NULL;
}
#endif /* CONFIG_FEATURE_INSMOD_VERSION_CHECKING */

#ifdef CONFIG_FEATURE_INSMOD_KSYMOOPS_SYMBOLS
/* add module source, timestamp, kernel version and a symbol for the
 * start of some sections.  this info is used by ksymoops to do better
 * debugging.
 */
static void 
add_ksymoops_symbols(struct obj_file *f, const char *filename,
		const char *m_name)
{
	static const char symprefix[] = "__insmod_";
	struct obj_section *sec;
	struct obj_symbol *sym;
	char *name, *absolute_filename;
	char str[STRVERSIONLEN], real[PATH_MAX];
	int i, l, lm_name, lfilename, use_ksymtab, version;
	struct stat statbuf;

	static const char *section_names[] = {
		".text",
		".rodata",
		".data",
		".bss"
			".sbss"
	};

	if (realpath(filename, real)) {
		absolute_filename = bb_xstrdup(real);
	}
	else {
		int save_errno = errno;
		bb_error_msg("cannot get realpath for %s", filename);
		errno = save_errno;
		perror("");
		absolute_filename = bb_xstrdup(filename);
	}

	lm_name = strlen(m_name);
	lfilename = strlen(absolute_filename);

	/* add to ksymtab if it already exists or there is no ksymtab and other symbols
	 * are not to be exported.  otherwise leave ksymtab alone for now, the
	 * "export all symbols" compatibility code will export these symbols later.
	 */
	use_ksymtab =  obj_find_section(f, "__ksymtab") || !flag_export;

	if ((sec = obj_find_section(f, ".this"))) {
		/* tag the module header with the object name, last modified
		 * timestamp and module version.  worst case for module version
		 * is 0xffffff, decimal 16777215.  putting all three fields in
		 * one symbol is less readable but saves kernel space.
		 */
		l = sizeof(symprefix)+			/* "__insmod_" */
			lm_name+				/* module name */
			2+					/* "_O" */
			lfilename+				/* object filename */
			2+					/* "_M" */
			2*sizeof(statbuf.st_mtime)+		/* mtime in hex */
			2+					/* "_V" */
			8+					/* version in dec */
			1;					/* nul */
		name = xmalloc(l);
		if (stat(absolute_filename, &statbuf) != 0)
			statbuf.st_mtime = 0;
		version = get_module_version(f, str);	/* -1 if not found */
		snprintf(name, l, "%s%s_O%s_M%0*lX_V%d",
				symprefix, m_name, absolute_filename,
				(int)(2*sizeof(statbuf.st_mtime)), statbuf.st_mtime,
				version);
		sym = obj_add_symbol(f, name, -1,
				ELFW(ST_INFO) (STB_GLOBAL, STT_NOTYPE),
				sec->idx, sec->header.sh_addr, 0);
		if (use_ksymtab)
			new_add_ksymtab(f, sym);
	}
	free(absolute_filename);

	/* record where the persistent data is going, same address as previous symbol */

	if (f->persist) {
		l = sizeof(symprefix)+		/* "__insmod_" */
			lm_name+		/* module name */
			2+			/* "_P" */
			strlen(f->persist)+	/* data store */
			1;			/* nul */
		name = xmalloc(l);
		snprintf(name, l, "%s%s_P%s",
				symprefix, m_name, f->persist);
		sym = obj_add_symbol(f, name, -1, ELFW(ST_INFO) (STB_GLOBAL, STT_NOTYPE),
				sec->idx, sec->header.sh_addr, 0);
		if (use_ksymtab)
			new_add_ksymtab(f, sym);
	}

	/* tag the desired sections if size is non-zero */

	for (i = 0; i < sizeof(section_names)/sizeof(section_names[0]); ++i) {
		if ((sec = obj_find_section(f, section_names[i])) &&
				sec->header.sh_size) {
			l = sizeof(symprefix)+		/* "__insmod_" */
				lm_name+		/* module name */
				2+			/* "_S" */
				strlen(sec->name)+	/* section name */
				2+			/* "_L" */
				8+			/* length in dec */
				1;			/* nul */
			name = xmalloc(l);
			snprintf(name, l, "%s%s_S%s_L%ld",
					symprefix, m_name, sec->name,
					(long)sec->header.sh_size);
			sym = obj_add_symbol(f, name, -1, ELFW(ST_INFO) (STB_GLOBAL, STT_NOTYPE),
					sec->idx, sec->header.sh_addr, 0);
			if (use_ksymtab)
				new_add_ksymtab(f, sym);
		}
	}
}
#endif /* CONFIG_FEATURE_INSMOD_KSYMOOPS_SYMBOLS */

static int process_module_arguments(struct obj_file *f, int argc, char **argv, int required)
{
	for (; argc > 0; ++argv, --argc) {
		struct obj_symbol *sym;
		int c;
		int min, max;
		int n;
		char *contents;
		char *input;
		char *fmt;
		char *key;
		char *loc;

		if ((input = strchr(*argv, '=')) == NULL)
			continue;

		n = input - *argv;
		input += 1; /* skip '=' */

		key = alloca(n + 6);

		if (m_has_modinfo) {
			memcpy(key, "parm_", 5);
			memcpy(key + 5, *argv, n);
			key[n + 5] = '\0';
			if ((fmt = get_modinfo_value(f, key)) == NULL) {
				if (required || flag_verbose) {
					bb_error_msg("Warning: ignoring %s, no such parameter in this module", *argv);
					continue;
				}
			}
			key += 5;

			if (isdigit(*fmt)) {
				min = strtoul(fmt, &fmt, 10);
				if (*fmt == '-')
					max = strtoul(fmt + 1, &fmt, 10);
				else
					max = min;
			} else
				min = max = 1;
		} else { /* not m_has_modinfo */
			memcpy(key, *argv, n);
			key[n] = '\0';

			if (isdigit(*input))
				fmt = "i";
			else
				fmt = "s";
			min = max = 0;
		}

		sym = obj_find_symbol(f, key);

		/*
		 * Also check that the parameter was not
		 * resolved from the kernel.
		 */
		if (sym == NULL || sym->secidx > SHN_HIRESERVE) {
			bb_error_msg("symbol for parameter %s not found", key);
			return 0;
		}

		contents = f->sections[sym->secidx]->contents;
		loc = contents + sym->value;
		n = 1;

		while (*input) {
			char *str;

			switch (*fmt) {
				case 's':
				case 'c':
					/*
					 * Do C quoting if we begin with a ",
					 * else slurp the lot.
					 */
					if (*input == '"') {
						char *r;

						str = alloca(strlen(input));
						for (r = str, input++; *input != '"'; ++input, ++r) {
							if (*input == '\0') {
								bb_error_msg("improperly terminated string argument for %s", key);
								return 0;
							}
							/* else */
							if (*input != '\\') {
								*r = *input;
								continue;
							}
							/* else  handle \ */
							switch (*++input) {
								case 'a': *r = '\a'; break;
								case 'b': *r = '\b'; break;
								case 'e': *r = '\033'; break;
								case 'f': *r = '\f'; break;
								case 'n': *r = '\n'; break;
								case 'r': *r = '\r'; break;
								case 't': *r = '\t'; break;

								case '0':
								case '1':
								case '2':
								case '3':
								case '4':
								case '5':
								case '6':
								case '7':
									  c = *input - '0';
									  if ('0' <= input[1] && input[1] <= '7') {
										  c = (c * 8) + *++input - '0';
										  if ('0' <= input[1] && input[1] <= '7')
											  c = (c * 8) + *++input - '0';
									  }
									  *r = c;
									  break;

								default: *r = *input; break;
							}
						}
						*r = '\0';
						++input;
					} else {
						/*
						 * The string is not quoted.
						 * We will break it using the comma
						 * (like for ints).
						 * If the user wants to include commas
						 * in a string, he just has to quote it
						 */
						char *r;

						/* Search the next comma */
						if ((r = strchr(input, ',')) != NULL) {
							/*
							 * Found a comma
							 * Recopy the current field
							 */
							str = alloca(r - input + 1);
							memcpy(str, input, r - input);
							str[r - input] = '\0';
							/* Keep next fields */
							input = r;
						} else {
							/* last string */
							str = input;
							input = "";
						}
					}

					if (*fmt == 's') {
						/* Normal string */
						obj_string_patch(f, sym->secidx, loc - contents, str);
						loc += tgt_sizeof_char_p;
					} else {
						/* Array of chars (in fact, matrix !) */
						long charssize; /* size of each member */

						/* Get the size of each member */
						/* Probably we should do that outside the loop ? */
						if (!isdigit(*(fmt + 1))) {
							bb_error_msg("parameter type 'c' for %s must be followed by"
									" the maximum size", key);
							return 0;
						}
						charssize = strtoul(fmt + 1, (char **) NULL, 10);

						/* Check length */
						if (strlen(str) >= charssize-1) {
							bb_error_msg("string too long for %s (max %ld)",
									key, charssize - 1);
							return 0;
						}
						/* Copy to location */
						strcpy((char *) loc, str);      /* safe, see check above */
						loc += charssize;
					}
					/*
					 * End of 's' and 'c'
					 */
					break;

				case 'b':
					*loc++ = strtoul(input, &input, 0);
					break;

				case 'h':
					*(short *) loc = strtoul(input, &input, 0);
					loc += tgt_sizeof_short;
					break;

				case 'i':
					*(int *) loc = strtoul(input, &input, 0);
					loc += tgt_sizeof_int;
					break;

				case 'l':
					*(long *) loc = strtoul(input, &input, 0);
					loc += tgt_sizeof_long;
					break;

				default:
					bb_error_msg("unknown parameter type '%c' for %s",
							*fmt, key);
					return 0;
			}
			/*
			 * end of switch (*fmt)
			 */

			while (*input && isspace(*input))
				++input;
			if (*input == '\0')
				break; /* while (*input) */
			/* else */

			if (*input == ',') {
				if (max && (++n > max)) {
					bb_error_msg("too many values for %s (max %d)", key, max);
					return 0;
				}
				++input;
				/* continue with while (*input) */
			} else {
				bb_error_msg("invalid argument syntax for %s: '%c'",
						key, *input);
				return 0;
			}
		} /* end of while (*input) */

		if (min && (n < min)) {
			bb_error_msg("too few values for %s (min %d)", key, min);
			return 0;
		}
	} /* end of for (;argc > 0;) */

	return 1;
}

#ifdef CONFIG_FEATURE_INSMOD_LOAD_MAP
static void print_load_map(struct obj_file *f)
{
	struct obj_symbol *sym;
	struct obj_symbol **all, **p;
	struct obj_section *sec;
	int i, nsyms, *loaded;

	/* Report on the section layout.  */

	printf("Sections:       Size      %-*s  Align\n",
			(int) (2 * sizeof(void *)), "Address");

	for (sec = f->load_order; sec; sec = sec->load_next) {
		int a;
		unsigned long tmp;

		for (a = -1, tmp = sec->header.sh_addralign; tmp; ++a)
			tmp >>= 1;
		if (a == -1)
			a = 0;

		printf("%-15s %08lx  %0*lx  2**%d\n",
				sec->name,
				(long)sec->header.sh_size,
				(int) (2 * sizeof(void *)),
				(long)sec->header.sh_addr,
				a);
	}
#ifdef CONFIG_FEATURE_INSMOD_LOAD_MAP_FULL
	/* Quick reference which section indicies are loaded.  */

	loaded = alloca(sizeof(int) * (i = f->header.e_shnum));
	while (--i >= 0)
		loaded[i] = (f->sections[i]->header.sh_flags & SHF_ALLOC) != 0;

	/* Collect the symbols we'll be listing.  */

	for (nsyms = i = 0; i < HASH_BUCKETS; ++i)
		for (sym = f->symtab[i]; sym; sym = sym->next)
			if (sym->secidx <= SHN_HIRESERVE
					&& (sym->secidx >= SHN_LORESERVE || loaded[sym->secidx]))
				++nsyms;

	all = alloca(nsyms * sizeof(struct obj_symbol *));

	for (i = 0, p = all; i < HASH_BUCKETS; ++i)
		for (sym = f->symtab[i]; sym; sym = sym->next)
			if (sym->secidx <= SHN_HIRESERVE
					&& (sym->secidx >= SHN_LORESERVE || loaded[sym->secidx]))
				*p++ = sym;

	/* And list them.  */
	printf("\nSymbols:\n");
	for (p = all; p < all + nsyms; ++p) {
		char type = '?';
		unsigned long value;

		sym = *p;
		if (sym->secidx == SHN_ABS) {
			type = 'A';
			value = sym->value;
		} else if (sym->secidx == SHN_UNDEF) {
			type = 'U';
			value = 0;
		} else {
			sec = f->sections[sym->secidx];

			if (sec->header.sh_type == SHT_NOBITS)
				type = 'B';
			else if (sec->header.sh_flags & SHF_ALLOC) {
				if (sec->header.sh_flags & SHF_EXECINSTR)
					type = 'T';
				else if (sec->header.sh_flags & SHF_WRITE)
					type = 'D';
				else
					type = 'R';
			}
			value = sym->value + sec->header.sh_addr;
		}

		if (ELFW(ST_BIND) (sym->info) == STB_LOCAL)
			type = tolower(type);

		printf("%0*lx %c %s\n", (int) (2 * sizeof(void *)), value,
				type, sym->name);
	}
#endif
}

#endif

#if defined(COMMON_3264) && defined(ONLY_32)
#define INSMOD_MAIN insmod_main_2_4_32      /* 32 bit version */
#elif defined(COMMON_3264) && defined(ONLY_64)
#define INSMOD_MAIN insmod_main_2_4_64      /* 64 bit version */
#else
#define INSMOD_MAIN insmod_main_2_4         /* Not common code */
#endif

int INSMOD_MAIN(int fp, int argc, char **argv, char *m_name, char *_filename)
{
	struct obj_file *f;
#ifdef CONFIG_FEATURE_INSMOD_VERSION_CHECKING
	int k_crcs;
	char m_strversion[STRVERSIONLEN];
	int m_version;
	int m_crcs;
#endif
	unsigned long m_size;
	ElfW(Addr) m_addr;
	int gpl;
	struct obj_section *sec;
        m_filename = _filename;

	if ((f = obj_load(fp, ET_REL, m_filename)) == NULL)
		bb_perror_msg_and_die("Could not load the module");

	sec = obj_find_section(f, ".modinfo");
	if (sec)
		m_has_modinfo = 1;

	/* Version correspondence?  */
#ifdef CONFIG_FEATURE_INSMOD_VERSION_CHECKING
	m_version = get_module_version(f, m_strversion);
	if (m_version == -1) {
		bb_error_msg_and_die("couldn't find the kernel version the module was compiled for");
	}
#endif /* CONFIG_FEATURE_INSMOD_VERSION_CHECKING */

	if (!new_get_kernel_symbols())
		return EXIT_FAILURE;

#ifdef CONFIG_FEATURE_INSMOD_VERSION_CHECKING
	k_crcs = is_kernel_checksummed();
	m_crcs = is_module_checksummed(f);
	if ((m_crcs == 0 || k_crcs == 0) &&
			strncmp(k_strversion, m_strversion, STRVERSIONLEN) != 0) {
		if (flag_force_load) {
			bb_error_msg("Warning: kernel-module version mismatch\n"
					"\t%s was compiled for kernel version %s\n"
					"\twhile this kernel is version %s",
					m_name, m_strversion, k_strversion);
		} else {
			if (!flag_quiet)
				bb_error_msg("kernel-module version mismatch\n"
						"\t%s was compiled for kernel version %s\n"
						"\twhile this kernel is version %s.",
						m_name, m_strversion, k_strversion);
			return EXIT_FAILURE;
		}
	}
	if (m_crcs != k_crcs)
		obj_set_symbol_compare(f, ncv_strcmp, ncv_symbol_hash);
#endif /* CONFIG_FEATURE_INSMOD_VERSION_CHECKING */

	/* Let the module know about the kernel symbols.  */
	gpl = obj_gpl_license(f, NULL) == 0;
	add_kernel_symbols(f, gpl);

	/* Allocate common symbols, symbol tables, and string tables.  */
	if (!new_create_this_module(f, m_name))
		return EXIT_FAILURE;

	if (!obj_check_undefineds(f, 0)) {
		return EXIT_FAILURE;
	}
	obj_allocate_commons(f);
#ifdef CONFIG_FEATURE_CHECK_TAINTED_MODULE
	check_tainted_module(f, m_name);
#endif

	if (argc > 1) {
		if (!process_module_arguments(f, argc - 1, argv + 1, 1))
			return EXIT_FAILURE;
	}

	arch_create_got(f);
	hide_special_symbols(f);

#ifdef CONFIG_FEATURE_INSMOD_KSYMOOPS_SYMBOLS
	add_ksymoops_symbols(f, m_filename, m_name);
#endif /* CONFIG_FEATURE_INSMOD_KSYMOOPS_SYMBOLS */

	new_create_module_ksymtab(f);

	/* Find current size of the module */
	m_size = obj_load_size(f);

	m_addr = create_module(m_name, m_size);
	if (m_addr == -1) switch (errno) {
		case EEXIST:
			bb_error_msg("A module named %s already exists", m_name);
			return EXIT_FAILURE;
		case ENOMEM:
			bb_error_msg("Can't allocate kernel memory for module; needed %lu bytes", m_size);
			return EXIT_FAILURE;
		default:
			bb_perror_msg("create_module: %s", m_name);
			return EXIT_FAILURE;
	}

	if (!obj_relocate(f, m_addr)) {
		delete_module(m_name);
		return EXIT_FAILURE;
	}

	if (!new_init_module(m_name, f, m_size))
	{
		delete_module(m_name);
		return EXIT_FAILURE;
	}

#ifdef CONFIG_FEATURE_INSMOD_LOAD_MAP
	if(flag_print_load_map)
		print_load_map(f);
#endif

	return EXIT_SUCCESS;
}

#if defined(COMMON_3264) && defined(ONLY_64)
int insmod_main_2_4_32(int fp, int argc, char **argv, char *m_name, char *_filename);

int insmod_main_2_4(int fp, int argc, char **argv, char *m_name, char *_filename)
{
	if (arch64())
		return insmod_main_2_4_64(fp, argc, argv, m_name, _filename);
	else
		return insmod_main_2_4_32(fp, argc, argv, m_name, _filename);
}
#endif  /* defined(COMMON_3264) && defined(ONLY_64) */

