/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#include <arpa/inet.h>

#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXDATASIZE 100 // max number of bytes we can get at once 




// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd;  // listen on sock_fd, new connection on new_fd
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];


	if (argc != 3)
	{
		fprintf(stderr, "usage: clientS hostname port\n");
		exit(1);
	}
	char * PORTS = argv[2];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;


	if ((rv = getaddrinfo(argv[1], PORTS, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
	}

	// loop through all the results and connect to the first
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "client: failed to connect\n");
		exit(1);
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	// loop to prompt user to enter message 
	// and send it to the server
	// adds message to buf array
	while (1)
	{

		char *message = NULL;
		size_t message_len = MAXDATASIZE;

		printf("client: got connection from %s\n", s);
		printf("Enter message: "); 
		getline(&message, &message_len, stdin); 
		message[strcspn(message, "\n")] = 0;
		if (send(sockfd, message, MAXDATASIZE, 0) == -1) {
			perror("send");
		}

		memset(buf, 0, MAXDATASIZE); // clear buf
		
	}
	close(sockfd); // close socket
	
	return 0;
}