#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <assert.h>
#include <string.h>

#include "tftp_epoll.h"
#include "tftp_loop.h"
#include "../util/util.h"
#include "../util/sys_define.h"
#include "../log/tftp_log.h"
#include "../thread/tftp_thread.h"
#include "../connection/tftp_connection.h"
#include "../io/tftp_io.h"
#include "../io/tftp_protocol.h"

#define MAX_EVENTS 		1024

int last_thread = -1;

server_state state = {0, 0, 0, 0, 0};
extern worker_thread workers[WORKER_MAX];

pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;

static int send_cmd(tftp_cmd* cmd, const worker_thread* thd)
{
	int total_to_write = sizeof(tftp_cmd);
	int len = 0;
	
	assert(cmd != NULL && thd != NULL && thd->fd[1] > 0);
	
	while(total_to_write > 0) {
		if((len = write(thd->fd[1], cmd, sizeof(tftp_cmd))) < 0) {
			if(errno == EINTR)
				continue;
			T_FTP_LOG(TFTP_LOG_ERROR, "write cmd to fd:%d failed:%s\n", thd->fd[1], strerror(errno));	
			return T_FTP_FAIL;
		}
		
		total_to_write -= len;
	}
	
	T_FTP_LOG(TFTP_LOG_DEBUG, "write cmd to worker:%lu, fd:%d success\n", thd->tid, thd->fd[1]);	
	return T_FTP_SUCCESS;
}

static int on_accept(int cmd_fd, int data_fd)
{
	int cmd_conn, data_conn;
	struct sockaddr_in client_addr;
	socklen_t client_len = 0;
	int tid;	
	tftp_cmd cmd;

	assert(cmd_fd > 0 && data_fd > 0);
	
	while(1) {
		if((cmd_conn = accept(cmd_fd, (struct sockaddr*) &client_addr, &client_len)) < 0) {
			if (errno == EINTR)
				continue;		
			else {
				T_FTP_LOG(TFTP_LOG_ERROR, "%s", strerror(errno));
				return T_FTP_FAIL;
			}
		}
		break;	
	}
	
	while(1) {
		if((data_conn = accept(data_fd, (struct sockaddr*) &client_addr, &client_len)) < 0) {
			if (errno == EINTR)
				continue;		
			else {
				T_FTP_LOG(TFTP_LOG_ERROR, "%s", strerror(errno));
				return T_FTP_FAIL;
			}
		}
		break;	
	}

	// set_non_block
	if(set_nonblocking(cmd_conn) == T_FTP_FAIL
		|| set_nonblocking(data_conn) == T_FTP_FAIL) {
		T_FTP_LOG(TFTP_LOG_ERROR, "set nonblocking failed\n");
		close(cmd_conn);
		close(data_conn);
		return T_FTP_FAIL;
	}
	
	assert(data_conn > 0 && cmd_conn > 0);
	T_FTP_LOG(TFTP_LOG_DEBUG, "new connnect: %d, %d\n", cmd_conn, data_conn);
	
	// send command	
	// round-robin算法，只用一个last_thread就实现了
	// 线程的调度算法
	tid = (last_thread + 1) % WORKER_MAX;
	cmd.cmd = TFTP_CMD_NEW_CONN;
	cmd.cmd_fd = cmd_conn;
	cmd.data_fd = data_conn;
	
	if(send_cmd(&cmd, workers + tid) == T_FTP_FAIL) {
		T_FTP_LOG(TFTP_LOG_WARNING, "send worker thread:%d cmd  of new conn failed\n", tid);
		return T_FTP_FAIL;
	}
		
	last_thread = tid;

	return T_FTP_SUCCESS;
}

static void close_connection(int epoll_fd, conn_item* item)
{
	assert(epoll_fd > 0 && item);

	if(item->file_buf != NULL) {
		free_large_chain(item->file_buf);
		item->file_buf = NULL;
	}
	
	del_conn_cache(item->cmd_fd);
	epoll_del_event(epoll_fd, item->cmd_fd);
	
	del_conn_cache(item->data_fd);
	epoll_del_event(epoll_fd, item->data_fd);

	free_conn_item(item);
	
	pthread_mutex_lock(&state_lock);
	state.live_conn_counts--;
	state.closed_conn_counts++;
	pthread_mutex_unlock(&state_lock);	
}

