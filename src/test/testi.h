/*
 * Header file for test functions.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#ifndef _TEST_INTERNAL_H
#define _TEST_INTERNAL_H 1

int test_tmpfd(char *, int, int *);
int test_fdprintf(int, char *, ...);
void test_usleep(int);

#endif /* _TEST_INTERNAL_H */

/* EOF */
