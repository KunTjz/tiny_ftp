#ifndef __TFTP_THREAD_H__
#define __TFTP_THREAD_H__

#include <pthread.h>

#define WORKER_MAX 3

typedef struct {
	pthread_t tid;
	
	// connection_queue
	// cq_item* head;
	// cq_item* tail;
	
	int	fd[2]; // the pipe to communicate with master
}worker_thread;

int start_workers();

#endif
