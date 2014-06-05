#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include "tftp_epoll.h"
#include "../util/sys_define.h"
#include "../log/tftp_log.h"

#define MAX_CONNECTION 	1000
#define MAX_LINE 		4096
#define MAX_EVENTS		1000

int epoll_init()
{
	int epoll_fd = epoll_create(MAX_EVENTS);
	if(epoll_fd < 0) {
		T_FTP_LOG(TFTP_LOG_ERROR, "epoll_create:%s", strerror(errno));
		return T_FTP_FAIL;
	}
	
	return epoll_fd;
}

int epoll_add_event(int epoll_fd, uint32_t op, int fd)
{
	struct epoll_event ev;

	ev.data.fd = fd;
	ev.events = op;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		T_FTP_LOG(TFTP_LOG_ERROR, "epoll_ctl:%s", strerror(errno));
		return T_FTP_FAIL;
	}

	return T_FTP_SUCCESS;
}

int epoll_modify_mod(int epoll_fd, uint32_t mod, int fd)
{
	struct epoll_event ev;
	
	ev.data.fd = fd;
	ev.events = mod;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
		T_FTP_LOG(TFTP_LOG_ERROR, "epoll_ctl:%s", strerror(errno));
		return T_FTP_FAIL;
	}

	return T_FTP_SUCCESS;
}

int epoll_del_event(int epoll_fd, int fd)
{
	struct epoll_event ev;

	ev.data.fd = fd;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &ev) < 0) {
		T_FTP_LOG(TFTP_LOG_ERROR, "epoll_ctl:%s", strerror(errno));
		return T_FTP_FAIL;
	}
	
	return T_FTP_SUCCESS;
}

/*int epoll_start(int epoll_fd, unsigned int max_events, int listen_sock)
{
	int nfds;
	struct epoll_event events[max_events];

	nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, EPOLL_DELAY);
	if (nfds == -1) {
		perror("epoll_wait");
		return T_FTP_FAIL;
	}

	for(int n = 0; n < nfds; ++n) {
		if (events[n].data.fd == listen_sock) {
			if(on_accept(epoll_fd, listen_sock) == T_FTP_FAIL)
				continue;
		}
		else if(events[n].events & EPOLLIN)//如果是已经连接的用户，并且收到数据，那么进行读入。
		{
			if(on_read(epoll_fd, events[n].data.fd) == T_FTP_FAIL) {
				on_close(epoll_fd, events[n].data.fd);
				continue;
			}

		}
		else if(events[n].events & EPOLLOUT) // 如果有数据发送
		{
			on_write(epoll_fd, events[n].data.fd);
			on_close(epoll_fd, events[n].data.fd);
		}
	}

	return T_FTP_SUCCESS;
}*/
