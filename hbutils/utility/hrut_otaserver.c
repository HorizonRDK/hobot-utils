/*
 *    COPYRIGHT NOTICE
 *    Copyright 2020 Horizon Robotics, Inc.
 *    All rights reserved.
 */
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <math.h>
#include <stdint.h>
#include <errno.h>

#define ARGC 10
#define READ_BUFF 4096
#define PAGE_SIZE 8192
#define READ_BLOCK_MAX PAGE_SIZE
#define CMD_RUN 16
// static int isOver = 0;
#define PORT 51000
#define IPSTR htons(INADDR_ANY)
#define LIS_MAX 5

pthread_mutex_t mutex;
/**** Supported commands *****/
static char allow_cmds[][16] = {"ls\0",    "df\0",    "cat\0",
                                "hrut\0",  "tftp\0",  "md5sum\0",
                                "./mnt\0", "touch\0", "otaupdate\0"};

int create_socket();
static int command_illegal(char* cmd);
void send_file(int c, char* name);
void recv_file(int sockfd, char* name);
void run_cmd(int sockfd, char* cmd);
void* work_thread(void* arg);
int thread_start(int64_t c);
ssize_t send_message(int sockfd, const void *buf, size_t len, int flags);

/***** Check the legitimacy of the order *****/
static int command_illegal(char* cmd) {
    int i;
    for (i = 0; i < sizeof(allow_cmds) / 16; i++) {
        if (!strncmp(cmd, allow_cmds[i], strlen(allow_cmds[i]))) {
            return 0;
        }
    }
    return 1;
}

int create_socket() {
    int sockfd;
    int opt = 1;
    int res;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        return -1;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    // saddr.sin_addr.s_addr = inet_addr(IPSTR);
    saddr.sin_addr.s_addr = IPSTR;

    res = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&opt,
               sizeof(opt));
    if (res == -1) {
        close(sockfd);
        return -1;
    }
    res = bind(sockfd, (struct sockaddr*)&saddr, sizeof(saddr));
    if (res == -1) {
        close(sockfd);
        return -1;
    }

    listen(sockfd, LIS_MAX);

    return sockfd;
}

ssize_t send_message(int sockfd, const void *buf, size_t len, int flags) {
    ssize_t ret;
    ret = send(sockfd, buf, len, flags);
    if (ret < 0)
        printf("send faild: %s\n", strerror(errno));
    return ret;
}


void send_file(int c, char* name) {
    ssize_t ret = 0;
    int fd = 0;
    off_t size = 0;
    size_t num = 0;
    char cli_status[64] = {0};
    char res_buff[128] = {0};
    char sendbuff[PAGE_SIZE] = {0};
    if (name == NULL) {
        (void)send_message(c, "err#no name", 11, 0);
        return;
    }
    fd = open(name, O_RDONLY);
    printf("@@ server The send_file called!\n");
    if (fd == -1) {
        (void)send_message(c, "err", 3, 0);
        return;
    }

    size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    printf("file size:%ld\n", size);
    snprintf(res_buff, sizeof(res_buff), "ok#%ld", size);
    printf("@@ server step2");
    if ((ret = send_message(c, res_buff, strlen(res_buff), 0)) < 0) {
        close(fd);
        return;
    }
    if (recv(c, cli_status, 63, 0) <= 0) {
        close(fd);
        return;
    }

    if (strncmp(cli_status, "ok", 2) != 0) {
        close(fd);
        return;
    }

    while ((num = read(fd, sendbuff, PAGE_SIZE)) > 0) {
        if ((ret = send_message(c, sendbuff, num, 0)) < 0) {
            close(fd);
            return;
        }
    }
    printf("file size:%ld\n", size);
    close(fd);
    printf("file download succeed!\n");
    return;
}

void recv_file(int sockfd, char* name) {
    char buff[128] = {0};
    char recvbuff[PAGE_SIZE] = {0};
    int fd, size = 0;
    ssize_t num, ret = 0;
    ssize_t curr_size = 0;
    printf("@@@ The recv_file execute!\n");
    if (recv(sockfd, buff, 127, 0) <= 0) {
        return;
    }

    printf("@@==> buff=%s\n", buff);
    if (strncmp(buff, "ok#", 3) != 0) {
        printf("Error:%s\n", buff + 3);
        return;
    }

    printf("file size:%s\n", buff + 3);
    sscanf(buff + 3, "%d", &size);

    if (size == 0) {
        (void)send_message(sockfd, "err", 3, 0);
        return;
    }
    fd = open(name, O_WRONLY | O_CREAT, 0755);
    if (fd == -1) {
        (void)send_message(sockfd, "err", 3, 0);
        return;
    }
    if ((ret = send_message(sockfd, "ok", 2, 0)) < 0) {
        close(fd);
        return;
    }
    while ((num = recv(sockfd, recvbuff, PAGE_SIZE, 0)) > 0) {
        if (write(fd, recvbuff, num) < 0) {
            printf("write faild: %s\n", strerror(errno));
            close(fd);
            return;
        }
        curr_size += num;
        fflush(stdout);
        if (curr_size >= size) {
            // break;
        }
    }
    printf("file upload succeed!\n");
    close(fd);
    return;
}

