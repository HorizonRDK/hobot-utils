/*
 *    COPYRIGHT NOTICE
 *    Copyright 2020 Horizon Robotics, Inc.
 *    All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>

#include "dyn_debug.h"

#define SECTOMS (1000)
#define SECTOUS (1000000)
#define SECTONS (1000000000)
#define MSTONS  (1000000)
#define MAX_SEM_WAIT_TIMEOUT    ((long long)(~0ULL>>1))

sem_t *s_client_sem = NULL;
sem_t *s_server_w_sem = NULL;
sem_t *s_server_r_sem = NULL;
int s_share_fd;
void *s_share_mem = NULL;
int client_id[MAX_CLIENT];
int client_num = 0;
dyn_client_data_cb g_data_cb = NULL;

static int dyn_debug_get_users(void);

static int64_t get_monotime_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return (ts.tv_sec * SECTOMS) + (ts.tv_nsec / SECTOUS);
}


static int sem_timedwait_realtime(sem_t *sem, int64_t msecs)
{
	struct timespec ts;
	uint64_t add = 0;
	uint64_t secs = msecs/1000;

	clock_gettime(CLOCK_REALTIME, &ts);
	msecs = msecs % 1000;

	msecs = msecs * 1000 * 1000 + ts.tv_nsec;
	add = msecs / (1000 * 1000 * 1000);
	ts.tv_sec += (add + secs);
	ts.tv_nsec = msecs % (1000 * 1000 * 1000);

	return sem_timedwait(sem, &ts);
}

static int32_t sem_timedwait_msecs_retry(sem_t *sem, int64_t msecs)
{
	int64_t in_monotime, mid_monotime;
	int64_t tmp_timeout;
	int32_t ret = -1;

	tmp_timeout = msecs;
	while (tmp_timeout > 0) {
		in_monotime = get_monotime_ms();
		ret = sem_timedwait_realtime(sem, tmp_timeout);
		if (!ret) { /* success */
			return ret;
		} else {
			/* only ETIMEDOUT need to retry sem_wait */
			if (errno == ETIMEDOUT) {
				mid_monotime = get_monotime_ms();
				if ((mid_monotime - in_monotime) < tmp_timeout) {
					tmp_timeout -= mid_monotime - in_monotime;
				} else { /* time passed return */
					break;
				}
			} else { /* other errno return */
				break;
			}
		}
	}

	return ret;
}

static int32_t sem_timedwait_msecs(sem_t *sem, int64_t msecs)
{
	int32_t ret = -1;

	if (sem == NULL) {
		return -1;
	}

	if (msecs == MAX_SEM_WAIT_TIMEOUT) { /* max time */
		return sem_wait(sem);
	} else if (msecs > 0) {
		return sem_timedwait_msecs_retry(sem, msecs);
	} else {
		return sem_trywait(sem);
	}

	return ret;
}

int dyn_debug_server_init(const char *name, dyn_client_data_cb data_cb)
{
	char share_name[64];
	int users;

    snprintf(share_name, 64, "/%s", name);

	s_share_fd = shm_open(share_name,
			O_RDWR | O_CREAT | O_EXCL, 0777);
	if(s_share_fd < 0){
		s_share_fd = shm_open(share_name, O_RDWR, 0777);
	}else
		ftruncate(s_share_fd, MAX_MSG_SIZE);

	if (s_share_fd < 0) {
		printf("dyn debug already started\n");
		return 0;
	}

	s_share_mem = mmap(NULL, MAX_MSG_SIZE,
			PROT_READ | PROT_WRITE, MAP_SHARED,
			s_share_fd, 0);

    snprintf(share_name, 64, "/%s-w", name);
    s_server_w_sem = sem_open(share_name, O_CREAT, 0666, 0);
	if (s_server_w_sem == SEM_FAILED) {
		printf("dyn debug debug sem error\n");
		return -1;
	}

    snprintf(share_name, 64, "/%s-r", name);
    s_server_r_sem = sem_open(share_name, O_CREAT, 0666, 0);
	if (s_server_r_sem == SEM_FAILED) {
		sem_close(s_server_w_sem);
		printf("dyn debug debug sem error\n");
		return -1;
	}

    snprintf(share_name, 64, "/%s-client", name);
    s_client_sem = sem_open(share_name, O_CREAT, 0666, 0);
	if (s_client_sem == SEM_FAILED) {
		sem_close(s_server_r_sem);
		sem_close(s_server_w_sem);
		printf("dyn debug debug sem error\n");
		return -1;
	}

	users = dyn_debug_get_users();
	if (users <= 0) {
		printf("no user client have\n");
		return -1;
	}

	client_num = users;
	g_data_cb = data_cb;

	return users;
}

