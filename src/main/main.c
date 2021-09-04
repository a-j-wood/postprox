/*
 * Main program entry point - read the command line options, then perform
 * the appropriate actions.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#include "config.h"
#include "options.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_MCHECK_H
#include <mcheck.h>
#endif


#ifdef DEBUG
void log_open(char *, unsigned char);
#else
void log_open(char *);
#endif
void log_close(void);
int smtp_main(opts_t, int, int, int, int);


/*
 * Process command-line arguments and set option flags, then call functions
 * to initialise, and finally enter the main loop.
 */
int main(int argc, char **argv)
{
	opts_t opts;
	int retcode;

#ifdef HAVE_MCHECK_H
	if (getenv("MALLOC_TRACE"))	    /* RATS: ignore (value unused) */
		mtrace();
#endif

#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif

	opts = opts_parse(argc, argv);
	if (!opts)
		return 1;

	if (opts->action == ACTION_NONE) {
		opts_free(opts);
		return 0;
	}
#ifdef DEBUG
	log_open(opts->program_name, opts->debug);
	if (opts->debug > 0)
		log_line(LOGPRI_DEBUG, "version %s initialising", VERSION);
#else
	log_open(opts->program_name);
#endif

	if (opts->listenport != 0) {
		/* TODO: write listen mode */
		fprintf(stderr, "%s\n",
			_("Listen mode is not yet implemented."));
		retcode = 1;
	} else {
		retcode =
		    smtp_main(opts, fileno(stdin), fileno(stdout), -1, -1);
	}

#ifdef DEBUG
	if (opts->debug > 0)
		log_line(LOGPRI_DEBUG, "exiting with status %d", retcode);
#endif

	log_close();

	opts_free(opts);

	return retcode;
}

/* EOF */
