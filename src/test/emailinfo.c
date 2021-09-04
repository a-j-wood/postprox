/*
 * Test functions to check that information about the message goes through.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#include "config.h"
#include "options.h"
#include "library.h"
#include "testi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>


int smtp_main(opts_t, int, int, int, int);


/*
 * Return nonzero if the next line in fptr doesn't match the given string
 * (checking only up to the first 99 characters).
 */
int test_proxyinfo_checkline(FILE * fptr, char *str)
{
	char linebuf[256];		 /* RATS: ignore (OK) */
	char *ptr;

	linebuf[0] = 0;
	linebuf[sizeof(linebuf) - 1] = 0;
	fgets(linebuf, sizeof(linebuf) - 1, fptr);

	ptr = strchr(linebuf, '\n');
	if (ptr)
		ptr[0] = 0;

	if (strlen(str) < 99) {
		if (strcmp(str, linebuf) == 0)
			return 0;
		return 1;
	}

	if (strncmp(str, linebuf, 99) == 0)
		return 0;
	return 1;
}


/*
 * Check that the IP address, HELO, MAIL FROM, and RCPT TO sent through the
 * proxy end up in the filter's environment correctly with the first 99
 * characters intact.  Returns nonzero on failure.
 */
int test_proxyinfo(opts_t opts, char *ipaddr, char *helo, char *sender,
		   char *recipient)
{
	char filterfile[256];		 /* RATS: ignore (OK) */
	char filteroutfile[256];	 /* RATS: ignore (OK) */
	int filterfd, filteroutfd;
	FILE *fptr;
	int insrv_wfd;
	int outsrv_wfd;
	int insrv_rfd[2];
	int outsrv_rfd[2];
	pid_t child;
	int status;
	int ret;

	if (pipe(insrv_rfd)) {
		return 1;
	}

	if (pipe(outsrv_rfd)) {
		close(insrv_rfd[0]);
		close(insrv_rfd[1]);
		return 1;
	}

	insrv_wfd = open("/dev/null", O_WRONLY);
	if (insrv_wfd < 0) {
		close(insrv_rfd[0]);
		close(insrv_rfd[1]);
		close(outsrv_rfd[0]);
		close(outsrv_rfd[1]);
		return 1;
	}

	outsrv_wfd = open("/dev/null", O_WRONLY);
	if (outsrv_wfd < 0) {
		close(insrv_rfd[0]);
		close(insrv_rfd[1]);
		close(outsrv_rfd[0]);
		close(outsrv_rfd[1]);
		close(insrv_wfd);
		return 1;
	}

	if (test_tmpfd(filterfile, sizeof(filterfile), &filterfd)) {
		close(insrv_rfd[0]);
		close(insrv_rfd[1]);
		close(outsrv_rfd[0]);
		close(outsrv_rfd[1]);
		close(insrv_wfd);
		close(outsrv_wfd);
		return 1;
	}

	if (test_tmpfd(filteroutfile, sizeof(filteroutfile), &filteroutfd)) {
		close(insrv_rfd[0]);
		close(insrv_rfd[1]);
		close(outsrv_rfd[0]);
		close(outsrv_rfd[1]);
		close(insrv_wfd);
		close(outsrv_wfd);
		close(filterfd);
		remove(filterfile);
		return 1;
	}

	test_fdprintf(filterfd, "#!/bin/sh\n");
	test_fdprintf(filterfd, "echo \"$REMOTEIP\" >> %s\n",
		      filteroutfile);
	test_fdprintf(filterfd, "echo \"$HELO\" >> %s\n", filteroutfile);
	test_fdprintf(filterfd, "echo \"$SENDER\" >> %s\n", filteroutfile);
	test_fdprintf(filterfd, "echo \"$RECIPIENT\" >> %s\n",
		      filteroutfile);
	test_fdprintf(filterfd, "exit 0\n");

	close(filterfd);
	close(filteroutfd);

	chmod(filterfile, S_IRUSR | S_IWUSR | S_IXUSR);

	child = fork();

	if (child < 0) {
		close(insrv_rfd[0]);
		close(insrv_rfd[1]);
		close(outsrv_rfd[0]);
		close(outsrv_rfd[1]);
		close(insrv_wfd);
		close(outsrv_wfd);
		remove(filterfile);
		remove(filteroutfile);
		return 1;
	}

	if (child == 0) {
		int childret;

		close(insrv_rfd[1]);
		close(outsrv_rfd[1]);

		opts->filtercommand = filterfile;

		childret =
		    smtp_main(opts, insrv_rfd[0], insrv_wfd, outsrv_rfd[0],
			      outsrv_wfd);

		close(insrv_rfd[0]);
		close(outsrv_rfd[0]);
		close(insrv_wfd);
		close(outsrv_wfd);

		exit(childret);
	}

	close(insrv_rfd[0]);
	close(outsrv_rfd[0]);

	test_fdprintf(outsrv_rfd[1], "%s\r\n", "220 test ESMTP");
	test_fdprintf(insrv_rfd[1], "%s\r\n", "HELO me");
	test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 test");
	test_fdprintf(insrv_rfd[1],
		      "XFORWARD FOO=BAR ADDR=%s HELO=%s TEST=xxx\r\n",
		      ipaddr, helo);
	test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 test");

	test_fdprintf(insrv_rfd[1], "MAIL FROM:<%s>\r\n", sender);
	test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 OK");

	test_fdprintf(insrv_rfd[1], "RCPT TO:<%s>\r\n", recipient);
	test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 OK");

	test_fdprintf(insrv_rfd[1], "RCPT TO:<junk@junk.junk>\r\n");
	test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 OK");

	test_fdprintf(insrv_rfd[1], "%s\r\n", "DATA");

	test_fdprintf(insrv_rfd[1], "%s\r\n", "blank");

	test_fdprintf(insrv_rfd[1], "%s\r\n", ".");
	test_fdprintf(outsrv_rfd[1], "%s\r\n",
		      "354 End data with <CR><LF>.<CR><LF>");
	test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 OK sent");

	test_fdprintf(outsrv_rfd[1], "%s\r\n", "221 Bye");

	close(insrv_rfd[1]);
	close(outsrv_rfd[1]);

	waitpid(child, &status, 0);

	close(insrv_wfd);
	close(outsrv_wfd);

	remove(filterfile);

	fptr = fopen(filteroutfile, "r");
	if (fptr == NULL) {
		remove(filteroutfile);
		return 1;
	}

	ret = 0;
	if (test_proxyinfo_checkline(fptr, ipaddr))
		ret = 1;
	if (test_proxyinfo_checkline(fptr, helo))
		ret = 1;
	if (test_proxyinfo_checkline(fptr, sender))
		ret = 1;
	if (test_proxyinfo_checkline(fptr, recipient))
		ret = 1;

	fclose(fptr);
	remove(filteroutfile);

	return ret;
}


