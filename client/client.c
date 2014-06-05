#include <sys/socket.h> 
#include <sys/time.h>
#include <sys/signal.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#include "cmd_header.h"

#define dataLen 1024 //缓冲区大小
char user_cmd[10]; //客户端出发的命令
char cmd_arg[20]; //客户端输入的文件或目录名
char buf[dataLen]; //缓冲区

void cmd_pwd(int sock,int sockmsg); //处理pwd命令
void cmd_dir(int sock,int sockmsg); //处理dir命令
void cmd_cd(int sock,int sockmsg,char *dirName); //处理cd命令
void cmd_help(int sock,int sockmsg); //处理?命令
void cmd_get(int sock,int sockmsg,char *fileName); //处理get命令
void cmd_put(int sock,int sockmsg,char *fileName); //处理put命令
void cmd_quit(int sock,int sockmsg); //处理put命令


int main(int argc,char *argv[])
{
	int cmd_len,arg_len;
	int sock, sockmsg;
	struct sockaddr_in server, servermsg;
	struct hostent *hp;
	sock = socket(AF_INET,SOCK_STREAM,0);
	sockmsg = socket(AF_INET,SOCK_STREAM,0);
	if (sock < 0 || sockmsg < 0)
	{
		perror("opening stream socket");
		exit(1);
	}

	if(argc != 3)
	{
		printf("Usage:./client <address> <port>\n");
		return 0;
	}
	server.sin_family=AF_INET;
	server.sin_port=htons(atoi(argv[2]));
	inet_pton(AF_INET,argv[1],&server.sin_addr);
	servermsg.sin_family=AF_INET;
	servermsg.sin_port=htons(atoi(argv[2]) + 1);
	inet_pton(AF_INET,argv[1],&servermsg.sin_addr);
	if (connect(sock,(struct sockaddr *)&server,sizeof server)<0||connect(sockmsg,(struct sockaddr *)&servermsg,sizeof servermsg)<0)
	{
		perror("connecting stream socket");
		exit(1);
	}
	char help[300];
	//read(sock,help,300);
	//printf("%s\n",help);
	while(1)
	{
		memset(user_cmd,0,10);
		memset(cmd_arg,0,20);
		printf("command: ");
		scanf("%s",user_cmd);
		cmd_len = strlen(user_cmd);
		if(strcmp(user_cmd,"quit") == 0)   // quit命令
		{
			cmd_quit(sock,sockmsg);
			close(sockmsg);
			close(sock);
			printf("connection closed\n\n");
			exit(0);
		}
		else if(strcmp(user_cmd,"?")==0) //?命令
			cmd_help(sock,sockmsg);
		else if(strcmp(user_cmd,"pwd")==0) //pwd命令
			cmd_pwd(sock,sockmsg);
		else if(strcmp(user_cmd,"ls")==0) //dir命令
			cmd_dir(sock,sockmsg);
		else if(strcmp(user_cmd,"cd")==0)   // cd命令
		{
			scanf("%s",cmd_arg);
			cmd_cd(sock,sockmsg,cmd_arg);
		}
		else if(strcmp(user_cmd,"get")==0)   //get命令
		{
			scanf("%s",cmd_arg);
			cmd_get(sock,sockmsg,cmd_arg);
		}
		else if(strcmp(user_cmd,"put")==0)   // put命令
		{
			scanf("%s",cmd_arg);
			cmd_put(sock,sockmsg,cmd_arg);
		}
		else
			printf("bad command!\n");
	}
}

void cmd_pwd(int sock,int sockmsg)
{
	char dirName[30];
	write(sockmsg,user_cmd,sizeof(user_cmd));
	read(sock,dirName,30);
	printf("%s\n",dirName);
}

void cmd_dir(int sock, int sockmsg)
{

	char request[1024];
	char respond[4096];
	cmd_header* ch;
	int len;

	generate_cmd(request, NULL, CMD_LS_DIR);

	
	ch = (cmd_header*)request;

	write(sock, request, ntohl(ch->package_len));
	read(sockmsg, respond, sizeof(cmd_header));
	
	len = read(sockmsg, respond + sizeof(cmd_header), 4096 - sizeof(cmd_header));
	respond[len + sizeof(cmd_header)] = '\0';	

	ch = (cmd_header*)respond;
	if(ntohl(ch->cmd_type) == CMD_REQUEST_ERROR)
		printf("request error:");
	printf("%s\n", respond+sizeof(cmd_header));
	
	/*int i, fileNum=0;
	char fileInfo[50];
	write(sockmsg,user_cmd,sizeof(user_cmd));
	read(sock,&fileNum,sizeof(int));
	printf("--------------------------------------------------------\n");
	printf("file number : %d\n",fileNum);
	if(fileNum > 0)
	{
		for(i=0; i<fileNum; i++)
		{
			memset(fileInfo,0,sizeof(fileInfo));
			read(sock,fileInfo,sizeof(fileInfo));
			printf("%s\n",fileInfo);
		}
		printf("--------------------------------------------------------\n");
	}
	else if(fileNum == 0)
	{
		printf("directory of server point is empty.\n");
		return;
	}
	else
	{
		printf("error in command 'dir'\n");
		return;
	}*/
}
void cmd_cd(int sock,int sockmsg,char *dirName)
{
	int len = 0;
	char request[1024];
	char respond[4096];
	cmd_header* ch;

	generate_cmd(request, dirName, CMD_CD_DIR);

	
	ch = (cmd_header*)request;

	write(sock, request, ntohl(ch->package_len));
	read(sockmsg, respond, sizeof(cmd_header));
	

	ch = (cmd_header*)respond;
	
	if(ntohl(ch->package_len) > sizeof(cmd_header)) {
		len = read(sockmsg, respond + sizeof(cmd_header), 4096 - sizeof(cmd_header));
	}
	else
		printf("empty dir\n");
	respond[len+sizeof(cmd_header)] = '\0';
	
	if(ntohl(ch->cmd_type) == CMD_REQUEST_ERROR)
		printf("request error:");
	printf("%s\n", respond+sizeof(cmd_header));
	

	/*char currentDirPath[200];
	write(sockmsg,user_cmd,sizeof(user_cmd));
	write(sockmsg,cmd_arg,sizeof(cmd_arg));
	read(sock,currentDirPath,sizeof(currentDirPath));
	printf("now in directory : %s\n",currentDirPath);*/
}
void cmd_help(int sock, int sockmsg)
{
	char help[300];
	write(sockmsg,user_cmd,sizeof(user_cmd));
	read(sock,help,300);
	printf("%s\n",help);
}

