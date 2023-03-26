/*
 *    COPYRIGHT NOTICE
 *    Copyright 2020 Horizon Robotics, Inc.
 *    All rights reserved.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <asm/types.h>
#include <pthread.h>
#include <getopt.h>
#include <semaphore.h>
#include <dirent.h>
#include <limits.h>
#include <float.h>
#include <sys/syscall.h>
#include <sched.h>

#define DDR_MONITOR_SAMPLE_CONFIG_SET   _IOW('m', 3, ddr_monitor_sample)

#define DEFAULT_NUMBER_STR_MAX_LENGTH   (128)
#define DDR_MONITOR_MEMSIZE             (1024 * 400)
#define XJ3_DDR_PORT_NUM                 (8)
#define DDR_MAX_REC_NUM 				(400)
#define J3_CORE_NUM                     (4)
#define KERNEL_HZ                       (250)
#define CHIP_LOG_FILE_NAME_LEN_MAX      (128)
#define CHIP_LOG_FILE_NUM_MAX           (100)

#define TIME_SYNC_WAIT_MAX_CNT          (60)
#define TIME_CURR_MIN_YEAY              (2020)

#define J3_PWM_LOSS_FILE    "/sys/devices/platform/soc/soc:pwm@0/pwm_loss_cnt"
#define J3_BPU0_LOAD_FILE   "/sys/devices/system/bpu/bpu0/ratio"
#define J3_BPU1_LOAD_FILE   "/sys/devices/system/bpu/bpu1/ratio"
#define J3_SOC_ID_FILE      "/sys/class/socinfo/soc_uid"
#define J3_DDR_MONITOR_FILE "/dev/ddrmonitor"
#define CPU_LOAD_FILE       "/proc/stat"
#define CHIP_LOG_PATH       "/userdata/log/chip_log/"
#define J3_EFUSE_CNT_FILE  "/sys/class/socinfo/efuse_bit_cnt"

// debug level
#define LOG_ERR (1)
#define LOG_WRN (2)
#define LOG_INF (3)
#define LOG_DBG (4)

const float eps = (float)(1e-6);
#define float_cmp(a, b) (((a) - (b)) > (eps))

// static char *junc_temp_type[3] = {"", "BOARD", "CPU"};

typedef	unsigned int uint32_t;

typedef struct ddr_monitor_sample_s {
	unsigned int sample_period;
	unsigned int sample_size;
} ddr_monitor_sample;

typedef struct {
	int cur_fd;
	int cur_file_len;
	int max_count;
	int file_max_len;
	int cur_index;
	int cur_count;
	char file_name[CHIP_LOG_FILE_NUM_MAX][CHIP_LOG_FILE_NAME_LEN_MAX];
} save_file_config_t;

typedef struct {
    unsigned int msg_type;
    unsigned int msg_len;
    char data[DDR_MONITOR_MEMSIZE + 1024];
} msg_t;

typedef struct {
    int head;
    int rear;
	int count;
    sem_t sem;
    msg_t *msg;
} queue_t;

typedef struct {
	int valid;
	int junc_temp_fd;
	int junc_name_fd;
} junction_temp_t;

typedef struct {
	char cpu[5];
	unsigned long long all;
	unsigned long long user;
	unsigned long long nice;
	unsigned long long sys;
	unsigned long long idle;
	unsigned long long iowait;
	unsigned long long irq;
	unsigned long long softirq;
	unsigned long long steal;/* virtual */
	unsigned long long guest;
	unsigned long long guest_nice;
} cpu_load_t;

typedef struct {
	float user_usage;
	float nice_usage;
	float sys_usage;
	float idle_usage;
	float iowait_usage;
	float irq_usage;
	float softirq_usage;
} cpu_usage_t;

struct ddr_portdata {
    uint32_t waddr_num;
    uint32_t wdata_num;
    uint32_t waddr_cyc;
    uint32_t waddr_latency;
    uint32_t raddr_num;
    uint32_t rdata_num;
    uint32_t raddr_cyc;
    uint32_t raddr_latency;
    uint32_t max_rtrans_cnt;
    uint32_t max_rvalid_cnt;
    uint32_t acc_rtrans_cnt;
    uint32_t min_rtrans_cnt;
    uint32_t max_wtrans_cnt;
    uint32_t max_wready_cnt;
    uint32_t acc_wtrans_cnt;
    uint32_t min_wtrans_cnt;
};

struct ddr_monitor_data {
    unsigned long long curtime;
    struct ddr_portdata portdata[XJ3_DDR_PORT_NUM];
    uint32_t rd_cmd_num;
    uint32_t wr_cmd_num;
    uint32_t mwr_cmd_num;
    uint32_t rdwr_swi_num;
    uint32_t war_haz_num;
    uint32_t raw_haz_num;
    uint32_t waw_haz_num;
    uint32_t act_cmd_num;
    uint32_t act_cmd_rd_num;
    uint32_t per_cmd_num;
    uint32_t per_cmd_rdwr_num;
};

typedef struct {
	/* soc id */
	int soc_id_fd;
	int save_soc_id_flag;

        /* efuse */
        int efuse_cnt_fd;

	/* cpu */
	FILE *cpu_load_fp;
	int query_cpu_load_period_ms;
	int save_cpu_load_period_ms;
	int save_cpu_load_threshold;

	float cpu_load_max;
	float cpu_load_min;
	float cpu_load_avg;
	float cpu_load_cur;
	int cpu_load_record_num;

	/* bpu0 */
	int bpu0_load_fd;
	int query_bpu0_load_period_ms;
	int save_bpu0_load_period_ms;
	int save_bpu0_load_threshold;

	float bpu0_load_max;
	float bpu0_load_min;
	float bpu0_load_avg;
	float bpu0_load_cur;
	int bpu0_load_record_num;

	/* bpu1 */
	int bpu1_load_fd;
	int query_bpu1_load_period_ms;
	int save_bpu1_load_period_ms;
	int save_bpu1_load_threshold;

	float bpu1_load_max;
	float bpu1_load_min;
	float bpu1_load_avg;
	float bpu1_load_cur;
	int bpu1_load_record_num;

	/* temp */
	junction_temp_t junc_temp[10];
	int query_junc_temp_period_ms;
	int save_junc_temp_period_ms;
	float save_junc_temp_threshold;

	float junc_temp_max;
	float junc_temp_min;
	float junc_temp_cur;
	float junc_temp_avg;
	int junc_temp_record_num;

	/* ddr */
	int ddr_monitor_fd;
	struct ddr_monitor_data *ddr_info;
	int query_ddr_band_period_ms;
	int save_ddr_band_period_ms;
	int save_ddr_band_threshold;

	/* pwm */
	int pwm_loss_fd;
	int query_pwm_loss_period_ms;
	int save_pwm_loss_period_ms;
	int pwm_loss_cnt_cur;

	int sleep_period_ms;
	int is_exit;
	int is_volume;
	int priority;
	int sched_police;
} global_config_t;

static global_config_t g_log_cnf = {0};
static save_file_config_t g_file_cnf = {0};

static pthread_t g_ddr_monitor_thread_id;
static pthread_t g_save_file_thread_id;

static queue_t g_file_queue = {0};
char *g_curr = NULL;
static ddr_monitor_sample const ddr_sample = {1000, 1};

static cpu_load_t prev_cpu_load[5] = {0};
static cpu_load_t curr_cpu_load[5] = {0};
static cpu_usage_t curr_cpu_usage[5] = {0};

static char soc_id_buf[256]    = {0};
static char efuse_cnt_buf[256] = {0};
static char cpu_load_buf[512]  = {0};
static char bpu0_load_buf[256] = {0};
static char bpu1_load_buf[256] = {0};
static char junc_temp_buf[256] = {0};
static char pwm_loss_buf[256] = {0};
static char ddr_monitor_buf[DDR_MONITOR_MEMSIZE] = {0};
static char all_buf[1024*100] = {0};

static char *port_type[XJ3_DDR_PORT_NUM + 2] = {"CPU",
	"BIF", "BPU0", "BPU1", "VIO0", "VPU", "VIO1", "PERI", "SUM", "ALL"};

int log_level = LOG_INF;

#define pr_debug(level, ...) \
    do { \
        if (level <= log_level) { \
            switch (level) { \
                case LOG_ERR: \
                    printf("\"%s\" line %d [err]: ", __FILE__, __LINE__); \
                break; \
                case LOG_WRN: \
                    printf("\"%s\" line %d [wrn]: ", __FILE__, __LINE__); \
                break; \
                case LOG_INF: \
                    printf("\"%s\" line %d [inf]: ", __FILE__, __LINE__); \
                break; \
                case LOG_DBG: \
                    printf("\"%s\" line %d [dbg]: ", __FILE__, __LINE__); \
                break; \
                default: \
                    printf("\"%s\" line %d [???]: ", __FILE__, __LINE__); \
                break; \
            } \
            printf(__VA_ARGS__); \
        } \
    } while (0)

static int msg_queue_init(queue_t *queue, int msg_count)
{
    if (!queue) {
        printf("Invalid Queue!\n");
        return -1;
    }
    queue->rear = 0;
    queue->head = 0;
	queue->count = msg_count;

	queue->msg = malloc(sizeof(msg_t) * msg_count);
	if (queue->msg == NULL) {
		printf("malloc error!\n");
        return -1;
	}

    sem_init(&queue->sem, 0, 1);

    return 0;
}

