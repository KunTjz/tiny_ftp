#ifndef __TFTP_IO_H__
#define __TFTP_IO_H__

enum {
	READ_ERROR = 0,
	PACKAGE_NOT_COMPLETE,
	READ_CMD_HEADER_SUCCESS,
	READ_CMD_NO_DATA,

	PARSE_CMD_HEADER_ERROR,
	PARSE_CMD_HEADER_SUCCESS,
	
	READ_CMD_DATA_SUCCESS,
};

struct connection;

int read_cmd_header(int fd, struct connection* item);
int parse_cmd_header(int fd, struct connection* item);
int read_cmd_data(int fd, struct connection* item);
int parse_cmd(int fd, struct connection* item);
int send_data(int fd, struct connection* item);
int recv_file(int fd, struct connection* item);

#endif