static int drive_machine(int epoll_fd, int fd, void* it)
{
	int ret;
	int stop = 0;
	conn_item* item = (conn_item*)(it);
	/*if(item == NULL) {
		T_FTP_LOG(TFTP_LOG_WARNING, "get connection by fd:%d failed\n", fd);
		epoll_del_event(epoll_fd, fd);
		return T_FTP_FAIL;
	}*/


	while(stop != 1) {
		
		T_FTP_LOG(TFTP_LOG_DEBUG, "fd:%d,state:%d,cmd_fd:%d, data_fd:%d\n", fd, item->state, item->cmd_fd, item->data_fd);	
		
		switch(item->state) {
			case TFTP_READ_COMMAND_HEADER:
				if(fd != item->cmd_fd) {
					stop = 1;
					break;
				}	
				ret = read_cmd_header(fd, item);
				if(ret == READ_CMD_HEADER_SUCCESS) {
					item->state = TFTP_PARSE_COMMAND_HEADER;
				}
				else if(ret == READ_CMD_NO_DATA
						|| ret == PACKAGE_NOT_COMPLETE) {
					// do nothing
				}
				else { // READ_ERROR 
					// error occur, close the connection
					close_connection(epoll_fd, item);
					stop = 1;
				}

				break;

			case TFTP_PARSE_COMMAND_HEADER:
				ret = parse_cmd_header(fd, item);
				if(ret == PARSE_CMD_HEADER_SUCCESS) {
					item->state = TFTP_READ_COMMAND_DATA;
				}
				else {
					close_connection(epoll_fd, item);
					stop = 1;
				}
				break;

			case TFTP_READ_COMMAND_DATA:
				ret = read_cmd_data(fd, item);
				if(ret == READ_CMD_DATA_SUCCESS) {
					item->state = TFTP_PARSE_CMD;
				}
				else {
					close_connection(epoll_fd, item);
					stop = 1;
				}
				break;

			case TFTP_PARSE_CMD:
				ret = parse_cmd(fd, item);
				if(ret == T_FTP_FAIL) {
					close_connection(epoll_fd, item);
					stop = 1;
				}
				break;

			case TFTP_SEND_DATA:
				ret = send_data(item->data_fd, item);
				if(ret == T_FTP_FAIL) {
					close_connection(epoll_fd, item);
					break;
				}
				item->state = TFTP_READ_COMMAND_HEADER;
				stop = 1;
				break;

			case TFTP_UPLOAD_FILE:
				if(fd != item->data_fd) {
					stop = 1;
					break;
				}
					
				ret = recv_file(item->data_fd, item);
				if(ret == CMD_TRANS_CONTINUE) {
					// todo
				}
				else if(ret == CMD_TRANS_FIN) {
					item->state = TFTP_READ_COMMAND_HEADER;
				}
				else { // ret == CMD_TRANS_ERROR
					close(item->file_buf->fd);
					item->file_buf->fd = -1;
					close_connection(epoll_fd, item);
				}
				stop = 1;
				break;

			case TFTP_DOWNLOAD_FILE:
				break;

			default:
				T_FTP_LOG(TFTP_LOG_ERROR, "fd:%d has unknown state\n", fd);
				stop = 1;
				break;
		}
	}
	
	return T_FTP_SUCCESS;
}

static int add_conn_item(int epoll_fd, 
							int cmd_fd, 
							int data_fd, 
							worker_thread* thd)
{
	conn_item* it = get_conn_item();

	assert(epoll_fd > 0 
			&& cmd_fd > 0 
			&& data_fd > 0 
			&& thd != NULL);
	
	if(it == NULL) {
		T_FTP_LOG(TFTP_LOG_NOTICE, "generate new conn failed\n");
		return T_FTP_FAIL;
	}

	it->cmd_fd = cmd_fd;
	it->data_fd = data_fd;
	it->thd = thd;
	it->state = TFTP_READ_COMMAND_HEADER;
	it->handler = drive_machine;
	strcpy(it->current_path, DEFAULT_FILE_PATH);
	//TODO:  set each kind of buf

	if(epoll_add_event(epoll_fd, EPOLLIN, cmd_fd) == T_FTP_FAIL
			|| epoll_add_event(epoll_fd, EPOLLIN, data_fd) == T_FTP_FAIL) {
		T_FTP_LOG(TFTP_LOG_ERROR, "add cmd_fd or add data_fd failed\n");
		return T_FTP_FAIL;
	}

	//TODO: set call back funcs	

	add_conn_cache(cmd_fd, it);
	add_conn_cache(data_fd, it);

	pthread_mutex_lock(&state_lock);
	state.live_conn_counts++;	
	pthread_mutex_unlock(&state_lock);
	
	return T_FTP_SUCCESS;
}

