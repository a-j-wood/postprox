/*
 * Global program option structure and the parsing function prototype.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#ifndef _OPTIONS_H
#define _OPTIONS_H 1

struct opts_s;
typedef struct opts_s *opts_t;

typedef enum {
	ACTION_NONE,
	ACTION_PROXY,
	ACTION__MAX
} action_t;

struct opts_s {           /* structure describing run-time options */
	char *program_name;            /* name the program is running as */
	char *serverhost;              /* output SMTP server hostname/IP */
	unsigned short serverport;     /* output SMTP server port */
	char *listenhost;              /* IP to listen on */
	unsigned short listenport;     /* port to listen on, 0 for stdin */
	char *filtercommand;           /* message filtering command */
	char *tempdirectory;           /* temporary directory to use */
	int timeout;                   /* filter timeout */
	unsigned char rejectfailures;  /* set if reject on filter fail */
	unsigned char verbose;         /* set if verbose logging enabled */
#ifdef DEBUG
	unsigned char debug;           /* set if debug logging enabled */
#endif
	int argc;                      /* number of non-option arguments */
	action_t action;               /* what action we are to take */
	char **argv;                   /* array of non-option arguments */
};

extern opts_t opts_parse(int, char **);
extern void opts_free(opts_t);

#endif /* _OPTIONS_H */

/* EOF */
