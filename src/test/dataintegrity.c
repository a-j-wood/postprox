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


int smtp_main(opts_t, int, int, int, int);


/*
 * Test the integrity of data sent through the proxy by sending the given
 * data as a fake email and making sure the SMTP conversation ends up the
 * same at the output as it did on the input. Returns nonzero on failure.
 */
int test_proxydata(opts_t opts, char *data, int datasize)
{
	char insrv_rcfile[256];		 /* RATS: ignore */
	char outsrv_wfile[256];		 /* RATS: ignore */
	int insrv_rcfd;
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

	if (test_tmpfd(insrv_rcfile, sizeof(insrv_rcfile), &insrv_rcfd)) {
		close(insrv_rfd[0]);
		close(insrv_rfd[1]);
		close(outsrv_rfd[0]);
		close(outsrv_rfd[1]);
		close(insrv_wfd);
		return 1;
	}

	if (test_tmpfd(outsrv_wfile, sizeof(outsrv_wfile), &outsrv_wfd)) {
		close(insrv_rfd[0]);
		close(insrv_rfd[1]);
		close(outsrv_rfd[0]);
		close(outsrv_rfd[1]);
		close(insrv_wfd);
		close(insrv_rcfd);
		remove(insrv_rcfile);
		return 1;
	}

	child = fork();

	if (child < 0) {
		close(insrv_rfd[0]);
		close(insrv_rfd[1]);
		close(outsrv_rfd[0]);
		close(outsrv_rfd[1]);
		close(insrv_wfd);
		close(insrv_rcfd);
		remove(insrv_rcfile);
		close(outsrv_wfd);
		remove(outsrv_wfile);
		return 1;
	}

	if (child == 0) {
		int childret;

		close(insrv_rfd[1]);
		close(outsrv_rfd[1]);
		close(insrv_rcfd);

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
	test_usleep(100);
	test_fdprintf(insrv_rfd[1], "%s\r\n", "HELO me");
	test_fdprintf(insrv_rcfd, "%s\r\n", "HELO me");
	test_usleep(100);
	test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 test");
	test_usleep(100);

	test_fdprintf(insrv_rfd[1], "%s\r\n", "MAIL FROM:<test@test.com>");
	test_fdprintf(insrv_rcfd, "%s\r\n", "MAIL FROM:<test@test.com>");
	test_usleep(100);
	test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 OK");
	test_usleep(100);

	test_fdprintf(insrv_rfd[1], "%s\r\n", "RCPT TO:<test@test.com>");
	test_fdprintf(insrv_rcfd, "%s\r\n", "RCPT TO:<test@test.com>");
	test_usleep(100);
	test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 OK");
	test_usleep(100);

	test_fdprintf(insrv_rfd[1], "%s\r\n", "DATA");
	test_fdprintf(insrv_rcfd, "%s\r\n", "DATA");
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
				write_retry(insrv_rfd[1], ".", 1);
				write_retry(insrv_rcfd, ".", 1);
			}

			/*
			 * Network newlines, i.e. convert \n to \r\n.
			 */
			nextptr = memchr(ptr, '\n', bytesleft);
			if (nextptr) {
				int offset;
				offset = nextptr - ptr;
				if (offset > 0) {
					write_retry(insrv_rfd[1], ptr,
						    offset);
					write_retry(insrv_rcfd, ptr,
						    offset);
				}
				write_retry(insrv_rfd[1], "\r\n", 2);
				write_retry(insrv_rcfd, "\r\n", 2);
				ptr = nextptr;
				ptr++;
				bytesleft -= offset;
				bytesleft--;
			} else {
				write_retry(insrv_rfd[1], ptr, bytesleft);
				write_retry(insrv_rcfd, ptr, bytesleft);
				bytesleft = 0;
			}
		}
	}

	if (data[datasize - 1] != '\n') {
		test_fdprintf(insrv_rfd[1], "\r\n");
		test_fdprintf(insrv_rcfd, "\r\n");
	}

	test_fdprintf(insrv_rfd[1], "%s\r\n", ".");
	test_fdprintf(insrv_rcfd, "%s\r\n", ".");
	test_usleep(2000);
	test_fdprintf(outsrv_rfd[1], "%s\r\n",
		      "354 End data with <CR><LF>.<CR><LF>");
	test_usleep(1000);
	test_fdprintf(outsrv_rfd[1], "%s\r\n", "250 OK sent");
	test_usleep(100);

	/*
	 * Here we just disconnect instead of sending the QUIT, but we store
	 * QUIT in the read-compare-file, because the proxy will send one
	 * back anyway due to the closing of the connection. Because of
	 * timing issues if we send the QUIT explicitly, then sometimes we
	 * see two coming back, throwing off the test.
	 *
	 * So we DON'T do: test_fdprintf(insrv_rfd[1], "%s\r\n", "QUIT");
	 */
	test_fdprintf(insrv_rcfd, "%s\r\n", "QUIT");
	test_fdprintf(outsrv_rfd[1], "%s\r\n", "221 Bye");

	close(insrv_rfd[1]);
	close(outsrv_rfd[1]);

	waitpid(child, &status, 0);

	/*
	 * Poor-man's cmp(1).
	 */
	ret = 0;
	{
		FILE *fptr1;
		FILE *fptr2;
		char buf1[1024];	 /* RATS: ignore */
		char buf2[1024];	 /* RATS: ignore */
		int got1, got2;

		fptr1 = fopen(insrv_rcfile, "rb");
		fptr2 = fopen(outsrv_wfile, "rb");

		while ((fptr1 != NULL)
		       && (fptr2 != NULL)
		       && (!feof(fptr1))
		       && (!feof(fptr2))
		    ) {
			got1 = fread(buf1, 1, sizeof(buf1), fptr1);
			if (got1 < 0) {
				ret = 1;
				break;
			}
			got2 = fread(buf2, got1, 1, fptr2);
			if (got2 < 1) {
				ret = 1;
				break;
			}
			if (memcmp(buf1, buf2, got1) != 0) {
				ret = 1;
				break;
			}
		}
		fclose(fptr1);
		fclose(fptr2);
	}

	/*
	 * On failure, we leave the SMTP input and output files, and mark
	 * them as such by appending INPUT and OUTPUT to them.
	 */
	if (ret == 0) {
		remove(insrv_rcfile);
		remove(outsrv_wfile);
	} else {
		test_fdprintf(insrv_rcfd, "\n%s\n", _("--INPUT--"));
		test_fdprintf(outsrv_wfd, "\n%s\n", _("--OUTPUT--"));
	}

	close(insrv_rcfd);
	close(insrv_wfd);
	close(outsrv_wfd);

	return ret;
}


