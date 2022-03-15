#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include "json.hpp"

#define BUFFER_SIZE 1024

using namespace std;

int setnonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

int readn(int fd, void *vptr, size_t n)
{
	size_t nleft = n;
	size_t nread = 0;
	unsigned char *ptr = (unsigned char*)vptr;

	while(nleft > 0)
	{
		nread = read(fd, ptr, nleft);	
		if(nread == -1)
		{
			if (EINTR == errno)	
				nread = 0;
			else
				return -1;
		}
		else if (nread == 0)
			break;

		nleft -= nread;
		ptr += nread;
	}

	return n-nleft;

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
	pollfd fds[2];
	fds[0].fd = 0;
	fds[0].events = POLLIN;
	fds[0].revents = 0;

	fds[1].fd = sockfd;
	fds[1].events = POLLIN | POLLRDHUP;
	fds[1].revents = 0;

	char read_buf[BUFFER_SIZE];
	int pipefd[2];
	int ret = pipe(pipefd);
	assert(ret != -1);

	while(1)
	{
		ret = poll(fds, 2, -1);
		if(ret < 0)
		{
			printf("poll failure\n");
			break;
		}
		if(fds[1].revents & POLLRDHUP)
		{
			printf("server close the connection\n");
			break;
		}
		else if (fds[1].revents & POLLIN)
		{
			int len = 0;
			readn(fds[1].fd, &len, sizeof(len));

			memset(read_buf, 0, sizeof(read_buf));
			readn(fds[1].fd, read_buf, len);
			string str(read_buf);
			nlohmann::json j = nlohmann::json::parse(str);
			int type = j["type"];
			string msg = j["msg"].get<string>();

			printf("len = %d, type=%d\n", len, type);
			printf("%s\n", msg.c_str());
		}

		if(fds[0].revents & POLLIN)
		{
			ret = splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
			ret = splice(pipefd[0], NULL, sockfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
		}
	}

	close(sockfd);
	return 0;
}

