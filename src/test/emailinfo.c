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


/*@ -redecl @*/
extern int smtp_main(opts_t, int, int, int, int);

/*@ -mustfreeonly -compmempass @*/


/*
 * Return nonzero if the next line in fptr doesn't match the given string
 * (checking only up to the first 99 characters).
 */
static int test_proxyinfo_checkline( /*@null@ */ FILE * fptr, char *str)
{
	char linebuf[256];		 /* RATS: ignore (OK) */
	char *ptr;

	if (fptr == NULL)
		return 1;

	linebuf[0] = '\0';
	linebuf[sizeof(linebuf) - 1] = '\0';
	(void) fgets(linebuf, (int) (sizeof(linebuf) - 1), fptr);

	ptr = strchr(linebuf, '\n');
	if (ptr != NULL)
		ptr[0] = '\0';

	if (strlen(str) < (size_t) 99) {
		if (strcmp(str, linebuf) == 0)
			return 0;
		return 1;
	}

	if (strncmp(str, linebuf, (size_t) 99) == 0)
		return 0;
	return 1;
}


/*
 * Check that the IP address, HELO, MAIL FROM, and RCPT TO sent through the
 * proxy end up in the filter's environment correctly with the first 99
 * characters intact.  Returns nonzero on failure.
 */
static int test_proxyinfo(opts_t opts, char *ipaddr, char *helo,
			  char *sender, char *recipient)
{
	char filterfile[256];		 /* RATS: ignore (OK) */
	char filteroutfile[256];	 /* RATS: ignore (OK) */
	int filterfd, filteroutfd;
	FILE *fptr;
	int insrv_wfd;
	int outsrv_wfd;
	int insrv_rfd[2];
	int outsrv_rfd[2];
	pid_t child, wret;
	int status;
	int ret;

	memset(filterfile, 0, sizeof(filterfile));
	memset(filteroutfile, 0, sizeof(filteroutfile));
	filterfd = -1;
	filteroutfd = -1;
	insrv_wfd = -1;
	outsrv_wfd = -1;
	insrv_rfd[0] = -1;
	insrv_rfd[1] = -1;
	outsrv_rfd[0] = -1;
	outsrv_rfd[1] = -1;
	fptr = NULL;
	child = 0;
	ret = 0;

	FAIL_IF(pipe(insrv_rfd) != 0);
	FAIL_IF(pipe(outsrv_rfd) != 0);

	insrv_wfd = open("/dev/null", O_WRONLY);
	FAIL_IF(insrv_wfd < 0);

	outsrv_wfd = open("/dev/null", O_WRONLY);
	FAIL_IF(outsrv_wfd < 0);

	FAIL_IF(test_tmpfd(filterfile, (int) sizeof(filterfile), &filterfd)
		!= 0);
	FAIL_IF(test_tmpfd
		(filteroutfile, (int) sizeof(filteroutfile),
		 &filteroutfd) != 0);

	FAIL_IF(test_fdprintf(filterfd, "#!/bin/sh\n") != 0);
	FAIL_IF(test_fdprintf
		(filterfd, "echo \"$REMOTEIP\" >> %s\n",
		 filteroutfile) != 0);
	FAIL_IF(test_fdprintf
		(filterfd, "echo \"$HELO\" >> %s\n", filteroutfile) != 0);
	FAIL_IF(test_fdprintf
		(filterfd, "echo \"$SENDER\" >> %s\n",
		 filteroutfile) != 0);
	FAIL_IF(test_fdprintf
		(filterfd, "echo \"$RECIPIENT\" >> %s\n",
		 filteroutfile) != 0);
	FAIL_IF(test_fdprintf(filterfd, "exit 0\n") != 0);

	FAIL_IF(close(filterfd) != 0);
	FAIL_IF(close(filteroutfd) != 0);

	FAIL_IF(chmod(filterfile, S_IRUSR | S_IWUSR | S_IXUSR) != 0);

	child = fork();
	FAIL_IF(child < 0);

	if (child == 0) {
		int childret;

		(void) close(insrv_rfd[1]);
		(void) close(outsrv_rfd[1]);

		opts->filtercommand = filterfile;

		childret =
		    smtp_main(opts, insrv_rfd[0], insrv_wfd, outsrv_rfd[0],
			      outsrv_wfd);

		(void) close(insrv_rfd[0]);
		(void) close(outsrv_rfd[0]);
		(void) close(insrv_wfd);
		(void) close(outsrv_wfd);

		exit(childret);
	}

	FAIL_IF(close(insrv_rfd[0]) != 0);
	insrv_rfd[0] = -1;
	FAIL_IF(close(outsrv_rfd[0]) != 0);
	outsrv_rfd[0] = -1;

	FAIL_IF(test_fdprintf(outsrv_rfd[1], "%s\r\n", "220 test ESMTP") !=
		0);
	FAIL_IF(test_fdprintf(insrv_rfd[1], "%s\r\n", "HELO me") != 0);
	FAIL_IF(test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 test") != 0);
	FAIL_IF(test_fdprintf
		(insrv_rfd[1],
		 "XFORWARD FOO=BAR ADDR=%s HELO=%s TEST=xxx\r\n", ipaddr,
		 helo) != 0);
	FAIL_IF(test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 test") != 0);

	FAIL_IF(test_fdprintf(insrv_rfd[1], "MAIL FROM:<%s>\r\n", sender)
		!= 0);
	FAIL_IF(test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 OK") != 0);

	FAIL_IF(test_fdprintf(insrv_rfd[1], "RCPT TO:<%s>\r\n", recipient)
		!= 0);
	FAIL_IF(test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 OK") != 0);

	FAIL_IF(test_fdprintf(insrv_rfd[1], "RCPT TO:<junk@junk.junk>\r\n")
		!= 0);
	FAIL_IF(test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 OK") != 0);

	FAIL_IF(test_fdprintf(insrv_rfd[1], "%s\r\n", "DATA") != 0);

	FAIL_IF(test_fdprintf(insrv_rfd[1], "%s\r\n", "blank") != 0);

	FAIL_IF(test_fdprintf(insrv_rfd[1], "%s\r\n", ".") != 0);
	FAIL_IF(test_fdprintf
		(outsrv_rfd[1], "%s\r\n",
		 "354 End data with <CR><LF>.<CR><LF>") != 0);
	FAIL_IF(test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 OK sent") !=
		0);

	FAIL_IF(test_fdprintf(outsrv_rfd[1], "%s\r\n", "221 Bye") != 0);

	FAIL_IF(close(insrv_rfd[1]) != 0);
	insrv_rfd[1] = -1;
	FAIL_IF(close(outsrv_rfd[1]) != 0);
	outsrv_rfd[1] = -1;

	wret = waitpid(child, &status, 0);
	FAIL_IF(wret != child);

	FAIL_IF(close(insrv_wfd) != 0);
	insrv_wfd = -1;
	FAIL_IF(close(outsrv_wfd) != 0);
	outsrv_wfd = -1;

	FAIL_IF(remove(filterfile) != 0);
	filterfile[0] = '\0';

	fptr = fopen(filteroutfile, "r");
	FAIL_IF(fptr == NULL);

	FAIL_IF(test_proxyinfo_checkline(fptr, ipaddr) != 0);
	FAIL_IF(test_proxyinfo_checkline(fptr, helo) != 0);
	FAIL_IF(test_proxyinfo_checkline(fptr, sender) != 0);
	FAIL_IF(test_proxyinfo_checkline(fptr, recipient) != 0);

	FAIL_IF(fptr == NULL || fclose(fptr) != 0);
	fptr = NULL;
	FAIL_IF(remove(filteroutfile) != 0);
	filteroutfile[0] = '\0';

      end:
	CLOSE_IF_VALID(insrv_rfd[0]);
	CLOSE_IF_VALID(insrv_rfd[1]);
	CLOSE_IF_VALID(outsrv_rfd[0]);
	CLOSE_IF_VALID(outsrv_rfd[1]);
	CLOSE_IF_VALID(insrv_wfd);
	CLOSE_IF_VALID(outsrv_wfd);
	FCLOSE_IF_VALID(fptr);
	REMOVE_IF_VALID(filterfile);
	REMOVE_IF_VALID(filteroutfile);

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
	int len, i, ret;

	srand((unsigned int) 1234);	    /* RATS: ignore (predictable) */
	for (len = 10; len < 800;
	     len += (1 + rand() % (len < 100 ? 15 : 63))) {
		printf("%-4d\b\b\b\b", len);
		for (i = 0; i < len; i++) {
			buf1[i] = (char) ((int) 'A' + rand() % 31);
			buf2[i] = (char) ((int) 'A' + rand() % 31);
			buf3[i] = (char) ((int) 'A' + rand() % 31);
			buf4[i] = (char) ((int) 'A' + rand() % 31);
		}
		buf1[i] = '\0';
		buf2[i] = '\0';
		buf3[i] = '\0';
		buf4[i] = '\0';
		ret = test_proxyinfo(opts, buf1, buf2, buf3, buf4);
		if (ret != 0)
			return ret;
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
			buf1[i] = (char) ((int) 'A' + rand() % 31);
			buf2[i] = (char) ((int) 'A' + rand() % 31);
			buf3[i] = (char) ((int) 'A' + rand() % 31);
			buf4[i] = (char) ((int) 'A' + rand() % 31);
		}
		buf1[i] = '\0';
		buf2[i] = '\0';
		buf3[i] = '\0';
		buf4[i] = '\0';
		ret = test_proxyinfo(opts, buf1, buf2, buf3, buf4);
	}

	printf("    \b\b\b\b");

	return 0;
}

/* EOF */