/*
 * Check the SMTP proxy can handle a simple email.
 */
int test_data_integrity_simple(opts_t opts)
{
	char *str;

	str = "From: test\nTo: test\nSubject: test\n\nTest!\n";

	return test_proxydata(opts, str, strlen(str));
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
		memset(buf, '.', i);
		buf[i] = '\n';
		buf[i + 1] = 0;
		strcat(str, buf);	    /* RATS: ignore */
	}

	return test_proxydata(opts, str, strlen(str));
}


/*
 * Check the SMTP proxy can handle mail with some long lines in it.
 */
int test_data_integrity_longlines(opts_t opts)
{
	char str[256 * 1024];		 /* RATS: ignore */
	char buf[4096];			 /* RATS: ignore */
	int i;

	str[0] = 0;

	for (i = 900; i < 1100; i++) {
		sprintf(buf, "%*s\n", i, " ");
		strcat(str, buf);	    /* RATS: ignore */
	}

	return test_proxydata(opts, str, strlen(str));
}


/*
 * Check the SMTP proxy can handle mail consisting of a big lump of binary
 * data.
 */
int test_data_integrity_randomdata(opts_t opts)
{
	char buf[163840];		 /* RATS: ignore */
	int i;

	srand(1234);			    /* RATS: ignore (predictable) */
	for (i = 0; i < sizeof(buf); i++) {
		buf[i] = rand();
	}

	return test_proxydata(opts, buf, sizeof(buf));
}

/* EOF */
