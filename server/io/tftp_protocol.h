#ifndef __TFTP_PROTOCOL_H__
#define __TFTP_PROTOCOL_H__

#define MAGIC_NUM 9527

enum {
	CMD_LS_DIR = 0,
	CMD_CD_DIR,
	CMD_REQUEST_ERROR,
	CMD_REQUEST_SUCCESS,
	
	CMD_UPLOAD_FILE,
	CMD_DOWNLOAD_FILE,
	CMD_CANCLE_TRANS,
	
	CMD_TRANS_FIN,
	CMD_TRANS_CONTINUE,
	CMD_TRANS_ERROR
};

typedef struct {
	unsigned int magic;
	unsigned int cmd_type;
	unsigned int package_len;
}cmd_header;

#endif
