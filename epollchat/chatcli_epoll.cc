#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include <sys/epoll.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define BUFFER_SIZE 64
#define MAX_EVENT_NUM 1024

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

	int connfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(connfd>=0);
	if(connect(connfd, (struct sockaddr*)&srvaddr, sizeof(srvaddr)) < 0)
	{
		printf("connect error\n");
		close(connfd);
		return 1;
	}
	struct epoll_event events[MAX_EVENT_NUM];
	int epollfd = epoll_create(2);
	assert(epollfd != -1);
	struct epoll_event connEvent;
	connEvent.data.fd = connfd;
	connEvent.events = EPOLLIN | EPOLLRDHUP;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &connEvent);
	setnonblocking(connfd);

	epoll_event readEvent;
	readEvent.data.fd = 0;
	readEvent.events = EPOLLIN;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, 0, &readEvent);
	setnonblocking(0);

	char read_buf[BUFFER_SIZE];
	int pipefd[2];
	int ret = pipe(pipefd);
	assert(ret != -1);

	while(1)
	{
		ret = epoll_wait(epollfd, events, MAX_EVENT_NUM, -1);
		if(ret < 0)
		{
			printf("epoll failure\n");
			break;
		}
		for (int i = 0; i < ret; i++)
		{
			int sockfd = events[i].data.fd;	
			if(sockfd == connfd)
			{
				if (events[i].events & EPOLLRDHUP)
				{
					printf("server close the connection\n");
					break;
				}
				else if (events[i].events & EPOLLIN)
				{
					memset(read_buf, 0, sizeof(read_buf));
					recv(sockfd, read_buf, BUFFER_SIZE-1, 0);
					printf("%s", read_buf);
				}
			}else if (events[i].events & EPOLLIN)
			{
				ret = splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
				ret = splice(pipefd[0], NULL, connfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
			}
		}
	}

	close(connfd);
	close(epollfd);
	return 0;
}

