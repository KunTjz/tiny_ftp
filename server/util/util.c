#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <strings.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include <string.h>

#include "../log/tftp_log.h"
#include "sys_define.h"
#include "util.h"

#define LISTENQ 		20

int set_nonblocking(int fd)
{
    int val;

    if((val = fcntl(fd, F_GETFL, 0)) < 0) {
        perror("fcntl(fd, F_GETFL) failed:)");
        return T_FTP_FAIL;
    }
    if(fcntl(fd, F_SETFL, O_NONBLOCK | val) < 0) {
        perror("fcntl(fd, F_SETFL, O_NONBLOCK) failed: ");
        return T_FTP_FAIL;
    }

    return T_FTP_SUCCESS;
}

int tcp_listen(int port)
{
	int option = 1;
	struct sockaddr_in serverAddr;
	int listenfd = socket (AF_INET, SOCK_STREAM, 0);
	if(listenfd < 0) {
		perror("socket failed: ");
		return T_FTP_FAIL;
	}

	if(set_nonblocking(listenfd) == T_FTP_FAIL)
		return T_FTP_FAIL;	

	bzero(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(port);

	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0) {
		perror("set SO_REUSEADDR fail");
		return T_FTP_FAIL;
	}

	if (bind(listenfd, (struct sockaddr*) &serverAddr, sizeof (serverAddr)) < 0) {
		perror("bind failed: ");
		return T_FTP_FAIL;
	}

	if(listen(listenfd, LISTENQ) < 0) {
		perror("listen failed: ");
		return T_FTP_FAIL;
	}

	return listenfd;
}

int read_dir(const char* path, char* buf)
{
	struct dirent* dt;	
	struct stat st;
	char file_path[1024];
	char file_info[256];
	char* p = buf;

	DIR* dir = opendir (path);
	if (dir == NULL) {
		sprintf(buf, "open dir:%s failed:%s\n", path, strerror(errno));
		return T_FTP_FAIL;
	}

	while((dt = readdir (dir)) != NULL) {
		if(!strcmp(dt->d_name, ".") || 
				!strcmp (dt->d_name, "..")) {
			continue;
		}

		snprintf(file_path, 1024, "%s/%s", path, dt->d_name);
	
		T_FTP_LOG(TFTP_LOG_DEBUG, "path:%s\n", file_path);

		if (stat(file_path, &st) != 0) {
			closedir(dir);
			sprintf(buf, "stat file:%s failed:%s\n", file_path, strerror(errno));
			return T_FTP_FAIL;
		}

		if(S_ISDIR(st.st_mode))
			file_info[0] = 'd';
		else if(S_ISREG(st.st_mode))
			file_info[0] = '-';
		else if(S_ISLNK(st.st_mode))
			file_info[0] = 'l';
		else 
			file_info[0] = '?';

		sprintf(file_info + 1, "\t%ld\t%s\n", st.st_size, dt->d_name);
		strncpy(p, file_info, strlen(file_info));
		p += strlen(file_info);
	}	
	
	*p = '\0';
	printf("buf:%s\n", buf);

	closedir(dir);
	return T_FTP_SUCCESS;
}

void up_dir(char* path)
{
	int len = strlen(path);
	char* p = path + len - 1;	

	while(*p != '/') {
		*p-- = '\0';
	}
	*p = '\0';
}

int is_in_dir(char* path, char* to)
{
	struct dirent* dt;	
	struct stat st;
	char file_path[1024];

	DIR* dir = opendir (path);
	if (dir == NULL) {
		return T_FTP_FAIL;
	}

	while((dt = readdir (dir)) != NULL) {
		if(!strcmp(dt->d_name, ".") || 
			!strcmp (dt->d_name, "..")) {
			continue;
		}
	
		if(strcmp(dt->d_name, to) != 0)
			continue;

		snprintf(file_path, 1024, "%s/%s", path, dt->d_name);

		if (stat(file_path, &st) != 0) {
			closedir(dir);
			return T_FTP_FAIL;
		}

		if(S_ISDIR(st.st_mode)) {
			closedir(dir);
			return T_FTP_SUCCESS;
		}
		else {
			closedir(dir);
			return T_FTP_FAIL;
		}
	}	

	closedir(dir);
	return T_FTP_FAIL;
}
