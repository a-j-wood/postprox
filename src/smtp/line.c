/*
 * Process a line of incoming SMTP.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#include "config.h"
#include "options.h"
#include "log.h"
#include "library.h"
#include "smtpi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


int smtp_runfilter(opts_t, char *, char *, char *, int, char *, char *,
		   char *, char *);


/*
 * Deal with the end of DATA mode (a "." on a line on its own) by running
 * the content filter. If the content filter says the message is OK, send
 * the spool file contents as DATA to the output SMTP server; if not, RSET
 * the output SMTP server and send the filter error code to the input SMTP
 * server.
 *
 * Returns nonzero on error.
 */
static int smtp_processline__dataend(ppsmtp_t state)
{
	char filtererr[256];		 /* RATS: ignore (OK) */
	char errbuf[256];		 /* RATS: ignore (OK) */
	unsigned long messagesize;
	struct stat sb;
	char *ptr;
	int ret;
	fdline_t spoolline;

#ifdef DEBUG
	if (state->opts->debug > 0)
		log_line(LOGPRI_DEBUG,
			 "received DATA chunk, leaving DATA mode");
#endif

	if (stat(state->spoolfile, &sb) == 0) {	/* RATS: ignore (OK) */
		messagesize = sb.st_size;
	} else {
		log_line(LOGPRI_ERROR,
			 _("stat of spool file failed: %s"),
			 strerror(errno));
		messagesize = 0;
	}

	filtererr[0] = 0;
	ret =
	    smtp_runfilter(state->opts, state->spoolfile,
			   state->outspoolfile, filtererr,
			   sizeof(filtererr),
			   (state->ipaddr == NULL ? "" : state->ipaddr),
			   (state->helo == NULL ? "" : state->helo),
			   (state->sender == NULL ? "" : state->sender),
			   (state->recipient ==
			    NULL ? "" : state->recipient));

	unlink(state->spoolfile);
	free(state->spoolfile);
	state->spoolfile = NULL;

	unlink(state->outspoolfile);
	free(state->outspoolfile);
	state->outspoolfile = NULL;

	/*
	 * End the filter error output at the first \r or \n.
	 */
	ptr = strchr(filtererr, '\r');
	if (ptr)
		ptr[0] = 0;
	ptr = strchr(filtererr, '\n');
	if (ptr)
		ptr[0] = 0;

	if (ret > 0) {
		/*
		 * The filter rejected the message.
		 */
		close(state->spoolfd);
		state->spoolfd = -1;
		close(state->outspoolfd);
		state->outspoolfd = -1;

		/*
		 * Prefix the filter's error message with a 554 error code
		 * if it doesn't start with an SMTP error code and a space.
		 */
		if (((filtererr[0] >= '2') && (filtererr[0] <= '5'))
		    && ((filtererr[1] >= '0') && (filtererr[1] <= '9'))
		    && ((filtererr[2] >= '0') && (filtererr[2] <= '9'))
		    && (filtererr[3] == ' ')
		    ) {
			sprintf(errbuf, "%.200s\r\n", filtererr);
		} else if (filtererr[0] != 0) {
			sprintf(errbuf, "554 %.200s\r\n", filtererr);
		} else {
			sprintf(errbuf, "554 %.200s\r\n",
				_("Content rejected"));
		}

		smtp_write_in(state, errbuf, strlen(errbuf));
		smtp_write_out(state, "RSET\r\n", 6);
		state->ignoreresponse++;
		state->datamode = 0;

		if (state->opts->verbose > 0) {
			char *ptr;
			ptr = strchr(errbuf, '\r');
			if (ptr)
				ptr[0] = 0;
			log_line(LOGPRI_INFO,
				 "%s: %s=%.40s (%.60s), %s=%.99s, %s=%.99s, %s=%lu: %.200s",
				 _("reject"), _("host"),
				 (state->ipaddr ==
				  NULL ? "???" : state->ipaddr),
				 (state->helo ==
				  NULL ? "???" : state->helo), _("from"),
				 (state->sender ==
				  NULL ? "???" : state->sender), _("to"),
				 (state->recipient ==
				  NULL ? "???" : state->recipient),
				 _("size"), messagesize, errbuf);
		}

		return 0;

	} else if ((ret < 0) && (state->opts->rejectfailures)) {
		/*
		 * The filter failed to run or timed out, and we are
		 * supposed to temporarily reject in this case.
		 */
		close(state->spoolfd);
		state->spoolfd = -1;
		close(state->outspoolfd);
		state->outspoolfd = -1;

		sprintf(errbuf, "451 %.200s\r\n",
			_("Error running content filter"));
		smtp_write_in(state, errbuf, strlen(errbuf));
		smtp_write_out(state, "RSET\r\n", 6);
		state->ignoreresponse++;
		state->datamode = 0;

		if (state->opts->verbose > 0) {
			log_line(LOGPRI_INFO,
				 "%s: %s=%.40s (%.60s), %s=%.99s, %s=%.99s, %s=%lu: %.200s",
				 _("reject"), _("host"),
				 (state->ipaddr ==
				  NULL ? "???" : state->ipaddr),
				 (state->helo ==
				  NULL ? "???" : state->helo), _("from"),
				 (state->sender ==
				  NULL ? "???" : state->sender), _("to"),
				 (state->recipient ==
				  NULL ? "???" : state->recipient),
				 _("size"), messagesize,
				 _("failed to run filter"));
		}

		return 0;
	}

	/*
	 * The filter accepted the message, so we need to send the whole
	 * DATA chunk to the output SMTP server.
	 */

	/*
	 * First we need to check whether the outspoolfile is empty - if
	 * not, we send its contents instead of the contents of spoolfile,
	 * since that means the filter might have modified the email. If
	 * outspoolfile is empty, we send the original spoolfile contents.
	 */

	if (lseek(state->outspoolfd, 0, SEEK_END) > 0) {
		/*
		 * Output spool fd is >0 bytes, so read from outspoolfile.
		 */
		lseek(state->outspoolfd, 0, SEEK_SET);
		spoolline = fdline_open(state->outspoolfd);
	} else {
		/*
		 * Output spool fd is <=0 bytes, so read from spoolfile.
		 */
		lseek(state->spoolfd, 0, SEEK_SET);
		spoolline = fdline_open(state->spoolfd);
	}

	if (spoolline == NULL) {
		log_line(LOGPRI_ERROR,
			 _("read from spool file failed: %s"),
			 strerror(errno));
		sprintf(errbuf, "451 %.200s\r\n",
			_("Spool file read error"));
		smtp_write_in(state, errbuf, strlen(errbuf));
		smtp_write_out(state, "RSET\r\n", 6);
		state->ignoreresponse++;

		close(state->spoolfd);
		state->spoolfd = -1;
		close(state->outspoolfd);
		state->outspoolfd = -1;

		state->datamode = 0;
		return 1;
	}
#ifdef DEBUG
	if (state->opts->debug > 0)
		log_line(LOGPRI_DEBUG, "sending DATA to output server");
#endif

	/*
	 * Start sending the DATA, remembering not to pass the output
	 * server's 354 response back to the input server (since the input
	 * server has already sent its data after we told it to go ahead
	 * earlier, in __datastart below).
	 */
	smtp_write_out(state, "DATA\r\n", 6);
	state->ignoreresponse++;

	while (fdline_read_eof(spoolline) == 0) {
		char linebuf[1024];	 /* RATS: ignore (checked) */
		char outbuf[1200];	 /* RATS: ignore (checked) */
		char *ptr;
		int linelen;
		int startline;

		linelen =
		    fdline_read(spoolline, linebuf, sizeof(linebuf),
				&startline);
		if (linelen < 0) {
			log_line(LOGPRI_ERROR,
				 _("read from spool file failed: %s"),
				 strerror(errno));
			sprintf(errbuf, "451 %.200s\r\n",
				_("Spool file read error"));
			smtp_write_in(state, errbuf, strlen(errbuf));
			fdline_close(spoolline);

			close(state->spoolfd);
			state->spoolfd = -1;
			close(state->outspoolfd);
			state->outspoolfd = -1;

			state->datamode = 0;
			return 1;
		} else if (linelen == 0) {
			/*
			 * Remember a zero-length line from fdline_read()
			 * does NOT mean EOF.
			 */
			continue;
		}

		/*
		 * Transparency procedure (RFC2821 section 4.5.2)
		 */
		if ((linebuf[0] == '.') && (startline)) {
			outbuf[0] = '.';
			memcpy(outbuf + 1, linebuf, linelen);
			linelen++;
		} else {
			memcpy(outbuf, linebuf, linelen);
		}

		ptr = memchr(outbuf, '\n', linelen);
		if (ptr) {
			strcpy(ptr, "\r\n");
			linelen++;
		}

		if (smtp_write_out(state, outbuf, linelen)) {
			log_line(LOGPRI_ERROR,
				 _
				 ("write to output server from spool file failed: %s"),
				 strerror(errno));
			sprintf(errbuf, "451 %.200s\r\n",
				_("Error writing to output server"));
			smtp_write_in(state, errbuf, strlen(errbuf));
			fdline_close(spoolline);

			close(state->spoolfd);
			state->spoolfd = -1;
			close(state->outspoolfd);
			state->outspoolfd = -1;

			state->datamode = 0;
			return 1;
		}
	}

	smtp_write_out(state, ".\r\n", 3);

	fdline_close(spoolline);

	close(state->spoolfd);
	state->spoolfd = -1;
	close(state->outspoolfd);
	state->outspoolfd = -1;

	state->datamode = 0;

#ifdef DEBUG
	if (state->opts->debug > 0)
		log_line(LOGPRI_DEBUG, "DATA sent to output server");
#endif

	if (state->opts->verbose > 0) {
		if (ret < 0) {
			log_line(LOGPRI_INFO,
				 "%s: %s=%.40s (%.60s), %s=%.99s, %s=%.99s, %s=%lu: %.200s",
				 _("accept"), _("host"),
				 (state->ipaddr ==
				  NULL ? "???" : state->ipaddr),
				 (state->helo ==
				  NULL ? "???" : state->helo), _("from"),
				 (state->sender ==
				  NULL ? "???" : state->sender), _("to"),
				 (state->recipient ==
				  NULL ? "???" : state->recipient),
				 _("size"), messagesize,
				 _("filter failed"));
		} else {
			log_line(LOGPRI_INFO,
				 "%s: %s=%.40s (%.60s), %s=%.99s, %s=%.99s, %s=%lu: %.200s",
				 _("accept"), _("host"),
				 (state->ipaddr ==
				  NULL ? "???" : state->ipaddr),
				 (state->helo ==
				  NULL ? "???" : state->helo), _("from"),
				 (state->sender ==
				  NULL ? "???" : state->sender), _("to"),
				 (state->recipient ==
				  NULL ? "???" : state->recipient),
				 _("size"), messagesize,
				 _("filter passed"));
		}
	}

	return 0;
}


