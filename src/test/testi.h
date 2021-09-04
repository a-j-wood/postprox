/*
 * Header file for test functions.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#ifndef IV_TEST_INTERNAL_H
#define IV_TEST_INTERNAL_H 1

extern int test_tmpfd(char *, int, int *);
extern int test_fdprintf(int, char *, ...);
extern void test_usleep(int);

#define FAIL_IF(cond) if (cond) do { ret = __LINE__; goto end; } while (1 == 0)
#define CLOSE_IF_VALID(x) if (x >= 0) (void) close(x)
#define FCLOSE_IF_VALID(x) if (x != NULL) (void) fclose(x)
#define REMOVE_IF_VALID(x) if (x != NULL && x[0] != '\0') (void) remove(x)

#endif /* IV_TEST_INTERNAL_H */

/* EOF */
