/*
 * Header file for various library functions.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#ifndef _LIBRARY_H
#define _LIBRARY_H 1

struct ppnetfd_s;
typedef struct ppnetfd_s *ppnetfd_t;
struct fdline_s;
typedef struct fdline_s *fdline_t;

ppnetfd_t connection_open(const char *, unsigned short);
void connection_close(ppnetfd_t);
int connection_fdr(ppnetfd_t);
int connection_fdw(ppnetfd_t);

fdline_t fdline_open(int);
void fdline_close(fdline_t);
int fdline_read_available(fdline_t);
int fdline_read_eof(fdline_t);
int fdline_read_fdeof(fdline_t);
int fdline_read(fdline_t, char *, int, int *);

int write_retry(int, char *, int);

#endif /* _LIBRARY_H */

/* EOF */
