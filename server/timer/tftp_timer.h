#ifndef __TFTP_TIMER_H__
#define __TFTP_TIMER_H__

struct connection;

typedrf struct event_timer{
	time_t start;
	time_t last_len;
}tftp_timer;

int update_timer(int worker_index, struct connection* item);
int add_timer(int worker_index, struct connection* item);
int del_timer(int worker_index, struct connection* item);
struct connection* get_least_timer(int index);

#endif
