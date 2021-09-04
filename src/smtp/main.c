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
#include <unistd.h>


int smtp_mainloop(ppsmtp_t state);


/*
 * Connect to the output SMTP server (unless output SMTP server file
 * descriptors are given, in which case use those), then perform proxying
 * until the input SMTP server (on infdr/infdw) has finished.
 *
 * Returns nonzero on error.
 *
 * Before returning, any files or connections opened by this function will
 * be closed.
 */
int smtp_main(opts_t opts, int infdr, int infdw, int outfdr, int outfdw)
{
	ppnetfd_t smtpout;
	struct ppsmtp_s state;
	int ret;

	memset(&state, 0, sizeof(state));

	state.opts = opts;
	smtpout = NULL;

	state.ipaddr = NULL;
	state.helo = NULL;
	state.sender = NULL;
	state.recipient = NULL;

	/*
	 * If no output server FDs were given, connect to the output server
	 * over TCP/IP.
	 */
	if ((outfdr < 0) || (outfdw < 0)) {
		smtpout =
		    connection_open(opts->serverhost, opts->serverport);
		if (smtpout == NULL) {
			log_line(LOGPRI_ERROR,
				 _
				 ("failed to connect to output server: %s"),
				 strerror(errno));
			printf("421 %s\r\n",
			       _
			       ("Service not available - proxy failed to connect"));
			return 1;
		}
		state.outfdr = connection_fdr(smtpout);
		state.outfdw = connection_fdw(smtpout);
	} else {
		state.outfdr = outfdr;
		state.outfdw = outfdw;
	}

	/*
	 * Create a line object for reading lines from the output SMTP
	 * server.
	 */
	state.outsrvline = fdline_open(state.outfdr);
	if (state.outsrvline == NULL) {
		log_line(LOGPRI_ERROR,
			 _("failed to open line for output server: %s"),
			 strerror(errno));
		printf("421 %s\r\n",
		       _("Service not available - internal error"));
		if (smtpout)
			connection_close(smtpout);
		return 1;
	}

	/*
	 * Create a line object for reading lines from the input SMTP
	 * server.
	 */
	state.infdr = infdr;
	state.infdw = infdw;
	state.insrvline = fdline_open(state.infdr);
	if (state.insrvline == NULL) {
		log_line(LOGPRI_ERROR,
			 _("failed to open line for input server: %s"),
			 strerror(errno));
		printf("421 %s\r\n",
		       _("Service not available - internal error"));
		fdline_close(state.outsrvline);
		if (smtpout)
			connection_close(smtpout);
		return 1;
	}

	state.spoolfd = -1;

#ifdef DEBUG
	if (opts->debug > 0)
		log_line(LOGPRI_DEBUG,
			 "connection established, entering main loop");
#endif

	/*
	 * Run the main proxy loop.
	 */
	ret = smtp_mainloop(&state);

#ifdef DEBUG
	if (opts->debug > 0)
		log_line(LOGPRI_DEBUG,
			 "leaving main loop with exit code %d", ret);
#endif

	/*
	 * Now we clean up by freeing the line objects, closing the TCP/IP
	 * connection if we opened one, and closing and removing the spool
	 * file and output spool file if they are left over.
	 */
	fdline_close(state.insrvline);
	fdline_close(state.outsrvline);

	if (smtpout)
		connection_close(smtpout);

	if (state.spoolfile) {
		if (state.spoolfd >= 0)
			close(state.spoolfd);
		state.spoolfd = -1;

		unlink(state.spoolfile);
		free(state.spoolfile);
		state.spoolfile = NULL;
	}

	if (state.outspoolfile) {
		if (state.outspoolfd >= 0)
			close(state.outspoolfd);
		state.outspoolfd = -1;

		unlink(state.outspoolfile);
		free(state.outspoolfile);
		state.outspoolfile = NULL;
	}

	if (state.ipaddr != NULL)
		free(state.ipaddr);
	if (state.helo != NULL)
		free(state.helo);
	if (state.sender != NULL)
		free(state.sender);
	if (state.recipient != NULL)
		free(state.recipient);

#ifdef DEBUG
	if (opts->debug > 0)
		log_line(LOGPRI_DEBUG, "connection closed");
#endif

	return ret;
}

/* EOF */
