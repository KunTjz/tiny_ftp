#ifndef __SYS_DEFINE_H__
#define __SYS_DEFINE_H__

#define T_FTP_FAIL 		-1
#define T_FTP_SUCCESS	1
#define LOCK_FILE		"tinyFtpd.pid"
#define LOCK_MODE		(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

#define COMMAND_PORT		12800 
#define	DATA_PORT			12801

#define DEFAULT_FILE_PATH	"/home/rechard/workplace/service/tiny_ftpd/work_dir"

#endif