static int msg_dequeue(queue_t *queue, msg_t *msg)
{
    if (!queue || !msg) {
        printf("Invalid Queue!\n");
        return -1;
    }
	// only one consumer,no need to lock head
    if (queue->rear == queue->head) {
        // printf("Empty Queue!\n");
        return -2;
    }

    memcpy(msg, &(queue->msg[queue->head]), sizeof(msg_t));

    queue->head = (queue->head + 1) % queue->count;

	return 0;
}

static int
msg_enqueue(queue_t *queue, unsigned int msg_type, unsigned int msg_len,
				char *buf)
{
    if (!queue || !buf) {
        printf("invalid Queue buf!\n");
        return -1;
    }

    if (queue->head == (queue->rear + 1) % queue->count) {
        printf("Full Queue!\n");
        return -1;
    }

    sem_wait(&queue->sem);
	(queue->msg[queue->rear]).msg_type = msg_type;
	(queue->msg[queue->rear]).msg_len = msg_len;
	memset((queue->msg[queue->rear]).data, 0, msg_len+1);
	memcpy((queue->msg[queue->rear]).data, buf, msg_len);
	queue->rear = (queue->rear+1) % queue->count;
	sem_post(&queue->sem);

	return 0;
}
/*
static int parse_chars(char *src, char *dst, unsigned int cmd,
						unsigned int port_num)
{
	char *p = NULL;
	char *head = NULL;
	char *tail = NULL;
	char temp[128] = {0};

	if (cmd != port_num) {
		snprintf(temp, sizeof("p[0]"), "p[%d]", cmd);
	    p = strstr(src, temp);
		if (NULL == p) {
			printf("can not find %s\n", temp);
			return -1;
		}
		head = strchr(p, '(');
		if (NULL == head) {
			printf("can not find ( \n");
			return -1;
		}
		head = head + strlen("bw:") + 1;
	} else {
		p = strstr(src, "ddrc");
		if (NULL == p) {
			return -1;
		}
		head = p + strlen("ddrc") + 1;
	}

    tail = strchr(head, ' ');
	tail = tail - 1;
	g_curr = tail;
	if (NULL == tail) {
		printf("can not find space\n");
		return -1;
	}
    memcpy(dst, head, tail - head + 1);

	return (int)(tail - head + 1);
}
*/
static int isdigitstr(char *str)
{
	printf("isdigitstr str=%s strspn=%ld, strlen=%ld\n",
					str, strspn(str, "0123456789"),
					strlen(str));
    return (strspn(str, "-0123456789") == strlen(str));
}

static float calc_average_float(float last_avg, float next_val, int num)
{
	float avg_val = 0;

	if (num <= 1) {
		avg_val = next_val;
	} else {
		avg_val = (last_avg * (((float)num - 1) / (float)num)
				+ next_val * (1 / (float)num));
	}

	return avg_val;
}

static int read_file(char *path, int *fd, char *buf, int size)
{
	int len = 0;
	int ret = 0;

	if (buf == NULL || path == NULL) {
		printf("error buf == NULL || path == NULL\n");
		return -1;
	}

	if (*fd <= 0) {
		*fd = open(path, O_RDONLY);
		if (*fd < 0) {
			pr_debug(LOG_DBG, "open file %s error\n", path);
			return -1;
		}
	}

	ret = (int)lseek(*fd, 0, SEEK_SET);
	if (ret < 0) {
		printf("lseek file=%s, ret=%d error\n", path, ret);
		return -1;
	}

	len = (int)read(*fd, buf, size);
	if (len <= 0) {
		printf("read file=%s, fd=%d, len=%d error\n", path, *fd, len);
		return -1;
	}

	return len;
}

static int get_chip_id(char *buf)
{
	int len = 0;

	if (buf == NULL) {
		return -1;
	}

	len = read_file(J3_SOC_ID_FILE, &g_log_cnf.soc_id_fd,
						buf, DEFAULT_NUMBER_STR_MAX_LENGTH);
	if (len > 0 && buf[len-1] == '\n') {
		buf[len-1] = 0;
		len -= 1;
	}

	return len;
}

static int get_efuse_cnt(char *buf)
{
        int len = 0;

        if (buf == NULL) {
                return -1;
        }

        len = read_file(J3_EFUSE_CNT_FILE, &g_log_cnf.efuse_cnt_fd,
                                                buf, DEFAULT_NUMBER_STR_MAX_LENGTH);//NOLINT
        if (len > 0 && buf[len-1] == '\n') {
                buf[len-1] = 0;
                len -= 1;
        }

        /* read only once.close fd */
        if (g_log_cnf.efuse_cnt_fd > 0) {
                close(g_log_cnf.efuse_cnt_fd);
                g_log_cnf.efuse_cnt_fd = 0;
        }

        return len;
}

static int gener_cpu_load_buf(char *buf)
{
	int i = 0;
	int offset = 0;

	offset += sprintf(buf + offset, "CPU  Load: cur:%6.2f max:%6.2f "//NOLINT
				"min:%6.2f avg:%6.2f\n",
				g_log_cnf.cpu_load_cur,
				g_log_cnf.cpu_load_max,
				g_log_cnf.cpu_load_min,
				g_log_cnf.cpu_load_avg);

	for (i = 0; i< J3_CORE_NUM + 1; i++) {
		offset += sprintf(buf + offset, "%4s %6.2f usr %6.2f sys %6.2f nic"//NOLINT
				" %6.2f idle %6.2f iowait %6.2f irq %6.2f softirq\n",
				curr_cpu_load[i].cpu,
				curr_cpu_usage[i].user_usage,
				curr_cpu_usage[i].sys_usage,
				curr_cpu_usage[i].nice_usage,
				curr_cpu_usage[i].idle_usage,
				curr_cpu_usage[i].iowait_usage,
				curr_cpu_usage[i].irq_usage,
				curr_cpu_usage[i].softirq_usage);
	}

	/* Let's think about the sub-zero temperature */
	g_log_cnf.cpu_load_record_num = 0;
	g_log_cnf.cpu_load_max = FLT_MIN;
	g_log_cnf.cpu_load_min = FLT_MAX;
	g_log_cnf.cpu_load_avg = 0;

	return offset;
}

static int get_cpu_load(char *buf)
{
	int i = 0;
	int offset = 0;
	char temp_buf[256] = {0};
	char *ret_temp_buf = NULL;
	unsigned long long all_jiffies;
	float all_usage = 0.0;

	if (g_log_cnf.cpu_load_fp == NULL) {
		g_log_cnf.cpu_load_fp = fopen(CPU_LOAD_FILE, "r");
		if (g_log_cnf.cpu_load_fp == NULL) {
			return -1;
		}
	}

	for (i = 0; i < J3_CORE_NUM + 1; i++) {
		memset(temp_buf, 0, sizeof(temp_buf));
		ret_temp_buf = fgets(temp_buf, sizeof(temp_buf), g_log_cnf.cpu_load_fp);
		pr_debug(LOG_INF, "ret_temp_buf=%s", ret_temp_buf ? ret_temp_buf:"");
		sscanf(temp_buf, "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
			curr_cpu_load[i].cpu,
			&curr_cpu_load[i].user, &curr_cpu_load[i].nice,
			&curr_cpu_load[i].sys, &curr_cpu_load[i].idle,
			&curr_cpu_load[i].iowait, &curr_cpu_load[i].irq,
			&curr_cpu_load[i].softirq, &curr_cpu_load[i].steal,
			&curr_cpu_load[i].guest, &curr_cpu_load[i].guest_nice);

			curr_cpu_load[i].all = curr_cpu_load[i].user + curr_cpu_load[i].nice
								+ curr_cpu_load[i].sys + curr_cpu_load[i].idle
								+ curr_cpu_load[i].iowait + curr_cpu_load[i].irq
								+ curr_cpu_load[i].softirq + curr_cpu_load[i].steal
								+ curr_cpu_load[i].guest + curr_cpu_load[i].guest_nice;
	}

	if (prev_cpu_load[0].user != 0) {
		for (i = 0; i < J3_CORE_NUM + 1; i++) {
			all_jiffies = curr_cpu_load[i].all - prev_cpu_load[i].all;
			curr_cpu_usage[i].user_usage =
						(float)(curr_cpu_load[i].user - prev_cpu_load[i].user) / (float)all_jiffies * 100;// NOLINT
			curr_cpu_usage[i].nice_usage =
						(float)(curr_cpu_load[i].nice - prev_cpu_load[i].nice) / (float)all_jiffies * 100;// NOLINT
			curr_cpu_usage[i].sys_usage =
						(float)(curr_cpu_load[i].sys - prev_cpu_load[i].sys) / (float)all_jiffies * 100;// NOLINT
			curr_cpu_usage[i].idle_usage =
						(float)(curr_cpu_load[i].idle - prev_cpu_load[i].idle) / (float)all_jiffies * 100;// NOLINT
			curr_cpu_usage[i].iowait_usage =
						(float)(curr_cpu_load[i].iowait - prev_cpu_load[i].iowait) / (float)all_jiffies * 100;// NOLINT
			curr_cpu_usage[i].irq_usage =
						(float)(curr_cpu_load[i].irq - prev_cpu_load[i].irq) / (float)all_jiffies * 100;// NOLINT
			curr_cpu_usage[i].softirq_usage =
						(float)(curr_cpu_load[i].softirq - prev_cpu_load[i].softirq) / (float)all_jiffies * 100;// NOLINT


			if (i == 0) {
				all_usage = curr_cpu_usage[i].user_usage + curr_cpu_usage[i].nice_usage
							+ curr_cpu_usage[i].sys_usage + curr_cpu_usage[i].iowait_usage
							+ curr_cpu_usage[i].irq_usage + curr_cpu_usage[i].softirq_usage;
			}
		}
	}

	memcpy(&prev_cpu_load, &curr_cpu_load, sizeof(curr_cpu_load));

	/* close operation enable kernel update */
	fclose(g_log_cnf.cpu_load_fp);
	g_log_cnf.cpu_load_fp = NULL;

	g_log_cnf.cpu_load_cur = all_usage;
	if (g_log_cnf.cpu_load_cur > g_log_cnf.cpu_load_max) {
		g_log_cnf.cpu_load_max = g_log_cnf.cpu_load_cur;
	}

	if (g_log_cnf.cpu_load_cur < g_log_cnf.cpu_load_min) {
		g_log_cnf.cpu_load_min = g_log_cnf.cpu_load_cur;
	}

	g_log_cnf.cpu_load_record_num++;

	g_log_cnf.cpu_load_avg = calc_average_float(g_log_cnf.cpu_load_avg,
			g_log_cnf.cpu_load_cur, g_log_cnf.cpu_load_record_num);

	return offset;
}

