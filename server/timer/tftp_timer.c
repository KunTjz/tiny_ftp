#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <set>
#include <time.h>

#include "../log/tftp_log.h"
#include "tftp_timer.h"
#include "../util/sys_define.h"
#include "../connection/tftp_connection.h"


// 还未投入使用，原理就是利用红黑树保存item，每次epoll_wait前用
// 找出已经超时的连接，close它
// 然后找出最快要超时的一个连接，拿它剩余的超时时间来作为epoll_wait的
// 等待时间
// nignx即只有做的

struct  timer_compare
{
	bool  operator()(const conn_item* s1, const conn_item* s2)  const
	{
		return (s1->timer.start +  s1->timer.last_len) < 
				(s2->timer.start + s2->timer.last_len) ;
	}
};

std::set<conn_item*, timer_compare>  rb_timer[WORKER_MAX];

pthread_mutex_t timer_lock = PTHREAD_MUTEX_INITIALIZER;

int add_timer(int index, struct connection* item)
{
	assert(index >= 0 && index < WORKER_MAX && item != NULL);
	
	rb_timer[index].insert(item);
	
	return T_FTP_SUCCESS;
}

int del_timer(int index, struct connection* item)
{
	assert(index >= 0 && index < WORKER_MAX && item != NULL);
	
	std::set<conn_item*, timer_compare>::iterator it = rb_timer[index].find(item);
	if(it != rb_timer[index].end())
		rb_timer[index].erase(it);
	
	return T_FTP_SUCCESS;
}

int update_timer(int index, struct connection* item)
{
	assert(index >= 0 && index < WORKER_MAX && item != NULL);
	
	del_timer(index, item);
	
	item->timer.start = time(NULL);	
	add_timer(index, item);
}

struct connection* get_least_timer(int index)
{
	std::set<conn_item*, timer_compare>::iterator it;
	conn_item* ret = NULL;

	it = rb_timer[index].begine();
	if(it != rb.timer[index].end()) {
		ret = *it;
		// del it
		rb_timer[index].erase(it);
	}

	return ret;
}
