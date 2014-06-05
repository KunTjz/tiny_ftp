#ifndef __TFTP_CONNECTION_H__
#define __TFTP_CONNECTION_H__

#include "../thread/tftp_thread.h"

// 磁盘缓存大小一般是4K，设置成它可以减少磁盘操作次数
#define LARGE_CHAIN_DEFAULT_SIZE 4 * 1024
typedef struct chain{
	char* data;
	int   fd;
	unsigned int total;
	unsigned int curr;
	unsigned int done;

	struct chain* next;
}large_chain;

typedef struct {
	large_chain* head;
	large_chain* tail;
	unsigned int size;
}chain_queue;

//struct event_timer;

typedef int (*event_handler)(int epoll_fd, int fd, void* item);

enum {
	TFTP_READ_COMMAND_HEADER = 0,
	TFTP_PARSE_COMMAND_HEADER,
	TFTP_READ_COMMAND_DATA,
	TFTP_PARSE_CMD,
	TFTP_SEND_DATA,
	TFTP_UPLOAD_FILE,
	TFTP_DOWNLOAD_FILE
};

#define DEFAULT_CMD_RD_BUF_SIZE 	1024
#define	DEFAULT_DATA_WT_BUF_SIZE	2048	
#define MAX_PATH_LEN				1024

typedef struct connection{
	int 			cmd_fd;
	int 			data_fd;
	worker_thread*  thd;

	event_handler   		handler;
//	struct	event_timer 	timer;	

	char			current_path[MAX_PATH_LEN];	
	int				state;

	char*			cmd_rd_buf;
	unsigned int    cmd_rd_total;
	unsigned int 	cmd_rd_curr;
	unsigned int 	cmd_rd_parsed;

	char*			data_wt_buf;
	unsigned int	data_wt_total;
	unsigned int	data_wt_curr;
	unsigned int	data_wt_writed;	
	
	// buf for upload or download file
	large_chain*	file_buf;
	
	struct connection*		next;
}conn_item;

typedef struct {
	conn_item* head;
	conn_item* tail;
	unsigned int size;
}connection_queue;

#define TFTP_CMD_NEW_CONN	'c'
typedef struct {
	char cmd;
	int	cmd_fd;
	int data_fd;
}tftp_cmd;


conn_item* get_conn_item();
void free_conn_item(conn_item* item);

large_chain* get_large_chain();
void free_large_chain(large_chain* lc);

int add_conn_cache(int fd, conn_item* item);
conn_item* find_conn_item(int fd);
void del_conn_cache(int fd);

#endif