static int gener_bpu0_load_buf(char *buf)
{
	int ret = 0;
	int offset = 0;

	ret = sprintf(buf + offset, "BPU0 Load: cur:%6.2f max:%6.2f "// NOLINT
				"min:%6.2f avg:%6.2f\n",
				g_log_cnf.bpu0_load_cur,
				g_log_cnf.bpu0_load_max,
				g_log_cnf.bpu0_load_min,
				g_log_cnf.bpu0_load_avg);

	/* Let's think about the sub-zero temperature */
	g_log_cnf.bpu0_load_record_num = 0;
	g_log_cnf.bpu0_load_max = FLT_MIN;
	g_log_cnf.bpu0_load_min = FLT_MAX;
	g_log_cnf.bpu0_load_avg = 0;

	return ret;
}

static int get_bpu0_load(char *buf)
{
	int len = 0;
	char buf_load_tmp[DEFAULT_NUMBER_STR_MAX_LENGTH] = {0};

	if (buf == NULL) {
		return -1;
	}

	len = read_file(J3_BPU0_LOAD_FILE, &g_log_cnf.bpu0_load_fd,
						buf_load_tmp, DEFAULT_NUMBER_STR_MAX_LENGTH);
	if (len > 0 && buf_load_tmp[len-1] == '\n') {
		buf_load_tmp[len-1] = 0;
		len -= 1;
	}

	g_log_cnf.bpu0_load_cur = (float)(atoi(buf_load_tmp) * 1.0);
	if (g_log_cnf.bpu0_load_cur > g_log_cnf.bpu0_load_max) {
		g_log_cnf.bpu0_load_max = g_log_cnf.bpu0_load_cur;
	}

	if (g_log_cnf.bpu0_load_cur < g_log_cnf.bpu0_load_min) {
		g_log_cnf.bpu0_load_min = g_log_cnf.bpu0_load_cur;
	}

	g_log_cnf.bpu0_load_record_num++;

	g_log_cnf.bpu0_load_avg = calc_average_float(g_log_cnf.bpu0_load_avg,
			g_log_cnf.bpu0_load_cur, g_log_cnf.bpu0_load_record_num);

	pr_debug(LOG_DBG, "len=%d, buf=%s\n", len, buf);

	return 0;
}

static int gener_bpu1_load_buf(char *buf)
{
	int ret = 0;
	int offset = 0;

	ret = sprintf(buf + offset, "BPU1 Load: cur:%6.2f max:%6.2f "// NOLINT
				"min:%6.2f avg:%6.2f\n",
				g_log_cnf.bpu1_load_cur,
				g_log_cnf.bpu1_load_max,
				g_log_cnf.bpu1_load_min,
				g_log_cnf.bpu1_load_avg);

	/* Let's think about the sub-zero temperature */
	g_log_cnf.bpu1_load_record_num = 0;
	g_log_cnf.bpu1_load_max = FLT_MIN;
	g_log_cnf.bpu1_load_min = FLT_MAX;
	g_log_cnf.bpu1_load_avg = 0;

	return ret;
}

static int get_bpu1_load(char *buf)
{
	int len = 0;
	char buf_load_tmp[DEFAULT_NUMBER_STR_MAX_LENGTH] = {0};

	if (buf == NULL) {
		return -1;
	}

	len = read_file(J3_BPU1_LOAD_FILE, &g_log_cnf.bpu1_load_fd,
						buf_load_tmp, DEFAULT_NUMBER_STR_MAX_LENGTH);
	if (len > 0 && buf_load_tmp[len-1] == '\n') {
		buf_load_tmp[len-1] = 0;
		len -= 1;
	}

	g_log_cnf.bpu1_load_cur = (float)(atoi(buf_load_tmp) * 1.0);
	if (g_log_cnf.bpu1_load_cur > g_log_cnf.bpu1_load_max) {
		g_log_cnf.bpu1_load_max = g_log_cnf.bpu1_load_cur;
	}

	if (g_log_cnf.bpu1_load_cur < g_log_cnf.bpu1_load_min) {
		g_log_cnf.bpu1_load_min = g_log_cnf.bpu1_load_cur;
	}
	g_log_cnf.bpu1_load_record_num++;
	g_log_cnf.bpu1_load_avg = calc_average_float(g_log_cnf.bpu1_load_avg,
			g_log_cnf.bpu1_load_cur, g_log_cnf.bpu1_load_record_num);

	pr_debug(LOG_DBG, "len=%d, buf=%s\n", len, buf);

	return 0;
}

static int gener_junction_temp_buf(char *buf)
{
	int ret = 0;
	int offset = 0;

	ret = sprintf(buf + offset, "CPU  Temp: cur:%6.2f max:%6.2f min:%6.2f avg:%6.2f\n",// NOLINT
				g_log_cnf.junc_temp_cur,
				g_log_cnf.junc_temp_max,
				g_log_cnf.junc_temp_min,
				g_log_cnf.junc_temp_avg);

	/* Let's think about the sub-zero temperature */
	g_log_cnf.junc_temp_record_num = 0;
	g_log_cnf.junc_temp_max = FLT_MIN;
	g_log_cnf.junc_temp_min = FLT_MAX;
	g_log_cnf.junc_temp_avg = 0;

	return ret;
}

static int get_junction_temp(char *buf)
{
	int len = 0;
	int i = 0;
	int type = 0;
	int ret = 0;

	char buf_temp_tmp[DEFAULT_NUMBER_STR_MAX_LENGTH] = {0};
	char buf_name_tmp[DEFAULT_NUMBER_STR_MAX_LENGTH] = {0};

	char hwmon_temp_path[256] = {0};
	char hwmon_name_path[256] = {0};

	if (buf == NULL) {
		return -1;
	}

	for (i = 0; i < 3; i++) {
		sprintf(hwmon_temp_path, "/sys/class/hwmon/hwmon%d/temp1_input", i);//NOLINT
		sprintf(hwmon_name_path, "/sys/class/hwmon/hwmon%d/name", i);//NOLINT

		len = read_file(hwmon_name_path, &(g_log_cnf.junc_temp[i].junc_name_fd),
						buf_name_tmp, DEFAULT_NUMBER_STR_MAX_LENGTH);
		if (len < 0) {
			continue;
		}

		len = read_file(hwmon_temp_path, &(g_log_cnf.junc_temp[i].junc_temp_fd),
						buf_temp_tmp, DEFAULT_NUMBER_STR_MAX_LENGTH);
		if (len < 0) {
			continue;
		}

		if (strncmp(buf_name_tmp, "tmp75c", 6) == 0) {
			type = 1;
		} else if (strncmp(buf_name_tmp, "x2_temp", 7) == 0
			|| strncmp(buf_name_tmp, "pvt_ts", 6) == 0) {
			type = 2;
		}

		if (/*type == 1 || */type == 2) {
			/*
			ret = sprintf(buf + offset, "%s: %f", junc_temp_type[type], atoi(buf_temp_tmp)/1000.0);
			if (ret > 0) {
				offset += ret;
			}
			*/
			g_log_cnf.junc_temp_cur = (float)(atoi(buf_temp_tmp) /1000.0);
			if (g_log_cnf.junc_temp_cur > g_log_cnf.junc_temp_max) {
				g_log_cnf.junc_temp_max = g_log_cnf.junc_temp_cur;
			}

			if (g_log_cnf.junc_temp_cur < g_log_cnf.junc_temp_min) {
				g_log_cnf.junc_temp_min = g_log_cnf.junc_temp_cur;
			}
			g_log_cnf.junc_temp_record_num++;

			g_log_cnf.junc_temp_avg = calc_average_float(g_log_cnf.junc_temp_avg,
						g_log_cnf.junc_temp_cur, g_log_cnf.junc_temp_record_num);
		}

		type = 0;
		memset(buf_name_tmp, 0, sizeof(buf_name_tmp));
		memset(buf_temp_tmp, 0, sizeof(buf_temp_tmp));
	}

	return ret;
}

static int gener_pwm_loss_buf(char *buf)
{
	int offset = 0;

	offset += sprintf(buf + offset, "PWM  Loss: %d\n",//NOLINT
				g_log_cnf.pwm_loss_cnt_cur);

	return offset;
}

