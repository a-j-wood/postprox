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


void display_help(void);
void display_license(void);
void display_version(void);


/*
 * Free an opts_t object.
 */
void opts_free(opts_t opts)
{
	if (!opts)
		return;
	if (opts->argv)
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
#ifdef HAVE_GETOPT_LONG
	struct option long_options[] = {
		{"help", 0, 0, 'h'},
		{"license", 0, 0, 'L'},
		{"version", 0, 0, 'V'},
		{"listen", 1, 0, 'l'},
		{"command", 1, 0, 'c'},
		{"tempdir", 1, 0, 'd'},
		{"timeout", 1, 0, 't'},
		{"reject", 0, 0, 'r'},
		{"verbose", 0, 0, 'v'},
# ifdef DEBUG
		{"debug", 0, 0, 'D'},
# endif				/* DEBUG */
		{0, 0, 0, 0}
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

	opts = calloc(1, sizeof(*opts));
	if (!opts) {
		fprintf(stderr,		    /* RATS: ignore (OK) */
			_("%s: option structure allocation failed (%s)"),
			argv[0], strerror(errno));
		fprintf(stderr, "\n");
		return 0;
	}

	opts->program_name = argv[0];

	opts->argc = 0;
	opts->argv = calloc(argc + 1, sizeof(char *));
	if (!opts->argv) {
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
	opts->serverport = 25;
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
			break;
		case 'L':
			display_license();
			opts->action = ACTION_NONE;
			return opts;
			break;
		case 'V':
			display_version();
			opts->action = ACTION_NONE;
			return opts;
			break;
		case 'l':
			ptr = strchr(optarg, ':');
			if (ptr) {
				opts->listenhost = optarg;
				opts->listenport = atoi(ptr + 1);
			} else {
				opts->listenport = atoi(optarg);
				/*
				 * Catch IPs with no port, eg "127.0.0.1".
				 */
				if (strchr(optarg, '.'))
					opts->listenport = 0;
			}
			if (opts->listenport == 0) {
				fprintf(stderr, "%s: %s: %s", argv[0],
					_("invalid listen port"), optarg);
				fprintf(stderr, "\n");
				opts_free(opts);
				return 0;
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
			return 0;
			break;
		}

	} while (c != -1);

	if ((opts->action == ACTION_PROXY) && (optind < argc)) {
		optarg = argv[optind];
		ptr = strchr(optarg, ':');
		if (ptr) {
			opts->serverhost = optarg;
			opts->serverport = atoi(ptr + 1);
		} else {
			opts->serverhost = optarg;
		}
	}

	if ((opts->action == ACTION_PROXY) && (opts->serverhost == NULL)) {
		fprintf(stderr, "%s: %s\n", argv[0],
			_("no output server specified"));
		opts_free(opts);
		return 0;
	}

	return opts;
}

/* EOF */