/*
 * Process a line of DATA mode data from the input server, spooling it to
 * the temporary file. If we reach the end, run the filter and deal with it
 * accordingly. If we've been in DATA mode for a while send a NOOP to the
 * output SMTP server so it doesn't time out.
 *
 * Returns nonzero on error.
 *
 * Note that this function may modify the contents of state->linebuf.
 */
static int smtp_processline__datamode(ppsmtp_t state)
{
	char *lineptr;
	int linelen;

	lineptr = state->linebuf;
	linelen = state->linelen;

	if (linelen < 1)
		return 0;

	/*
	 * Un-escape leading "."; if the "." is on its own on a line, this
	 * is the end of the DATA.
	 */
	if ((lineptr[0] == '.') && (state->startline)) {
		lineptr++;
		linelen--;
		if ((lineptr[0] == '\r') || (lineptr[0] == '\n')) {
			return smtp_processline__dataend(state);
		}
	}

	/*
	 * Remove the last \r before \n, before writing to the spool file
	 * (i.e. remove network encoding).
	 */
	if ((linelen > 1) && (lineptr[linelen - 2] == '\r')) {
		lineptr[linelen - 2] = '\n';
		linelen--;
	}

	/*
	 * Store the line to the spool file, aborting on error.
	 */
	if (write_retry(state->spoolfd, lineptr, linelen)) {
		char errbuf[256];	 /* RATS: ignore (checked) */

		log_line(LOGPRI_ERROR, _("write to spool file failed: %s"),
			 strerror(errno));
		sprintf(errbuf, "421 %.200s\r\n",
			_
			("Service not available - spool file write failed"));
		smtp_write_in(state, errbuf, strlen(errbuf));
		smtp_write_out(state, "QUIT\r\n", 6);

		return 1;
	}

	/*
	 * Send a NOOP to the output server if we've not spoken to it for 1
	 * minute, so the connection doesn't time out.
	 */
	if (difftime(time(NULL), state->last_smtpout) > 60) {
		smtp_write_out(state, "NOOP\r\n", 6);
		state->ignoreresponse++;
#ifdef DEBUG
		if (state->opts->debug > 0)
			log_line(LOGPRI_DEBUG,
				 "sent NOOP to output server");
#endif
	}

	return 0;
}


