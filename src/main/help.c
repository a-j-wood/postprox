/*
 * Output command-line help to stdout.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct optdesc_s {
	char *optshort;
	char *optlong;
	char *param;
	char *description;
};


/*
 * Display command-line help.
 */
void display_help(void)
{
	/*@ -staticinittrans -readonlytrans -observertrans -nullassign @ */
	struct optdesc_s optlist[] = {
		{"-c", "--command", _("COMMAND"),
		 _("use COMMAND as the filtering command")},
		{"-d", "--tempdir", _("DIR"),
		 _("use DIR for temporary files instead of /tmp")},
		{"-t", "--timeout", "TIMEOUT",
		 _("filter timeout TIMEOUT sec instead of 30")},
		{"-r", "--reject", NULL,
		 _("reject mail with 451 if filter fails to run")},
		{"-v", "--verbose", NULL,
		 _("more verbose logging")},
		{"-l", "--listen", _("[IP:]PORT"),
		 _("listen on PORT instead of using stdin/out")},
#ifdef DEBUG
		{"-D", "--debug", NULL,
		 _("enable debugging output")},
#endif
		{"", NULL, NULL, NULL},
		{"-h", "--help", NULL,
		 _("show this help and exit")},
		{"-L", "--license", NULL,
		 _("show this program's license")},
		{"-V", "--version", NULL,
		 _("show version information and exit")},
		{NULL, NULL, NULL, NULL}
	};
	int i, col1max = 0, tw = 77;
	char *optbuf;

	printf(_("Usage: %s [OPTION] HOST:PORT"),	/* RATS: ignore */
	       PROGRAM_NAME);
	printf("\n%s\n\n",
	       _
	       ("Read SMTP on standard input, proxying to another SMTP server, "
		"with filtering."));

	for (i = 0; optlist[i].optshort != NULL; i++) {
		int width = 0;

		width = 2 + (int) strlen(optlist[i].optshort);	/* RATS: ignore */
#ifdef HAVE_GETOPT_LONG
		if (optlist[i].optlong != NULL)
			width += 2 + strlen(optlist[i].optlong);	/* RATS: ignore */
#endif
		if (optlist[i].param != NULL)
			width += 1 + strlen(optlist[i].param);	/* RATS: ignore */

		if (width > col1max)
			col1max = width;
	}

	col1max++;

	optbuf = malloc((size_t) (col1max + 16));
	if (optbuf == NULL) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		exit(EXIT_FAILURE);
	}

	for (i = 0; optlist[i].optshort != NULL; i++) {
		char *start;
		char *end;

		if (optlist[i].optshort[0] == '\0') {
			printf("\n");
			continue;
		}
#ifdef HAVE_SNPRINTF
		(void) snprintf(optbuf, (size_t) (col1max + 16),
#else
		(void) sprintf(optbuf,	    /* RATS: ignore */
#endif
			       "%s%s%s%s%s", optlist[i].optshort,
#ifdef HAVE_GETOPT_LONG
			       optlist[i].optlong != NULL ? ", " : "",
			       optlist[i].optlong !=
			       NULL ? optlist[i].optlong : "",
#else
			       "", "",
#endif
			       optlist[i].param != NULL ? " " : "",
			       optlist[i].param !=
			       NULL ? optlist[i].param : "");

		printf("  %-*s ", col1max - 2, optbuf);

		if (optlist[i].description == NULL) {
			printf("\n");
			continue;
		}

		start = optlist[i].description;

		while (strlen(start)	    /* RATS: ignore */
		       >(size_t) (tw - col1max)) {
			end = start + tw - col1max;
			while ((end > start) && (end[0] != ' '))
				end--;
			if (end == start) {
				end = start + tw - col1max;
			} else {
				end++;
			}
			printf("%.*s\n%*s ", (int) (end - start), start,
			       col1max, "");
			if (end == start)
				end++;
			start = end;
		}

		printf("%s\n", start);
	}

	printf("\n");
	printf(_("Please report any bugs to %s."),	/* RATS: ignore */
	       BUG_REPORTS_TO);
	printf("\n");
}

/* EOF */
