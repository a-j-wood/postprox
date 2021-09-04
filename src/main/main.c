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
#include <string.h>
#include <errno.h>
#ifdef HAVE_MCHECK_H
#include <mcheck.h>
#endif


/*@ -redecl @*/
#ifdef DEBUG
extern void log_open(char *, int);
#else
extern void log_open(char *);
#endif
extern void log_close(void);
extern int smtp_main(opts_t, int, int, int, int);


/*
 * Process command-line arguments and set option flags, then call functions
 * to initialise, and finally enter the main loop.
 */
int main(int argc, char **argv)
{
	opts_t opts;
	int retcode;

#ifdef HAVE_MCHECK_H
	if (getenv("MALLOC_TRACE") != NULL) /* RATS: ignore (value unused) */
		mtrace();
#endif

#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif

	opts = opts_parse(argc, argv);
	if (opts == NULL)
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
		int fd_in;
		int fd_out;

		fd_in = fileno(stdin);
		fd_out = fileno(stdout);

		if (fd_in < 0) {
			fprintf(stderr, "%s: %s\n",
				_("Standard input is unreadable"),
				strerror(errno));
			retcode = 1;
		} else if (fd_out < 0) {
			fprintf(stderr, "%s: %s\n",
				_("Standard output is unreadable"),
				strerror(errno));
			retcode = 1;
		} else {
			retcode = smtp_main(opts, fd_in, fd_out, -1, -1);
		}
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
