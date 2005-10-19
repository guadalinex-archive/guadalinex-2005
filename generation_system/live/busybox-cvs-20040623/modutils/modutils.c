#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include "modutils.h"

int k_version;
char k_strversion[STRVERSIONLEN];

/* temporary */
int flag_force_load = 0;
int flag_autoclean = 0;
int flag_verbose = 0;
int flag_quiet = 0;
int flag_export = 1;
int flag_unresolved_error = 0;
int flag_showerror = 0;
int flag_quick = 0;
int flag_dry = 0;

/* Get the kernel version in the canonical integer form.  */

int get_kernel_version(char str[STRVERSIONLEN])
{
	char *p, *q;
	int a, b, c;
        struct utsname uts_info;

	if (uname (&uts_info))
		return -1;

	strncpy(str, uts_info.release, STRVERSIONLEN-1);
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

