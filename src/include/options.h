/*
 * Global program option structure and the parsing function prototype.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#ifndef IV_OPTIONS_H
#define IV_OPTIONS_H 1

struct opts_s;
typedef struct opts_s *opts_t;

typedef enum {
	ACTION_NONE,
	ACTION_PROXY,
	ACTION__MAX
} action_t;

struct opts_s {           /* structure describing run-time options */
	char *program_name;            /* name the program is running as */
	char /*@null@*/ *serverhost;   /* output SMTP server hostname/IP */
	unsigned short serverport;     /* output SMTP server port */
	char *listenhost;              /* IP to listen on */
	unsigned short listenport;     /* port to listen on, 0 for stdin */
	char *filtercommand;           /* message filtering command */
	char *tempdirectory;           /* temporary directory to use */
	int timeout;                   /* filter timeout */
	int rejectfailures;            /* set if reject on filter fail */
	int verbose;                   /* set if verbose logging enabled */
#ifdef DEBUG
	int debug;                     /* set if debug logging enabled */
#endif
	int argc;                      /* number of non-option arguments */
	action_t action;               /* what action we are to take */
	char **argv;                   /* array of non-option arguments */
};

extern /*@only@*/ /*@null@*/ opts_t opts_parse(int, char **);
extern void opts_free(/*@only@*/ /*@out@*/ /*@null@*/ opts_t);

#endif /* IV_OPTIONS_H */

/* EOF */
