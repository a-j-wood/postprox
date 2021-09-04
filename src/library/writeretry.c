/*
 * A write() utility function.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#include "config.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>


/*
 * Simple retrying write() call. Write the given buffer to the given file
 * descriptor, returning nonzero on error. Errors are logged.
 */
int write_retry(int fd, char *buf, int bufsize)
{
	ssize_t written;
	size_t count;

	if (fd < 0) {
		log_line(LOGPRI_ERROR, "%s",
			 _("attempted to write to negative fd"));
		return -1;
	}

	if (buf == NULL) {
		log_line(LOGPRI_ERROR, "%s",
			 _("attempted to write from null buffer"));
		return -1;
	}

	count = bufsize;
	while (count > 0) {
		written = write(fd, buf, count);
		if (written < 0) {
			log_line(LOGPRI_ERROR,
				 _("write failed to fd %d: %s"), fd,
				 strerror(errno));
			return -1;
		} else if (written == 0) {
			log_line(LOGPRI_ERROR,
				 _("write() wrote 0 bytes to fd %d"), fd);
			return -1;
		}
		buf += written;
		count -= written;
	}

	return 0;
}

/* EOF */