static int get_pwm_loss(char *buf)
{
	int len = 0;
	char buf_pwm_tmp[DEFAULT_NUMBER_STR_MAX_LENGTH] = {0};

	if (buf == NULL) {
		return -1;
	}

	len = read_file(J3_PWM_LOSS_FILE, &g_log_cnf.pwm_loss_fd,
						buf_pwm_tmp, DEFAULT_NUMBER_STR_MAX_LENGTH);
	if (len > 0 && buf_pwm_tmp[len-1] == '\n') {
		buf_pwm_tmp[len-1] = 0;
		len -= 1;
	}

	if (len > 0) {
		g_log_cnf.pwm_loss_cnt_cur = atoi(buf_pwm_tmp);
	}
	pr_debug(LOG_DBG, "len=%d\n", len);

	return len;
}

static int fill_ddr_info(char *buf, struct ddr_monitor_data *ddr_info, int num)
{
	int i;
	int cur = 0;
	int offset = 0;
	int show_port_id = XJ3_DDR_PORT_NUM + 1;
	int rd_cmd_bytes = 64;
	int wr_cmd_bytes = 64;
	unsigned long read_bw = 0;
	unsigned long write_bw = 0;
	unsigned long bw_sum = 0;

	if (!ddr_info) {
		printf("%s ddr_info is NULL\n", __func__);
		printf("ddr_monitor not started \n");
		return 0;
	}

	for (cur = 0; cur < num; cur++) {
		offset += sprintf(buf + offset, "DDR cur info:");//NOLINT
		if (show_port_id >= XJ3_DDR_PORT_NUM) {
			offset += sprintf(buf + offset, "\nMB/S   P0:CPU   P1:BIF  P2:BPU0"//NOLINT
				"P3:BPU1  P4:VIO0   P5:VPU  P6:VIO1  P7:Peri      SUM    CMD_SUM\n");
		} else  {
			offset += sprintf(buf + offset, "P%d:%s     SUM  MB/S",//NOLINT
				show_port_id, port_type[show_port_id]);
			offset += sprintf(buf + offset, "\n             ");//NOLINT
		}
		offset += sprintf(buf + offset, "Read :   ");//NOLINT
		for (i = 0; i < XJ3_DDR_PORT_NUM; i++) {
			if (ddr_info[cur].portdata[i].raddr_num) {
				read_bw = ((unsigned long) ddr_info[cur].portdata[i].rdata_num) *
						  16 * (1000 / ddr_sample.sample_period) >> 20;
				if (show_port_id >= XJ3_DDR_PORT_NUM || show_port_id == i)
				offset += sprintf(buf + offset, "%4lu     ", read_bw);//NOLINT
				bw_sum += read_bw;
			} else {
				if (show_port_id >= XJ3_DDR_PORT_NUM || show_port_id == i)
					offset += sprintf(buf + offset, "%4u     ", 0);//NOLINT
			}
		}
		offset += sprintf(buf + offset, "%4lu     ", bw_sum);//NOLINT
		bw_sum = 0;
		read_bw = ((unsigned long) ddr_info[cur].rd_cmd_num) *
				  rd_cmd_bytes * (1000 / ddr_sample.sample_period) >> 20;
		if (show_port_id >= XJ3_DDR_PORT_NUM)
			offset += sprintf(buf + offset, "%4lu\n", read_bw);//NOLINT

		if (show_port_id < XJ3_DDR_PORT_NUM)
			offset += sprintf(buf + offset, "\n             ");//NOLINT
		offset += sprintf(buf + offset, "Write:   ");//NOLINT
		for (i = 0; i < XJ3_DDR_PORT_NUM; i++) {
			if (ddr_info[cur].portdata[i].waddr_num) {
				write_bw = ((unsigned long) ddr_info[cur].portdata[i].wdata_num) *
							16 * (1000 / ddr_sample.sample_period) >> 20;
				if (show_port_id >= XJ3_DDR_PORT_NUM || show_port_id == i)
					offset += sprintf(buf + offset, "%4lu     ", write_bw);//NOLINT
				bw_sum += write_bw;
			} else {
				if (show_port_id >= XJ3_DDR_PORT_NUM || show_port_id == i)
					offset += sprintf(buf + offset, "%4u     ", 0);//NOLINT
			}
		}
		offset += sprintf(buf + offset, "%4lu     ", bw_sum);//NOLINT
		bw_sum = 0;
		write_bw = ((unsigned long) ddr_info[cur].wr_cmd_num) *
					wr_cmd_bytes * (1000 / ddr_sample.sample_period) >> 20;
		if (show_port_id >= XJ3_DDR_PORT_NUM)
			offset += sprintf(buf + offset, "%4lu\n", write_bw);//NOLINT
		offset += sprintf(buf + offset, "\n");//NOLINT
	}
	return offset;
}

static int get_ddr_band(char *buf)
{
	int ret = 0;
	int ddr_info_sz = DDR_MAX_REC_NUM * sizeof(struct ddr_monitor_data);

	if (buf == NULL) {
		return -1;
	}

	if (g_log_cnf.ddr_monitor_fd <= 0) {
		g_log_cnf.ddr_monitor_fd = open(J3_DDR_MONITOR_FILE, O_RDWR | O_NONBLOCK, 0);
		if (g_log_cnf.ddr_monitor_fd <= 0) {
			printf("func=%s open file=%s error\n", __FUNCTION__, J3_DDR_MONITOR_FILE);
			return -1;
		}

		ret = (int)ioctl(g_log_cnf.ddr_monitor_fd, DDR_MONITOR_SAMPLE_CONFIG_SET,
								&ddr_sample);
		if (ret != 0) {
			printf("func=%s ioctl file=%s error\n", __FUNCTION__, J3_DDR_MONITOR_FILE);
			close(g_log_cnf.ddr_monitor_fd);
			g_log_cnf.ddr_monitor_fd = 0;
			return -1;
		}

		if (g_log_cnf.ddr_info == NULL) {
			g_log_cnf.ddr_info = malloc(ddr_info_sz);
		}

		if (g_log_cnf.ddr_info == NULL) {
			printf("func=%s mmap file=%s error\n", __FUNCTION__, J3_DDR_MONITOR_FILE);
			close(g_log_cnf.ddr_monitor_fd);
			g_log_cnf.ddr_monitor_fd = 0;
			return -1;
		}
	}

	if (g_log_cnf.ddr_monitor_fd > 0 && g_log_cnf.ddr_info != NULL) {
		ret = (int)read(g_log_cnf.ddr_monitor_fd, g_log_cnf.ddr_info,
							ddr_info_sz);
		if (ret > 0 && ret <= ddr_info_sz) {
			pr_debug(LOG_DBG, "len=%d, buf=%s\n", ret, (char *)g_log_cnf.ddr_info);
			ret = fill_ddr_info(buf, g_log_cnf.ddr_info,
				ret / (int)sizeof(struct ddr_monitor_data));
			pr_debug(LOG_DBG, "len=%d, buf=%s\n", ret, buf);
		} else {
			return -1;
		}
	}

	return ret;
}

static void *ddr_monitor_thread(void *arg)
{
	int ret = 0;
	unsigned int loop_cnt = 0;
	unsigned int temp_over_thres_cnt = 0;
	unsigned int save_ddr_band_cnt = 0;
	unsigned int save_ddr_band_cnt_orig = 0;
	struct sched_param param = {0};

	save_ddr_band_cnt = g_log_cnf.save_ddr_band_period_ms /
					(ddr_sample.sample_period * ddr_sample.sample_size);

	if (save_ddr_band_cnt <= 0) {
		save_ddr_band_cnt = 1;
	}
	save_ddr_band_cnt_orig = save_ddr_band_cnt;

	pr_debug(LOG_INF, "ddr_monitor_thread in. save_ddr_band_cnt=%d\n",
						save_ddr_band_cnt);

	if (1) {
		param.sched_priority = g_log_cnf.priority;
		ret = sched_setscheduler((int)syscall(SYS_gettid), g_log_cnf.sched_police, &param);//NOLINT
		if (ret) {
			printf("sched_setscheduler error!sched_method=%d, "
					"sched_priority=%d, ret=%d\n",
					g_log_cnf.sched_police, g_log_cnf.priority, ret);
		}
	}

	g_log_cnf.ddr_info = NULL;

	while (!g_log_cnf.is_exit) {
		/* */
		if (g_log_cnf.is_volume) {
			if (g_log_cnf.junc_temp_cur >= g_log_cnf.save_junc_temp_threshold) {
				/* first start recording query cycle */
				if (temp_over_thres_cnt == 0) {
					save_ddr_band_cnt = 1;
				}
				temp_over_thres_cnt++;

				/* second recording save cycle/2 */
				if (temp_over_thres_cnt == save_ddr_band_cnt_orig) {
					save_ddr_band_cnt = save_ddr_band_cnt_orig / 2;
				}

				pr_debug(LOG_WRN, "high temperature! cur_temp=%.0f, "
							"temp_threshold=%.0f, cnt=%d\n",
							g_log_cnf.junc_temp_cur,
							g_log_cnf.save_junc_temp_threshold,
							temp_over_thres_cnt);
			} else {
				/* restore to the original value */
				temp_over_thres_cnt = 0;
				save_ddr_band_cnt = INT_MAX;  // save_ddr_band_cnt_orig;
				pr_debug(LOG_DBG, "normal temperature! cur_temp=%.0f, "
							"temp_threshold=%.0f, cnt=%d\n",
							g_log_cnf.junc_temp_cur,
							g_log_cnf.save_junc_temp_threshold,
							temp_over_thres_cnt);
			}
			/*  */
		}

		ret = get_ddr_band(ddr_monitor_buf);
		if (ret >= 0 && (loop_cnt % save_ddr_band_cnt) == 0) {
			ret = msg_enqueue(&g_file_queue, 1, ret, ddr_monitor_buf);
			if (ret != 0) {
				pr_debug(LOG_ERR, "msg_enqueue error, ret=%d\n", ret);
			}
			memset(ddr_monitor_buf, 0, sizeof(ddr_monitor_buf));
		}/* threshold save data */
		loop_cnt++;
	}

	pr_debug(LOG_INF, "ddr_monitor_thread exit now!");

	if (g_log_cnf.ddr_info != NULL) {
		free(g_log_cnf.ddr_info);
		g_log_cnf.ddr_info = NULL;
	}

	if (g_log_cnf.ddr_monitor_fd > 0) {
		close(g_log_cnf.ddr_monitor_fd);
		g_log_cnf.ddr_monitor_fd = 0;
	}

	pr_debug(LOG_INF, "ddr_monitor_thread exit success!");

	return NULL;
}

