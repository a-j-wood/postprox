/*
 * Test functions to check data integrity when sent through the proxy.
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


/*
 * Test the integrity of data sent through the proxy by sending the given
 * data as a fake email and making sure the SMTP conversation ends up the
 * same at the output as it did on the input. Returns nonzero on failure.
 */
static int test_proxydata(opts_t opts, char *data, int datasize)
{
	char insrv_rcfile[256];		 /* RATS: ignore */
	char outsrv_wfile[256];		 /* RATS: ignore */
	int insrv_rcfd;
	int insrv_wfd;
	int outsrv_wfd;
	int insrv_rfd[2];
	int outsrv_rfd[2];
	pid_t child, wret;
	FILE *fcmp_a;
	FILE *fcmp_b;
	int status;
	int ret;

	memset(insrv_rcfile, 0, sizeof(insrv_rcfile));
	memset(outsrv_wfile, 0, sizeof(outsrv_wfile));
	insrv_rcfd = -1;
	insrv_wfd = -1;
	outsrv_wfd = -1;
	insrv_rfd[0] = -1;
	insrv_rfd[1] = -1;
	outsrv_rfd[0] = -1;
	outsrv_rfd[1] = -1;
	fcmp_a = NULL;
	fcmp_b = NULL;
	child = 0;

	ret = 0;

	FAIL_IF(pipe(insrv_rfd) != 0);
	FAIL_IF(pipe(outsrv_rfd) != 0);

	insrv_wfd = open("/dev/null", O_WRONLY);
	FAIL_IF(insrv_wfd < 0);

	FAIL_IF(test_tmpfd
		(insrv_rcfile, (int) sizeof(insrv_rcfile),
		 &insrv_rcfd) != 0);
	FAIL_IF(test_tmpfd
		(outsrv_wfile, (int) sizeof(outsrv_wfile),
		 &outsrv_wfd) != 0);

	child = fork();

	FAIL_IF(child < 0);

	if (child == 0) {
		int childret;

		(void) close(insrv_rfd[1]);
		(void) close(outsrv_rfd[1]);
		(void) close(insrv_rcfd);

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
	test_usleep(100);
	FAIL_IF(test_fdprintf(insrv_rfd[1], "%s\r\n", "HELO me") != 0);
	FAIL_IF(test_fdprintf(insrv_rcfd, "%s\r\n", "HELO me") != 0);
	test_usleep(100);
	FAIL_IF(test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 test") != 0);
	test_usleep(100);

	FAIL_IF(test_fdprintf
		(insrv_rfd[1], "%s\r\n",
		 "MAIL FROM:<test@test.com>") != 0);
	FAIL_IF(test_fdprintf
		(insrv_rcfd, "%s\r\n", "MAIL FROM:<test@test.com>") != 0);
	test_usleep(100);
	FAIL_IF(test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 OK") != 0);
	test_usleep(100);

	FAIL_IF(test_fdprintf
		(insrv_rfd[1], "%s\r\n", "RCPT TO:<test@test.com>") != 0);
	FAIL_IF(test_fdprintf
		(insrv_rcfd, "%s\r\n", "RCPT TO:<test@test.com>") != 0);
	test_usleep(100);
	FAIL_IF(test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 OK") != 0);
	test_usleep(100);

	FAIL_IF(test_fdprintf(insrv_rfd[1], "%s\r\n", "DATA") != 0);
	FAIL_IF(test_fdprintf(insrv_rcfd, "%s\r\n", "DATA") != 0);
	test_usleep(100);

	/*
	 * We now need to encode the data we want to send using the
	 * transparency procedure outlined in RFC2821 section 4.5.2.
	 */
	{
		int bytesleft;
		char *nextptr;
		char *ptr;

		ptr = data;
		bytesleft = datasize;

		while (bytesleft > 0) {

			/*
			 * Escape leading dots.
			 */
			if (ptr[0] == '.') {
				FAIL_IF(write_retry(insrv_rfd[1], ".", 1)
					!= 0);
				FAIL_IF(write_retry(insrv_rcfd, ".", 1) !=
					0);
			}

			/*
			 * Network newlines, i.e. convert \n to \r\n.
			 */
			nextptr =
			    memchr(ptr, (int) '\n', (size_t) bytesleft);
			if (nextptr != NULL) {
				int offset;
				offset = nextptr - ptr;
				if (offset > 0) {
					FAIL_IF(write_retry
						(insrv_rfd[1], ptr,
						 offset) != 0);
					FAIL_IF(write_retry
						(insrv_rcfd, ptr,
						 offset) != 0);
				}
				FAIL_IF(write_retry
					(insrv_rfd[1], "\r\n", 2) != 0);
				FAIL_IF(write_retry(insrv_rcfd, "\r\n", 2)
					!= 0);
				ptr = nextptr;
				ptr++;
				bytesleft -= offset;
				bytesleft--;
			} else {
				FAIL_IF(write_retry
					(insrv_rfd[1], ptr,
					 bytesleft) != 0);
				FAIL_IF(write_retry
					(insrv_rcfd, ptr, bytesleft) != 0);
				bytesleft = 0;
			}
		}
	}

	if (data[datasize - 1] != '\n') {
		FAIL_IF(test_fdprintf(insrv_rfd[1], "\r\n") != 0);
		FAIL_IF(test_fdprintf(insrv_rcfd, "\r\n") != 0);
	}

	FAIL_IF(test_fdprintf(insrv_rfd[1], "%s\r\n", ".") != 0);
	FAIL_IF(test_fdprintf(insrv_rcfd, "%s\r\n", ".") != 0);
	test_usleep(2000);
	FAIL_IF(test_fdprintf
		(outsrv_rfd[1], "%s\r\n",
		 "354 End data with <CR><LF>.<CR><LF>") != 0);
	test_usleep(1000);
	FAIL_IF(test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 OK sent") !=
		0);
	test_usleep(100);

	/*
	 * Here we have to wait before disconnecting after the QUIT, because
	 * if we disconnect before the proxy processes the QUIT, it will
	 * also issue one itself, so QUIT will appear twice in the output
	 * file but only once in the read-compare file.
	 */
	FAIL_IF(test_fdprintf(insrv_rfd[1], "%s\r\n", "QUIT") != 0);
	FAIL_IF(test_fdprintf(insrv_rcfd, "%s\r\n", "QUIT") != 0);
	test_usleep(100000);
	FAIL_IF(test_fdprintf(outsrv_rfd[1], "%s\r\n", "221 Bye") != 0);

	FAIL_IF(close(insrv_rfd[1]) != 0);
	insrv_rfd[1] = -1;
	FAIL_IF(close(outsrv_rfd[1]) != 0);
	outsrv_rfd[1] = -1;

	wret = waitpid(child, &status, 0);
	FAIL_IF(wret != child);

	/*
	 * Poor-man's cmp(1).
	 */
	{
		char buf1[1024];	 /* RATS: ignore */
		char buf2[1024];	 /* RATS: ignore */
		size_t got1, got2;

		fcmp_a = fopen(insrv_rcfile, "rb");
		FAIL_IF(fcmp_a == NULL);
		fcmp_b = fopen(outsrv_wfile, "rb");
		FAIL_IF(fcmp_b == NULL);

		while ((fcmp_a != NULL)
		       && (fcmp_b != NULL)
		       && (feof(fcmp_a) == 0)
		       && (feof(fcmp_b) == 0)
		    ) {
			got1 =
			    fread(buf1, (size_t) 1, sizeof(buf1), fcmp_a);
			FAIL_IF(got1 < (size_t) 1);
			got2 =
			    fread(buf2, (size_t) got1, (size_t) 1, fcmp_b);
			FAIL_IF(got2 < (size_t) 1);
			FAIL_IF(memcmp(buf1, buf2, (size_t) got1) != 0);
		}

		FAIL_IF(fcmp_a == NULL || fclose(fcmp_a) != 0);
		fcmp_a = NULL;
		FAIL_IF(fcmp_b == NULL || fclose(fcmp_b) != 0);
		fcmp_b = NULL;
	}

      end:
	CLOSE_IF_VALID(insrv_rfd[0]);
	CLOSE_IF_VALID(insrv_rfd[1]);
	CLOSE_IF_VALID(outsrv_rfd[0]);
	CLOSE_IF_VALID(outsrv_rfd[1]);
	FCLOSE_IF_VALID(fcmp_a);
	FCLOSE_IF_VALID(fcmp_b);
	/*
	 * On failure, we leave the SMTP input and output files, and mark
	 * them as such by appending INPUT and OUTPUT to them.
	 */
	if (ret != 0) {
		if (insrv_rcfd >= 0)
			(void) test_fdprintf(insrv_rcfd, "\n%s\n",
					     _("--INPUT--"));
		if (outsrv_wfd >= 0)
			(void) test_fdprintf(outsrv_wfd, "\n%s\n",
					     _("--OUTPUT--"));
	}
	CLOSE_IF_VALID(insrv_rcfd);
	CLOSE_IF_VALID(insrv_wfd);
	CLOSE_IF_VALID(outsrv_wfd);
	if (ret == 0) {
		REMOVE_IF_VALID(insrv_rcfile);
		REMOVE_IF_VALID(outsrv_wfile);
	}

	return ret;

}


/*
 * Check the SMTP proxy can handle a simple email.
 */
int test_data_integrity_simple(opts_t opts)
{
	char *str;

	str = "From: test\nTo: test\nSubject: test\n\nTest!\n";

	return test_proxydata(opts, str, (int) strlen(str));
}

/*
 * Check the SMTP proxy can handle mail with dots in it.
 */
int test_data_integrity_dots(opts_t opts)
{
	char str[300 * 1024];		 /* RATS: ignore */
	char buf[4096];			 /* RATS: ignore */
	int i;

	strcpy(str, ".\n..\n...\n....\n.....\n");

	for (i = 800; i < 1100; i++) {
		memset(buf, (int) '.', (size_t) i);
		buf[i] = '\n';
		buf[i + 1] = '\0';
		strcat(str, buf);	    /* RATS: ignore */
	}

	return test_proxydata(opts, str, (int) strlen(str));
}


/*
 * Check the SMTP proxy can handle mail with some long lines in it.
 */
int test_data_integrity_longlines(opts_t opts)
{
	char str[256 * 1024];		 /* RATS: ignore */
	char buf[4096];			 /* RATS: ignore */
	int i;

	str[0] = '\0';

	for (i = 900; i < 1100; i++) {
#ifdef HAVE_SNPRINTF
		(void) snprintf(buf, sizeof(buf), "%*s\n", i, " ");
#else
		(void) sprintf(buf, "%*s\n", i, " ");
#endif
		strcat(str, buf);	    /* RATS: ignore */
	}

	return test_proxydata(opts, str, (int) strlen(str));
}


/*
 * Check the SMTP proxy can handle mail consisting of a big lump of binary
 * data.
 */
int test_data_integrity_randomdata(opts_t opts)
{
	char buf[163840];		 /* RATS: ignore */
	int i;

	memset(buf, 0, sizeof(buf));

	srand((unsigned int) 1234);	    /* RATS: ignore (predictable) */
	for (i = 0; i < (int) sizeof(buf); i++) {
		buf[i] = (char) rand();
	}

	return test_proxydata(opts, buf, (int) sizeof(buf));
}

/* EOF */
