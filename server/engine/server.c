#include <stdio.h>
#include <unistd.h>

#include "../util/sys_define.h"
#include "server_util.h"
#include "../event/tftp_loop.h"
#include "../thread/tftp_thread.h"
#include "../log/tftp_log.h"
#include "../memory/tftp_memory.h"

int main(int argc, char** argv)
{
	daemonize();

	if(already_running() == T_FTP_FAIL)
		printf("arlready running");

	if(init_log(LOG_FILE) == T_FTP_FAIL) {
		printf("init log failed, server stop\n");
		return 0;
	}	

	if(init_memory_pool() == T_FTP_FAIL) {
		T_FTP_LOG(TFTP_LOG_ERROR, "init memory failed\n");
		return 0;
	}
	
	if(start_workers() == T_FTP_FAIL) {
		T_FTP_LOG(TFTP_LOG_ERROR, "start workers failed\n");
		return 0;
	}	

	master_loop();	
    return 0;
}
