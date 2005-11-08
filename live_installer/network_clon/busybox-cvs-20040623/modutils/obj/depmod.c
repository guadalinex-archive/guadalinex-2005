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
#include "module.h"
#include "obj.h"
#include "modstat.h"
#include "../modutils.h"

#define ALLOC_MODULE	5000	/* Number of modules we can handle... */
#define ALLOC_SYM	10000	/* Number of symbols allocated per chunk */
#define MAX_MAP_SYM	100000	/* Maximum number of symbols in map */

typedef struct SYMBOL{
	struct SYMBOL *next;	/* List connected via hashing */
	struct MODULE *module;	/* Module declaring this symbol */
	const char *name;
	unsigned short hashval;
	unsigned short hashval_nosuffix;
	unsigned short length_nosuffix;
	char status;
} SYMBOL;

typedef struct MODULE {
	char *name;
	int resolved;
	struct {	/* Defined + required symbols for this module */
		SYMBOL **symtab;
		int n_syms;
	} defsym, undefs;
	char **generic_string;
	int n_generic_string;
	struct pci_device_id *pci_device;
	int n_pci_device;
	struct isapnp_device_id *isapnp_device;
	int n_isapnp_device;
	struct isapnp_card_id *isapnp_card;
	int n_isapnp_card;
	struct usb_device_id *usb_device;
	int n_usb_device;
	struct parport_device_id *parport_device;
	int n_parport_device;
	struct ieee1394_device_id *ieee1394_device;
	int n_ieee1394_device;
	struct pnpbios_device_id *pnpbios_device;
	int n_pnpbios_device;
} MODULE;

typedef enum SYM_STATUS {
	SYM_UNDEF,	/* Required by another module */
	SYM_DEFINED,	/* We have found a module that knows about it */
	SYM_RESOLVED
} SYM_STATUS;

typedef struct LIST_SYMBOL {
	struct LIST_SYMBOL *next;
	SYMBOL alloc[ALLOC_SYM];
} LIST_SYMBOL;

/*==================================================*/

static MODULE modules[ALLOC_MODULE];
static int n_modules;
static SYMBOL *symhash[2048];
static SYMBOL *symavail;
static SYMBOL *maxsyms;
static LIST_SYMBOL *chunk;

/*
 *	Create a symbol definition.
 *	Add defined symbol to the head of the list of defined symbols.
 *	Return the new symbol.
 */
static SYMBOL *addsym(const char *name, MODULE *module, SYM_STATUS status,
		      int ignore_suffix)
{
	SYMBOL added;
	SYMBOL *me;
	unsigned short hashval = 0;
	const char *pt = name, *pt_R;
	char *p;
	int l;

	for (pt = name; *pt; ++pt)
		hashval = (hashval << 1) ^ *pt;
	hashval %= 2048;

	memset(&added, 0, sizeof(SYMBOL));
	added.module = module;
	added.name = bb_xstrdup(name);
	added.hashval = hashval;
	added.status = status;

	/* Compute the hash without the trailing genksym suffix if required */
	if (ignore_suffix && (pt_R = strstr(name, "_R"))) {
		while ((p = strstr(pt_R+2, "_R"))) pt_R = p;
		l = strlen(pt_R);
		if (l >= 10) {	/* May be _R.*xxxxxxxx */
			(void)strtoul(pt_R+l-8, &p, 16);
			if (!*p) { /* It is _R.*xxxxxxxx */
				added.length_nosuffix = pt_R - name;
				hashval = 0;
				for (pt = name; pt < pt_R; ++pt)
					hashval = (hashval << 1) ^ *pt;
				hashval %= 2048;
				added.hashval_nosuffix = hashval;
			}
		}
	}
	else {
		added.hashval_nosuffix = 0;
		added.length_nosuffix = 0;
	}

	if (status == SYM_DEFINED) {
		if (symavail == NULL ||
		    symavail == maxsyms) {
			/*
			 * Allocate a new buffer for the collection of symbols
			 */
			LIST_SYMBOL *list = (LIST_SYMBOL *)xmalloc(sizeof(LIST_SYMBOL));
			list->next = chunk;
			chunk = list;
			symavail = list->alloc;
			maxsyms = list->alloc + ALLOC_SYM;
		}
		me = symavail++;
		*me = added;
		me->next = symhash[added.hashval];
		symhash[added.hashval] = me;
	} else {
		me = (SYMBOL *)xmalloc(sizeof(SYMBOL));
		*me = added;
	}

	return me;
}

