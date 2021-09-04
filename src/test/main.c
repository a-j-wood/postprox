/*
 * Main test program entry point.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#include "config.h"
#include "options.h"
#include "library.h"
#include "log.h"
#include "testi.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_MCHECK_H
#include <mcheck.h>
#endif

/*
 * All tests must be prototyped here, and listed in main() below. Each test
 * should return 0 on pass, 1 on fail, and take an opts_t argument which it
 * can modify the contents of however it likes (but it must free all memory
 * allocated).
 */
typedef int (*testfunc_t) (opts_t);
int test_write_retry(opts_t);
int test_data_integrity_simple(opts_t);
int test_data_integrity_dots(opts_t);
int test_data_integrity_longlines(opts_t);
int test_data_integrity_randomdata(opts_t);
int test_email_info_simple(opts_t);
int test_email_info_oversized(opts_t);


/*
 * Run all of the tests.
 */
int main(int argc, char **argv)
{
	struct opts_s opts;
	struct {
		char *name;
		testfunc_t func;
	} test[] = {
		{
		_("retrying write() function"), test_write_retry}, {
		_("simple data sending"), test_data_integrity_simple},
		{
		_("sending data with dots in it"),
			    test_data_integrity_dots}, {
		_("sending data with long lines"),
			    test_data_integrity_longlines}, {
		_("sending random binary data"),
			    test_data_integrity_randomdata}, {
		_("simple IP/HELO/sender/recipient collection"),
			    test_email_info_simple}, {
		_("oversized IP/HELO/sender/recipient values"),
			    test_email_info_oversized}, {
		NULL, NULL}
	};
	int testnum, numtests, fail;

#ifdef HAVE_MCHECK_H
	if (getenv("MALLOC_TRACE"))	    /* RATS: ignore (value unused) */
		mtrace();
#endif

#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif

	for (testnum = 0; test[testnum].name != NULL; testnum++);
	numtests = testnum;

	setvbuf(stdout, NULL, _IONBF, 0);

	fail = 0;

	for (testnum = 0; test[testnum].name != NULL; testnum++) {

		memset(&opts, 0, sizeof(opts));

		opts.program_name = PACKAGE;
		opts.action = ACTION_PROXY;
		opts.serverhost = "127.0.0.1";
		opts.serverport = 30000;
		opts.listenhost = "127.0.0.1";
		opts.listenport = 30001;
		opts.filtercommand = "true";
		opts.tempdirectory = "/tmp";
		opts.timeout = 30;
		opts.rejectfailures = 0;
		/*
		 * We switch on maximum verbosity and debugging so that any
		 * bugs caused by badly constructed logging code should be
		 * caught (so long as they're not actually in the log_*
		 * functions themselves).
		 */
		opts.verbose = 100;
#ifdef DEBUG
		opts.debug = 100;
#endif

		printf("%3d/%d: %-50s", testnum + 1, numtests,
		       test[testnum].name);
		if (test[testnum].func(&opts)) {
			printf(" %s\n", _("FAILED"));
			fail++;
		} else {
			printf(" %s\n", _("OK"));
		}
	}

	if (fail > 0) {
		printf("%s: %d\n", _("Number of tests failed"), fail);
		return 1;
	}

	printf("%s\n", _("All tests passed."));

	return 0;
}


/*
 * Dummy log function.
 */
void log_line(logpri_t priority, char *format, ...)
{
	char buf[1024];			 /* RATS: ignore (checked OK - ish) */
	char *ptr;
	va_list ap;

	va_start(ap, format);

	buf[0] = 0;

#if HAVE_VSNPRINTF
	vsnprintf(buf, sizeof(buf), format, ap);
#else
	vsprintf(buf, format, ap);	    /* RATS: ignore (unavoidable) */
#endif

	va_end(ap);

	buf[sizeof(buf) - 1] = 0;

	/*
	 * Strip \r and/or \n from the string.
	 */
	ptr = strchr(buf, '\r');
	if (ptr)
		ptr[0] = 0;
	ptr = strchr(buf, '\n');
	if (ptr)
		ptr[0] = 0;

	/*
	 * We could now do something with the line to be logged, if we
	 * wanted to.
	 */
}


/*
 * Utility function to create a temporary file. Returns nonzero on error. On
 * success, creates a temporary file whose name is put into "buf", and puts
 * the open-for-read-and-write file descriptor into *fdptr.
 */
int test_tmpfd(char *buf, int bufsz, int *fdptr)
{
	sprintf(buf, "%.*s/tXXXXXX", bufsz - 10, "/tmp");

#ifdef HAVE_MKSTEMP
	*fdptr = mkstemp(buf);
#else				/* !HAVE_MKSTEMP */
	mktemp(buf);
	*fdptr = open(buf, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
#endif				/* HAVE_MKSTEMP */

	if (*fdptr < 0)
		return -1;
	return 0;
}


/*
 * Utility function to write_retry() the given printf-like string to the
 * given file descriptor. Returns nonzero on error.
 */
int test_fdprintf(int fd, char *format, ...)
{
	int n;
	int sz;
	char *p;
	char *newp;
	va_list ap;
	int ret;

	/*
	 * Note that running under valgrind, the buffer extending thing
	 * below doesn't seem to work too well, so we start out with a big
	 * buffer.
	 */
	sz = 16384;

	p = malloc(sz);
	if (p == NULL)
		return -1;

	/*
	 * Try to allocate enough memory to print the string into. This is
	 * derived from the example in "man printf".
	 */
	while (1) {
		va_start(ap, format);
		n = vsnprintf(p, sz, format, ap);
		va_end(ap);

		if (n > -1 && n < sz)
			break;

		if (n > -1) {
			sz = n + 1;
		} else {
			sz = sz * 2;
		}

		newp = realloc(p, sz);	    /* RATS: ignore (insecure OK) */
		if (newp == NULL) {
			free(p);
			return -1;
		}
	}

	ret = write_retry(fd, p, strlen(p));
	free(p);
	return ret;
}


/*
 * Utility function to sleep for the given number of microseconds. It's not
 * accurate, since it doesn't restart on signal interruption.
 */
void test_usleep(int usec)
{
	struct timespec t;

	t.tv_sec = usec / 1000000;
	t.tv_nsec = usec * 1000;
	nanosleep(&t, NULL);
}

/* EOF */
