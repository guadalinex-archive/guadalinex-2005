#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

int
sys_init_module(const char *name, const struct module *info)
{
	return init_module(name, info);
}

int arch64(void)
{
	struct utsname uts;
	char *uname_m;
	if (uname(&uts))
		return(0);
	if ((uname_m = getenv("UNAME_MACHINE"))) {
		int l = strlen(uname_m);
		if (l >= sizeof(uts.machine))
			l = sizeof(uts.machine)-1;
		memcpy(uts.machine, uname_m, l);
		uts.machine[l] = '\0';
	}
	return(strstr(uts.machine, "64") != NULL || strstr(uts.machine, "s390x") != NULL);
}