/*
 * Check the SMTP proxy can handle simple message information.
 */
int test_email_info_simple(opts_t opts)
{
	return test_proxyinfo(opts, "127.0.0.1", "localhost", "me@my.com",
			      "you@your.com");
}


/*
 * Check the SMTP proxy can handle oversized message information.
 */
int test_email_info_oversized(opts_t opts)
{
	char buf1[4096];		 /* RATS: ignore */
	char buf2[4096];		 /* RATS: ignore */
	char buf3[4096];		 /* RATS: ignore */
	char buf4[4096];		 /* RATS: ignore */
	int len, i;

	srand(1234);			    /* RATS: ignore (predictable) */
	for (len = 10; len < 800;
	     len += (1 + rand() % (len < 100 ? 15 : 63))) {
		printf("%-4d\b\b\b\b", len);
		for (i = 0; i < len; i++) {
			buf1[i] = 'A' + rand() % 31;
			buf2[i] = 'A' + rand() % 31;
			buf3[i] = 'A' + rand() % 31;
			buf4[i] = 'A' + rand() % 31;
		}
		buf1[i] = 0;
		buf2[i] = 0;
		buf3[i] = 0;
		buf4[i] = 0;
		if (test_proxyinfo(opts, buf1, buf2, buf3, buf4))
			return 1;
	}

	/*
	 * We can expect things to break for really long strings, since the
	 * line buffering will break up the XFORWARD command, and this is
	 * acceptable (since it's bogus information we wouldn't particularly
	 * want to log it anyway).
	 *
	 * However, we run the test anyway (and discard the results) since
	 * this will bring to light any overflows when run as "make memtest"
	 * (i.e. with valgrind).
	 */
	for (len = 900; len < 3000; len += (1 + rand() % 1023)) {
		printf("%-4d\b\b\b\b", len);
		for (i = 0; i < len; i++) {
			buf1[i] = 'A' + rand() % 31;
			buf2[i] = 'A' + rand() % 31;
			buf3[i] = 'A' + rand() % 31;
			buf4[i] = 'A' + rand() % 31;
		}
		buf1[i] = 0;
		buf2[i] = 0;
		buf3[i] = 0;
		buf4[i] = 0;
		test_proxyinfo(opts, buf1, buf2, buf3, buf4);
	}

	printf("    \b\b\b\b");

	return 0;
}

/* EOF */
