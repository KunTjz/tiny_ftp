#include <errno.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tftp_thread.h"
#include "../util/sys_define.h"
#include "../log/tftp_log.h"
#include "../event/tftp_loop.h"

pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t init_cond = PTHREAD_COND_INITIALIZER;

int init_count = 0;

worker_thread workers[WORKER_MAX];

static int find_worker_index(pthread_t tid)
{
	int i = 0;
	for(; i < WORKER_MAX; ++i) {
		if(pthread_equal(tid, workers[i].tid) != 0)
			return i;
	}
	
	return T_FTP_FAIL;
}

void *thread_function(void *arg)
{
	// pthread_t 与pid_t不同，后者是无符号整数，而前者是一个结构体，所以不能够printf打印
	// 比较的话用pthread_equal来比较
	pthread_t tid = pthread_self();
	int index;
	
	pthread_mutex_lock(&init_lock);
	while(init_count < WORKER_MAX)
		pthread_cond_wait(&init_cond, &init_lock);
	pthread_mutex_unlock(&init_lock);

	index = find_worker_index(tid);
	if(index == T_FTP_FAIL) {
		T_FTP_LOG(TFTP_LOG_ERROR, "get worker index failed, thread exit\n");
		return NULL;
	}

	T_FTP_LOG(TFTP_LOG_DEBUG, "worker thread:%lu, %d start, init_count:%d\n", tid, index, init_count);
	worker_loop(index);
	sleep(1000);

	return NULL;
}

int start_workers()
{
	int i = 0;
	pthread_t tid;
	int fd[2];

	pthread_mutex_lock(&init_lock);

	for(; i < WORKER_MAX; ++i) {
		if(pthread_create(&tid, NULL, thread_function, NULL) < 0) {
			T_FTP_LOG(TFTP_LOG_ERROR, "phtread_create:%s\n", strerror(errno));
			return T_FTP_FAIL;
		}
		
		while(1) {
			if(pipe(fd) < 0) {
				if(errno == EINTR) 
					continue;
				T_FTP_LOG(TFTP_LOG_ERROR, "create pipe failed\n");
				return T_FTP_FAIL;
			}
			break;
		}
		
		workers[i].fd[0] = fd[0];
		workers[i].fd[1] = fd[1];
		workers[i].tid = tid;

		init_count++;
	}

	pthread_cond_broadcast(&init_cond);	
	pthread_mutex_unlock(&init_lock);	

	return T_FTP_SUCCESS;
}
