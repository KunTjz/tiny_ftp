#include <assert.h>
#include <string.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include<sys/types.h>
#include<sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "../util/sys_define.h"
#include "../util/util.h"
#include "../log/tftp_log.h"
#include "tftp_protocol.h"
#include "tftp_io.h"
#include "../connection/tftp_connection.h"

static void cmd_ls(conn_item* item)
{
	cmd_header ch;
	char* buf = item->data_wt_buf;
	ch.cmd_type = htonl(CMD_REQUEST_SUCCESS);
	
	assert(item);

	if(read_dir(item->current_path, buf + sizeof(cmd_header)) == T_FTP_FAIL){
		ch.cmd_type = htonl(CMD_REQUEST_ERROR);
	}

	ch.magic = htonl(MAGIC_NUM);
	ch.package_len = htonl(strlen(buf + sizeof(cmd_header)) + sizeof(cmd_header));
	
	*(cmd_header*)buf = ch;
	item->data_wt_curr = ntohl(ch.package_len);
	
	//T_FTP_LOG(TFTP_LOG_DEBUG, "cmd_ls: %d, %d,%d,%s\n", ntohl(ch.magic), ch.cmd_type, ntohl(ch.package_len), item->data_wt_buf);
}

static void cmd_cd(conn_item* item)
{
	int len;
	cmd_header* ch;
	cmd_header cmd;
	cmd_header* respond = &cmd;
	char to[256];
	assert(item && item->current_path && item->cmd_rd_buf);

	ch = (cmd_header*)item->cmd_rd_buf;
//	assert(cmd->cmd_type == CMD_CD_DIR);
	
	respond->magic = htonl(MAGIC_NUM);
	snprintf(to, item->cmd_rd_curr - sizeof(cmd_header) + 1,
			"%s", item->cmd_rd_buf + sizeof(cmd_header));

	if(strcmp(to, "..") == 0) {
		if(strcmp(item->current_path, DEFAULT_FILE_PATH) == 0) {
			respond->cmd_type = htonl(CMD_REQUEST_ERROR);
			len = sprintf(item->data_wt_buf + sizeof(cmd_header), "already  reach the root dir\n");
			item->data_wt_curr = len + sizeof(cmd_header);
			respond->package_len = htonl(item->data_wt_curr);
			*(cmd_header*)item->data_wt_buf = cmd;
			return;
		}
	
		up_dir(item->current_path);
		cmd_ls(item);
		return;
	}
	
	if(is_in_dir(item->current_path, to) == T_FTP_FAIL) {
		respond->cmd_type = htonl(CMD_REQUEST_ERROR);
		len = sprintf(item->data_wt_buf + sizeof(cmd_header), "%s is not a dir\n", to);	
		item->data_wt_curr = len + sizeof(cmd_header);
		respond->package_len = htonl(item->data_wt_curr);
		*((cmd_header*)item->data_wt_buf) = cmd;
		return;
	}
	
	sprintf(item->current_path + strlen(item->current_path), "/%s", to);
	cmd_ls(item);
}

static int try_cmd_upload(conn_item* item)
{
	char* data;
	char file_name[256];
	char file_path[1024];
	int file_size;
	int header_len = sizeof(cmd_header);
	unsigned int len;
	cmd_header* ch;
	assert(item && item->cmd_rd_buf);

	ch = (cmd_header*)item->cmd_rd_buf;	
	len = ntohl(ch->package_len) - header_len;
	
	data = item->cmd_rd_buf + header_len;
	if(sscanf(data, "%s %d", file_name, &file_size) != 2) {
		len = sprintf(item->data_wt_buf, "get file info failed\n");
		item->data_wt_curr = len;
		return T_FTP_FAIL;
	}
	file_size = ntohl(file_size);
	
	T_FTP_LOG(TFTP_LOG_DEBUG, "cmd_upload: file:%s, size:%d", file_name, file_size);

	//TODO: check if we can upload
	//check_disk_size();

	item->file_buf = get_large_chain();
	if(item->file_buf == NULL) {
		len = sprintf(item->data_wt_buf, "can't not upload file: not enough memory for file_buf\n");
		item->data_wt_curr = len;
		return T_FTP_FAIL;
	}

	item->file_buf->total = file_size;
	item->file_buf->done = 0;
	item->file_buf->curr = 0;
	
	// create file
	sprintf(file_path, "%s/%s", item->current_path, file_name);
	item->file_buf->fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR);
	if(item->file_buf->fd < 0) {
		free_large_chain(item->file_buf);
		item->file_buf = NULL;		

		len = sprintf(item->data_wt_buf, "create file failed:%s\n", strerror(errno));
		item->data_wt_curr = len;
		return T_FTP_FAIL;
	}

	// send success cmd
	ch->cmd_type = CMD_REQUEST_SUCCESS;
	ch->package_len = htonl(sizeof(cmd_header));
	strncpy(item->data_wt_buf, (char*)ch, sizeof(cmd_header));
	if(write(item->data_fd, item->data_wt_buf, sizeof(cmd_header)) != sizeof(cmd_header)) {
		T_FTP_LOG(TFTP_LOG_ERROR, "request cmd upload file error\n");
		return T_FTP_FAIL;
	}

	return T_FTP_SUCCESS;
}

