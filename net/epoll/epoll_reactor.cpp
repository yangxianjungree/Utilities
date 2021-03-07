#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <arpa/inet.h>

#include <errno.h>
#include <sys/epoll.h>

int main(int argc, char* argv[])
{
	if (argc < 2) {
		return -1;
	}

	int port = atoi(argv[1]);

	int sockFd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockFd < 0) {
		return -1;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sockFd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0) {
		return -2;
	}

	if (listen(sockFd, 5) < 0) {
		return -3;
	}

	// epoll operator.
	int epFd = epoll_create(1);

	struct epoll_event ev, events[512] = {0};
	ev.events = EPOLLIN;
	ev.data.fd = sockFd;

	epoll_ctl(epFd, EPOLL_CTL_ADD, sockFd, &ev);

	// pthread_cond_waittime();

	while (1) {
		int nReady = epoll_wait(epFd, events, 1024, -1);
		if (nReady < -1) {
			break;
		}

		int i = 0;
	}

	return 0;
}