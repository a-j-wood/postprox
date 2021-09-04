/*
 * Main SMTP proxy entry point.
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
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#else
#include <sys/time.h>
#include <sys/types.h>
/* unistd.h would go here too, but we're including it below */
#endif
#include <unistd.h>


int smtp_processline(ppsmtp_t);


/*
 * Perform proxying until the input SMTP server closes the connection.
 * Returns nonzero on error.
 *
 * Note that this is a very naive proxy which trusts its input (copies the
 * input server's output to the output server's input, and the output
 * server's output to the input server's input), and so should only ever be
 * run in the manner described in the man page.
 */
int smtp_mainloop(ppsmtp_t state)
{
	int maxrfd, ret, dealt_with_ineof;

	/*
	 * Highest numbered read file descriptor, for use with select().
	 */
	maxrfd = state->outfdr;
	if (state->infdr > maxrfd)
		maxrfd = state->infdr;

	ret = 0;

	dealt_with_ineof = 0;

	while ((ret == 0) && (fdline_read_eof(state->outsrvline) == 0)) {
		fd_set readfds;
		struct timeval timeout;
		int n;

		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

		FD_ZERO(&readfds);

		if (fdline_read_fdeof(state->insrvline) == 0) {
			FD_SET(state->infdr, &readfds);
		}
		if (fdline_read_fdeof(state->outsrvline) == 0) {
			FD_SET(state->outfdr, &readfds);
		}

		/*
		 * Don't wait for select() if there's data already buffered.
		 */
		if (fdline_read_available(state->insrvline))
			timeout.tv_sec = 0;
		if (fdline_read_available(state->outsrvline))
			timeout.tv_sec = 0;

		n = select(maxrfd + 1, &readfds, NULL, NULL, &timeout);

		if (n < 0) {
			log_line(LOGPRI_ERROR, _("select call failed: %s"),
				 strerror(errno));
			ret = 1;
			break;
		}

		/*
		 * Data coming in from the input SMTP server.
		 */
		if (fdline_read_available(state->insrvline)
		    || (FD_ISSET(state->infdr, &readfds))
		    ) {

			state->linelen =
			    fdline_read(state->insrvline, state->linebuf,
					sizeof(state->linebuf),
					&(state->startline));

			if (state->linelen < 0) {
				ret = 1;
#ifdef DEBUG
				if (state->opts->debug > 0)
					log_line(LOGPRI_DEBUG,
						 "Error on input server");
#endif
				smtp_write_out(state, "QUIT\r\n", 6);
				continue;
			}
#ifdef DEBUG
			if ((state->opts->debug > 1)
			    && ((state->datamode == 0)
				|| (state->opts->debug > 2))
			    && (state->linelen > 0)
			    )
				log_line(LOGPRI_DEBUG, "< %.*s",
					 state->linelen, state->linebuf);
#endif
			/*
			 * At this point we have a valid line of input
			 * that's come in from the input SMTP server and
			 * which either needs passing on to the output SMTP
			 * server or needs spooling to a file, depending on
			 * whether we're in DATA mode or not.
			 */
			if (smtp_processline(state))
				ret = 1;
		}

		/*
		 * Data coming in from the output SMTP server.
		 */
		if (fdline_read_available(state->outsrvline)
		    || (FD_ISSET(state->outfdr, &readfds))
		    ) {

			state->linelen =
			    fdline_read(state->outsrvline, state->linebuf,
					sizeof(state->linebuf),
					&(state->startline));

			if (state->linelen < 0) {
				ret = 1;
#ifdef DEBUG
				if (state->opts->debug > 0)
					log_line(LOGPRI_DEBUG,
						 "Error on output server");
#endif
				sprintf(state->linebuf, "421 %.255s\r\n",
					_
					("Service not available - error reading from output server"));
				smtp_write_in(state, state->linebuf,
					      strlen(state->linebuf));
				continue;
			}
			/*
			 * Here we have a valid line of response data from
			 * the output SMTP server; we just pass this on to
			 * the input SMTP server, with no changes, unless
			 * we're supposed to be ignoring a response, in
			 * which case we just discard the line.
			 *
			 * Responses need to be ignored when we've sent an
			 * additional NOOP or RSET to the output server that
			 * the input server doesn't know about; we just drop
			 * those responses so the input server doesn't get
			 * confused.
			 */
			if ((state->linelen > 0)
			    && (state->startline)
			    && (state->ignoreresponse > 0)
			    && ((state->linebuf[0] >= '2')
				&& (state->linebuf[0] <= '5')
			    )
			    ) {
				state->ignoreresponse--;
#ifdef DEBUG
				if (state->opts->debug > 1) {
					log_line(LOGPRI_DEBUG,
						 "Not sending this to input server: > %.*s",
						 state->linelen,
						 state->linebuf);
				}
#endif
			} else if (state->linelen > 0) {

				/*
				 * Note that after sending QUIT, the input
				 * server may just disconnect without
				 * waiting for the output server to reply.
				 */

				if (fdline_read_fdeof(state->insrvline) ==
				    0) {
					smtp_write_in(state,
						      state->linebuf,
						      state->linelen);
				}
			}
		}

		/*
		 * Check for EOF on the input SMTP server. If we see it,
		 * send a QUIT to the output SMTP server.
		 */
		if ((fdline_read_eof(state->insrvline))
		    && (!dealt_with_ineof)) {
#ifdef DEBUG
			if (state->opts->debug > 0)
				log_line(LOGPRI_DEBUG,
					 "EOF on input server");
#endif
			if (!fdline_read_eof(state->outsrvline))
				smtp_write_out(state, "QUIT\r\n", 6);
			dealt_with_ineof = 1;
		}
#ifdef DEBUG
		/*
		 * Report EOF on the output SMTP server.
		 */
		if (fdline_read_eof(state->outsrvline)) {
			if (state->opts->debug > 0)
				log_line(LOGPRI_DEBUG,
					 "EOF on output server");
		}
#endif
	}

	return ret;
}

/* EOF */