static int get_file_size(const char *file_name)
{
	struct stat buf = {0};

	if (stat(file_name, &buf) < 0) {
		return -1;
	}

	return (int)buf.st_size;
}

static int create_dir(const char *path_name)
{
	char dir_name[256] = {0};
	int i, len;

	strcpy(dir_name, path_name);//NOLINT
	len = (int)strlen(dir_name);

	for (i = 1; i < len; i++) {
		if (dir_name[i] == '/') {
			dir_name[i] = 0;
			if (access(dir_name, 0) != 0) {
				if (mkdir(dir_name, 0755) == -1) {
					pr_debug(LOG_ERR, "mkdir %s error\n", path_name);
					return -1;
				}
			}
			dir_name[i] = '/';
		}
	}

	return 0;
}

static int read_file_list(char *path)
{
	int ret = 0;
	int i, j, n;
	int size = 0;
	DIR *dir;
	struct dirent *ptr;
	char str_tmp[CHIP_LOG_FILE_NAME_LEN_MAX] = {0};

	g_file_cnf.cur_count = 0;

	if (g_file_cnf.max_count <= 0) {
		pr_debug(LOG_WRN, "max_count == 0\n");
		return 0;
	}

	ret = create_dir(CHIP_LOG_PATH);
	if (ret < 0) {
		pr_debug(LOG_ERR, "create_dir error");
		return -1;
	}

	if ((dir = opendir(path)) == NULL) {
		pr_debug(LOG_ERR, "open dir error\n");
		return -1;
	}

	while ((ptr = readdir(dir)) != NULL) {
		if (strcmp(ptr->d_name, ".") == 0
					|| strcmp(ptr->d_name, "..") == 0) {  // current dir OR parrent dir
			continue;
		} else if (ptr->d_type == 8) {  // file
			pr_debug(LOG_DBG, "f_name:%s%s\n", path, ptr->d_name);
			if (strstr(ptr->d_name, "chip-log-") != NULL) {
				sprintf(g_file_cnf.file_name[g_file_cnf.cur_count], "%s%s",//NOLINT
							CHIP_LOG_PATH, ptr->d_name);
				pr_debug(LOG_DBG, "count=%d name=%s\n",
							g_file_cnf.cur_count,
							g_file_cnf.file_name[g_file_cnf.cur_count]);

				g_file_cnf.cur_count++;
				/*  */
				if (g_file_cnf.cur_count >= g_file_cnf.max_count) {
					pr_debug(LOG_WRN, "only allow max files count=%d\n",
									g_file_cnf.max_count);
					/*  */
					break;
				}
			}
		} else if (ptr->d_type == 10) {  // link file
			pr_debug(LOG_DBG, "l_name:%s/%s\n", path, ptr->d_name);
			continue;
		} else if (ptr->d_type == 4) {  // dir
			pr_debug(LOG_DBG, "d_name:%s/%s\n", path, ptr->d_name);
			continue;
		}
	}

	for (i = 0; i < g_file_cnf.cur_count; i++) {
		pr_debug(LOG_DBG, "index=%d file %s\n", i, g_file_cnf.file_name[i]);
	}

	/* sorted by time */
	for (i = 0; i < g_file_cnf.cur_count; i++) {
		n = 0;
		for (j = 0; j < g_file_cnf.cur_count - i - 1; j++) {
			pr_debug(LOG_DBG, "i=%d, j=%d [%s, %s] cmp=%d\n", i, j,
					g_file_cnf.file_name[j],
					g_file_cnf.file_name[j+1],
					strcmp(g_file_cnf.file_name[j], g_file_cnf.file_name[j+1]));
			if (strcmp(g_file_cnf.file_name[j], g_file_cnf.file_name[j+1]) > 0) {
				memcpy(str_tmp, g_file_cnf.file_name[j], CHIP_LOG_FILE_NAME_LEN_MAX);

				memset(g_file_cnf.file_name[j], 0, CHIP_LOG_FILE_NAME_LEN_MAX);
				memcpy(g_file_cnf.file_name[j], g_file_cnf.file_name[j + 1], CHIP_LOG_FILE_NAME_LEN_MAX);//NOLINT

				memset(g_file_cnf.file_name[j + 1], 0, CHIP_LOG_FILE_NAME_LEN_MAX);
				memcpy(g_file_cnf.file_name[j + 1], str_tmp, CHIP_LOG_FILE_NAME_LEN_MAX);
				memset(str_tmp, 0, CHIP_LOG_FILE_NAME_LEN_MAX);
				n++;
			}
		}
		if (n == 0)
			break;
	}

	for (i = 0; i < g_file_cnf.cur_count; i++) {
		pr_debug(LOG_DBG, "index=%d file %s\n", i, g_file_cnf.file_name[i]);
	}

	/* consider append write */
	if (g_file_cnf.cur_count > 0) {
		size = get_file_size(g_file_cnf.file_name[g_file_cnf.cur_count - 1]);
		if (size > 0 && size < g_file_cnf.file_max_len) {
			g_file_cnf.cur_index = g_file_cnf.cur_count - 1;
			g_file_cnf.cur_fd = open(g_file_cnf.file_name[g_file_cnf.cur_index],
												O_RDWR | O_APPEND);
			if (g_file_cnf.cur_fd > 0) {
				g_file_cnf.cur_file_len = size;
			}
		} else {
			if (g_file_cnf.cur_count >= g_file_cnf.max_count) {
				g_file_cnf.cur_index = 0;
			} else {
				g_file_cnf.cur_index = g_file_cnf.cur_count;
			}
		}
	} else {
		g_file_cnf.cur_index = 0;
	}

	/*
	if (g_file_cnf.cur_count >= g_file_cnf.max_count) {
		g_file_cnf.cur_index = 0;
	} else {
		g_file_cnf.cur_index = g_file_cnf.cur_count;
	}*/

	pr_debug(LOG_INF, "cur index=%d\n", g_file_cnf.cur_index);

	closedir(dir);

	return 0;
}

static int get_local_time(char *buf)
{
	time_t ltime;
	struct tm* today;
	char systime[128] = {0};

	time(&ltime);
	today = localtime(&ltime);//NOLINT

	strftime(systime, 120, "%Y-%m-%d-%H-%M-%S", today);

	memcpy(buf, systime, sizeof(systime));

	return 0;
}

static int save_log_file(char *buf, int len)
{
	int ret = 0;
	int max_try_cnt = 100;
	char systime[128] = {0};
	char str_tmp[CHIP_LOG_FILE_NAME_LEN_MAX] = {0};

	if (buf == NULL || len == 0) {
		pr_debug(LOG_ERR, "invalid param\n");
	}

	if (g_file_cnf.max_count <= 0) {
		return -1;
	}

	if (g_file_cnf.cur_index >= g_file_cnf.max_count) {
		g_file_cnf.cur_index = 0;
	}

	if (g_file_cnf.cur_fd <= 0) {
		/* delete */
		if (strlen(g_file_cnf.file_name[g_file_cnf.cur_index]) > 0) {
			remove(g_file_cnf.file_name[g_file_cnf.cur_index]);
			memset(g_file_cnf.file_name[g_file_cnf.cur_index],
										0, CHIP_LOG_FILE_NAME_LEN_MAX);
		} else {
			g_file_cnf.cur_count++;
		}

		g_file_cnf.cur_file_len = 0;
		/* create */
		get_local_time(systime);
		sprintf(g_file_cnf.file_name[g_file_cnf.cur_index], "%schip-log-%s.txt",//NOLINT
												CHIP_LOG_PATH, systime);
		g_file_cnf.cur_fd = open(g_file_cnf.file_name[g_file_cnf.cur_index],
												O_RDWR | O_CREAT, 0644);
		if (g_file_cnf.cur_fd <= 0) {
			pr_debug(LOG_ERR, "create file %s ret=%d error\n",
						g_file_cnf.file_name[g_file_cnf.cur_index],
						g_file_cnf.cur_fd);
			return -1;
		}

		lseek(g_file_cnf.cur_fd, 0, SEEK_SET);

		/* save soc_id to file head */
		if (g_log_cnf.save_soc_id_flag == 1) {
			int offset = 0;
			offset = snprintf(str_tmp, CHIP_LOG_FILE_NAME_LEN_MAX, "chip id: %s . efuse bit cnt: %s\n", soc_id_buf, efuse_cnt_buf);//NOLINT
			ret = (int)write(g_file_cnf.cur_fd, str_tmp, offset);
			if (ret > 0) {
				g_file_cnf.cur_file_len += ret;
			}
		}
	}

	/* cur file length  */
	g_file_cnf.cur_file_len += len;

	/* g_file_cnf.cur_fd must > 0 */
	do {
		ret = (int)write(g_file_cnf.cur_fd, buf, len);
		len -= ret;
		max_try_cnt--;
	}while(len > 0 && max_try_cnt > 0);

	if (len > 0) {
		pr_debug(LOG_WRN, "write file failed, len=%d, max_try_cnt=%d\n",
								len, max_try_cnt);
	}

	/* cur file length over max length */
	if (g_file_cnf.cur_file_len > g_file_cnf.file_max_len) {
		if (g_file_cnf.cur_fd > 0) {
			fsync(g_file_cnf.cur_fd);
			close(g_file_cnf.cur_fd);
			g_file_cnf.cur_fd = 0;
		}
		g_file_cnf.cur_index = (g_file_cnf.cur_index + 1) % g_file_cnf.max_count;
	}

	return 0;
}