/************************************************************
 * Module.c
 */

static void resolve(void)
{
	MODULE *mod;
	SYMBOL **sym;
	int i;
	int n;
	int resolved;

	/*
	 * Resolve, starting with the most recently added
	 * module symbol definitions.
	 */
	if (n_modules <= 0)
		return;
	/* else */
	for (i = n_modules - 1, mod = modules + i; i >= 0; --i, --mod) {
		if (mod->resolved)
			continue;

		for (resolved = 1, n = 0, sym = mod->undefs.symtab;
		     n < mod->undefs.n_syms; ++sym, ++n) {
			SYMBOL *psym = *sym, *find;
			const char *name;
			unsigned short hashval;
			unsigned short hashval_nosuffix;
			unsigned short length_nosuffix;

			if (psym->status != SYM_UNDEF)
				continue;

			/* This is an undefined symbol.   Find first
			   definition.  Symbol hash linked list has
			   newest first, so we want the last match in
			   the chain.  In particular, this prefers
			   kernel symbols to that of any module.

			   If we are ignoring the genksyms suffix in this
			   comparision then there are two hashes to try.
			 */

			name = psym->name;
			hashval = psym->hashval;
			hashval_nosuffix = psym->hashval_nosuffix;
			length_nosuffix = psym->length_nosuffix;
#ifdef	ARCH_ppc64
		look_again:
#endif
			find = symhash[hashval];
			while (find) {
				if (strcmp(find->name, name) == 0) {
					psym->status = SYM_RESOLVED;
					psym->module = find->module;
				}
				find = find->next;
			}
			if (psym->status == SYM_UNDEF && length_nosuffix) {
				/* Try again ignoring the suffix */
				find = symhash[hashval_nosuffix];
				while (find) {
					if (strncmp(find->name, name,
					    length_nosuffix) == 0 &&
					    !find->name[length_nosuffix]) {
						psym->status = SYM_RESOLVED;
						psym->module = find->module;
					}
					find = find->next;
				}
			}
			if (psym->status == SYM_UNDEF) {
#ifdef	ARCH_ppc64
				/* ppc64 is one of those architectures with
				   function descriptors.  A function is
				   exported and accessed across object
				   boundaries via its function descriptor.
				   The function code symbol happens to be the
				   function name, prefixed with '.', and a
				   function call is a branch to the code
				   symbol.  The linker recognises when a call
				   crosses object boundaries, and inserts a
				   stub to call via the function descriptor.
				   obj_ppc64.c of course does the same thing,
				   so here we recognise that an undefined code
				   symbol can be satisfied by the
				   corresponding function descriptor symbol.
				*/
				if (name == psym->name && name[0] == '.') {
					const char *pt;

					name++;
					if (length_nosuffix != 0)
						length_nosuffix--;
					hashval = 0;
					for (pt = name; *pt; ++pt) {
						if (pt == name + length_nosuffix)
							hashval_nosuffix = hashval;
						hashval = (hashval << 1) ^ *pt;
					}
					hashval %= 2048;
					hashval_nosuffix %= 2048;
					goto look_again;
				}
#endif	/* ARCH_ppc64 */
				resolved = 0;
			}
		}
		mod->resolved = resolved;
	}
}

static const char has_kernel_changed[] =
	"\n\tIt is likely that the kernel structure has changed, if so then"
	"\n\tyou probably need a new version of modutils to handle this kernel."
	"\n\tCheck linux/Documentation/Changes.";

/*
 *	Extract any generic string from the given symbol.
 *	Note: assumes same machine and arch for depmod and module.
 */
static void extract_generic_string(struct obj_file *f, MODULE *mod)
{
	struct obj_section *sec;
	char *p, *ep, *s, **latest;

	sec = obj_find_section(f, ".modstring");
	if (sec == NULL)
		return;
	p = sec->contents;
	ep = p + sec->header.sh_size;
	while (p < ep) {
		s = p;
		while (*p != '\0' && p < ep)
			p++;
		if (p >= ep) {
			bb_error_msg("unterminated generic string '%*s'", (int)(p - s), s);
			break;
		}
		if (p++ == s)		/* empty string? */
			continue;
		mod->generic_string = xrealloc(mod->generic_string, ++(mod->n_generic_string)*sizeof(*(mod->generic_string)));
		latest = mod->generic_string + mod->n_generic_string-1;
		*latest = bb_xstrdup(s);
	}
}

