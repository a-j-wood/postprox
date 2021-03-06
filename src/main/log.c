/*
 * Logging functions.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#include "config.h"
#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>


/*
 * Open a connection to the system log.
 */
#ifdef DEBUG
void log_open(char *ident, int debug)
#else
void log_open(char *ident)
#endif
{
	char *ptr;
	int option;

	ptr = strrchr(ident, '/');
	if (ptr != NULL) {
		ptr++;
	} else {
		ptr = ident;
	}

	option = LOG_PID;
#ifdef DEBUG
	if (debug > 3)
		option |= LOG_PERROR;
#endif

	openlog(ptr, option, LOG_MAIL);
}


/*
 * Log a message with the given priority.
 */
void log_line(logpri_t priority, char *format, ...)
{
	char buf[2048];			 /* RATS: ignore (checked OK - ish) */
	char *ptr;
	va_list ap;

	va_start(ap, format);

	buf[0] = '\0';

#if HAVE_VSNPRINTF
	(void) vsnprintf(buf, sizeof(buf), format, ap);
#else
	(void) vsprintf(buf, format, ap);   /* RATS: ignore (unavoidable) */
#endif

	va_end(ap);

	buf[sizeof(buf) - 1] = '\0';

	/*
	 * Strip \r and/or \n from the string.
	 */
	ptr = strchr(buf, '\r');
	if (ptr != NULL)
		ptr[0] = '\0';
	ptr = strchr(buf, '\n');
	if (ptr != NULL)
		ptr[0] = '\0';

	/*
	 * Limit total length to no more than 900 bytes.
	 */
	if (strlen(buf) > (size_t) 900)
		strcpy(buf + 896, "...");

	switch (priority) {
	case LOGPRI_DEBUG:
		/*
		 * We use LOG_INFO for debugging instead of LOG_DEBUG, since
		 * LOG_DEBUG is usually thrown away and the user must have
		 * explicitly asked for debugging to be turned on so they
		 * probably want to see debugging info.
		 */
		syslog(LOG_INFO, /* RATS: ignore */ "%s: %.900s",
		       _("debug"), buf);
		break;
	case LOGPRI_INFO:
		syslog(LOG_INFO, "%.900s", buf /* RATS: ignore */ );
		break;
	case LOGPRI_WARNING:
		syslog(LOG_WARNING, "%.900s", buf /* RATS: ignore */ );
		break;
	case LOGPRI_ERROR:
		syslog(LOG_ERR, "%.900s", buf /* RATS: ignore */ );
		break;
	default:
		break;
	}
}


/*
 * Close the connection to the system log.
 */
void log_close(void)
{
	closelog();
}

/* EOF */
