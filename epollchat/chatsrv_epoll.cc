#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <map>

#include <sys/epoll.h>
#include <assert.h>
#include <errno.h>

#define USER_LIMIT 5
#define BUFFER_SIZE 64
#define MAX_EVENT_NUM 1024

using namespace std;

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

int addepollfd(int epollfd, int fd, uint32_t eventflag)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = eventflag;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblock(fd);
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
	
	map<int, char*> fdreadMap;
	map<int, char*> fdwriteMap;
	epoll_event events[MAX_EVENT_NUM];
	int epollfd = epoll_create(USER_LIMIT+1);
	addepollfd(epollfd, listenfd, EPOLLIN|EPOLLERR);
	int user_counter = 0;

	while(1)
	{
		ret = epoll_wait(epollfd, events, MAX_EVENT_NUM, -1);
		if(ret < 0)
		{
			printf("epoll failure\n");
			break;
		}

		for(int i = 0; i < ret; i++)
		{
			int sockfd = events[i].data.fd;
			if ((sockfd == listenfd) && (events[i].events & EPOLLIN))
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

				setnonblock(connfd);

				uint32_t flag = EPOLLIN|EPOLLRDHUP|EPOLLERR;
				addepollfd(epollfd, connfd, flag);
				char *buf = (char *)malloc(sizeof(char)*BUFFER_SIZE);
				fdreadMap[connfd] = buf; 
				fdwriteMap[connfd] = NULL;
				user_counter++;

				printf("comes a new user, now have %d users\n", user_counter);
			}
			else if(events[i].events & EPOLLERR)
			{
				printf("get an error from %d\n", sockfd);
				char errors[100];
				memset(errors, 0, 100);
				socklen_t length = sizeof(errors);
				if(getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0)
				{
					printf("get socket option failed\n");
				}
				continue;
			}
			else if(events[i].events & EPOLLRDHUP)
			{
				struct epoll_event tmp_event;
				tmp_event.events = events[i].events;
				tmp_event.events = sockfd;
				epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, &tmp_event);
				close(sockfd);
				free(fdreadMap[sockfd]);
				fdreadMap.erase(sockfd);
				fdwriteMap.erase(sockfd);
				user_counter--;
				printf("a client left\n");
			}
			else if(events[i].events & EPOLLIN)
			{
				memset(fdreadMap[sockfd], 0, BUFFER_SIZE);
				ret = recv(sockfd, fdreadMap[sockfd], BUFFER_SIZE-1, 0);
				printf("get %d bytes of client data %s from %d\n", ret, fdreadMap[sockfd], sockfd);
				if (ret < 0)
				{
					if(errno != EAGAIN)
					{
						struct epoll_event tmp_event;
						tmp_event.events = events[i].events;
						tmp_event.events = sockfd;
						epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, &tmp_event);
						close(sockfd);
						free(fdreadMap[sockfd]);
						fdreadMap.erase(sockfd);
						fdwriteMap.erase(sockfd);
						user_counter--;
						printf("a client left\n");
					}
				}
				else if (ret == 0)
				{
				}
				else
				{
					for (map<int, char*>::iterator iter = fdreadMap.begin(); iter != fdreadMap.end(); iter++)
					{
						if(iter->first == sockfd)
						{
							continue;
						}

						epoll_event tmp_event;
						tmp_event.data.fd = iter->first;
						tmp_event.events = EPOLLOUT | EPOLLRDHUP | EPOLLERR;
						epoll_ctl(epollfd, EPOLL_CTL_MOD, iter->first, &tmp_event);
						fdwriteMap[iter->first] = fdreadMap[sockfd];
					}
				}
			}
			else if(events[i].events & EPOLLOUT)
			{
				if(!fdwriteMap[sockfd])
				{
					continue;
				}
				ret = send(sockfd, fdwriteMap[sockfd], strlen(fdwriteMap[sockfd]), 0);
				assert(ret != -1);
				fdwriteMap[sockfd] = NULL;

				epoll_event tmp_event;
				tmp_event.data.fd = sockfd;
				tmp_event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
				epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &tmp_event);
			}
		}
	}

	close(listenfd);
	return 0; 
}
