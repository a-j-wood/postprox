/*
 * Test functions for write_retry().
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#include "config.h"
#include "options.h"
#include "library.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


/*
 * This test function is not much more than a stub; it checks write_retry()
 * does basic sanity checks and can write to /dev/null, and that's about it.
 */
int test_write_retry(opts_t opts)
{
	int fd;
	/*
	 * Basic parameter sanity checking.
	 */
	if (write_retry(-1, "", 0) == 0)
		return 1;
	if (write_retry(1, 0, 0) == 0)
		return 1;

	fd = open("/dev/null", O_WRONLY);
	if (write_retry(fd, "----------", 10)) {
		close(fd);
		return 1;
	}

	close(fd);

	return 0;
}

/* EOF */
