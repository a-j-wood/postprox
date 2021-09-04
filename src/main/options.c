/*
 * Parse command-line options.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#include "config.h"
#include "options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <unistd.h>

#ifndef HAVE_GETOPT

int minigetopt(int, char **, char *);
extern char *minioptarg;
extern int minioptind, miniopterr, minioptopt;

#define getopt minigetopt
#define optarg minioptarg
#define optind minioptind
#define opterr miniopterr
#define optopt minioptopt

#endif				/* !HAVE_GETOPT */

/*@ -redecl @*/
extern void display_help(void);
extern void display_license(void);
extern void display_version(void);


/*
 * Free an opts_t object.
 */
void opts_free(opts_t opts)
{
	/*@ -usedef @ */
	if (opts == NULL)
		return;
	if (opts->argv != NULL)
		free(opts->argv);
	free(opts);
}


/*
 * Parse the given command-line arguments into an opts_t object, handling
 * "help", "license" and "version" options internally.
 *
 * Returns an opts_t, or 0 on error.
 *
 * Note that the contents of *argv[] (i.e. the command line parameters)
 * aren't copied anywhere, just the pointers are copied, so make sure the
 * command line data isn't overwritten or argv[1] free()d or whatever.
 */
opts_t opts_parse(int argc, char **argv)
{
	/*@ -readonlytrans -nullassign @ */
	/*@ -unqualifiedtrans -observertrans -statictrans @ */
	/*@ -mustfreeonly -globstate -branchstate @ */
#ifdef HAVE_GETOPT_LONG
	struct option long_options[] = {
		{"help", 0, NULL, (int) 'h'},
		{"license", 0, NULL, (int) 'L'},
		{"version", 0, NULL, (int) 'V'},
		{"listen", 1, NULL, (int) 'l'},
		{"command", 1, NULL, (int) 'c'},
		{"tempdir", 1, NULL, (int) 'd'},
		{"timeout", 1, NULL, (int) 't'},
		{"reject", 0, NULL, (int) 'r'},
		{"verbose", 0, NULL, (int) 'v'},
# ifdef DEBUG
		{"debug", 0, NULL, (int) 'D'},
# endif				/* DEBUG */
		{NULL, 0, NULL, 0}
	};
	int option_index = 0;
#endif				/* HAVE_GETOPT_LONG */
#ifdef DEBUG
	char *short_options = "hLVl:c:d:t:rvD";
#else
	char *short_options = "hLVl:c:d:t:rv";
#endif
	char *ptr;
	int c;
	opts_t opts;

	opts = calloc((size_t) 1, sizeof(*opts));
	if (opts == NULL) {
		fprintf(stderr,		    /* RATS: ignore (OK) */
			_("%s: option structure allocation failed (%s)"),
			argv[0], strerror(errno));
		fprintf(stderr, "\n");
		return NULL;
	}

	opts->program_name = argv[0];

	opts->argc = 0;
	opts->argv = calloc((size_t) (argc + 1), sizeof(char *));
	if (opts->argv == NULL) {
		fprintf(stderr,		    /* RATS: ignore (OK) */
			_
			("%s: option structure argv allocation failed (%s)"),
			argv[0], strerror(errno));
		fprintf(stderr, "\n");
		opts_free(opts);
		return 0;
	}

	opts->action = ACTION_PROXY;
	opts->serverhost = NULL;
	opts->serverport = (unsigned short) 25;
	opts->listenhost = "127.0.0.1";
	opts->listenport = 0;
	opts->filtercommand = "true";
	opts->tempdirectory = "/tmp";
	opts->timeout = 30;
	opts->rejectfailures = 0;
	opts->verbose = 0;
#ifdef DEBUG
	opts->debug = 0;
#endif

	do {
#ifdef HAVE_GETOPT_LONG
		c = getopt_long(argc, argv, /* RATS: ignore */
				short_options, long_options,
				&option_index);
#else
		c = getopt(argc, argv, short_options);	/* RATS: ignore */
#endif

		if (c < 0)
			continue;

		switch (c) {
		case 'h':
			display_help();
			opts->action = ACTION_NONE;
			return opts;
		case 'L':
			display_license();
			opts->action = ACTION_NONE;
			return opts;
		case 'V':
			display_version();
			opts->action = ACTION_NONE;
			return opts;
		case 'l':
			ptr = strchr(optarg, ':');
			if (ptr != NULL) {
				opts->listenhost = optarg;
				opts->listenport =
				    (unsigned short) atoi(ptr + 1);
			} else {
				opts->listenport =
				    (unsigned short) atoi(optarg);
				/*
				 * Catch IPs with no port, eg "127.0.0.1".
				 */
				if (strchr(optarg, '.') != NULL)
					opts->listenport = 0;
			}
			if (opts->listenport == 0) {
				fprintf(stderr, "%s: %s: %s", argv[0],
					_("invalid listen port"),
					optarg ==
					NULL ? "(null)" : optarg);
				fprintf(stderr, "\n");
				opts_free(opts);
				return NULL;
			}
			break;
		case 'c':
			opts->filtercommand = optarg;
			break;
		case 'd':
			opts->tempdirectory = optarg;
			break;
		case 't':
			opts->timeout = atoi(optarg);
			break;
		case 'r':
			opts->rejectfailures = 1;
			break;
		case 'v':
			opts->verbose++;
			break;
#ifdef DEBUG
		case 'D':
			opts->debug++;
			break;
#endif
		default:
#ifdef HAVE_GETOPT_LONG
			fprintf(stderr,	    /* RATS: ignore (OK) */
				_("Try `%s --help' for more information."),
				argv[0]);
#else
			fprintf(stderr,	    /* RATS: ignore (OK) */
				_("Try `%s -h' for more information."),
				argv[0]);
#endif
			fprintf(stderr, "\n");
			opts_free(opts);
			return NULL;
		}

	} while (c != -1);

	if ((opts->action == ACTION_PROXY) && (optind < argc)) {
		optarg = argv[optind];
		ptr = strchr(optarg, ':');
		if (ptr != NULL) {
			opts->serverhost = optarg;
			opts->serverport = (unsigned short) atoi(ptr + 1);
		} else {
			opts->serverhost = optarg;
		}
	}

	if ((opts->action == ACTION_PROXY) && (opts->serverhost == NULL)) {
		fprintf(stderr, "%s: %s\n", argv[0],
			_("no output server specified"));
		opts_free(opts);
		return NULL;
	}

	return opts;
}

/* EOF */