static void *save_file_thread(void *arg)
{
	int ret = 0;
	unsigned int loop_cnt = 0;
	msg_t msg = {0};

	while (!g_log_cnf.is_exit) {
		ret = msg_dequeue(&g_file_queue, &msg);
		if (ret == 0) {
			printf("type=%d, len=%d\n%s\n", msg.msg_type, msg.msg_len, msg.data);
			save_log_file(msg.data, msg.msg_len);
		} else {
			// pr_debug(LOG_DBG, "msg_dequeue error, ret=%d\n", ret);
			usleep(50 * 1000);
		}
		memset(&msg, 0, sizeof(msg));
		loop_cnt++;
	}

	pr_debug(LOG_INF, "save_file_thread exit now!");
	if (g_file_cnf.cur_fd > 0) {
		pr_debug(LOG_INF, "save_file_thread close file!");
		fsync(g_file_cnf.cur_fd);
		close(g_file_cnf.cur_fd);
		g_file_cnf.cur_fd = 0;
	}
	pr_debug(LOG_INF, "save_file_thread exit success!");
	return NULL;
}

static int set_default_value()
{
	g_log_cnf.sleep_period_ms = 1000;

	g_log_cnf.query_cpu_load_period_ms  = 1000;
	g_log_cnf.query_bpu0_load_period_ms = 1000;
	g_log_cnf.query_bpu1_load_period_ms = 1000;
	g_log_cnf.query_junc_temp_period_ms = 1000;
	g_log_cnf.query_ddr_band_period_ms  = 1000;
	g_log_cnf.query_pwm_loss_period_ms  = 60*1000;

	/* save period ms */
	g_log_cnf.save_cpu_load_period_ms  = 10000;
	g_log_cnf.save_bpu0_load_period_ms = 10000;
	g_log_cnf.save_bpu1_load_period_ms = 10000;
	g_log_cnf.save_junc_temp_period_ms = 10000;
	g_log_cnf.save_ddr_band_period_ms  = 10000;

	g_log_cnf.save_cpu_load_threshold  = -1;
	g_log_cnf.save_bpu0_load_threshold = -1;
	g_log_cnf.save_bpu1_load_threshold = -1;
	g_log_cnf.save_junc_temp_threshold = 115.0;
	g_log_cnf.save_ddr_band_threshold  = -1;


	g_log_cnf.bpu0_load_max = FLT_MIN;
	g_log_cnf.bpu0_load_min = FLT_MAX;

	g_log_cnf.bpu1_load_max = FLT_MIN;
	g_log_cnf.bpu1_load_min = FLT_MAX;

	g_log_cnf.cpu_load_max = FLT_MIN;
	g_log_cnf.cpu_load_min = FLT_MAX;

	g_log_cnf.junc_temp_max = FLT_MIN;
	g_log_cnf.junc_temp_min = FLT_MAX;

	g_file_cnf.cur_fd = 0;
	g_file_cnf.cur_file_len = 0;
	g_file_cnf.max_count = 100;
	g_file_cnf.file_max_len = 2*1024;
	g_file_cnf.cur_index = 0;
	g_file_cnf.cur_count = 0;

	printf("--cpu %d %d --bpu %d %d --temp %d %d --ddr %d %s "
					"--temp_thresh %.0f --file %d %d --sleep %d --level %d\n",
					g_log_cnf.query_cpu_load_period_ms, g_log_cnf.save_cpu_load_period_ms,
					g_log_cnf.query_bpu0_load_period_ms, g_log_cnf.save_bpu0_load_period_ms,
					g_log_cnf.query_junc_temp_period_ms, g_log_cnf.save_junc_temp_period_ms,
					g_log_cnf.save_ddr_band_period_ms,
					g_log_cnf.is_volume > 0 ? "--volume" : " ",
					g_log_cnf.save_junc_temp_threshold,
					g_file_cnf.max_count, g_file_cnf.file_max_len,
					g_log_cnf.sleep_period_ms, log_level);

	return 0;
}

static void print_help() {
	printf("Usage: ./chip_info [options]\n"//NOLINT
         " --cpu  a b   cpu load    a:period query time(ms) b:period save time(ms)  default 1000 10000\n"//NOLINT
		 " --bpu  a b   bpu0/1 load a:period query time(ms) b:period save time(ms)  default 1000 10000\n"//NOLINT
		 " --temp a b   junc temp   a:period query time(ms) b:period save time(ms)  default 1000 10000\n"//NOLINT
		 " --ddr  a     ddr band    a:period save time(ms)  default 10000, period query time is a constant value of 1000ms\n"//NOLINT
		 " --temp_thresh a a:temp save threshold default 115\n"//NOLINT
		 " --pwm a        a:period query time(ms) default 60000\n"//NOLINT
		 " --sleep a      a:sleep time(ms) default 1000\n"//NOLINT
		 " --volume       volume production version default test version \n"//NOLINT
		 " --level a      a:print level 1-4\n"//NOLINT
		 " --sched a b    a:priority b:0-SCHED_NORMAL 1-SCHED_FIFO 2-SCHED_RR default SCHED_NORMAL 0\n"//NOLINT
		 " --file  a b    a:file max num b:file length(KB) default 100 2048KB\n"//NOLINT
         " --help\n");
}

