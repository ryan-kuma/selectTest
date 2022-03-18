#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#include <unistd.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "json.hpp"

#include <assert.h>
#include <errno.h>

#include <iostream>

#define USER_LIMIT 2
#define BUFFER_SIZE 1024
#define FD_LIMIT 65535

using namespace std;

static char buf[USER_LIMIT][BUFFER_SIZE];

int setnonblock(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

int setreuseaddr(int fd)
{
	int on = 1;
	int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	return ret;
}

int main(int argc, char* argv[])
{
	if(argc <= 2)
	{
		printf("usage: %s ip_address port_number\n", basename(argv[0]));
		return 1;
	}

	const char *ip = argv[1];
	int port = atoi(argv[2]);
	
	int ret = 0;
	struct sockaddr_in address;
	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr);
	address.sin_port = htons(port);

	int listenfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(listenfd >= 0);
	ret = setreuseaddr(listenfd);
	assert(ret != -1);


	ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
	assert(ret != -1);

	ret = listen(listenfd, 5);
	assert(ret != -1);
	
	pollfd fds[USER_LIMIT+1];
	int user_counter = 0;
	for (int i = 1; i <= USER_LIMIT; i++)
	{
		fds[i].fd = -1;
		fds[i].events = 0;
	}
	fds[0].fd = listenfd;
	fds[0].events = POLLIN|POLLERR;
	fds[0].revents = 0;

	while(1)
	{
		ret = poll(fds, user_counter+1, -1);
		if(ret < 0)
		{
			printf("poll failure\n");
			break;
		}

		for(int i = 0; i < user_counter + 1; i++)
		{
			if ((fds[i].fd == listenfd) && (fds[i].revents & POLLIN))
			{
				struct sockaddr_in client_addr;
				socklen_t addrlen = sizeof(client_addr);
				int connfd = accept(listenfd, (struct sockaddr*)&client_addr, &addrlen);
				if(connfd < 0)
				{
					printf("errno is %d\n", errno);
					continue;
				}
				if(user_counter >= USER_LIMIT)
				{
					const char* info = "too many users\n";
					printf("%s", info);
					send(connfd, info, strlen(info), 0);
					close(connfd);
					continue;
				}

				user_counter++;
				setnonblock(connfd);

				fds[user_counter].fd = connfd;
				fds[user_counter].events = POLLIN|POLLRDHUP|POLLERR;
				fds[user_counter].revents = 0;

				printf("comes a new user, now have %d users\n", user_counter);
			}
			else if(fds[i].revents & POLLERR)
			{
				printf("get an error from %d\n", fds[i].fd);
				char errors[100];
				memset(errors, 0, 100);
				socklen_t length = sizeof(errors);
				if(getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0)
				{
					printf("get socket option failed\n");
				}
				continue;
			}
			else if(fds[i].revents & POLLRDHUP)
			{
				close(fds[i].fd);
				fds[i] = fds[user_counter];
				i--;
				user_counter--;
				printf("a client left\n");

			}
			else if(fds[i].revents & POLLIN)
			{
				int connfd = fds[i].fd;
				char recvBuf[BUFFER_SIZE] = {0};
				ret = recv(connfd, recvBuf, BUFFER_SIZE-1, 0);
				printf("get %d bytes of client data %s from %d\n", ret, recvBuf, connfd);

				if (ret < 0)
				{
					if(errno != EAGAIN)
					{
						close(connfd);
						fds[i] = fds[user_counter];
						i--;
						user_counter--;
					}
				}
				else if (ret == 0)
				{
				}
				else
				{
					nlohmann::json jsdic;
					string message(recvBuf);
					jsdic["type"] = 1;
					jsdic["msg"] = message;
					string msg = jsdic.dump();

					memset(buf[i], 0, BUFFER_SIZE);
					snprintf(buf[i], msg.size()+1,"%s",msg.c_str());

					fds[i].events |= ~POLLIN;
					fds[i].events |= POLLOUT;
				}
			}
			else if(fds[i].revents & POLLOUT)
			{
				int connfd = fds[i].fd;

				int len = strlen(buf[i]);
				ret = send(connfd, &len, 4, 0);
				ret = send(connfd, buf[i], len, 0);
//				fds[i].events |= ~POLLOUT;
				fds[i].events = POLLIN|POLLRDHUP|POLLERR;
			}
		}
	}

	close(listenfd);
	return 0; 
}