int read_cmd_header(int fd, conn_item* item)
{
	// 需要使用自定义包头来解决tcp粘包问题	
	int len;
	int result = READ_ERROR;
	int total_to_read = sizeof(cmd_header);
	char* buf = item->cmd_rd_buf;
	item->cmd_rd_curr = 0;	

	if(ioctl(fd, FIONREAD, &len) < 0) {
		T_FTP_LOG(TFTP_LOG_ERROR, "iocatl(%d, FIONREAD) failed:%s", fd, strerror(errno));
		return READ_ERROR;
	}

	if(len > 0 && len < total_to_read) {
		return PACKAGE_NOT_COMPLETE;
	}
	
	while(total_to_read > 0) {
		if((len = read(fd, buf, total_to_read)) < 0) {
			if(errno == EAGAIN || errno == EWOULDBLOCK) {
				T_FTP_LOG(TFTP_LOG_NOTICE, "error:%s while read from fd:%d", strerror(errno), fd);
				break;
			}
			return READ_ERROR;
		}
		
		if(len == 0)
			break;

		total_to_read -= len;
		item->cmd_rd_curr += len;
		result = READ_CMD_HEADER_SUCCESS;
	}

	return result;
}

int parse_cmd_header(int fd, conn_item* item)
{
	cmd_header ch;
	assert(fd > 0 && item && item->cmd_rd_buf);
	
	ch.magic = ntohl(*((uint32_t*)item->cmd_rd_buf));
	if(ch.magic != MAGIC_NUM) {
		T_FTP_LOG(TFTP_LOG_ERROR, "magic num not match\n");
		return PARSE_CMD_HEADER_ERROR;	
	}
	
	ch.cmd_type = ntohl(*((uint32_t*)(item->cmd_rd_buf + sizeof(uint32_t))));
	ch.package_len = ntohl(*((uint32_t*)(item->cmd_rd_buf + sizeof(uint32_t) + sizeof(uint32_t))));
	
	//T_FTP_LOG(TFTP_LOG_DEBUG, "magic:%d, cmd_type:%d, len:%d\n", ch.magic, ch.cmd_type, ch.package_len);
	
	if( ch.package_len < sizeof(cmd_header) ||
		ch.package_len > item->cmd_rd_total - sizeof(cmd_header)) {
		T_FTP_LOG(TFTP_LOG_ERROR, "cmd len:%d overflow\n", ch.package_len);
		return PARSE_CMD_HEADER_ERROR;
	}

	*((cmd_header*)item->cmd_rd_buf) = ch;
	return PARSE_CMD_HEADER_SUCCESS;
}