void run_cmd(int sockfd, char* cmd) {
    FILE* pp = NULL;
    char* ptr = NULL;
    char* sptr = NULL;
    char *pp_char = NULL;
    ssize_t ret;
    printf("@@@@ cmd=%s\n", cmd);
    sptr = strtok_r(cmd, " ", &ptr);
    printf("#### ptr=%s\n", ptr);
    printf("&&&& sptr=%s\n", sptr);
    char temp_buf[PAGE_SIZE] = {0};
    memset(temp_buf, 0, sizeof(temp_buf));
    printf("@@ The cmd-->ptr is %s\n", ptr);

    // Determine if the command is available
    if (command_illegal(ptr) == 1) {
        printf("  @@The tool does not currently support this command");
        (void)send_message(sockfd, "Illegal_order", 13, 0);
        return;
    } else if (command_illegal(ptr) == 0) {
        if ((ret = send_message(sockfd, "legal_order_t", 13, 0)) < 0)
            return;
    }

    snprintf(temp_buf, PAGE_SIZE, "%s 2>&1", ptr);
    printf("popen:%s\n", temp_buf);
    pp = popen(temp_buf, "r");
    if (!pp) {
        printf("popen fail\n");
        (void)send_message(sockfd, "err", 3, 0);
        goto err0;
    } else {
        printf("popen success\n");
        if ((ret = send_message(sockfd, "_ok", 3, 0)) < 0)
            goto err1;
    }
    if ((send_message(sockfd, "begin", 5, 0)) < 0) {
        goto err1;
    }
    pp_char = malloc(sizeof(char) * READ_BLOCK_MAX);
    if (!pp_char) {
        goto err1;
    }

    memset(pp_char, 0, READ_BLOCK_MAX);
    printf("^^^ server begin send_run data!\n");
    while ((fgets(pp_char, READ_BLOCK_MAX, pp)) != NULL) {
        if ((ret = send_message(sockfd, pp_char, READ_BLOCK_MAX, 0)) < 0) {
            free(pp_char);
            goto err1;
        }
        memset(pp_char, 0, sizeof(char) * READ_BLOCK_MAX);
    }

    if (pp_char) {
        free(pp_char);
        pp_char = NULL;
    }

    if ((ret = send_message(sockfd, "end", 3, 0)) < 0) {
        free(pp_char);
        goto err1;
    }
    printf("run cmd succeed \n");
err1:
    pp_char = NULL;
    pclose(pp);
err0:
    return;
}

void* work_thread(void* arg) {
    int c = (int)(*(int *)arg);
    char buff[256] = {0};
    char* myargv[ARGC] = {0};
    char* ptr = NULL, *cmd = NULL, *s = NULL;
    char run_buff[256] = {0};
    ssize_t n = 0, i = 0;
    ssize_t ret;
    while (1) {
        memset(buff, 0, sizeof(buff));
	    n = recv(c, buff, 256, 0);
        printf("$$$ The work_thread request cmd %s\n", buff);
        if (n <= 0) {
            printf("one client over\n");
            break;
        }
        memcpy(run_buff, buff, sizeof(buff));
        s = strtok_r(buff, " ", &ptr);
        while (s != NULL) {
            myargv[i++] = s;
            s = strtok_r(NULL, " ", &ptr);
        }
        cmd = myargv[0];  // cmd
        if (cmd == NULL) {
            if ((ret = send_message(c, "err", 3, 0)) < 0)
                return NULL;
            continue;
        }

        if (strcmp(cmd, "get") == 0) {
            send_file(c, myargv[1]);
        } else if (strcmp(cmd, "put") == 0) {
            printf("@@@ The upload execute! \n");
            recv_file(c, myargv[1]);
        } else if (strcmp(cmd, "run") == 0) {
            run_cmd(c, run_buff);
        } else {
            int pipefd[2];
            pipe(pipefd);

            pid_t pid = fork();
            if (pid == -1) {
                if ((ret = send_message(c, "err", 3, 0)) < 0)
                    return NULL;
                continue;
            }

            if (pid == 0) {
                dup2(pipefd[1], 1);
                dup2(pipefd[1], 2);

                execvp(cmd, myargv);
                perror("cmd err");
                exit(0);
            }

            close(pipefd[1]);
            // wait child process
            wait(NULL);
            char readbuff[READ_BUFF] = {"ok#"};
            if ((ret = read(pipefd[0], readbuff + 3, READ_BUFF - 4)) < 0) {
                close(pipefd[0]);
                return NULL;
            }
            if ((ret = send_message(c, readbuff, strlen(readbuff), 0)) < 0)
                return NULL;
        }
    }
    close(c);
    return NULL;
}

int thread_start(int64_t c) {
    pthread_t id;
    int res = pthread_create(&id, NULL, work_thread, (void *) c);
    if (res != 0)
        return -1;
    return 0;
}

int main(void) {
    int sockfd = create_socket();
    while (1) {
        struct sockaddr_in caddr;
        int len = sizeof(caddr);
        int c = accept(sockfd, (struct sockaddr*)&caddr, (socklen_t *)&len);
        if (c < 0 || !(fabs(c - (int)c) < 1e-8))
            continue;
        printf("accept c =%d\n", c);
        int res = thread_start(c);
        if (res == -1)
            close(c);
    }
    return 0;
}
