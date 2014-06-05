#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>

#include "cmd_header.h"

void generate_cmd(char* buf, char* data, unsigned int cmd_type)
{
	cmd_header ch;
	ch.magic = htonl(MAGIC_NUM);
	ch.cmd_type = htonl(cmd_type);
	if(data == NULL)
		ch.package_len = htonl(sizeof(cmd_header));
	else {
		ch.package_len = htonl(sizeof(cmd_header) + strlen(data));
		strcpy(buf + sizeof(cmd_header), data);
	}
	
	*(cmd_header*)buf = ch;
}

