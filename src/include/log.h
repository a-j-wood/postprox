/*
 * Functions and definitions for logging.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#ifndef IV_LOG_H
#define IV_LOG_H 1

typedef enum {
	LOGPRI_DEBUG,
	LOGPRI_INFO,
	LOGPRI_WARNING,
	LOGPRI_ERROR
} logpri_t;

void log_line(logpri_t, char *, ...);

#endif /* IV_LOG_H */

/* EOF */
