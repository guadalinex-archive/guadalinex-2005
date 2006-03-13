/*
 * getpwnam.c - This file is part of the libc-8086/pwd package for ELKS,
 * Copyright (C) 1995, 1996 Nat Friedman <ndf@linux.mit.edu>.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "busybox.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "pwd_.h"


struct passwd *getpwnam(const char *name)
{
	int passwd_fd;
	struct passwd *passwd;

	if (name == NULL) {
		errno = EINVAL;
		return NULL;
	}

	if ((passwd_fd = open(bb_path_passwd_file, O_RDONLY)) < 0)
		return NULL;

	while ((passwd = __getpwent(passwd_fd)) != NULL)
		if (!strcmp(passwd->pw_name, name)) {
			close(passwd_fd);
			return passwd;
		}

	close(passwd_fd);
	return NULL;
}