/*
 *	Read the symbols in an object and register them in the symbol table.
 *	Return -1 if there is an error.
 */
static int loadobj(const char *objname, int ignore_suffix)
{
	static char *currdir;
	static int currdir_len;
	int fp;
	MODULE *mod;
	SYMBOL *undefs[5000];
	SYMBOL *defsym[5000];
	struct obj_file *f;
	struct obj_section *sect;
	struct obj_symbol *objsym;
	int i;
	int is_2_2;
	int ksymtab;
	int len;
	int n_undefs = 0;
	int n_defsym = 0;
	char *p;
	void *image;
	unsigned long m_size;

	p = strrchr(objname, '/');
	len = 1 + (int)(p - objname);

	if ((fp = open(objname, O_RDONLY)) < 0)
		return 1;

	if (!(f = obj_load(fp, ET_REL, objname))) {
		close(fp);
		return -1;
	}
	close(fp);

	/*
	 * Allow arch code to define _GLOBAL_OFFSET_TABLE_
	 */
	arch_create_got(f);

	/*
	 * If we have changed base directory
	 * then use the defined symbols from modules
	 * in the _same_ directory to resolve whatever
	 * undefined symbols there are.
	 *
	 * This strategy ensures that we will have
	 * as correct dependencies as possible,
	 * even if the same symbol is defined by
	 * other modules in other directories.
	 */
	if (currdir_len != len || currdir == NULL ||
	    strncmp(currdir, objname, len) != 0) {
		if (currdir)
			resolve();
		currdir = bb_xstrdup(objname);
		currdir_len = len;
	}

	mod = modules + n_modules++;
	mod->name = bb_xstrdup(objname);

	if ((sect = obj_find_section(f, "__ksymtab")) != NULL)
		ksymtab = sect->idx; /* Only in 2.2 (or at least not 2.0) */
	else
		ksymtab = -1;

	if (sect ||
	    obj_find_section(f, ".modinfo") ||
	    obj_find_symbol(f, "__this_module"))
		is_2_2 = 1;
	else
		is_2_2 = 0;

	for (i = 0; i < HASH_BUCKETS; ++i) {
		for (objsym = f->symtab[i]; objsym; objsym = objsym->next) {
			if (objsym->secidx == SHN_UNDEF) {
				if (ELFW(ST_BIND)(objsym->info) != STB_WEAK &&
				    objsym->r_type /* assumes that R_arch_NONE is always 0 on all arch */) {
					undefs[n_undefs++] = addsym(objsym->name,
								    mod,
								    SYM_UNDEF,
								    ignore_suffix);
				}
				continue;
			}
			/* else */

			if (is_2_2 && ksymtab != -1) {
				/* A 2.2 module using EXPORT_SYMBOL */
				if (objsym->secidx == ksymtab &&
				    ELFW(ST_BIND)(objsym->info) == STB_GLOBAL) {
					/* Ignore leading "__ksymtab_" */
					defsym[n_defsym++] = addsym(objsym->name + 10,
								    mod,
								    SYM_DEFINED,
								    ignore_suffix);
				}
				continue;
			}
			/* else */

			if (is_2_2) {
				/*
				 * A 2.2 module _not_ using EXPORT_SYMBOL
				 * It might still want to export symbols,
				 * although strictly speaking it shouldn't...
				 * (Seen in pcmcia)
				 */
				if (ELFW(ST_BIND)(objsym->info) == STB_GLOBAL) {
					defsym[n_defsym++] = addsym(objsym->name,
								    mod,
								    SYM_DEFINED,
								    ignore_suffix);
				}
				continue;
			}
			/* else */

			/* Not undefined or 2.2 ksymtab entry */
			if (objsym->secidx < SHN_LORESERVE
			/*
			 * The test below is removed for 2.0 compatibility
			 * since some 2.0-modules (correctly) hide the
			 * symbols it exports via register_symtab()
			 */
			/* && ELFW(ST_BIND)(objsym->info) == STB_GLOBAL */
			     ) {
				defsym[n_defsym++] = addsym(objsym->name,
							    mod,
							    SYM_DEFINED,
							    ignore_suffix);
			}
		}
	}

	/*
	 *	Finalize the information about a module.
	 */
	mod->defsym.n_syms = n_defsym;
	if (n_defsym > 0) {
		int size = n_defsym * sizeof(SYMBOL *);

		mod->defsym.symtab = (SYMBOL **)xmalloc(size);
		memcpy(mod->defsym.symtab, defsym, size);
	} else
		mod->defsym.symtab = NULL;

	mod->undefs.n_syms = n_undefs;
	if (n_undefs > 0) {
		int size = n_undefs * sizeof(SYMBOL *);

		mod->undefs.symtab = (SYMBOL **) xmalloc(size);
		memcpy(mod->undefs.symtab, undefs, size);
		mod->resolved = 0;
	} else {
		mod->undefs.symtab = NULL;
		mod->resolved = 1;
	}

	/* Do a pseudo relocation to base address 0x1000 (arbitrary).
	 * All undefined symbols are treated as absolute 0.  This builds
	 * enough of a module to allow extraction of internal data such
	 * as device tables.
	 */
	obj_clear_undefineds(f);
	obj_allocate_commons(f);
	m_size = obj_load_size(f);
	if (!obj_relocate(f, 0x1000)) {
		bb_error_msg("depmod obj_relocate failed\n");
		return(-1);
	}
	extract_generic_string(f, mod);
	image = xmalloc(m_size);
	obj_create_image(f, image);
	free(image);

	obj_free(f);
	return 0;
}