static void on_new_connection(int epoll_fd, int fd, int index)
{
	assert(epoll_fd > 0 && fd > 0);
	tftp_cmd cmd;;
	int len;	

	while(1) {
		if((len = read(fd, &cmd, sizeof(tftp_cmd))) < 0) {
			if(errno == EINTR)
				continue;
			
			T_FTP_LOG(TFTP_LOG_NOTICE, "worker thread:%d read master cmd failed\n", index);
			return;
		}
		break;
	}

	T_FTP_LOG(TFTP_LOG_DEBUG, "worker thread:%d receive cmd_fd:%d, data_fd:%d\n", 
				index, cmd.cmd_fd, cmd.data_fd);	

	switch(cmd.cmd) {
		case TFTP_CMD_NEW_CONN:
			add_conn_item(epoll_fd, cmd.cmd_fd, cmd.data_fd, &workers[index]);		
			break;
	
		default:
			break;
	}
}

void master_loop()
{
	int cmd_fd = tcp_listen(COMMAND_PORT);
	int data_fd = tcp_listen(DATA_PORT);
	int nfds, epoll_fd, fail_count = 0;	
	struct epoll_event events[2];

	if(cmd_fd == T_FTP_FAIL || data_fd == T_FTP_FAIL) {
		T_FTP_LOG(TFTP_LOG_ERROR, "build listen socket failed\n");
		return;
	}

	epoll_fd = epoll_init();
	if(epoll_fd == T_FTP_FAIL) {
		T_FTP_LOG(TFTP_LOG_ERROR, "init epoll_fd failed\n");
		return;
	}
	
	if(epoll_add_event(epoll_fd, EPOLLIN, cmd_fd) == T_FTP_FAIL
		|| epoll_add_event(epoll_fd, EPOLLIN, data_fd) == T_FTP_FAIL) {
		T_FTP_LOG(TFTP_LOG_ERROR, "add cmd_fd or add data_fd failed\n");
		return;
	}

	while(1) {
		nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, EPOLL_DELAY);
		if (nfds == -1) {
			if(errno == EINTR)
				continue;
			T_FTP_LOG(TFTP_LOG_ERROR, "%s", strerror(errno));
			break;
		}
		
		if(nfds != 2)
			continue;
		
		if((events[0].data.fd == cmd_fd 
			&& events[1].data.fd == data_fd)
			|| (events[0].data.fd == data_fd 
			&& events[1].data.fd == cmd_fd)) {
			
			if(on_accept(cmd_fd, data_fd) == T_FTP_FAIL) {
				if(++fail_count > 5) {
					T_FTP_LOG(TFTP_LOG_NOTICE, "accept new connect failed more than 5 times\n");
					break;
				}
				continue;
			}
			fail_count = 0;
		}
	}
	
	close(cmd_fd);
	close(data_fd);
	close(epoll_fd);
	return;
}

void worker_loop(int index)
{
	int i, nfds;
	int epoll_fd = epoll_init();
	conn_item* item;
	struct epoll_event events[MAX_EVENTS];
	
	if(epoll_fd == T_FTP_FAIL) {
		T_FTP_LOG(TFTP_LOG_ERROR, "init epoll_fd failed\n");
		return;
	}

	if(workers[index].fd[0] < 0 || 
	epoll_add_event(epoll_fd, 
					EPOLLIN, 
					workers[index].fd[0]) == T_FTP_FAIL) {
		T_FTP_LOG(TFTP_LOG_ERROR, "worker thread:%d add fd[0]:%d failed\n", index, workers[index].fd[0]);
		return;
	}
	

	while(1) {
		nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, EPOLL_DELAY);
		if(nfds < 0) {
			if(errno == EINTR)
				continue;
			T_FTP_LOG(TFTP_LOG_ERROR, "%s", strerror(errno));
			break;
		}
		
		for(i = 0; i < nfds; ++i) {
			if(events[i].data.fd == workers[index].fd[0]) {
				on_new_connection(epoll_fd, workers[index].fd[0], index);
			}
			else {
				item = find_conn_item(events[i].data.fd);
				if(item == NULL) {
					T_FTP_LOG(TFTP_LOG_WARNING, "get connection by fd:%d failed\n", events[i].data.fd);
					epoll_del_event(epoll_fd, events[i].data.fd);
					continue;
				}
				item->handler(epoll_fd, events[i].data.fd, (void*)item);
			}
		}
	}
	
	return;
}
