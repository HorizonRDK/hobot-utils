#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include "dyn_debug.h"

#define BPU_DEBUG_NAME "libcnn"

#define MEM_SHOW 0x100
#define DEBUG_LEVEL 0x2000
#define DUMP_BPU 0x3000

void print_usage(const char *prog)
{
	printf("Usage: %s [-m/-d/-p]\n", prog);
	puts("  -m --mem show bpu mem status\n");
	puts("  -d --debug change plat bpu debug level\n");
	puts("  -p --dump dump bpu cores fcs infos\n"
		);
}

int debug_data_cb(char *data, int len)
{
	if (!data || !len)
		return 0;

	return 0;
}

int main(int argc, char *argv[])
{
	static const char short_options[] = "m:d:p:h";
	static const struct option long_options[] = {
		{ "mem",  1, 0, 'm' },
		{ "debug",  1, 0, 'd' },
		{ "dump",  1, 0, 'p' },
		{ NULL, 0, 0, 0 }
	};
	int cmd_ret;
	int show_bpu_mem = 0;
	int debug_level = -1;
	int dump_infos = -1;
	int ret;

	if(argc < 2){
		print_usage(argv[0]);
		return -1;
	}

	while (1) {
		cmd_ret = getopt_long(argc, argv, short_options, long_options, NULL);

		if (cmd_ret == -1)
			break;

		switch (cmd_ret) {
			case 'm':
				show_bpu_mem = atoi(optarg);
				break;
			case 'd':
				debug_level = atoi(optarg);
				break;
			case 'p':
				dump_infos = atoi(optarg);
				break;
			default:
				print_usage(argv[0]);
				return -1;
				break;
		}
	}

	ret = dyn_debug_server_init(BPU_DEBUG_NAME, debug_data_cb);
	if (ret < 0) {
		printf("BPU_DEBUG: Can't Find BPU app, You can Try Again!\n");
		return ret;
	}

	if (show_bpu_mem) {
		dyn_debug_server_print(MEM_SHOW, 0);
	}

	if (debug_level >= 0) {
		dyn_debug_server_print(DEBUG_LEVEL, debug_level);
	}

	if (dump_infos >= 0) {
		dyn_debug_server_print(DUMP_BPU, 0);
	}

	dyn_debug_server_exit();

	return 0;
}