/*
 * Create a temporary file suitable for use as a spool file, placing its
 * name in a malloced buffer stored in *filename and putting the file
 * descriptor in *fd. Returns nonzero on error.
 */
static int smtp_processline__tempfile(ppsmtp_t state, char **filenameptr,
				      int *fdptr)
{
	char *filename;
	int fd;

	filename = malloc(strlen(state->opts->tempdirectory) + 64);
	if (filename == NULL) {
		log_line(LOGPRI_ERROR,
			 _("could not create spool filename: %s"),
			 strerror(errno));
		return 1;
	}

	strcpy(filename, state->opts->tempdirectory);	/* RATS: ignore (OK) */
	strcat(filename, "/spXXXXXX");

#ifdef HAVE_MKSTEMP
	fd = mkstemp(filename);
#else				/* !HAVE_MKSTEMP */
	mktemp(filename);
	fd = open(filename, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
#endif				/* HAVE_MKSTEMP */

	if (fd < 0) {
		log_line(LOGPRI_ERROR,
			 _("could not create spool file: %s"),
			 strerror(errno));
		free(filename);
		return 1;
	}

	chmod(filename, S_IRUSR | S_IWUSR);
	fcntl(fd, F_SETFD, FD_CLOEXEC);	    /* close on exec */

	*filenameptr = filename;
	*fdptr = fd;
	return 0;
}


/*
 * Enter DATA mode by creating a spool file with a unique name, and also
 * creating an output spool file ready to receive filter output. Returns
 * nonzero on error.
 */
static int smtp_processline__datastart(ppsmtp_t state)
{
	char okbuf[256];		 /* RATS: ignore (checked) */
	char *filename;
	int fd;

	if (smtp_processline__tempfile(state, &filename, &fd)) {
		char errbuf[256];	 /* RATS: ignore (checked) */
		sprintf(errbuf, "421 %.200s\r\n",
			_
			("Service not available - spool file creation error"));
		smtp_write_in(state, errbuf, strlen(errbuf));
		smtp_write_out(state, "QUIT\r\n", 6);
		return 1;
	}

	state->spoolfile = filename;
	state->spoolfd = fd;

	if (smtp_processline__tempfile(state, &filename, &fd)) {
		char errbuf[256];	 /* RATS: ignore (checked) */
		sprintf(errbuf, "421 %.200s\r\n",
			_
			("Service not available - spool file creation error"));
		smtp_write_in(state, errbuf, strlen(errbuf));
		smtp_write_out(state, "QUIT\r\n", 6);
		return 1;
	}

	state->outspoolfile = filename;
	state->outspoolfd = fd;

	state->datamode = 1;

#ifdef DEBUG
	if (state->opts->debug > 0)
		log_line(LOGPRI_DEBUG,
			 "entered DATA mode, spool file on fd %d is %s, "
			 "output spool file on fd %d is %s",
			 state->spoolfd, state->spoolfile,
			 state->outspoolfd, state->outspoolfile);
#endif

	/*
	 * Tell the input server to go ahead with the DATA.
	 */
	sprintf(okbuf, "354 %.200s\r\n",
		_("End data with <CR><LF>.<CR><LF>"));
	smtp_write_in(state, okbuf, strlen(okbuf));

	return 0;
}


/*
 * Read any information we need from an XFORWARD line.
 */
static void smtp_processline__xforward(ppsmtp_t state)
{
	int i;

	for (i = 0; i < state->linelen; i++) {
		char cmpbuf[256];	 /* RATS: ignore (OK) */
		char valbuf[128];	 /* RATS: ignore (OK) */
		int left;

		if (state->linebuf[i] != ' ')
			continue;

		left = state->linelen - i;
		if (left < 1)
			continue;

		if (left > sizeof(cmpbuf) - 2)
			left = sizeof(cmpbuf) - 2;

		strncpy(cmpbuf, state->linebuf + i + 1, left);
		cmpbuf[left] = 0;

		/* TODO: xtext decoding (see RFC1891) */

		valbuf[0] = 0;
		if ((state->ipaddr == NULL)
		    && (sscanf(cmpbuf, " ADDR=%99s", valbuf) == 1)	/* RATS: ignore */
		    ) {
			valbuf[99] = 0;
			state->ipaddr = calloc(1, strlen(valbuf) + 1);
			if (state->ipaddr != NULL)
				strcpy(state->ipaddr, valbuf);	/* RATS: ignore */
		}

		valbuf[0] = 0;
		if ((state->helo == NULL)
		    && (sscanf(cmpbuf, " HELO=%99s", valbuf) == 1)	/* RATS: ignore */
		    ) {
			valbuf[99] = 0;
			state->helo = calloc(1, strlen(valbuf) + 1);
			if (state->helo != NULL)
				strcpy(state->helo, valbuf);	/* RATS: ignore */
		}
	}
}


/*
 * Find an email address inside <> in the current line, and return it as a
 * freshly malloc()ed string. Returns NULL if no email address could be
 * found. If the email address is blank, the string given back will be "<>".
 */
static char *smtp_processline__getaddress(ppsmtp_t state)
{
	char tmpbuf[256];		 /* RATS: ignore (OK) */
	char *ptr;
	int len;
	char *newstr;

	len = state->linelen;
	if (len > sizeof(tmpbuf) - 2)
		len = sizeof(tmpbuf) - 2;
	if (len < 6)
		return NULL;

	strncpy(tmpbuf, state->linebuf, len);
	tmpbuf[len] = 0;

	ptr = strchr(tmpbuf, '>');
	if (ptr)
		ptr[0] = 0;

	ptr = strchr(tmpbuf, '<');
	if (ptr == NULL)
		return NULL;
	ptr++;

	if (ptr[0] == 0) {
		strcpy(tmpbuf, "<>");
		ptr = tmpbuf;
	}

	tmpbuf[sizeof(tmpbuf) - 1] = 0;

	newstr = calloc(1, strlen(ptr) + 1);
	if (newstr == NULL)
		return NULL;

	strcpy(newstr, ptr);		    /* RATS: ignore */

	if (strlen(newstr) > 99)
		newstr[99] = 0;

	return newstr;
}


/*
 * Read any information we need from a MAIL FROM line.
 */
static void smtp_processline__mailfrom(ppsmtp_t state)
{
	if (state->sender != NULL)
		return;
	state->sender = smtp_processline__getaddress(state);
}


/*
 * Read any information we need from a RCPT TO line.
 */
static void smtp_processline__rcptto(ppsmtp_t state)
{
	if (state->recipient != NULL)
		return;
	state->recipient = smtp_processline__getaddress(state);
}


/*
 * Process a line of SMTP from the input server, and either pass it on to
 * the output server or spool it to a temporary file if we're in DATA mode. 
 *
 * Returns nonzero on error, and reports the error if applicable.
 */
int smtp_processline(ppsmtp_t state)
{
	if (state->linelen == 0)
		return 0;

	if (state->datamode)
		return smtp_processline__datamode(state);

	if ((state->startline)
	    && (state->linelen > 4)
	    && (strncasecmp(state->linebuf, "DATA", 4) == 0)
	    && ((state->linebuf[4] == '\r')
		|| (state->linebuf[4] == '\n')
	    )
	    ) {
		return smtp_processline__datastart(state);
	}

	if (state->startline) {
		if ((state->linelen > 15)
		    && (strncasecmp(state->linebuf, "XFORWARD ", 9) == 0)) {
			smtp_processline__xforward(state);
		}
		if ((state->linelen > 11)
		    && (strncasecmp(state->linebuf, "MAIL FROM:", 10) ==
			0)) {
			smtp_processline__mailfrom(state);
		}
		if ((state->linelen > 9)
		    && (strncasecmp(state->linebuf, "RCPT TO:", 8) == 0)) {
			smtp_processline__rcptto(state);
		}
	}

	if (smtp_write_out(state, state->linebuf, state->linelen)) {
		char errbuf[256];	 /* RATS: ignore (checked) */

		log_line(LOGPRI_ERROR,
			 _("write to output server failed: %s"),
			 strerror(errno));
		sprintf(errbuf, "421 %.200s\r\n",
			_
			("Service not available - output server write failed"));
		smtp_write_in(state, errbuf, strlen(errbuf));
		return 1;
	}

	return 0;
}

/* EOF */
