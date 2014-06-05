#ifndef __HTTP_EPOLL_H__
#define __HTTP_EPOLL_H__

#define EPOLL_DELAY  	500

int epoll_init();
int epoll_add_event(int epoll_fd, uint32_t mod, int fd);
int epoll_del_event(int epoll_fd, int fd);
int epoll_modify_mod(int epoll_fd, uint32_t mod, int fd);
//int epoll_start(int epoll_fd, unsigned int max_events, int listen_sock);

#endif