/*
 * Add the symbol defined in the kernel, simulating a pseudo module.
 * Create a dummy module entry in the list of modules.
 * This module is used to handle the exported kernel symbols.
 *
 * Load from the currently running kernel
 * OR
 * from a file containing kernel symbols from a different kernel.
 * The file can be either a "System.map" file or a copy of "/proc/ksyms".
 *
 * Return -1 if any error.
 */
static int addksyms(const char *file_syms)
{
	FILE *fp;
	MODULE *mod = modules + n_modules++;
	SYMBOL *symtab[MAX_MAP_SYM];
	struct module_symbol *ksym;
	unsigned int so_far = 0;
	int is_mapfile = 0;
	int n_syms = 0;
	int size;
	char line[PATH_MAX];
	char *p;

	mod->name = "-";
	mod->defsym.n_syms = 0;
	mod->defsym.symtab = NULL;
	mod->undefs.n_syms = 0;
	mod->undefs.symtab = NULL;

	/* Fake the _mod_use_count_ symbol provided by insmod */
	/* Dummy */
	symtab[n_syms++] = addsym("mod_use_count_", mod, SYM_DEFINED, 0);
	symtab[n_syms++] = addsym("__this_module", mod, SYM_DEFINED, 0);

	/*
	 * Specification: depmod / kernel syms only
	 * When initialising its symbol table from the kernel
	 * depmod silently discards all symbol from loaded modules.
	 *
	 * This means that depmod may be used at any time to compute
	 * the dependancy table, even if there are modules already
	 * loaded.
	 *
	 * depmod use the kernel system call to obtain the
	 * symbol table, not /proc/ksyms.
	 */

	if (file_syms) {
		if ((fp = fopen(file_syms, "r")) == NULL) {
			bb_error_msg("Can't read %s", file_syms);
			return -1;
		}
		if (!fgets(line, sizeof(line), fp)) {
			fclose(fp);
			bb_error_msg("premature EOF on %s", file_syms);
			return -1;
		}

		p = strtok(line, " \t\n");
		if (!p) {
			fclose(fp);
			bb_error_msg("Illegal format of %s", file_syms);
			return -1;
		}
		if (!isspace(*line))	/* Adressless symbol? */
			p = strtok(NULL, " \t\n");
		/* The second word is either the symbol name or a type */
		if (p && p[0] && !p[1]) { /* System.map */
			is_mapfile = 1;
			p = strtok(NULL, " \t\n");
		} else { /* /proc/ksyms copy */
			if (p && strtok(NULL, " \t\n"))
				p = NULL;
		}

		if (p && strncmp(p, "GPLONLY_", 8) == 0)
			p += 8;
		if (p)
			symtab[n_syms++] = addsym(p, mod, SYM_DEFINED, 0);

		while (fgets(line, sizeof(line), fp)) {
			if (!is_mapfile && strchr(line, '['))
				continue;
			p = strtok(line, " \t\n"); /* Skip first word */
			if (!isspace(*line))	/* Adressless symbol? */
				p = strtok(NULL, " \t\n");
			if (is_mapfile) {
				if (!p || !p[0] || p[1])
					continue;
				p = strtok(NULL, " \t\n");
				/* Sparc has symbols like '.div' that need to be
				 * exported.  They strip the '.' and prefix the
				 * symbol with '__sparc_dot_'.  There is no need
				 * for 'sparc' in the name, other systems with
				 * the same problem should use just '__dot_'.
				 * Support both prefixes, ready for other
				 * systems.
				 * */
				if (!strncmp(p, "__kstrtab___dot_", 15)) {
					p += 15;
					*p = '.';
				}
				else if (!strncmp(p, "__kstrtab___sparc_dot_", 21)) {
					p += 21;
					*p = '.';
				}
				else if (!strncmp(p, "__kstrtab_", 10))
					p += 10;
				else if (!strncmp(p, "__export_priv_", 14)) {
					/* Sparc has some weird exported symbols marked
					 * __export_priv_ instead of the normal __kstrtab_.
					 * Replace the 'v' with '_' and point at the start of
					 * the '__' before the name.  I see no good reason
					 * to use __export_priv_, but for compatibility with
					 * old sparc kernels, it is supported.  Try to remove
					 * __export_priv_ from sparc and remove this code
					 * four releases after the last kernel that uses
					 * __export_priv_.
					 */
					p += 12;
					*p = '_';
				} else
					continue;
			}
			if (strncmp(p, "GPLONLY_", 8) == 0)
				p += 8;
			assert(n_syms < MAX_MAP_SYM);
			symtab[n_syms++] = addsym(p, mod, SYM_DEFINED, 0);
		}
		fclose(fp);
	} else { /* Use the exported symbols from currently running kernel */
		if (!new_get_kernel_info(K_SYMBOLS))
			return -1;

		for (ksym = ksyms; so_far < nksyms; ++so_far, ksym++) {
			if (strncmp((char *)ksym->name, "GPLONLY_", 8) == 0)
				ksym->name = ((char *)ksym->name) + 8;
			assert(n_syms < MAX_MAP_SYM);
			symtab[n_syms++] = addsym((char *)ksym->name, mod, SYM_DEFINED, 0);
		}
	}

	size = n_syms * sizeof(SYMBOL *);
	mod->defsym.symtab = (SYMBOL **)xmalloc(size);
	mod->defsym.n_syms = n_syms;
	memcpy(mod->defsym.symtab, symtab, size);

	return 0;
}

