/*
 * Open and close network connections.
 *
 * Copyright 2006 Andrew Wood, distributed under the Artistic License.
 */

#include "config.h"
#include "library.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>


struct ppnetfd_s {
	int fd;
};


/*
 * Open a connection to the given server, and return a ppnetfd_t structure
 * describing it, or NULL on error. If an error occurs, the calling function
 * should use strerror(errno) to find out what it was.
 */
ppnetfd_t connection_open(const char *host, unsigned short port)
{
	struct hostent *hostinfo;
	struct sockaddr_in addr;
	char *hostname;
	char *ptr;
	int sock;
	ppnetfd_t connection;

	hostname = malloc(strlen(host) + 1);
	if (hostname == NULL)
		return NULL;
	strcpy(hostname, host);		    /* RATS: ignore (checked) */
	ptr = strchr(hostname, ':');
	if (ptr)
		*ptr = 0;

	hostinfo = gethostbyname(hostname); /* RATS: ignore (unavoidable) */
	free(hostname);
	if (hostinfo == NULL)
		return NULL;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr = *((struct in_addr *) hostinfo->h_addr);

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		return NULL;

	if (connect(sock, (struct sockaddr *) (&addr), sizeof(addr)) < 0) {
		close(sock);
		return NULL;
	}

	connection = calloc(1, sizeof(*connection));
	if (connection == NULL) {
		shutdown(sock, SHUT_RDWR);
		close(sock);
		return NULL;
	}

	fcntl(sock, F_SETFD, FD_CLOEXEC);   /* set close-on-exec */

	connection->fd = sock;

	return connection;
}


/*
 * Close the given connection and free the ppnetfd_t structure.
 */
void connection_close(ppnetfd_t connection)
{
	if (connection == NULL)
		return;
	shutdown(connection->fd, SHUT_RDWR);
	close(connection->fd);
	free(connection);
}


/*
 * Return a file descriptor suitable for reading from.
 */
int connection_fdr(ppnetfd_t connection)
{
	if (connection == NULL)
		return -1;
	return connection->fd;
}


/*
 * Return a file descriptor suitable for writing to.
 */
int connection_fdw(ppnetfd_t connection)
{
	if (connection == NULL)
		return -1;
	return connection->fd;
}

/* EOF */
