/*
 * Run a filter on a spooled message.
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
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>


/*
 * Run the given filter command after setting up the file descriptors and
 * environment variables.
 */
static void smtp_runfilter__child(char *command, char *file, char *outfile,
				  int errfd, char *ipaddr, char *helo,
				  char *sender, char *recipient)
{
	char *varname[] =
	    { "EMAIL", "OUTFILE", "REMOTEIP", "HELO", "SENDER",
		"RECIPIENT", NULL
	};
	char *varval[] =
	    { file, outfile, ipaddr, helo, sender, recipient, NULL };
	char *envvar;
	int nullfdw, nullfdr, i;

	for (i = 0; varname[i] != NULL; i++) {
		if (varval[i] == NULL) {
			putenv(varname[i]);
		} else {
			envvar =
			    malloc(strlen(varname[i]) + 2 +
				   strlen(varval[i]));
			if (envvar == NULL) {
				log_line(LOGPRI_ERROR,
					 _
					 ("failed to set environment for command: %s"),
					 strerror(errno));
				exit(10);
			} else {
				sprintf(envvar, "%s=%s",	/* RATS: ignore */
					varname[i], varval[i]);
				if (putenv(envvar)) {
					log_line(LOGPRI_ERROR,
						 _
						 ("failed to set environment for command: %s"),
						 strerror(errno));
					exit(11);
				}
			}
		}
	}

	nullfdr = open("/dev/null", O_RDONLY);
	nullfdw = open("/dev/null", O_WRONLY);
	if (nullfdr < 0) {
		close(fileno(stdin));
	} else {
		dup2(nullfdr, fileno(stdin));
		close(nullfdr);
	}
	if (nullfdw < 0) {
		close(fileno(stdout));
	} else {
		dup2(nullfdw, fileno(stdout));
		close(nullfdw);
	}

	if (dup2(errfd, fileno(stderr)) < 0) {
		log_line(LOGPRI_ERROR,
			 _("failed to set stderr for command: %s"),
			 strerror(errno));
		exit(3);
	}
	close(errfd);

	execl("/bin/sh", "sh", "-c", command, 0);

	log_line(LOGPRI_ERROR, _("failed to run command [%s]: %s"),
		 command, strerror(errno));
	exit(4);
}


/*
 * Signal handler for SIGALRM - do nothing.
 */
static void smtp_runfilter__sigalrm(int s)
{
}


/*
 * Run the filter command with $EMAIL set to the filename of the spool file,
 * and $OUTFILE set to the filename of the (optional) replacement for the
 * spool file, which if non-empty after the filter runs will be output as
 * the email instead of the contents of $EMAIL.  The last line of the filter
 * command's stderr will be put into errbuf, if the filter ran.
 *
 * Returns the filter exit status if the filter ran - 0 means accept the
 * message, 1 means reject it - or returns -1 if the filter timed out or
 * failed to run (or gave an exit status of 2 or more).
 */
int smtp_runfilter(opts_t opts, char *file, char *outfile, char *errbuf,
		   int bufsize, char *ipaddr, char *helo, char *sender,
		   char *recipient)
{
	char *errfile;
	int errfd;
	pid_t child, pid;
	int status, childexited, childexitcode;
	struct sigaction sa;
	sigset_t sigset;
	time_t started;

	errfile = malloc(strlen(opts->tempdirectory) + 64);
	if (errfile == NULL) {
		log_line(LOGPRI_ERROR,
			 _("could not create temporary filename: %s"),
			 strerror(errno));
		return -1;
	}

	strcpy(errfile, opts->tempdirectory);	/* RATS: ignore (OK) */
	strcat(errfile, "/speXXXXXX");

#ifdef HAVE_MKSTEMP
	errfd = mkstemp(errfile);
#else				/* !HAVE_MKSTEMP */
	mktemp(errfile);
	errfd =
	    open(errfile, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
#endif				/* HAVE_MKSTEMP */

	if (errfd < 0) {
		log_line(LOGPRI_ERROR,
			 _("could not create temporary file: %s"),
			 strerror(errno));
		free(errfile);
		return -1;
	}

	chmod(errfile, S_IRUSR | S_IWUSR);

	child = fork();
	if (child < 0) {
		log_line(LOGPRI_ERROR,
			 _("failed to create child process: %s"),
			 strerror(errno));
		close(errfd);
		unlink(errfile);
		free(errfile);
		return -1;
	} else if (child == 0) {
		smtp_runfilter__child(opts->filtercommand, file, outfile,
				      errfd, ipaddr, helo, sender,
				      recipient);
		exit(0);
	}

	close(errfd);

	/*
	 * Catch SIGALRM with the above signal handler, and make sure our
	 * signal mask wouldn't block it.
	 */
	sa.sa_handler = smtp_runfilter__sigalrm;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	sigaction(SIGALRM, &sa, NULL);

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &sigset, NULL);

	started = time(NULL);

#ifdef DEBUG
	if (opts->debug > 0)
		log_line(LOGPRI_DEBUG, "child process started, pid=%d",
			 child);
#endif

	/*
	 * Enforce the timeout by interrupting waitpid() with an alarm()
	 * call.
	 */
	pid = 0;
	while (difftime(time(NULL), started) < opts->timeout) {
		int alarmsec;

		alarmsec = opts->timeout - (difftime(time(NULL), started));
		if (alarmsec < 1)
			alarmsec = 1;
		alarm(alarmsec);

#ifdef DEBUG
		if (opts->debug > 0)
			log_line(LOGPRI_DEBUG, "alarm set for %dsec",
				 alarmsec);
#endif

		pid = waitpid(child, &status, 0);

		if (pid == child)
			break;
	}

	alarm(0);

	childexited = (pid == child) ? WIFEXITED(status) : 0;
	childexitcode = childexited ? WEXITSTATUS(status) : -1;

#ifdef DEBUG
	if (opts->debug > 0)
		log_line(LOGPRI_DEBUG,
			 "child process %s, exitcode=%d, delay=%.2fsec, "
			 "timeout=%dsec",
			 childexited ? "exited" : "timed out",
			 childexitcode, difftime(time(NULL), started),
			 opts->timeout);
#endif

	/*
	 * If the filter said to reject the message, we need to read the
	 * last line of the filter's stderr output and store as much of it
	 * as will fit into errbuf.
	 */
	if (childexited && (childexitcode == 1)) {
		FILE *fptr;

		fptr = fopen(errfile, "r");
		if (fptr) {
			while (!feof(fptr)) {
				fgets(errbuf, bufsize, fptr);
			}
			fclose(fptr);
		}
		errbuf[bufsize - 1] = 0;

#ifdef DEBUG
		if (opts->debug > 0)
			log_line(LOGPRI_DEBUG,
				 "child process error line: %.*s", bufsize,
				 errbuf);
#endif
	}

	unlink(errfile);
	free(errfile);

	/*
	 * If the child never exited, kill it immediately.
	 */
	if (childexited == 0 && (pid != child)) {
		kill(child, SIGKILL);
		waitpid(child, NULL, WNOHANG);
	}

	if ((childexited == 0)
	    || (childexitcode > 1)
	    || (childexitcode < 0)
	    )
		return -1;

	if (childexitcode == 1)
		return 1;

	return 0;
}

/* EOF */