/*
 *	Format the dependancy list of a module into a simple makefile.
 *	Print the dependancies in the depfile (or stdout if nflag is true).
 *	Print the pcimap in the pcimapfile (or stdout if nflag is true).
 *	Print the isapnpmap in the isapnpmapfile (or stdout if nflag is true).
 *	Print the usbmap in the usbmapfile (or stdout if nflag is true).
 *
 *	Documented limits which utilities can rely on.
 *	  No field name on a header line will be longer than 20 characters.  The
 *	  data in a field can be longer than 20 characters but the name will not.
 *	  No single line in a generated file will be longer than PATH_MAX-1
 *	  characters.
 */

static int prtdepend(const char *base_dir, const char *module_dir, int nflag)
{
	FILE *dep = stdout;
	FILE *generic_string = stdout;
	MODULE **tbdep;
	MODULE *ptmod;
	int i;
	int ret = 0;
	int skipchars;	/* For depmod -a in image of a tree */
	char *tmdn;

	tmdn = concat_path_file(module_dir, "modules.dep");

	if (!nflag) {
		dep = fopen(tmdn, "w");
	}

	skipchars = strlen(base_dir);
	tbdep = (MODULE **)alloca(sizeof(MODULE) * n_modules);

	ptmod = modules;
	for (i = 0; i < n_modules; i++, ptmod++) {
		SYMBOL **undefs = ptmod->undefs.symtab;
		int n_undefs = ptmod->undefs.n_syms;
		int nbdepmod = 0;
		int nberr = 0;
		int e;
		int m;

		/* Don't print a dependency list for the _kernel_ ... */
		if (strcmp(ptmod->name, "-") == 0)
			continue;

		/* Look for unresolved symbols in this module */
		for (e = 0; e < n_undefs; e++, undefs++) {
			MODULE *mod = (*undefs)->module;

			if ((*undefs)->status == SYM_RESOLVED) {
				/* Resolved by self or exported kernel symbol */
				if (strcmp(mod->name, ptmod->name) == 0 ||
				    strcmp(mod->name, "-") == 0)
					continue;

				/* No duplicate dependencies, please */
				for (m = 0; m < nbdepmod; m++) {
					if (tbdep[m] == mod)
						break;
				}
				if (m == nbdepmod)
					tbdep[nbdepmod++] = mod;
			} else {
				/* Kludge to preserve old depmod behaviour without -u.
				 * 2.4.13 change to keep going had the unwanted side
				 * effect of always treating unresolved symbols as an
				 * error.  Use the error() routine but do not count
				 * any errors.  Remove in 2.5.
				 */
				if (!flag_quiet && nberr == 0)
					bb_error_msg("*** Unresolved symbols in %s",
					      ptmod->name);
				if (!flag_quiet && flag_showerror)
						bb_error_msg("\t%s", (*undefs)->name);
				nberr++;
				if (flag_unresolved_error)
					ret = 1;
			}
		}

		fprintf(dep, "%s:", ptmod->name + skipchars);
		for (m = 0; m < nbdepmod; m++) {
			if (m != 0 /*&& (m & 3) == 0*/)
				fprintf(dep, " \\\n");
			fprintf(dep, "\t%s", tbdep[m]->name + skipchars);
		}
		fprintf(dep, "\n\n");
	}

	if (generic_string != stdout)
		fclose(generic_string);
	/* Close depfile last, it's timestamp is critical */
	if (dep != stdout)
		fclose(dep);
	return ret;
}