static int parse_cmd(int argc, char * argv[])
{
	int i = 0;

	for (i = 1; i < argc; i++) {
		printf("argc=%d, argv[%d]=%s**\n", argc, i, argv[i]);
		if (!strcmp(argv[i], "--cpu")) {
			i++;
			if ((i+1 < argc) && isdigitstr(argv[i])
				&& isdigitstr(argv[i+1])) {
				g_log_cnf.query_cpu_load_period_ms = atoi(argv[i]);
				g_log_cnf.save_cpu_load_period_ms  = atoi(argv[i+1]);
				// g_log_cnf.save_cpu_load_threshold  = atoi(argv[i+2]);
				i += 1;
			} else {
				printf("Not Invalid Params!!");
				print_help();
				return -1;
			}
			continue;
		} else if (!strcmp(argv[i], "--bpu")) {
			i++;
			if ((i+1 < argc) && isdigitstr(argv[i])
				&& isdigitstr(argv[i+1])) {
				g_log_cnf.query_bpu0_load_period_ms = atoi(argv[i]);
				g_log_cnf.save_bpu0_load_period_ms  = atoi(argv[i+1]);
				// g_log_cnf.save_bpu0_load_threshold  = atoi(argv[i+2]);
				i += 1;
			} else {
				printf("Not Invalid Params!!");
				print_help();
				return -1;
			}
			continue;
		} else if (!strcmp(argv[i], "--temp")) {
			i++;
			if ((i+1 < argc) && isdigitstr(argv[i])
				&& isdigitstr(argv[i+1])) {
				g_log_cnf.query_junc_temp_period_ms = atoi(argv[i]);
				g_log_cnf.save_junc_temp_period_ms  = atoi(argv[i+1]);
				// g_log_cnf.save_junc_temp_threshold  = atoi(argv[i+2]);
				i += 1;
			} else {
				printf("Not Invalid Params!!");
				print_help();
				return -1;
			}
			continue;
		} else if (!strcmp(argv[i], "--ddr")) {
			i++;
			if (i < argc && isdigitstr(argv[i])) {
				g_log_cnf.save_ddr_band_period_ms  = atoi(argv[i]);
			} else {
				printf("Not Invalid Params!!");
				print_help();
				return -1;
			}
			continue;
		} else if (!strcmp(argv[i], "--pwm")) {
			i++;
			if (i < argc && isdigitstr(argv[i])) {
				g_log_cnf.query_pwm_loss_period_ms = atoi(argv[i]);
			} else {
				printf("Not Invalid Params!!");
				print_help();
				return -1;
			}
			continue;
		} else if (!strcmp(argv[i], "--sleep")) {
			i++;
			if (i < argc && isdigitstr(argv[i])) {
				g_log_cnf.sleep_period_ms = atoi(argv[i]);
			} else {
				printf("Not Invalid Params!!");
				print_help();
				return -1;
			}
			continue;
		} else if (!strcmp(argv[i], "--temp_thresh")) {
			i++;
			if (i < argc && isdigitstr(argv[i])) {
				g_log_cnf.save_junc_temp_threshold = (float)(atoi(argv[i]) * 1.0);
			} else {
				printf("Not Invalid Params!!");
				print_help();
				return -1;
			}
			continue;
		} else if (!strcmp(argv[i], "--volume")) {
			g_log_cnf.is_volume = 1;
			continue;
		} else if (!strcmp(argv[i], "--level")) {
			i++;
			if (i < argc && isdigitstr(argv[i])) {
				log_level = atoi(argv[i]);
			} else {
				printf("Not Invalid Params!!");
				print_help();
				return -1;
			}
			continue;
		} else if (!strcmp(argv[i], "--sched")) {
			i++;
			if (i+1 < argc && isdigitstr(argv[i])
				&& isdigitstr(argv[i+1])) {
				g_log_cnf.priority = atoi(argv[i]);
				g_log_cnf.sched_police = atoi(argv[i+1]);
				i += 1;
			} else {
				printf("Not Invalid Params!!");
				print_help();
				return -1;
			}
			continue;
		}  else if (!strcmp(argv[i], "--file")) {
			i++;
			if (i+1 < argc && isdigitstr(argv[i])
				&& isdigitstr(argv[i+1])) {
				g_file_cnf.max_count = atoi(argv[i]);
				g_file_cnf.file_max_len = atoi(argv[i+1]);
				i += 1;
			} else {
				printf("Not Invalid Params!!");
				print_help();
				return -1;
			}
			continue;
		} else if (!strcmp(argv[i], "--help")) {
			print_help();
			return -1;
		} else {
			print_help();
			return -1;
		}
	}

	if (g_log_cnf.sleep_period_ms < 0) {
		printf("--sleep must be > 0 !\n");
		print_help();
		return -1;
	}

	/*  */
	if (g_log_cnf.query_cpu_load_period_ms < g_log_cnf.sleep_period_ms) {
		g_log_cnf.query_cpu_load_period_ms = g_log_cnf.sleep_period_ms;
	}

	if (g_log_cnf.save_cpu_load_period_ms < g_log_cnf.query_cpu_load_period_ms) {
		g_log_cnf.save_cpu_load_period_ms = g_log_cnf.query_cpu_load_period_ms;
	}

	/*  */
	if (g_log_cnf.query_bpu0_load_period_ms < g_log_cnf.sleep_period_ms) {
		g_log_cnf.query_bpu0_load_period_ms = g_log_cnf.sleep_period_ms;
	}

	if (g_log_cnf.save_bpu0_load_period_ms < g_log_cnf.query_bpu0_load_period_ms) {
		g_log_cnf.save_bpu0_load_period_ms  = g_log_cnf.query_bpu0_load_period_ms;
	}

	/*  */
	if (g_log_cnf.query_junc_temp_period_ms < g_log_cnf.sleep_period_ms) {
		g_log_cnf.query_junc_temp_period_ms = g_log_cnf.sleep_period_ms;
	}

	if (g_log_cnf.save_junc_temp_period_ms < g_log_cnf.query_junc_temp_period_ms) {
		g_log_cnf.save_junc_temp_period_ms = g_log_cnf.query_junc_temp_period_ms;
	}

	/*  */
	if (g_log_cnf.query_pwm_loss_period_ms < g_log_cnf.sleep_period_ms) {
		g_log_cnf.query_pwm_loss_period_ms = g_log_cnf.sleep_period_ms;
	}

	if (g_file_cnf.max_count < 0 || g_file_cnf.file_max_len < 0) {
		g_file_cnf.max_count = 0;
		g_file_cnf.file_max_len = 0;
	}

	g_file_cnf.file_max_len = g_file_cnf.file_max_len * 1024; /* KB->B */

	if (g_file_cnf.file_max_len < 0) {
		g_file_cnf.max_count = 0;
		g_file_cnf.file_max_len = 0;
	}

	/*
	if (g_file_cnf.max_count <=0 || g_file_cnf.max_count > 100) {
		g_file_cnf.max_count = 100;
	}

	if (g_file_cnf.file_max_len <=0
		|| g_file_cnf.max_count * g_file_cnf.file_max_len > 1024*1024*2*100) {
		g_file_cnf.file_max_len = 1024*1024*2;
	}
	*/

	g_log_cnf.query_bpu1_load_period_ms = g_log_cnf.query_bpu0_load_period_ms;
	g_log_cnf.save_bpu1_load_period_ms = g_log_cnf.save_bpu0_load_period_ms;
	g_log_cnf.save_bpu1_load_threshold = g_log_cnf.save_bpu0_load_threshold;

	printf("--cpu %d %d --bpu %d %d --temp %d %d --ddr %d %s "
					"--temp_thresh %.0f --file %d %d --sleep %d --level %d\n",
					g_log_cnf.query_cpu_load_period_ms,
					g_log_cnf.save_cpu_load_period_ms,
					g_log_cnf.query_bpu0_load_period_ms,
					g_log_cnf.save_bpu0_load_period_ms,
					g_log_cnf.query_junc_temp_period_ms,
					g_log_cnf.save_junc_temp_period_ms,
					g_log_cnf.save_ddr_band_period_ms,
					g_log_cnf.is_volume > 0 ? "--volume" : " ",
					g_log_cnf.save_junc_temp_threshold,
					g_file_cnf.max_count, g_file_cnf.file_max_len,
					g_log_cnf.sleep_period_ms, log_level);
	return 0;
}

static void signal_handle(int sig)
{
	printf("signal recv. exit\n");
	g_log_cnf.is_exit = 1;
}

