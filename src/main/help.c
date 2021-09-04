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
	struct optdesc_s optlist[] = {
		{"-c", "--command", _("COMMAND"),
		 _("use COMMAND as the filtering command")},
		{"-d", "--tempdir", _("DIR"),
		 _("use DIR for temporary files instead of /tmp")},
		{"-t", "--timeout", "TIMEOUT",
		 _("filter timeout TIMEOUT sec instead of 30")},
		{"-r", "--reject", 0,
		 _("reject mail with 451 if filter fails to run")},
		{"-v", "--verbose", 0,
		 _("more verbose logging")},
		{"-l", "--listen", _("[IP:]PORT"),
		 _("listen on PORT instead of using stdin/out")},
#ifdef DEBUG
		{"-D", "--debug", 0,
		 _("enable debugging output")},
#endif
		{"", 0, 0, 0},
		{"-h", "--help", 0,
		 _("show this help and exit")},
		{"-L", "--license", 0,
		 _("show this program's license")},
		{"-V", "--version", 0,
		 _("show version information and exit")},
		{0, 0, 0, 0}
	};
	int i, col1max = 0, tw = 77;
	char *optbuf;

	printf(_("Usage: %s [OPTION] HOST:PORT"),	/* RATS: ignore */
	       PROGRAM_NAME);
	printf("\n%s\n\n",
	       _
	       ("Read SMTP on standard input, proxying to another SMTP server, "
		"with filtering."));

	for (i = 0; optlist[i].optshort; i++) {
		int width = 0;

		width = 2 + strlen(optlist[i].optshort);	/* RATS: ignore */
#ifdef HAVE_GETOPT_LONG
		if (optlist[i].optlong)
			width += 2 + strlen(optlist[i].optlong);	/* RATS: ignore */
#endif
		if (optlist[i].param)
			width += 1 + strlen(optlist[i].param);	/* RATS: ignore */

		if (width > col1max)
			col1max = width;
	}

	col1max++;

	optbuf = malloc(col1max + 16);
	if (optbuf == NULL) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		exit(1);
	}

	for (i = 0; optlist[i].optshort; i++) {
		char *start;
		char *end;

		if (optlist[i].optshort[0] == 0) {
			printf("\n");
			continue;
		}

		sprintf(optbuf, "%s%s%s%s%s",	/* RATS: ignore (checked) */
			optlist[i].optshort,
#ifdef HAVE_GETOPT_LONG
			optlist[i].optlong ? ", " : "",
			optlist[i].optlong ? optlist[i].optlong : "",
#else
			"", "",
#endif
			optlist[i].param ? " " : "",
			optlist[i].param ? optlist[i].param : "");

		printf("  %-*s ", col1max - 2, optbuf);

		if (optlist[i].description == NULL) {
			printf("\n");
			continue;
		}

		start = optlist[i].description;

		while (strlen(start) /* RATS: ignore */ >tw - col1max) {
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