static int add_module(const char *filename, struct stat *statbuf,
		void *userdata)
{
	if (!strncmp (&filename[strlen(filename)-2], ".o", 2))
		loadobj(filename, 0);
	return (FALSE);
}

#if defined(COMMON_3264) && defined(ONLY_32)
#define DEPMOD_MAIN depmod_main_2_4_32      /* 32 bit version */
#elif defined(COMMON_3264) && defined(ONLY_64)
#define DEPMOD_MAIN depmod_main_2_4_64      /* 64 bit version */
#else
#define DEPMOD_MAIN depmod_main_2_4         /* Not common code */
#endif

int DEPMOD_MAIN(int argc, char *argv[], int all, const char *system_map, const char *base_dir, const char *module_dir, const char *file_syms)
{
	int ret;

	if (all)
	{
		if ((ret = addksyms(file_syms)) != -1) {
			recursive_action(module_dir, TRUE, FALSE, FALSE, add_module, 0, NULL);
			resolve();
			ret = prtdepend(base_dir, module_dir, flag_dry) || ret;
		}
	}
	else
	{
		/* not stdmode */
		if ((ret = addksyms(file_syms)) != -1) {
			for (; argc > 0 && ret != -1; ++argv, --argc)
				loadobj(*argv, file_syms != NULL);
			resolve();
			ret = prtdepend(base_dir, module_dir, flag_dry) || ret;
		}
	}

	return ret;
}

#if defined(COMMON_3264) && defined(ONLY_64)
extern int depmod_main_2_4_32(int argc, char *argv[], int all, const char *system_map, const char *base_dir, const char *module_dir, const char *file_syms);

int depmod_main_2_4(int argc, char *argv[], int all, const char *system_map, const char *base_dir, const char *module_dir, const char *file_syms)
{
        if (arch64())
                return depmod_main_2_4_64(argc, argv, all, system_map, base_dir, module_dir, file_syms);
        else
                return depmod_main_2_4_32(argc, argv, all, system_map, base_dir, module_dir, file_syms);
}
#endif  /* defined(COMMON_3264) && defined(ONLY_64) */