int read_cmd_data(int fd, conn_item* item)
{
	int len;
	int total_to_read;
	cmd_header ch;
	char* buf;
	assert(fd > 0 && item && item->cmd_rd_buf);

	ch = *((cmd_header*)(item->cmd_rd_buf));
	total_to_read = ch.package_len - sizeof(cmd_header);

	if(total_to_read == 0) {
		return READ_CMD_DATA_SUCCESS;
	}

	if(ioctl(fd, FIONREAD, &len) < 0) {
		T_FTP_LOG(TFTP_LOG_ERROR, "iocatl(%d, FIONREAD) failed:%s", fd, strerror(errno));
		return READ_ERROR;
	}

	if(len < total_to_read) {
		return PACKAGE_NOT_COMPLETE;
	}

	buf = item->cmd_rd_buf + sizeof(cmd_header);
	while(total_to_read > 0) {
		if((len = read(fd, buf, total_to_read)) < 0) {
			if(errno == EAGAIN || errno == EWOULDBLOCK) {
				T_FTP_LOG(TFTP_LOG_NOTICE, "error:%s while read from fd:%d", strerror(errno), fd);
				break;
			}
			return READ_ERROR;
		}
		
		if(len == 0)
			break;

		total_to_read -= len;
		item->cmd_rd_curr += len;
		buf += len;
	}

	return READ_CMD_DATA_SUCCESS;
}

int parse_cmd(int fd, conn_item* item)
{
	cmd_header* ch;
	assert(fd > 0 && item && item->cmd_rd_buf);
		
	ch = (cmd_header*)item->cmd_rd_buf;
	switch(ch->cmd_type) {
		case CMD_LS_DIR:
			cmd_ls(item);
			item->state = TFTP_SEND_DATA;
			break;
		
		case CMD_CD_DIR:
			cmd_cd(item);
			item->state = TFTP_SEND_DATA;
			break;

		case CMD_UPLOAD_FILE:
			if(try_cmd_upload(item) == T_FTP_FAIL) {
				item->state = TFTP_SEND_DATA;
			}
			else {
				item->state = TFTP_UPLOAD_FILE;
			}
			break;

		case CMD_DOWNLOAD_FILE:
			break;

		case CMD_CANCLE_TRANS:
			break;

		default:
			break;
	}

	return T_FTP_SUCCESS;
}

int send_data(int fd, conn_item* item)
{
	int len;
	int total_to_send = item->data_wt_curr;
	char* buf = item->data_wt_buf;

	assert(fd > 0 && item && item->data_wt_buf);

	while(total_to_send > 0) {
		if((len = write(fd, buf, total_to_send)) < 0) {
			if(errno == EAGAIN || errno == EWOULDBLOCK) {
				T_FTP_LOG(TFTP_LOG_NOTICE, "error:%s while write to fd:%d", strerror(errno), fd);
				break;
			}
			return T_FTP_FAIL;
		}

		if(len == 0)
			break;

		total_to_send -= len;
		buf += len;
		// item->data_wt_writed -= len;
	}

	return T_FTP_SUCCESS;
}

int recv_file(int fd, struct connection* item)
{
	// recv_data
	int len;
	int total_to_read = LARGE_CHAIN_DEFAULT_SIZE;
	int total_to_write;
	char* buf = item->file_buf->data;
	item->file_buf->curr = 0;

	assert(fd > 0 && item->file_buf && item->file_buf->fd > 0);

	if(((int)(item->file_buf->total) - (int)item->file_buf->done) < total_to_read) {
		total_to_read = item->file_buf->total - item->file_buf->done;
	}

	while(total_to_read > 0) {
		if((len = read(fd, buf, total_to_read)) < 0) {
			if(errno == EAGAIN || errno == EWOULDBLOCK) {
				T_FTP_LOG(TFTP_LOG_NOTICE, "error:%s while read from fd:%d", strerror(errno), fd);
				break;
			}
			return CMD_TRANS_ERROR;
		}
		
		if(len == 0)
			break;

		total_to_read -= len;
		item->file_buf->curr += len;
		buf += len;
	}
	
	// write file
	total_to_write = item->file_buf->curr;
	buf = item->file_buf->data;
	while(total_to_write > 0) {
		if((len = write(item->file_buf->fd, buf, total_to_write)) < 0) {
			if(errno == EAGAIN || errno == EWOULDBLOCK) {
				T_FTP_LOG(TFTP_LOG_NOTICE, "error:%s while write to fd:%d", strerror(errno), fd);
				break;
			}
			return CMD_TRANS_ERROR;
		}

		if(len == 0)
			break;

		total_to_write -= len;
		buf += len;
//		item->data_wt_writed -= len;
	}

	item->file_buf->done += item->file_buf->curr;
	return (item->file_buf->done >= item->file_buf->total) ? CMD_TRANS_FIN : CMD_TRANS_CONTINUE ;
}
