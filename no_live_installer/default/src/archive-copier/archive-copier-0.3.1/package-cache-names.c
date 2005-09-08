#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <regex.h>

#define LENOF(a) (sizeof(a) / sizeof(*(a)))

/* Array manipulation helpers. */

#define ARRAY_APPEND(array, element) do { \
    if (n_##array >= alloc_##array) { \
	if (alloc_##array) \
	    alloc_##array *= 2; \
	else \
	    alloc_##array = 16; \
	array = xrealloc(array, alloc_##array * sizeof *array); \
    } \
    array[n_##array++] = element; \
} while (0)

#define ARRAY_SORT(array, compare) do { \
    if (n_##array) \
	qsort(array, n_##array, sizeof *array, &compare); \
} while (0)

#define ARRAY_SEARCH(array, compare, key) \
    (key && bsearch(&key, array, n_##array, sizeof *array, &compare))

const char *force_base_prefix = "linux-image-";

void die(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);

    exit(1);
}

void die_errno(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, ": %s", strerror(errno));

    exit(1);
}

void *xmalloc(size_t size)
{
    void *ret = malloc(size);
    if (!ret)
	die("malloc failed");
    return ret;
}

void *xrealloc(void *ptr, size_t size)
{
    void *ret = realloc(ptr, size);
    if (!ret)
	die("realloc failed");
    return ret;
}

char *get_paragraph(const char **p, size_t *left)
{
    static char *space = NULL;
    static size_t space_alloc = 0;
    const char *sep;
    size_t len;

    /* get next stanza */
    sep = memmem(*p, *left, "\n\n", 2);
    if (sep)
	len = sep - *p;
    else
	len = *left;

    /* make sure we have enough space */
    if (!space || len + 1 > space_alloc) {
	space = xrealloc(space, len + 1);
	space_alloc = len + 1;
    }

    /* copy, terminate, advance */
    memcpy(space, *p, len);
    space[len] = '\0';
    *p = sep;
    *left -= len;
    if (*p) {
	*p += 2; /* skip \n\n */
	*left -= 2;
    }

    return space;
}

char *encode_colons(const char *version)
{
    static char *space = NULL;
    static size_t space_alloc = 0;
    size_t len;
    const char *p;
    char *spacep;

    /* make sure we have enough space */
    len = strlen(version) * 3 + 1;
    if (!space || len > space_alloc) {
	space = xrealloc(space, len);
	space_alloc = len + 1;
    }

    /* encode colons; copy everything else */
    for (p = version, spacep = space; *p; ++p) {
	if (*p == ':') {
	    strcpy(spacep, "%3a");
	    spacep += 3;
	} else
	    *spacep++ = *p;
    }
    *spacep = '\0';

    return space;
}

int packages_compare(const void *a, const void *b)
{
    const char *key = *(const char **) a;
    const char *memb = *(const char **) b;
    return strcmp(key, memb);
}

/* Do any of these strings match any of these regular expressions? */
int match_any_regex(regex_t *regexes, size_t n_regexes,
		    const char **strs, size_t n_strs)
{
    int i, j;
    if (!strs || !regexes)
	return 0;
    for (i = 0; i < n_regexes; ++i)
	for (j = 0; j < n_strs; ++j)
	    if (regexec(&regexes[i], strs[j], 0, 0, 0) == 0)
		return 1;
    return 0;
}

void usage(FILE *to, const char *ego)
{
    fprintf(to, "Usage: %s -P <Packages> [options]\n"
		"Options:\n"
		"\t-P <Packages>\n"
		"\t-D <desktop task> (regex)\n"
		"\t-S <ship task> (regex)\n"
		"\t-b <base package name>\n"
		"\t-d <desktop package name>\n"
		"\t-s <ship package name>\n",
		ego);
}

int main(int argc, char **argv)
{
    /* base package names */
    size_t n_base_packages = 0, alloc_base_packages = 0;
    const char **base_packages = NULL;

    /* desktop package names */
    size_t n_desktop_packages = 0, alloc_desktop_packages = 0;
    const char **desktop_packages = NULL;

    /* ship package names */
    size_t n_ship_packages = 0, alloc_ship_packages = 0;
    const char **ship_packages = NULL;

    /* desktop tasks */
    size_t n_desktop_tasks = 0, alloc_desktop_tasks = 0;
    const char **desktop_tasks = NULL;
    regex_t *desktop_task_regexes = NULL;

    /* ship tasks */
    size_t n_ship_tasks = 0, alloc_ship_tasks = 0;
    const char **ship_tasks = NULL;
    regex_t *ship_task_regexes = NULL;

    const char *packages_filename = NULL;
    int packages_fd;
    struct stat packages_st;
    char *packages;
    const char *packages_p;
    size_t packages_left;
    char *para;

    for (;;) {
	int c = getopt(argc, argv, "P:D:S:b:d:s:");
	if (c == -1)
	    break;

	switch (c) {
	    case 'P':
		packages_filename = optarg;
		break;
	    case 'D':
		ARRAY_APPEND(desktop_tasks, optarg);
		break;
	    case 'S':
		ARRAY_APPEND(ship_tasks, optarg);
		break;
	    case 'b':
		ARRAY_APPEND(base_packages, optarg);
		break;
	    case 'd':
		ARRAY_APPEND(desktop_packages, optarg);
		break;
	    case 's':
		ARRAY_APPEND(ship_packages, optarg);
		break;
	    default:
		usage(stderr, argv[0]);
		exit(1);
		break;
	}
    }

    if (!packages_filename) {
	usage(stderr, argv[0]);
	exit(1);
    }

    if (desktop_tasks) {
	int i;
	desktop_task_regexes =
	    malloc(n_desktop_tasks * sizeof *desktop_task_regexes);
	for (i = 0; i < n_desktop_tasks; ++i) {
	    int err = regcomp(&desktop_task_regexes[i], desktop_tasks[i],
			      REG_EXTENDED | REG_NOSUB);
	    if (err != 0) {
		int errmsgsize = regerror(err, &desktop_task_regexes[i],
					  NULL, 0);
		char *errmsg = xmalloc(errmsgsize);
		regerror(err, &desktop_task_regexes[i], errmsg, errmsgsize);
		die("regcomp '%s' failed: %s", errmsg);
	    }
	}
    }

    if (ship_tasks) {
	int i;
	ship_task_regexes = malloc(n_ship_tasks * sizeof *ship_task_regexes);
	for (i = 0; i < n_ship_tasks; ++i) {
	    int err = regcomp(&ship_task_regexes[i], ship_tasks[i],
			      REG_EXTENDED | REG_NOSUB);
	    if (err != 0) {
		int errmsgsize = regerror(err, &ship_task_regexes[i], NULL, 0);
		char *errmsg = xmalloc(errmsgsize);
		regerror(err, &ship_task_regexes[i], errmsg, errmsgsize);
		die("regcomp '%s' failed: %s", errmsg);
	    }
	}
    }

    ARRAY_SORT(base_packages, packages_compare);
    ARRAY_SORT(desktop_packages, packages_compare);
    ARRAY_SORT(ship_packages, packages_compare);

    packages_fd = open(packages_filename, O_RDONLY);
    if (packages_fd < 0)
	die_errno("open %s failed", packages_filename);
    if (fstat(packages_fd, &packages_st) < 0)
	die_errno("stat %s failed", packages_filename);
    packages = mmap((void *) 0, packages_st.st_size, PROT_READ, MAP_SHARED,
		    packages_fd, 0);
    if (packages == MAP_FAILED)
	die_errno("mmap %s failed", packages_filename);

    packages_p = packages;
    packages_left = (size_t) packages_st.st_size;
    for (para = get_paragraph(&packages_p, &packages_left); *para;
	 para = get_paragraph(&packages_p, &packages_left)) {
	const char *line;
	const char *package = NULL, *version = NULL, *filename = NULL;
	size_t n_tasks = 0, alloc_tasks = 0;
	const char **tasks = NULL;

	for (line = strsep(&para, "\n"); line; line = strsep(&para, "\n")) {
	    if (!strncasecmp(line, "Package: ", 9))
		package = line + 9;

	    if (!strncasecmp(line, "Version: ", 9))
		version = encode_colons(line + 9);
	    else if (!strncasecmp(line, "Filename: ", 10))
		filename = line + 10;
	    else if (!strncasecmp(line, "Task: ", 6)) {
		char *value = strdup(line + 6);
		const char *task;

		free(tasks);
		/* split value on ", " into array */
		for (task = strsep(&value, ", "); task;
		     task = strsep(&value, ", ")) {
		    ARRAY_APPEND(tasks, task);
		    if (value) {
			/* advance over any duplicate delimiters */
			while (*value && strchr(", ", *value))
			    ++value;
		    }
		}
		free(value);
	    }
	}

	if (version && filename) {
	    const char *status;
	    if (ARRAY_SEARCH(base_packages, packages_compare, package))
		status = "base";
	    else if (!strncmp(package, force_base_prefix,
			      strlen(force_base_prefix)))
		status = "base";
	    else if (match_any_regex(desktop_task_regexes, n_desktop_tasks,
				     tasks, n_tasks))
		status = "desktop";
	    else if (ARRAY_SEARCH(desktop_packages, packages_compare, package))
		status = "desktop";
	    else if (match_any_regex(ship_task_regexes, n_ship_tasks,
				     tasks, n_tasks))
		status = "ship";
	    else if (ARRAY_SEARCH(ship_packages, packages_compare, package))
		status = "ship";
	    else
		status = "supported";
	    printf("%s %s %s\n", filename, version, status);
	}
    }

    munmap(packages, packages_st.st_size);
    close(packages_fd);

    return 0;
}
