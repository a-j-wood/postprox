/*
 * Header file for SMTP functions.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#ifndef _SMTP_INTERNAL_H
#define _SMTP_INTERNAL_H 1

#ifndef _TIME_H
#include <time.h>
#endif

struct ppsmtp_s {
	opts_t opts;		/* program options */
	int infdr;		/* input server read descriptor */
	int infdw;		/* input server write descriptor */
	int outfdr;		/* output server read descriptor */
	int outfdw;		/* output server write descriptor */
	fdline_t insrvline;	/* line object, reading from input */
	fdline_t outsrvline;	/* line object, reading from output */
	char linebuf[1024];	/* RATS: ignore (checked) */
	int linelen;		/* length of line last read */
	int startline;		/* set if line starts at start of line */
	int datamode;		/* flag, set if in DATA mode */
	time_t last_smtpout;	/* time last wrote to smtpfdw */
	char *spoolfile;	/* filename we're spooling DATA to */
	int spoolfd;		/* file descriptor for this file */
	char *outspoolfile;	/* filename we're to read DATA from */
	int outspoolfd;		/* file descriptor forr this file */
	int ignoreresponse;	/* count of outserver responses to ignore */
	char *ipaddr;		/* IP address passed via XFORWARD */
	char *helo;		/* HELO passed via XFORWARD */
	char *sender;		/* envelope sender, given in MAIL FROM */
	char *recipient;	/* first recipient, given in RCPT TO */
};

typedef struct ppsmtp_s *ppsmtp_t;

int smtp_write_out(ppsmtp_t, char *, int);
int smtp_write_in(ppsmtp_t, char *, int);

#endif /* _SMTP_INTERNAL_H */

/* EOF */
