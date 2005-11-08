#include <stdarg.h>
#include <unistd.h>
#include "libbb.h"

#define error(args...)	bb_error_msg(args)

#define xstrdup		strdup

#define gzf_open	open
#define gzf_read	read
#define gzf_lseek	lseek
#define gzf_close	close

int arch64(void);
