/*
** server.c -- a stream socket server demo
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

#define PORTS "33235"	// sender
#define PORTR "35654"	// reciever
#define BACKLOG 10		// how many pending connections queue will hold
#define MAXDATASIZE 100 // max number of bytes we can get at once

char buf[MAXDATASIZE];
char *out_message;

int recMessage = 0;

char *addr;

pthread_mutex_t lock;
pthread_cond_t cond;
/*
* struct for receiving from sender client
* socket file descripter = sockfd, new file decripter= new_fd, address = their_addr
*/
typedef struct
{
	int sockfd;
	int new_fd;
	struct sockaddr_storage their_addr;
} myarg_rec;
/*
* struct for receiving from receiver client
* socket file descripter = sockfd, new file decripter= new_fd
*/
typedef struct
{
	int sockfd;
	int new_fd;
} myarg_send;

void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning

	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while (waitpid(-1, NULL, WNOHANG) > 0)
		;

	errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}
/*
 * sends message to client receiver
 * param: void *args
 * 
 * 
 * in the while loop we use mutex locks to wait until a message 
 * has been recieved to send to receiver client
 *
 * once the message has been sent we erase our message 
 * so we can assign it a new message
 *
 * once we are out of this loop, we close our sockets and unlock our locks
 *
 * when we stop waiting for a message, we also unlock our locks 
 *
 * 
*/
void *mySend(void *args)
{
	// sockets
	int sockfd = ((myarg_send *)args)->sockfd;
	int new_fd = ((myarg_send *)args)->new_fd;


	while (1)
	{
		pthread_mutex_lock(&lock);
		printf("SENDER: waiting for message\n");
		while (recMessage == 0)
		{
			pthread_cond_wait(&cond, &lock);
		}
		pthread_mutex_unlock(&lock);
		if (send(new_fd, out_message, MAXDATASIZE, 0) == -1)
		{
			perror("send");
		}
		printf("SENDER: sent packet\n");
		pthread_mutex_lock(&lock);
		recMessage = 0;
		free(out_message);
		pthread_mutex_unlock(&lock);
		
	}
	close(new_fd);
	close(sockfd);

	pthread_mutex_unlock(&lock);

	return 0;
}
/*
* recieves message from client sender
* params: void *args
*

* in the while loop we receive the num of bytes from the message 
* that was sent to us from sender client
*
* when the num of bytes=0, we close the receiving thread
* when a message is received, we allocate the message to put together IP addr, port number and message all together
* 
* we again use locks and set recMessage=1 for our conditionals
*/
void *myRec(void *args)
{
	char s[INET6_ADDRSTRLEN];
	int sockfd = ((myarg_rec *)args)->sockfd;
	int new_fd = ((myarg_rec *)args)->new_fd;
	int numbytes;
	struct sockaddr_storage their_addr = ((myarg_rec *)args)->their_addr;


	while (1)
	{

		printf("RECEIVER: waiting to recvfrom...\n");

		if ((numbytes = recv(new_fd, buf, MAXDATASIZE - 1, 0)) == -1)
		{
			perror("recv");
			exit(1);
		}
		void* retval = 0;
		if (numbytes == 0) {
			printf("Closed receiving thread\n");
			pthread_exit(retval);
		}

		buf[numbytes] = '\0';
		if (strlen(buf) == 0)
		{
			continue;
		}

		printf("RECEIVER: got packet from %s\n",
			   inet_ntop(their_addr.ss_family,
						 get_in_addr((struct sockaddr *)&their_addr),
						 s, sizeof s));

		// printf("listener: packet is %i bytes long\n", numbytes);
		buf[numbytes] = '\0';
		printf("RECEIVER: packet contains \"%s\"\n", buf);

		out_message = malloc(sizeof(buf) + sizeof(s) + sizeof(PORTS) + 4);

		strcpy(out_message, s);
		strcat(out_message, ", ");
		strcat(out_message, PORTS);
		strcat(out_message, ": ");
		strcat(out_message, buf);

		pthread_mutex_lock(&lock);
		recMessage = 1;
		pthread_mutex_unlock(&lock);
		pthread_cond_broadcast(&cond);
	}
	close(new_fd);
	close(sockfd);

	return 0;
}
/*
 * in this function we implement code for connections with recieving the message from sender client
 * 
 * bind and got connection, and create our detached threads
 * 
 * we have created a thread for every connection made with the sender client and new clients
 * 
 */
void *receive_connection_thread()
{
	int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	int yes = 1;
	int rv;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORTS, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
							 p->ai_protocol)) == -1)
		{
			perror("server receiver: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
					   sizeof(int)) == -1)
		{
			perror("setsockopt");
			continue;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			perror("server receiver: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)
	{
		fprintf(stderr, "SERVER RECEIVER: failed to bind\n");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1)
	{
		perror("listen");
		exit(1);
	}

	
	// while loop to create a thread for every connection
	while (1)
	{
		printf("SERVER RECEIVER (accepts connections from clientS (sender)): waiting for connections on port %s...\n", PORTS);
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1)
		{
			perror("accept");
		}

		inet_ntop(their_addr.ss_family,
				  get_in_addr((struct sockaddr *)&their_addr),
				  s, sizeof s);

		printf("SERVER RECEIVER: got connection from %s\n", s);

		// CREATE NEW RECEIVING THREAD
		pthread_attr_t attr;
		if (pthread_attr_init(&attr) != 0)
		{
			printf("error in attr init");
		}
		if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0)
		{
			printf("err in attr set det state");
		}

		myarg_rec args = {sockfd, new_fd, their_addr};
		pthread_t thrRec;
		pthread_create(&thrRec, &attr, myRec, &args);
	}
	return 0;
}
/*
 * in this function we implement code for connections with sending the message to receiving client
 * 
 * bind and got connection, and create our detached threads
 * 
 * we have created a thread for every connection made with the receiver client and new clients
 * 
 */
void *send_connection_thread()
{
	int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	int yes = 1;
	int rv;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORTR, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
							 p->ai_protocol)) == -1)
		{
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
					   sizeof(int)) == -1)
		{
			perror("setsockopt");
			continue;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)
	{
		fprintf(stderr, "SERVER SENDER: failed to bind\n");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1)
	{
		perror("listen");
		exit(1);
	}

	
	// while loop to create a thread for every connection
	while (1)
	{
		printf("SERVER SENDER (accepts connections from clientR (receiver)): waiting for connections on port %s...\n", PORTR);
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1)
		{
			perror("accept");
		}

		inet_ntop(their_addr.ss_family,
				  get_in_addr((struct sockaddr *)&their_addr),
				  s, sizeof s);

		printf("SERVER SENDER: got connection from %s\n", s);

		// CREATE NEW DETACHED SENDER THREAD
		pthread_attr_t attr;
		if (pthread_attr_init(&attr) != 0)
		{
			printf("error in attr init");
		}
		if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0)
		{
			printf("err in attr set det state");
		}

		myarg_rec args = {sockfd, new_fd};
		pthread_t thrRec;
		pthread_create(&thrRec, &attr, mySend, &args);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int continue_running = 1;

	// initializing our locks
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&cond, NULL);

	// creating our two types of threads for receving and sending
	pthread_t receivingThread, sendingThread;
	pthread_create(&receivingThread, NULL, receive_connection_thread, NULL);
	pthread_create(&sendingThread, NULL, send_connection_thread, NULL);

	// joining our two threads
	pthread_join(receivingThread, NULL);
	pthread_join(sendingThread, NULL);

	// while loop to continue to run 
	while (continue_running)
	{
		continue;
	}

	return 0;
}
