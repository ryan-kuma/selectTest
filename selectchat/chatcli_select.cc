#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define BUFFER_SIZE 64

int setnonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

int main(int argc, char* argv[])
{
	if(argc <= 2)
	{
		printf("Usage: %s ip_address port_number\n", argv[0]);
		return 1;
	}
	const char* ip = argv[1];
	int port = atoi(argv[2]);

	struct sockaddr_in srvaddr;
	bzero(&srvaddr, sizeof(srvaddr));
	srvaddr.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &srvaddr.sin_addr);
	srvaddr.sin_port = htons(port);

	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(sockfd>=0);
	if(connect(sockfd, (struct sockaddr*)&srvaddr, sizeof(srvaddr)) < 0)
	{
		printf("connect error\n");
		close(sockfd);
		return 1;
	}
	fd_set read_fds;
	fd_set write_fds;
	fd_set exception_fds;

	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	FD_ZERO(&exception_fds);

	char read_buf[BUFFER_SIZE];

	while(1)
	{
		FD_SET(sockfd, &read_fds);
		FD_SET(sockfd, &exception_fds);
		FD_SET(0, &read_fds);
		FD_SET(0, &exception_fds);

		int ret = select(sockfd+1, &read_fds, NULL, &exception_fds, NULL);
		if(ret < 0)
		{
			printf("select failure\n");
			break;
		}
		if (FD_ISSET(sockfd, &exception_fds))
		{
			printf("server close the connection\n");
			break;
		}
		if (FD_ISSET(sockfd, &read_fds))
		{
			memset(read_buf, 0, sizeof(read_buf));
			recv(sockfd, read_buf, BUFFER_SIZE-1, 0);
			printf("%s\n", read_buf);
		}

		if (FD_ISSET(0, &read_fds))
		{
			char *buf = NULL;
			size_t len;
			int n = 0;
			if ((n = getline(&buf,&len, stdin)) < 0)
			{
				printf("getline error\n");
				free(buf);
			}
			send(sockfd, buf, n+1, 0);
			free(buf);
		}
	}

	close(sockfd);
	return 0;
}

