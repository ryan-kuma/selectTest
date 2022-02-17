#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#include <unistd.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <assert.h>
#include <errno.h>

#define USER_LIMIT 5
#define BUFFER_SIZE 64
#define FD_LIMIT 65535

typedef struct client_data
{
	sockaddr_in addr;
	char buf[BUFFER_SIZE];
}CLIENT_DATA;

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
	
	CLIENT_DATA *users = new CLIENT_DATA[FD_LIMIT];
	int fds[USER_LIMIT];
	int user_counter = 0;
	for (int i = 0; i < USER_LIMIT; i++)
	{
		fds[i] = -1;
	}
	fd_set read_fds;
	fd_set write_fds;
	fd_set exception_fds;

	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	FD_ZERO(&exception_fds);

	int maxfd = listenfd;

	while(1)
	{
		FD_SET(listenfd, &read_fds);
		FD_SET(listenfd, &exception_fds);
		maxfd = listenfd;
		for (int i = 0; i < user_counter; i++) 
		{
			FD_SET(fds[i], &read_fds);

			if (fds[i] > maxfd)
			{
				maxfd = fds[i];
			}

		}
		ret = select(maxfd+1, &read_fds, NULL, &exception_fds, NULL);
		if(ret < 0)
		{
//				printf("errno is %d\n", errno);
				perror(0);
			printf("select failure\n");
			printf("maxfd is %d\n", maxfd+1);
			break;
		}
		if (FD_ISSET(listenfd, &read_fds))
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

			users[connfd].addr = client_addr;
			setnonblock(connfd);

			fds[user_counter] = connfd;
			user_counter++;
			if (connfd > maxfd)
				maxfd = connfd;
			printf("comes a new user, now have %d users\n", user_counter);

		}
		else if (FD_ISSET(listenfd, &exception_fds))
		{
			printf("get an error from listen\n");
			char errors[100];
			memset(errors, 0, 100);
			socklen_t length = sizeof(errors);
			if(getsockopt(listenfd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0)
			{
				printf("get socket option failed\n");
			}
			continue;
		}
		for(int i = 0; i < user_counter; i++)
		{
			if (FD_ISSET(fds[i], &read_fds))
			{
				int connfd = fds[i];
				memset(users[connfd].buf, 0, BUFFER_SIZE);
				ret = recv(connfd, users[connfd].buf, BUFFER_SIZE-1, 0);
				if (ret <= 0)
				{
					close(connfd);
					users[fds[i]] = users[fds[user_counter-1]];
					fds[i] = fds[user_counter-1];
//					for (int j = 1; j <= user_counter; j++)
//						users[fds[j]].write_buf = NULL;
					i--;
					user_counter--;
					printf("read a client left\n");
				}
				else
				{
					printf("get %d bytes of client data %s from %d\n", ret, users[connfd].buf, connfd);
					for (int j = 0; j < user_counter; j++)
					{
						if(fds[j] == connfd)
						{
							continue;
						}

						ret = send(fds[j], users[connfd].buf, strlen(users[connfd].buf), 0);
					}
				}
			}
		}
	}
	delete[] users;
	close(listenfd);
	return 0; 
}