void cmd_get(int sock,int sockmsg,char *fileName)
{
	int fd;
	long fileSize;
	char localFilePath[200];
	write(sockmsg,user_cmd,sizeof(user_cmd));
	write(sockmsg,cmd_arg,sizeof(cmd_arg));
	printf("%s\n%s\n",user_cmd,cmd_arg);
	memset(localFilePath,0,sizeof(localFilePath));
	getcwd(localFilePath,sizeof(localFilePath));
	strcat(localFilePath,"/");
	strcat(localFilePath,fileName);
	fd = open(localFilePath,O_RDWR|O_CREAT, S_IREAD|S_IWRITE);
	if(fd!=-1)
	{
		memset(buf,0,dataLen);
		read(sock,&fileSize,sizeof(long));
		while(fileSize>dataLen)
		{
			read(sock,buf,dataLen);
			write(fd,buf,dataLen);
			fileSize=fileSize-dataLen;
		}
		read(sock,buf,fileSize);
		write(fd,buf,fileSize);
		close(fd);
		printf("download completed\n");
	}
	else
		printf("open file %s failed\n",localFilePath);
}

void cmd_put(int sock,int sockmsg,char* fileName)
{
	int fd;
	int len = 0;
	char request[1024];
	char respond[4096];
	cmd_header* ch;
	char* file;
	
	char filePath[200];
	struct stat fileSta;
	long fileSize;
	memset(filePath, 0, sizeof(filePath));
	getcwd(filePath, sizeof(filePath));
	strcat(filePath, "/");
	strcat(filePath, fileName);
	fd=open(filePath,O_RDONLY, S_IREAD);

	if(fd < 0) {
		printf("open fail\n");
		return;
	}

	fstat(fd,&fileSta);
	fileSize=(long) fileSta.st_size;
	sprintf(filePath, "%s %d", fileName, htonl(fileSize));	

	generate_cmd(request, filePath, CMD_UPLOAD_FILE);

	ch = (cmd_header*)request;

	write(sock, request, ntohl(ch->package_len));

	read(sockmsg, respond, sizeof(cmd_header));

	ch = (cmd_header*)respond;

	if(ntohl(ch->package_len) > sizeof(cmd_header)) {
		len = read(sockmsg, respond + sizeof(cmd_header), 4096 - sizeof(cmd_header));
	}
	respond[len+sizeof(cmd_header)] = '\0';

	if(ntohl(ch->cmd_type) == CMD_REQUEST_ERROR) {
		printf("request error:");
		printf("%s\n", respond+sizeof(cmd_header));
		return;
	}

	file = (char*)malloc(4 * 4096);
	while(fileSize > 0) {
		len = read(fd, file, 4 * 4096);
		write(sockmsg, file, len);
		fileSize -= len;
	}
	free(file);
	/*write(sockmsg,user_cmd,sizeof(user_cmd));
	write(sockmsg,cmd_arg,sizeof(cmd_arg));
	int fd;
	long fileSize;
	int numRead;
	char filePath[200];
	struct stat fileSta;
	memset(filePath,0,sizeof(filePath));
	getcwd(filePath,sizeof(filePath));
	strcat(filePath,"/");
	strcat(filePath,fileName);
	fd=open(filePath,O_RDONLY, S_IREAD);
	if(fd!=-1)
	{
		fstat(fd,&fileSta);
		fileSize=(long) fileSta.st_size;
		write(sock,&fileSize,sizeof(long));
		memset(buf,0,dataLen);
		while(fileSize>dataLen)
		{
			read(fd,buf,dataLen);
			write(sock,buf,dataLen);
			fileSize=fileSize-dataLen;
		}
		read(fd,buf,fileSize);
		write(sock,buf,fileSize);
		close(fd);
		printf("upload completed\n");
	}
	else
		printf("open file %s failed\n",filePath);*/
}

void cmd_quit(int sock,int sockmsg)
{
	write(sockmsg,user_cmd,sizeof(user_cmd));
}
