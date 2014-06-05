#ifndef __TFTP_LOG_H__
#define __TFTP_LOG_H__

#define LOG_FILE "log/messages.txt"
#define ERROR_SIZE 100;

#include <stdarg.h>

enum {
	TFTP_LOG_INFO = 0,
	TFTP_LOG_WARNING,
	TFTP_LOG_NOTICE,
	TFTP_LOG_ERROR,
	TFTP_LOG_DEBUG,
	TFTP_LOG_CRIT
};

int init_log(const char* log_file);

//#ifdef __cplusplus 
//extern "C" { 
//#endif 
 void tftp_log(int log_mod, const char* str, ...);

//#ifdef __cplusplus 
//} 
//#endif 

#define T_FTP_LOG(log_mod, str, args...)	do {	\
													tftp_log(log_mod, str, ##args);	\
											} while (0);

#endif
