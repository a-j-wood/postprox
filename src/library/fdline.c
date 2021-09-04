/*
 * Functions to read single lines from a file descriptor, like fgets().
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#include "config.h"
#include "log.h"
#include "library.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>


struct fdline_s {
	int fd;				 /* file descriptor for line */
	char buf[1024];			 /* RATS: ignore (checked) - line buf */
	int bufused;			 /* number of bytes used in buffer */
	unsigned char lineready;	 /* set if a line is pre-buffered */
	unsigned char linestart;	 /* set if returned line is at start */
	unsigned char fd_eof;		 /* set if file descriptor is at EOF */
};


/*
 * Create a line reading object from the given file descriptor and return
 * it, or NULL on error.
 */
fdline_t fdline_open(int fd)
{
	fdline_t line;

	line = calloc(1, sizeof(*line));
	if (line == NULL)
		return NULL;

	line->fd = fd;
	line->bufused = 0;
	line->lineready = 0;
	line->linestart = 1;
	line->fd_eof = 0;

	return line;
}


/*
 * Destroy the given line reading object. This does NOT close the underlying
 * file descriptor.
 */
void fdline_close(fdline_t line)
{
	if (line == NULL)
		return;
	free(line);
}


/*
 * Return 1 if there is a line already waiting to be read from the given
 * line object, which will not trigger a read() on the underlying file
 * descriptor, 0 otherwise.
 */
int fdline_read_available(fdline_t line)
{
	if (line == NULL)
		return 0;
	if (line->fd_eof == 0)
		return line->lineready;
	if (line->bufused > 0)
		return 1;
	return 0;
}


/*
 * Return 1 if there is no more data to read and the underlying file
 * descriptor has reached EOF, 0 otherwise.
 */
int fdline_read_eof(fdline_t line)
{
	if (line == NULL)
		return 1;
	if (line->fd_eof == 0)
		return 0;
	if (line->lineready)
		return 0;
	return 1;
}


/*
 * Return 1 if the underlying file descriptor has reached EOF, 0 otherwise.
 */
int fdline_read_fdeof(fdline_t line)
{
	if (line == NULL)
		return 1;
	return line->fd_eof;
}


/*
 * Read a single line (\n terminated) from the given file descriptor and
 * store it in the given buffer (with the given maximum size). Up to
 * bufsize-1 bytes will be read, since the string will be null terminated. 
 *
 * For performance, we read in chunks of 1kb. This means that we might read
 * multiple lines in at once, and only return the first, so use
 * _read_available() above to check whether another line is available
 * without calling read() before deciding to select() on the file
 * descriptor. Also check whether the underlying file descriptor is at EOF
 * before adding it to select(), using _read_fdeof() above.
 *
 * Returns the length of the line read, including the terminating \r\n. This
 * may be zero if no data is currently available or there isn't a complete
 * line. A negative return value indicates an error (and the EOF flag is set
 * so we don't try reading any more).
 *
 * If "startline" is non-NULL, *startline will be set to 1 if the returned
 * line starts at the start, 0 if not. A returned line doesn't start at the
 * start if it is merely the continuation of a very long line that wouldn't
 * all fit in the buffer at once.
 *
 * Note that all this faffing about is just because we can't use fgets() on
 * a fdopen()ed file descriptor if we want to use select() as well.
 */
int fdline_read(fdline_t line, char *outbuf, int bufsize, int *startline)
{
	int linelen;
	char *ptr;

	outbuf[0] = 0;

	if (line == NULL)
		return 0;

	/*
	 * We don't have a line buffered up and ready, so read some more
	 * data.
	 */
	if ((line->lineready == 0) && (line->fd_eof == 0)) {
		int amountread;

		amountread = read(line->fd, /* RATS: ignore (checked) */
				  line->buf + line->bufused,
				  sizeof(line->buf) - line->bufused);

		if (amountread < 0) {
			/*
			 * Ignore "connection reset" and related errors;
			 * treat them as EOF.
			 */
			if ((errno == ECONNRESET)
			    || (errno == ENETDOWN)
			    || (errno == ENETRESET)
			    || (errno == ENETUNREACH)
			    || (errno == ENOTCONN)
			    ) {
				return 0;
			}
			log_line(LOGPRI_ERROR,
				 _("error reading from fd %d: %s"),
				 line->fd, strerror(errno));
			line->fd_eof = 1;
			return amountread;
		}

		if (amountread == 0) {
			line->fd_eof = 1;
		}

		line->bufused += amountread;
	}

	if (startline)
		*startline = line->linestart;

	/*
	 * Look for a newline in the buffered data. If one isn't found, then
	 * either there isn't a full line to read yet, or we've filled the
	 * buffer, so we have to return a partial line.
	 */
	ptr = memchr(line->buf, '\n', line->bufused);
	if (ptr == NULL) {
		if (line->fd_eof) {
			linelen = line->bufused;
		} else if (line->bufused < sizeof(line->buf)) {
			/*
			 * We've not yet filled the buffer, so return 0
			 * while we wait for more data to come in.
			 */
			return 0;
		} else {
			linelen = line->bufused;
		}
	} else {
		linelen = (ptr + 1) - line->buf;
	}

	/*
	 * Truncate our line to the size of our output buffer, leaving room
	 * for a zero byte to terminate the string.
	 */
	if (linelen > (bufsize - 1)) {
		linelen = bufsize - 1;
	}

	/*
	 * If the buffered data we've got ends in \r, then we have to leave
	 * that \r for the next buffer-load, in case it is part of a \r\n
	 * pair.
	 */
	if ((linelen > 0) && (line->buf[linelen - 1] == '\r')) {
		linelen--;
	}

	if (linelen < 1)
		return 0;

	/*
	 * Fill the output buffer with the line and zero terminate it, being
	 * careful not to overrun. If we have to truncate the line we're
	 * returning, we have to recalculate whether the line after this
	 * will start at the start of the line or not.
	 */
	memcpy(outbuf, line->buf, linelen);
	outbuf[linelen] = 0;

	/*
	 * Work out whether or not the next line we read after this one will
	 * start at the start of the line - i.e. set the linestart flag for
	 * the next line according to whether the line we're returning now
	 * contains a newline.
	 */
	ptr = memchr(outbuf, '\n', linelen);
	line->linestart = (ptr == NULL) ? 0 : 1;

	/*
	 * If there is still data left in the buffer, move it down over the
	 * top of the line we've just read.
	 */
	if (linelen < line->bufused) {
		memmove(line->buf, line->buf + linelen,
			line->bufused - linelen);
	}
	line->bufused -= linelen;

	/*
	 * Now we've shifted a line out of the buffer, re-check to see if
	 * there are any more lines buffered and set the flag accordingly.
	 */
	line->lineready = 0;
	if (line->bufused > 0) {
		ptr = memchr(line->buf, '\n', line->bufused);
		if (ptr)
			line->lineready = 1;
	}

	if (line->bufused >= sizeof(line->buf))
		line->lineready = 1;

	return linelen;
}

/* EOF */