int main(int argc, char * argv[])
{
	time_t time;
	struct tm *tm_ptr;
	char time_buf[128] = {0};
	struct sched_param param = {0};

	int ret = 0;
	int offset = 0;
	int time_len = 0;
	unsigned int loop_cnt = 0;
	int first_run_flag = 0;
	int time_sync_cnt = 0;
	int time_buf_len = 0;
	unsigned int temp_over_thres_cnt = 0;

	unsigned int query_cpu_load_cnt = 0;
	unsigned int query_bpu0_load_cnt = 0;
	unsigned int query_bpu1_load_cnt = 0;
	unsigned int query_junc_temp_cnt = 0;
	unsigned int query_ddr_band_cnt = 0;
	unsigned int query_pwm_loss_cnt = 0;

	unsigned int save_cpu_load_cnt = 0;
	unsigned int save_bpu0_load_cnt = 0;
	unsigned int save_bpu1_load_cnt = 0;
	unsigned int save_junc_temp_cnt = 0;
	unsigned int save_ddr_band_cnt = 0;


	unsigned int save_cpu_load_cnt_orig = 0;
	unsigned int save_bpu0_load_cnt_orig = 0;
	unsigned int save_bpu1_load_cnt_orig = 0;

	struct timespec mono_ts;
	struct timespec real_ts;

	set_default_value();

	ret = parse_cmd(argc, argv);
	if (ret < 0) {
		return -1;
	}

	ret = read_file_list(CHIP_LOG_PATH);
	if (ret < 0) {
		printf("read_file_list error\n");
		return -1;
	}

	query_cpu_load_cnt = g_log_cnf.query_cpu_load_period_ms / g_log_cnf.sleep_period_ms;//NOLINT
	query_bpu0_load_cnt = g_log_cnf.query_bpu0_load_period_ms / g_log_cnf.sleep_period_ms;//NOLINT
	query_bpu1_load_cnt = g_log_cnf.query_bpu1_load_period_ms / g_log_cnf.sleep_period_ms;//NOLINT
	query_junc_temp_cnt = g_log_cnf.query_junc_temp_period_ms / g_log_cnf.sleep_period_ms;//NOLINT
	query_pwm_loss_cnt = g_log_cnf.query_pwm_loss_period_ms / g_log_cnf.sleep_period_ms;//NOLINT

	save_cpu_load_cnt = g_log_cnf.save_cpu_load_period_ms / g_log_cnf.query_cpu_load_period_ms * query_cpu_load_cnt;//NOLINT
	save_bpu0_load_cnt = g_log_cnf.save_bpu0_load_period_ms / g_log_cnf.query_bpu0_load_period_ms * query_bpu0_load_cnt;//NOLINT
	save_bpu1_load_cnt = g_log_cnf.save_bpu1_load_period_ms / g_log_cnf.query_bpu1_load_period_ms * query_bpu1_load_cnt;//NOLINT
	save_junc_temp_cnt = g_log_cnf.save_junc_temp_period_ms / g_log_cnf.query_junc_temp_period_ms * query_junc_temp_cnt;//NOLINT

	save_cpu_load_cnt_orig  = save_cpu_load_cnt;
	save_bpu0_load_cnt_orig = save_bpu0_load_cnt;
	save_bpu1_load_cnt_orig = save_bpu1_load_cnt;

	pr_debug(LOG_INF, "cpu_cnt[%d,%d] bpu0_cnt[%d,%d] bpu1_cnt[%d,%d] "
					"temp_cnt[%d,%d] ddr_cnt[%d,%d]\n",
					query_cpu_load_cnt, save_cpu_load_cnt,
					query_bpu0_load_cnt, save_bpu0_load_cnt,
					query_bpu1_load_cnt, save_bpu1_load_cnt,
					query_junc_temp_cnt, save_junc_temp_cnt,
					query_ddr_band_cnt, save_ddr_band_cnt);

	ret = msg_queue_init(&g_file_queue, 10);

	if (ret < 0) {
		pr_debug(LOG_ERR, "msg_queue_init error\n");
		return -1;
	}

	/* time year >2020 || wait 1MIN */
	while (time_sync_cnt < TIME_SYNC_WAIT_MAX_CNT) {
		clock_gettime(CLOCK_REALTIME, &real_ts);
		time = real_ts.tv_sec;
		tm_ptr = localtime(&time);//NOLINT
		if (tm_ptr->tm_year + 1900 > TIME_CURR_MIN_YEAY) {
			break;
		}

		sleep(1);
		time_sync_cnt++;

		memset(time_buf, 0, sizeof(time_buf));
		strftime(time_buf, 120, "%Y-%m-%d %H:%M:%S", tm_ptr);
		pr_debug(LOG_DBG, "wait time %d > 2020. time_sync_cnt=%d, now %s\n",
					tm_ptr->tm_year+1900,
					time_sync_cnt,
					time_buf);
	}

	ret = get_chip_id(soc_id_buf);
        if (ret <= 0) {
                printf("get_chip_id error\n");
        }

        ret = get_efuse_cnt(efuse_cnt_buf);
        if (ret <= 0) {
                printf("get_efuse_cnt error\n");
        }

	if (pthread_create(&g_ddr_monitor_thread_id,
							NULL, ddr_monitor_thread, NULL) < 0) {
		pr_debug(LOG_ERR, "pthread_create error\n");
		return -1;
	}

	if (pthread_create(&g_save_file_thread_id,
							NULL, save_file_thread, NULL) < 0) {
		pr_debug(LOG_ERR, "pthread_create error\n");
		return -1;
	}

	if (1) {
		param.sched_priority = g_log_cnf.priority;
        ret = sched_setscheduler((int)syscall(SYS_gettid),
										g_log_cnf.sched_police, &param);
		if (ret) {
			printf("sched_setscheduler error! "
					"sched_method=%d, sched_priority=%d, ret=%d\n",
					g_log_cnf.sched_police, g_log_cnf.priority, ret);
		}
	}

	signal(SIGINT, signal_handle);

	while (!g_log_cnf.is_exit) {
		clock_gettime(CLOCK_MONOTONIC, &mono_ts);
		clock_gettime(CLOCK_REALTIME, &real_ts);

		time = real_ts.tv_sec;
		tm_ptr = localtime(&time);//NOLINT

		memset(time_buf, 0, sizeof(time_buf));
		time_buf_len = (int)strftime(time_buf, 120, "%Y-%m-%d %H:%M:%S", tm_ptr);
		pr_debug(LOG_DBG, "time_buf_len=%d\n", time_buf_len);

		offset += sprintf(all_buf+offset, "%s realtime:[%ld,%ld] "//NOLINT
							"monotime:[%ld,%ld]\n",
							time_buf, real_ts.tv_sec, real_ts.tv_nsec,
							mono_ts.tv_sec, mono_ts.tv_nsec);
		time_len = offset;

		if (first_run_flag == 0) {
			first_run_flag = 1;
			offset += sprintf(all_buf+offset, "chip_log start running\n");//NOLINT
		}

		/* */
		if (g_log_cnf.is_volume) {
			if (g_log_cnf.junc_temp_cur >= g_log_cnf.save_junc_temp_threshold) {
				/* first Start recording query cycle */
				if (temp_over_thres_cnt == 0) {
					// save_junc_temp_cnt = query_junc_temp_cnt;
					save_bpu0_load_cnt = query_bpu0_load_cnt;
					save_bpu1_load_cnt = query_bpu1_load_cnt;
					save_cpu_load_cnt  = query_cpu_load_cnt;
				}
				temp_over_thres_cnt++;
				/* second recording save cycle/2 */
				/*
				if (temp_over_thres_cnt == save_junc_temp_cnt_orig) {
					save_junc_temp_cnt = save_junc_temp_cnt_orig / 2;
				}*/

				if (temp_over_thres_cnt == save_bpu0_load_cnt_orig) {
					save_bpu0_load_cnt = save_bpu0_load_cnt_orig / 2;
				}

				if (temp_over_thres_cnt == save_bpu1_load_cnt_orig) {
					save_bpu1_load_cnt = save_bpu1_load_cnt_orig / 2;
				}

				if (temp_over_thres_cnt == save_cpu_load_cnt_orig) {
					save_cpu_load_cnt  = save_cpu_load_cnt_orig / 2;
				}

				pr_debug(LOG_DBG, "high temperature! cur_temp=%.0f,"
							" temp_threshold=%.0f, cnt=%d\n",
							g_log_cnf.junc_temp_cur, g_log_cnf.save_junc_temp_threshold,
							temp_over_thres_cnt);

			} else {
				/* restore to the original value */
				temp_over_thres_cnt = 0;
				// save_junc_temp_cnt = save_junc_temp_cnt_orig;
				save_bpu0_load_cnt = INT_MAX;
				save_bpu1_load_cnt = INT_MAX;
				save_cpu_load_cnt  = INT_MAX;

				pr_debug(LOG_DBG, "normal temperature! cur_temp=%.0f,"
							" temp_threshold=%.0f, cnt=%d\n",
							g_log_cnf.junc_temp_cur,
							g_log_cnf.save_junc_temp_threshold,
							temp_over_thres_cnt);
			}
			/*  */
		}

		pr_debug(LOG_DBG, "cpu_cnt[%d,%d] bpu0_cnt[%d,%d] bpu1_cnt[%d,%d] "
						"temp_cnt[%d,%d] ddr_cnt[%d,%d] pwm[%d]\n",
						query_cpu_load_cnt, save_cpu_load_cnt,
						query_bpu0_load_cnt, save_bpu0_load_cnt,
						query_bpu1_load_cnt, save_bpu1_load_cnt,
						query_junc_temp_cnt, save_junc_temp_cnt,
						query_ddr_band_cnt, save_ddr_band_cnt,
						query_pwm_loss_cnt);

		if (loop_cnt % query_junc_temp_cnt == 0) {
			pr_debug(LOG_DBG, "get_junction_temp bgn");
			ret = get_junction_temp(junc_temp_buf);
			if (ret >= 0) {
				/* period save data */
				if ((loop_cnt % save_junc_temp_cnt) == 0) {
					pr_debug(LOG_DBG, "gener_junction_temp_buf bgn");
					ret = gener_junction_temp_buf(junc_temp_buf);
					if (ret > 0 && ret <= DEFAULT_NUMBER_STR_MAX_LENGTH) {
						offset += sprintf(all_buf+offset, "%s", junc_temp_buf);//NOLINT
					}
					memset(junc_temp_buf, 0, sizeof(junc_temp_buf));
				}
			}
		}

		if (loop_cnt % query_pwm_loss_cnt == 0) {
			pr_debug(LOG_DBG, "get_pwm_loss bgn");
			ret = get_pwm_loss(pwm_loss_buf);
			if (ret >= 0) {
				pr_debug(LOG_DBG, "gener_pwm_loss_buf bgn");
				ret = gener_pwm_loss_buf(pwm_loss_buf);
				if (ret > 0 && ret <= DEFAULT_NUMBER_STR_MAX_LENGTH) {
					offset += sprintf(all_buf+offset, "%s", pwm_loss_buf);//NOLINT
				}
				memset(pwm_loss_buf, 0, sizeof(pwm_loss_buf));
			}
		}

		if (loop_cnt % query_bpu0_load_cnt == 0) {
			pr_debug(LOG_DBG, "get_bpu0_load bgn");
			ret = get_bpu0_load(bpu0_load_buf);
			if (ret >= 0) {
				/* period save data */
				if ((loop_cnt % save_bpu0_load_cnt) == 0) {
					pr_debug(LOG_DBG, "gener_bpu0_load_buf bgn");
					ret = gener_bpu0_load_buf(bpu0_load_buf);
					if (ret > 0 && ret <= DEFAULT_NUMBER_STR_MAX_LENGTH) {
						offset += sprintf(all_buf+offset, "%s", bpu0_load_buf);//NOLINT
					}
					memset(bpu0_load_buf, 0, sizeof(bpu0_load_buf));
				}
			}
		}

		if (loop_cnt % query_bpu1_load_cnt == 0) {
			pr_debug(LOG_DBG, "get_bpu1_load bgn");
			ret = get_bpu1_load(bpu1_load_buf);
			if (ret >= 0) {
				/* period save data */
				if ((loop_cnt % save_bpu1_load_cnt) == 0) {
					pr_debug(LOG_DBG, "gener_bpu1_load_buf bgn");
					ret = gener_bpu1_load_buf(bpu1_load_buf);
					if (ret > 0 && ret <= DEFAULT_NUMBER_STR_MAX_LENGTH) {
						offset += sprintf(all_buf+offset, "%s", bpu1_load_buf);//NOLINT
					}
					memset(bpu1_load_buf, 0, sizeof(bpu1_load_buf));
				}
			}
		}

		if (loop_cnt % query_cpu_load_cnt == 0) {
			pr_debug(LOG_DBG, "get_cpu_load bgn");
			ret = get_cpu_load(cpu_load_buf);
			if (ret >= 0) {
				/* period save data */
				if((loop_cnt % save_cpu_load_cnt) == 0) {
					pr_debug(LOG_DBG, "gener_cpu_load_buf bgn");
					ret = gener_cpu_load_buf(cpu_load_buf);
					if (ret > 0 && ret < 512) {
						offset += sprintf(all_buf+offset, "%s", cpu_load_buf);//NOLINT
					}
					memset(cpu_load_buf, 0, sizeof(cpu_load_buf));
				}
			}
		}

		if (offset != time_len) {
			pr_debug(LOG_DBG, "offset=%d time_len=%d\n", offset, time_len);
			ret = msg_enqueue(&g_file_queue, 0, offset, all_buf);
			if (ret != 0) {
				pr_debug(LOG_ERR, "msg_enqueue error, length=%d, ret=%d\n", offset, ret);
			}
		}

		offset = 0;
		loop_cnt++;
		memset(all_buf, 0 , sizeof(all_buf));

		usleep(g_log_cnf.sleep_period_ms * 1000);
	}

	pr_debug(LOG_INF, "main exit now!\n");
	pthread_join(g_ddr_monitor_thread_id, NULL);
	pthread_join(g_save_file_thread_id, NULL);
	pr_debug(LOG_INF, "main exit success!\n");

	return 0;
}
