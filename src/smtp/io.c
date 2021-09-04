/*
 * Input/output functions for the SMTP servers.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#include "config.h"
#include "options.h"
#include "log.h"
#include "library.h"
#include "smtpi.h"

#include <stdio.h>
#include <time.h>


/*
 * Write the given string to the output SMTP server, returning nonzero on
 * error.
 */
int smtp_write_out(ppsmtp_t state, char *str, int len)
{
	if (write_retry(state->outfdw, str, len))
		return 1;
	state->last_smtpout = time(NULL);
	return 0;
}


/*
 * Write the given string to the input SMTP server, returning nonzero on
 * error.
 */
int smtp_write_in(ppsmtp_t state, char *str, int len)
{
#ifdef DEBUG
	if (state->opts->debug > 1)
		log_line(LOGPRI_DEBUG, "> %.*s", len, str);
#endif
	if (write_retry(state->infdw, str, len))
		return 1;
	return 0;
}

/* EOF */
