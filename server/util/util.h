#ifndef __UTIL_H__
#define __UTIL_H__

typedef struct {
	int live_conn_counts;
	int closed_conn_counts;
	int mem_free_size;
	int mem_use_size;
	int mem_total_size;
}server_state;

int set_nonblocking(int fd);
int tcp_listen(int port);
int read_dir(const char* path, char* buf);
void up_dir(char* path);
int is_in_dir(char* path, char* to);

#endif