void dyn_debug_server_exit()
{
	if (s_client_sem) {
		sem_close(s_client_sem);
		s_client_sem = NULL;
	}

	if (s_server_r_sem) {
		sem_close(s_server_r_sem);
		s_server_r_sem = NULL;
	}

	if (s_server_w_sem) {
		sem_close(s_server_w_sem);
		s_server_w_sem = NULL;
	}

	close(s_share_fd);
}

static int dyn_debug_get_users(void)
{
	struct debug_msg_head *tmp_msg;
	int ret;
	int tmp_pid;
	int i = 0, j = 0, k = 0;
	int m = 0;

	while(j < 20) {
		tmp_msg = (struct debug_msg_head *)s_share_mem;
		tmp_msg->role = 0;
		tmp_msg->to = ALL_USER;
		tmp_msg->cmd = INIT_CMD;
		sem_post(s_server_r_sem);
		sem_post(s_server_w_sem);

		ret = sem_timedwait_msecs(s_client_sem, 50);
		if (ret < 0) {
			j++;
			continue;
		}
		tmp_msg = (struct debug_msg_head *)s_share_mem;
		if ((tmp_msg->to != 0) && (tmp_msg->cmd != INIT_CLIENT_CMD)) {
			usleep(5000);
			continue;
		}
		tmp_pid = tmp_msg->role;

		for(k = 0; k < m; k++) {
			if (client_id[k] == tmp_pid)
				break;
		}

		if (k == m) {
			client_id[m] = tmp_pid;
			m++;
		}

		sem_post(s_server_w_sem);
		sem_post(s_server_r_sem);
		i++;
	};

	return m;
}

int dyn_debug_server_client_cmd(int pid, int cmd, int arg)
{
	struct debug_msg_head *tmp_msg;
	char *msg_mem = NULL;
	int msg_pos = 0;
	int i, ret;

	i = 0;
	do {
		tmp_msg = (struct debug_msg_head *)s_share_mem;
		tmp_msg->role = 0;
		tmp_msg->to = pid;
		tmp_msg->cmd = cmd;
		tmp_msg->follow_msgs = 0;
		sem_post(s_server_r_sem);
		sem_post(s_server_w_sem);

		ret = sem_timedwait_msecs(s_client_sem, 500);
		if (ret < 0) {
			i = 20;
			break;
		}
		tmp_msg = (struct debug_msg_head *)s_share_mem;
		if (tmp_msg->to != 0) {
			usleep(5000);
			continue;
		} else {
			break;
		}

		i++;

	} while(i < 20);

	if (i >= 20) {
		printf("client[%d] response cmd[%d] error\n", pid, cmd);
		return -1;
	}

	if ((tmp_msg->cmd == REPLY_MSG) && tmp_msg->lenght) {
		msg_mem = s_share_mem;
		msg_pos = 0;
		do {
			tmp_msg = (struct debug_msg_head *)(msg_mem + msg_pos);
			if (tmp_msg->lenght) {
				*((char *)(tmp_msg)
						+ sizeof(struct debug_msg_head)
						+ tmp_msg->lenght) = '\0';
				printf("%s\n", (const char *)(tmp_msg)
						+ sizeof(struct debug_msg_head));
			}
			msg_pos += tmp_msg->lenght + (uint32_t)sizeof(struct debug_msg_head);
		} while (tmp_msg->follow_msgs);
	} else if ((tmp_msg->cmd == REPLY_RAW) && tmp_msg->lenght) {
		if (g_data_cb)
			g_data_cb((char *)tmp_msg + sizeof(struct debug_msg_head),
					tmp_msg->lenght);
	}

	return 0;
}

int dyn_debug_server_print(int cmd, int arg)
{
	int i;

	if (client_num <= 0) {
		printf("no clients\n");
		return -1;
	}

	for (i = 0; i < client_num; i++) {
		dyn_debug_server_client_cmd(client_id[i], cmd, arg);
	}

	return 0;
}
