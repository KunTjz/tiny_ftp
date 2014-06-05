#include <assert.h>
#include <map>

#include "tftp_connection.h"
#include "../log/tftp_log.h"
#include "../util/sys_define.h"
#include "../memory/tftp_memory.h"

std::map<int, conn_item*> conn_cache;
connection_queue free_cq = {NULL, NULL, 0};
chain_queue free_chq = {NULL, NULL, 0};

pthread_mutex_t conn_queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t chain_queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t conn_cache_lock = PTHREAD_MUTEX_INITIALIZER;

static large_chain* new_large_chain()
{
	int len;
	large_chain* lc;
	char* p;

	len = sizeof(large_chain) + LARGE_CHAIN_DEFAULT_SIZE;
	p = (char*)malloc_memory(len);
	lc = (large_chain*)p;
	
	if(lc == NULL) {
		T_FTP_LOG(TFTP_LOG_WARNING, "malloc large_chain failed\n");
		return NULL;
	}

	lc->data = p + sizeof(large_chain);
	lc->total = LARGE_CHAIN_DEFAULT_SIZE;
	lc->curr = 0;
	lc->done = 0;
	lc->next = NULL;

	return lc;
}

static conn_item* new_conn_item()
{
	int len;
	conn_item* item;
	char* p;
	
	len = sizeof(conn_item) + 
			DEFAULT_CMD_RD_BUF_SIZE + 
			DEFAULT_DATA_WT_BUF_SIZE;

	p = (char*)malloc_memory(len);
	item = (conn_item*)p;

	if(item == NULL) {
		T_FTP_LOG(TFTP_LOG_WARNING, "malloc conn_item failed\n");
		return NULL;
	}
	
	item->cmd_rd_buf = (char*)(p + sizeof(conn_item));
	item->cmd_rd_total = DEFAULT_CMD_RD_BUF_SIZE;
	item->cmd_rd_curr = 0;
	item->cmd_rd_parsed = 0;

	item->data_wt_buf = (char*)(p + sizeof(conn_item) + item->cmd_rd_total);
	item->data_wt_total = DEFAULT_DATA_WT_BUF_SIZE;
	item->data_wt_curr = 0;
	item->data_wt_writed = 0;

	item->file_buf = NULL;
	
	return item;
}


conn_item* get_conn_item()
{
	conn_item* it = NULL;

	if(free_cq.size == 0) {
		return new_conn_item();
	}

	pthread_mutex_lock(&conn_queue_lock);	

	it = free_cq.head;

	if(free_cq.head == free_cq.tail) {
		free_cq.head = free_cq.head->next;
		free_cq.tail = free_cq.head; 
	}
	else {
		free_cq.head = free_cq.head->next;
	}

	free_cq.size--;

	pthread_mutex_unlock(&conn_queue_lock);	

	return it;
}

void free_conn_item(conn_item* item)
{
	assert(item != NULL);
	
	pthread_mutex_lock(&conn_queue_lock);
	
	if(free_cq.tail == NULL) {
		free_cq.head = item;
		free_cq.tail = item;
	}
	else {
		free_cq.tail->next = item;
	}
	
	free_cq.size++;
	item->next = NULL;
	item->thd = NULL;

	pthread_mutex_unlock(&conn_queue_lock);
}

large_chain* get_large_chain()
{
	large_chain* lc = NULL;

	if(free_chq.size == 0) {
		return new_large_chain();
	}
	
	pthread_mutex_lock(&chain_queue_lock);	

	lc = free_chq.head;

	if(free_chq.head == free_chq.tail) {
		free_chq.head = free_chq.head->next;
		free_chq.tail = free_chq.head; 
	}
	else {
		free_chq.head = free_chq.head->next;
	}

	free_cq.size--;
	
	pthread_mutex_unlock(&chain_queue_lock);	

	return lc;
}

void free_large_chain(large_chain* lc)
{
	assert(lc != NULL);
	
	pthread_mutex_lock(&chain_queue_lock);
	
	if(free_chq.tail == NULL) {
		free_chq.head = lc;
		free_chq.tail = lc;
	}
	else {
		free_chq.tail->next = lc;
	}
	
	free_chq.size++;
	lc->next = NULL;

	//	是否需要在large_chain中保存
	//  它所属的conn_item的信息，然后在
	//  此处将conn_item置为NULL ????
	//  lc->item = NULL;
	pthread_mutex_unlock(&chain_queue_lock);
}

int add_conn_cache(int fd, conn_item* item)
{
	std::pair<std::map<int, conn_item*>::iterator, bool> ret;

	pthread_mutex_lock(&conn_cache_lock);
	ret = conn_cache.insert(std::pair<int, conn_item*>(fd, item));
	
	if(ret.second == false) {
		T_FTP_LOG(TFTP_LOG_WARNING, "fd:%d already in conn_cache, we choose to update the item\n", fd);
		conn_cache[fd] = item;
	}

	pthread_mutex_unlock(&conn_cache_lock);

	return T_FTP_SUCCESS;
}

conn_item* find_conn_item(int fd)
{
	std::map<int, conn_item*>::iterator it;
	it = conn_cache.find(fd);
	if(it == conn_cache.end()) {
		T_FTP_LOG(TFTP_LOG_NOTICE, "fd:%d not in conn_cache\n", fd);
		return NULL;
	}

	return it->second;
}

void del_conn_cache(int fd)
{
	pthread_mutex_lock(&conn_cache_lock);

	if(conn_cache.erase(fd) != (std::size_t)1) {
		T_FTP_LOG(TFTP_LOG_WARNING, "move fd:%d out of conn_cache failed, it may be deleted twice\n", fd);
	}

	pthread_mutex_unlock(&conn_cache_lock);
}
