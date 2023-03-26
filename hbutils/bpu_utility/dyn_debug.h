#ifndef __DYN_DEBUG_H__
#define __DYN_DEBUG_H__
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

#define ALL_USER (0xFFF)
#define MAX_CLIENT 16
#define MAX_MSG_SIZE 4096 

enum debug_cmd {
	INIT_CMD,
	INIT_CLIENT_CMD,
	DEBUG_1_CMD,
	REPLY_MSG,
	REPLY_RAW,
};

struct debug_msg_head {
	/* server is 0, client is pid */
	uint32_t role;
	uint32_t to;
	uint32_t cmd;
	uint32_t follow_msgs;
	uint32_t lenght;
};

typedef int (*dyn_debug_cb)(uint32_t cmd, int max_len);

int dyn_debug_print(int has_follow, const char *fmt, ...);
int dyn_debug_data(char *data, int len);
int dyn_debug_client_init(const char *name, dyn_debug_cb debug_cb);
void dyn_debug_client_exit(void);

typedef int (*dyn_client_data_cb)(char *data, int len);
int dyn_debug_server_init(const char *name, dyn_client_data_cb data_cb);
int dyn_debug_server_print(int cmd, int arg);
void dyn_debug_server_exit(void);

#ifdef __cplusplus
}
#endif
#endif
