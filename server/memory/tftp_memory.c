#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

#include "tftp_memory.h"
#include "../log/tftp_log.h"
#include "../util/sys_define.h"
#include "../util/util.h"

#define MEMORY_POOL_SIZE 100 * 1024 * 1024  // 100M

pthread_mutex_t memory_lock = PTHREAD_MUTEX_INITIALIZER;

extern server_state state;

void* memory_pool = NULL;

int init_memory_pool()
{
	assert(memory_pool == NULL);
	
	pthread_mutex_lock(&memory_lock);
	
	memory_pool = malloc(MEMORY_POOL_SIZE);
	if(memory_pool == NULL) {
		return T_FTP_FAIL;
	}
	
	state.mem_use_size = 0;
	state.mem_free_size = MEMORY_POOL_SIZE - state.mem_use_size;	

	pthread_mutex_unlock(&memory_lock);

	return T_FTP_SUCCESS;
}

void* malloc_memory(unsigned int size)
{
	void* p = NULL;

	assert(size >= 0 && size < MEMORY_POOL_SIZE);		
	
	pthread_mutex_lock(&memory_lock);

	if(size > (unsigned int)state.mem_free_size) {
		T_FTP_LOG(TFTP_LOG_NOTICE, "not enough memory, current state.mem_free_size:%d, request size:%d", state.mem_free_size, size);
		return NULL;
	}
	
	state.mem_use_size += size;
	state.mem_free_size -= size;
	p =  (char*)(memory_pool) + state.mem_use_size - size; 

	pthread_mutex_unlock(&memory_lock);
	
	return p;
}
