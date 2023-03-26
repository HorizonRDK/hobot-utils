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
#include <stdint.h>
#include <dirent.h>
#include <linux/fs.h>
#include <sys/mman.h>
#include <sys/types.h>
#include<sys/stat.h>
#include <errno.h>
#include <getopt.h>
#include <ctype.h>
#include "cJSON.h"

#define GPT_ENTRY_MAX 128
#define NAME_LEN_BYTES 72
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) sizeof(x) / sizeof(x[0])
#endif
#define BPU_LOAD_ADDR 0x28000000
#define TMP_BUF_SIZE  65536
#define MAX_HBM   4
#define BPU_ADDR_START 0x2c940000
#define BPU_PARTITION_NAME "bpu"
char *json_file = NULL;
int sig_flag = -1;
int hbm_addr = -1;

/*
* Struct created to handle gpt_entry
*/
struct gpt_entry {
    uint64_t part_type_guid_low;
    uint64_t part_type_guid_high;
    uint64_t uuid_low;
    uint64_t uuid_high;
    uint64_t start_lba;
    uint64_t end_lba;
    uint64_t attrs;
    uint16_t part_name[NAME_LEN_BYTES * sizeof(char) / sizeof(uint16_t)];
};

typedef struct hbm_json {
	uint32_t inst_size;
	uint32_t inst_offset;
	uint32_t par_size;
	uint32_t par_offset;
	uint32_t hbm_size;
	char hbm_name[100];
} hbm_json_t;

typedef struct hbm_info {
	char name[100];
	uint32_t hbm_size;
	uint32_t inst_load_addr;
	uint32_t inst_load_size;
	uint32_t par_load_addr;
	uint32_t par_load_size;
	uint32_t inst_data_offset;
	uint32_t par_data_offset;
	uint32_t signature_offset;
	uint32_t hbm_start_offset;
	uint32_t sig_flag;
}hbm_info_t;

typedef struct {
	hbm_info_t 					hbm[4];
	uint32_t					image_len;
	uint32_t					bpu_range_start;
	uint32_t					bpu_range_sz;
	uint32_t					bpu_all_fetch_only_sz;
} bpu_img_header;

typedef struct mbr_info {
	uint32_t len;
	uint32_t csum;
}mbr_info_t;

int8_t tmp_buf[TMP_BUF_SIZE];

/*
* Convert str from gpt part_name to normal string
*/
static void convert_partname(uint16_t *str, char *result) {
        unsigned label_count = 0;
        /* Naively convert UTF16-LE to 7 bits. */
        while (label_count < 36) {
                uint8_t c = (uint8_t)(str[label_count] & 0xff);
                if (c && !isprint(c))
                        c = '!';
                result[label_count] = c;
                label_count++;
        }
}

int32_t get_partition_id(const char* part_name) {
        int32_t ret;
        size_t ret_len;
        char *gpt_dev = "/dev/mmcblk0";
        char name_translated[GPT_ENTRY_MAX][NAME_LEN_BYTES] = { 0 };
	struct gpt_entry gpt_entries[GPT_ENTRY_MAX] = { 0 };
        FILE *gpt_on_disk = fopen(gpt_dev, "r");

        if (!part_name) {
                printf("part_name is NULL!\n");
                fclose(gpt_on_disk);
                return -1;
        } else if (!gpt_on_disk) {
                printf("mmc not found!\n");
                return -1;
        }

        /* Read gpt from mmc */
        /* first lba is mbr, second is gpt header, total 1024 bytes */
        fseek(gpt_on_disk, 1024, SEEK_SET);
        ret_len = fread(&gpt_entries, sizeof(struct gpt_entry),
                                 GPT_ENTRY_MAX, gpt_on_disk);
        if (ret_len != GPT_ENTRY_MAX) {
                printf("read short: %lu\n", ret_len);

                if (gpt_on_disk)
                        fclose(gpt_on_disk);

                return -1;
        }
        ret = -1;
        for (int i = 0; i < ARRAY_SIZE(gpt_entries); i++) {
                convert_partname(gpt_entries[i].part_name, name_translated[i]);
                if (!strcmp(part_name, name_translated[i])) {
                        ret = (i + 1);
                        break;
                }
        }
        if (gpt_on_disk)
                fclose(gpt_on_disk);

        return ret;
}


static int32_t platform_get_hbm_number()
{
	int fd;
	int cnt = 0;
	int i;
	ssize_t ret;
	size_t ret_len;
	int32_t partid = 0;
	char part_name[100] = {0};
	bpu_img_header tmp_header;

	partid = get_partition_id(BPU_PARTITION_NAME);
	if (partid < 0) {
		printf("get bpu partiton id failed\n");
		return -1;
	}
	snprintf(part_name, sizeof(part_name), "/dev/mmcblk0p%d", partid);
	fd = open(part_name, O_RDONLY, 444);
	if (fd < 0) {
	      printf("%s:%d open %s failed\n",  __func__, __LINE__, part_name);
	      return -1;
	}
	ret = lseek(fd, 0, SEEK_SET);
	if (ret < 0) {
		printf("lseek bpu image failed\n");
		close(fd);
		return -1;
	}
	ret = read(fd, &tmp_header, sizeof(bpu_img_header));
	if (ret < sizeof(bpu_img_header)) {
		printf("%s:%d read bpu image header error\n", __func__, __LINE__);
		close(fd);
		return -1;
	}
	for (i = 0; i < MAX_HBM; i++) {
		ret_len = strlen(tmp_header.hbm[i].name);
		if (ret_len)
			cnt++;
	}
	close(fd);
	return cnt;
}

static int32_t get_hbm_json(const char *path, hbm_json_t * hbm_info)
{
	int8_t *filebuf = NULL;
	cJSON *inst = NULL;
	cJSON * para = NULL;
	cJSON *summary = NULL;
	FILE *file_json = NULL;
	struct stat statbuf;
	cJSON *root = NULL;
	cJSON *node = NULL;
	int32_t ret;
	size_t ret_len;

	if (hbm_info == NULL) {
		return -1;
	}
	file_json = fopen(path, "r");
	if (file_json == NULL) {
		printf("[%d]: open error\n\n", __LINE__);
		return -1;
	}
	ret = stat(path, &statbuf);
	if (statbuf.st_size == 0 || ret != 0) {
		printf("[%d]: open error\n\n", __LINE__);
		fclose(file_json);
		return -1;
	}
	filebuf = (int8_t *)malloc(statbuf.st_size + 1);
	if (filebuf == NULL) {
		printf("[%d]: open error\n\n", __LINE__);
		fclose(file_json);
		return -1;
	}
	memset(filebuf, 0, statbuf.st_size + 1);
	ret_len = fread(filebuf, statbuf.st_size, 1, file_json);
	if (ret_len == 0) {
		printf("%s:[%d]: read file error\n", __FILE__, __LINE__);
		free(filebuf);
		fclose(file_json);
		return -1;
	}
	root = cJSON_Parse((const char *)filebuf);
	if (root == NULL) {
		printf("[%d]: open error\n\n", __LINE__);
		fclose(file_json);
		free(filebuf);
		return -1;
	}
	inst = cJSON_GetObjectItem((cJSON *)root, "inst");
	if (inst == NULL) {
		printf("[%d]: open error\n\n", __LINE__);
		free(filebuf);
		fclose(file_json);
		return -1;
	}
	if ((node = cJSON_GetObjectItem(inst, "Bpu_inst_size")) == NULL) {
		printf("bpu inst size is null\n");
		free(filebuf);
		fclose(file_json);
		return -1;
	}
	hbm_info->inst_size = node->valueint;
	if ((node = cJSON_GetObjectItem(inst, "Bpu_inst_offset")) == NULL) {
		printf("bpu inst offset is null\n");
		free(filebuf);
		fclose(file_json);
		return -1;
	}
	hbm_info->inst_offset = (uint32_t)strtol(node->valuestring, NULL, 16);
	para = cJSON_GetObjectItem((cJSON *)root, "param");
	if (para == NULL) {
		printf("[%d]: open error\n\n", __LINE__);
		free(filebuf);
		fclose(file_json);
		return -1;
	}
	if ((node = cJSON_GetObjectItem(para, "Bpu_param_size")) == NULL) {
		printf("bpu inst size is null\n");
		free(filebuf);
		fclose(file_json);
		return -1;
	}
	hbm_info->par_size = node->valueint;
	if ((node = cJSON_GetObjectItem(para, "Bpu_param_offset")) == NULL) {
		printf("bpu inst offset is null\n");
		free(filebuf);
		fclose(file_json);
		return -1;
	}
	hbm_info->par_offset = (uint32_t)strtol(node->valuestring, NULL, 16);


	summary = cJSON_GetObjectItem((cJSON *)root, "summary");
	if (summary == NULL) {
		printf("[%d]: open error\n\n", __LINE__);
		free(filebuf);
		fclose(file_json);
		return -1;
	}
	if ((node = cJSON_GetObjectItem(summary, "file_size")) == NULL) {
		printf("bpu inst size is null\n");
		free(filebuf);
		fclose(file_json);
		return -1;
	}
	hbm_info->hbm_size = node->valueint;
	if ((node = cJSON_GetObjectItem(summary, "file_name")) == NULL) {
		printf("bpu inst size is null\n");
		free(filebuf);
		fclose(file_json);
		return -1;
	}
	memcpy(hbm_info->hbm_name, node->valuestring, strlen(node->valuestring));
#if 0
	printf("bpu inst size:0x%x, inst offset:0x%x, param size:0x%x, param offset:0x%x\n",
		hbm_info->inst_size,
		hbm_info->inst_offset,
		hbm_info->par_size,
		hbm_info->par_offset);
	printf("file name:%s, file size:0x%x\n", hbm_info->hbm_name, hbm_info->hbm_size);
#endif
	free(filebuf);
	fclose(file_json);
	return 0;
}

int get_bpu_image_header(bpu_img_header *header)
{
	int32_t fd;
	ssize_t ret;
	int32_t partid = 0;
	char part_name[100] = {0};

	if (header == NULL) {
		printf("header is null\n");
		return -1;
	}
	partid = get_partition_id(BPU_PARTITION_NAME);
	if (partid < 0) {
		printf("get bpu partiton id failed\n");
		return -1;
	}
	snprintf(part_name, sizeof(part_name), "/dev/mmcblk0p%d", partid);
	fd = open(part_name, O_RDONLY, 444);
	if (fd < 0) {
		printf("%s:%d open %s failed\n",  __func__, __LINE__, part_name);
		return -1;
	}
	lseek(fd, 0, SEEK_SET);
	ret = read(fd, header, sizeof(bpu_img_header));
	if (ret < sizeof(bpu_img_header)) {
		printf("%s:%d read bpu image header error\n", __func__, __LINE__);
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

static int32_t dump_bpu_image_header()
{
	int32_t i;
	int32_t ret;
	bpu_img_header header;

	ret = get_bpu_image_header(&header);
	if (ret) {
		printf("%s:%d,get bpu image header error\n", __func__, __LINE__);
		return -1;
	}
	for (i = 0; i < MAX_HBM; i++) {
		printf("hbm[%d] name:%s\n", i, header.hbm[i].name);
		printf("hbm[%d] hbm size:0x%x\n", i, header.hbm[i].hbm_size);
		printf("hbm[%d] instruct load addr:0x%x\n", i, header.hbm[i].inst_load_addr);
		printf("hbm[%d] instruct load size:0x%x\n", i, header.hbm[i].inst_load_size);
		printf("hbm[%d] parameter load addr:0x%x\n", i, header.hbm[i].par_load_addr);
		printf("hbm[%d] parameter load size:0x%x\n", i, header.hbm[i].par_load_size);
		printf("hbm[%d] instruct data offset:0x%x\n", i, header.hbm[i].inst_data_offset);
		printf("hbm[%d] parameter data offset:0x%x\n", i, header.hbm[i].par_data_offset);
		printf("hbm[%d] signature offset:0x%x\n", i, header.hbm[i].signature_offset);
		printf("hbm[%d] hbm start offset:0x%x\n", i, header.hbm[i].hbm_start_offset);
	}
	printf("image len:0x%x\n", header.image_len);
	printf("bpu range start:0x%x\n", header.bpu_range_start);
	printf("bpu range size:0x%x\n", header.bpu_range_sz);
	printf("bpu all instruct size:0x%x\n", header.bpu_all_fetch_only_sz);

	return 0;
}

int32_t add_hbm(bpu_img_header *header, hbm_json_t *hbm, int32_t hbm_cnt, mbr_info_t *mbr_info)
{
	uint32_t image_len;
	uint32_t header_size;
	uint32_t image_align;
	ssize_t ret;
	int32_t ret_len;
	int32_t len;
	int32_t hbm_fd;
	int32_t bpu_img_fd;
	uint32_t csum = 0;
	int32_t i = 0;

	if (header == NULL || hbm == NULL) {
		return -1;
	}
	bpu_img_fd = open("/dev/mmcblk0p10", O_RDWR, 664);
	if (bpu_img_fd < 0) {
		printf("%s:%d open /dev/mmcblk0p10 failed\n", __func__, __LINE__);
		return -1;
	}
	image_len = header->image_len;
	header->hbm[hbm_cnt].hbm_start_offset = image_len;
	memcpy(header->hbm[hbm_cnt].name, hbm->hbm_name, strlen(hbm->hbm_name));
	header->hbm[hbm_cnt].hbm_size = hbm->hbm_size;
	header->hbm[hbm_cnt].inst_load_size = hbm->inst_size;
	header->hbm[hbm_cnt].par_load_size = hbm->par_size;

	if (header->bpu_range_sz > 0) {
		/*exist hbm in flash*/
		header->hbm[hbm_cnt].inst_load_addr = header->bpu_range_start + header->bpu_all_fetch_only_sz;
		header->hbm[hbm_cnt].par_load_addr = header->bpu_range_start + header->bpu_range_sz + hbm->inst_size;
	} else {
		if (hbm_addr == -1) {
			close(bpu_img_fd);
			printf("please input load address!!!\n");
			return -1;
		}
		header->hbm[hbm_cnt].inst_load_addr = hbm_addr;
		header->hbm[hbm_cnt].par_load_addr = hbm_addr +  header->hbm[hbm_cnt].inst_load_size;
		header->bpu_range_start = hbm_addr;
	}
	header->hbm[hbm_cnt].inst_data_offset = header->hbm[hbm_cnt].hbm_start_offset + hbm->inst_offset;
	header->hbm[hbm_cnt].par_data_offset = header->hbm[hbm_cnt].hbm_start_offset + hbm->par_offset;
	header->hbm[hbm_cnt].signature_offset = header->hbm[hbm_cnt].hbm_start_offset + hbm->hbm_size;
	header->hbm[hbm_cnt].sig_flag = sig_flag;
	for (i = 0; i < hbm_cnt; i++) {
		header->hbm[i].par_load_addr += hbm->inst_size;
	}
	header->bpu_range_sz += hbm->inst_size + hbm->par_size;
	header->bpu_all_fetch_only_sz += hbm->inst_size;

	image_len += hbm->hbm_size + 256;
	ret_len = image_len % 65536;
	image_align = ret_len ? (65536 - ret_len) : 0;
	header->image_len = image_align + image_len;
#if 0
	for (i = 0; i < MAX_HBM; i++) {
		printf("hbm[%d] name:%s!\n", i, header->hbm[i].name);
		printf("hbm[%d] hbm size:0x%x!\n", i, header->hbm[i].hbm_size);
		printf("hbm[%d] instruct load addr:0x%x!\n", i, header->hbm[i].inst_load_addr);
		printf("hbm[%d] instruct load size:0x%x!\n", i, header->hbm[i].inst_load_size);
		printf("hbm[%d] parameter load addr:0x%x!\n", i, header->hbm[i].par_load_addr);
		printf("hbm[%d] parameter load size:0x%x!\n", i, header->hbm[i].par_load_size);
		printf("hbm[%d] instruct data offset:0x%x!\n", i, header->hbm[i].inst_data_offset);
		printf("hbm[%d] parameter data offset:0x%x!\n", i, header->hbm[i].par_data_offset);
		printf("hbm[%d] signature offset:0x%x!\n", i, header->hbm[i].signature_offset);
		printf("hbm[%d] hbm start offset:0x%x!\n", i, header->hbm[i].hbm_start_offset);
	}
	printf("image len:0x%x!\n", header->image_len);
	printf("bpu range start:0x%x!\n", header->bpu_range_start);
	printf("bpu range size:0x%x!\n", header->bpu_range_sz);
	printf("bpu all instruct size:0x%x\n", header->bpu_all_fetch_only_sz);

#endif
	ret = lseek(bpu_img_fd, 0, SEEK_SET);
	if (ret < 0) {
		printf("lseek bpu image failed\n");
		close(bpu_img_fd);
		return -1;
	}
	ret = write(bpu_img_fd, header, sizeof(bpu_img_header));
	if (ret < sizeof(bpu_img_header)) {
		printf("%s:%d read bpu image header error\n", __func__, __LINE__);
		close(bpu_img_fd);
		return -1;
	}
	hbm_fd = open((char *)hbm->hbm_name, O_RDONLY, 444);
	if (hbm_fd < 0) {
		printf("open %s failed\n", hbm->hbm_name);
		close(bpu_img_fd);
		return -1;
	}
	header_size = sizeof(bpu_img_header);
	ret = lseek(bpu_img_fd, header->hbm[hbm_cnt].hbm_start_offset + header_size, SEEK_SET);
	if (ret < 0) {
		printf("lseek bpu image failed\n");
		close(bpu_img_fd);
		close(hbm_fd);
		return -1;
	}
	while (1) {
		ret = read(hbm_fd, tmp_buf, TMP_BUF_SIZE);
		if (ret < 0) {
			printf("read %s failed\n", hbm->hbm_name);
			close(bpu_img_fd);
			close(hbm_fd);
			return -1;
		} else if (ret == 0) {
			break;
		} else {
			ret = write(bpu_img_fd, tmp_buf, ret);
			if (ret < 0) {
				printf("read %s failed\n", hbm->hbm_name);
				close(bpu_img_fd);
				close(hbm_fd);
				return -1;
			}
			for (i = 0; i < ret; i++) {
				csum += tmp_buf[i];
			}
		}
	}
	/*accroding encrypt rules, hbm size should 64K align*/
	if (image_align > 0) {
		memset(tmp_buf, 0 , image_align);
		ret = write(bpu_img_fd, tmp_buf, image_align);
		if (ret < 0) {
			printf("write %s failed\n", hbm->hbm_name);
			close(bpu_img_fd);
			close(hbm_fd);
			return -1;
		}
	}
	/*emmc sector is 512 bytes, the whole bpu image should 512 align*/
	if ((header_size % 512) > 0) {
		len = 512 - header_size % 512;
		ret = write(bpu_img_fd, tmp_buf, len);
		if (ret < 0) {
			printf("write %s failed\n", hbm->hbm_name);
			close(bpu_img_fd);
			close(hbm_fd);
			return -1;
		}
	}
	mbr_info->csum = csum;
	mbr_info->len = len + (uint32_t)sizeof(bpu_img_header) + header->image_len;
	close(bpu_img_fd);
	close(hbm_fd);
	return 0;
}

#if 0
static void dump_mbr_info()
{
	int32_t mbr_fd;
	int32_t ret;
	uint32_t img_csum;
	uint32_t tmp[4];
	int i = 0;
	mbr_fd = open("/dev/mmcblk0", O_RDWR, 664);
	if (mbr_fd < 0) {
		printf("%s:%d open /dev/mmcblk0p6 failed:%s\n", __func__, __LINE__, strerror(errno));
	}
	for(i = 0; i < 512 / 16; i++) {
		read(mbr_fd, &tmp, 16);
		printf("0x%x:%x %x %x %x\n", i * 16, tmp[0], tmp[1], tmp[2], tmp[3]);
	}
	close(mbr_fd);
}
#endif

static int32_t modify_mbr(mbr_info_t *mbr_info)
{
	int32_t mbr_fd;
	ssize_t ret;
	int32_t i;
	uint32_t img_csum;
	uint32_t mbr_csum = 0;
	uint32_t tmp;
	uint8_t tmp_byte;
	mbr_fd = open("/dev/mmcblk0", O_RDWR, 664);
	if (mbr_fd < 0) {
		printf("%s:%d open /dev/mmcblk0p6 failed:%s\n", __func__, __LINE__, strerror(errno));
		return -1;
	}
	ret = lseek(mbr_fd, 62 * 4 , SEEK_SET);
	if (ret < 0) {
		printf("lseek mbr failed\n");
		close(mbr_fd);
		return -1;
	}
	ret = read(mbr_fd, &tmp, 4);
	if (ret < 0) {
		printf("read mbr failed\n");
		close(mbr_fd);
		return -1;
	}
	ret = lseek(mbr_fd, 62 * 4 , SEEK_SET);
	if (ret < 0) {
		printf("lseek mbr failed\n");
		close(mbr_fd);
		return -1;
	}

	ret = write(mbr_fd, &mbr_info->len, sizeof(mbr_info->len));
	if (ret < sizeof(mbr_info->len)) {
		printf("write mbr failed\n");
		close(mbr_fd);
		return -1;
	}
	ret = lseek(mbr_fd, 63 * 4 , SEEK_SET);
	if (ret < 0) {
		printf("lseek mbr failed\n");
		close(mbr_fd);
		return -1;
	}

	ret = read(mbr_fd, &img_csum, sizeof(img_csum));
	if (ret < sizeof(img_csum)) {
		printf("read mbr failed\n");
		close(mbr_fd);
		return -1;
	}
	img_csum += mbr_info->csum;
	ret = lseek(mbr_fd, 63 * 4 , SEEK_SET);
	if (ret < 0) {
		printf("lseek mbr failed\n");
		close(mbr_fd);
		return -1;
	}

	ret = write(mbr_fd, &img_csum, sizeof(img_csum));

	ret = lseek(mbr_fd, 0, SEEK_SET);
	if (ret < 0) {
		printf("lseek mbr failed\n");
		close(mbr_fd);
		return -1;
	}
	for (i = 0; i < 69 * 4; i++) {
		ret = read(mbr_fd, &tmp_byte, sizeof(tmp_byte));
		if (i > 11 && i < 16)
			continue;

		if (ret < sizeof(tmp_byte)) {
			printf("read mbr failed\n");
			close(mbr_fd);
			return -1;
		}
		mbr_csum += tmp_byte;
	}
	ret = lseek(mbr_fd, 12, SEEK_SET);
	if (ret < 0) {
		printf("lseek mbr failed\n");
		close(mbr_fd);
		return -1;
	}
	ret = write(mbr_fd, &mbr_csum, sizeof(mbr_csum));
	close(mbr_fd);
	return 0;
}

#if 0
static void test_mbr_csum()
{
	int32_t mbr_fd;
	int32_t ret;
	int32_t i;
	uint32_t img_csum;
	uint32_t mbr_csum = 0;
	uint8_t tmp;
	mbr_fd = open("/dev/mmcblk0", O_RDWR, 664);
	if (mbr_fd < 0) {
		printf("%s:%d open /dev/mmcblk0p0 failed:%s\n", __func__, __LINE__, strerror(errno));
		return -1;
	}
	lseek(mbr_fd, 0, SEEK_SET);
	for (i = 0; i < 69 * 4; i++) {
		ret = read(mbr_fd, &tmp, sizeof(tmp));
		if (!(i % 16))
			printf("\n");
		printf("%x ", tmp);
		if (i > 11 && i < 16)
			continue;
		if (ret < sizeof(tmp)) {
			printf("read mbr failed\n");
			return -1;
		}
		mbr_csum += tmp;
	}
	printf("mbr csum:0x%x\n", mbr_csum);
}
#endif


static const char short_options[] = "j:s:a:h";
static const struct option long_options[] = {
                                         {"json", required_argument, NULL, 'j'},
                                         {"size", required_argument, NULL, 's'},
                                         {"addr", required_argument, NULL, 'a'},
                                         {"help", no_argument, NULL, 'h'},
                                         {0, 0, 0, 0}};

int main(int argc, char *argv[])
{
	int32_t ret;
	hbm_json_t hbm_info = {0};
	bpu_img_header tmp_header;
	mbr_info_t mbr_info;
	int32_t cmd_parser_ret = 0;

	while ((cmd_parser_ret = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
		switch (cmd_parser_ret) {
			case 'j':
				json_file = optarg;
				break;

			case 's':
				sig_flag = atoi(optarg);
				break;

			case 'a':
				hbm_addr = (int)strtol(optarg, NULL, 16);
				break;
			case 'h':
				printf("**********HRUT_HBM TOOL HELP INFORMATION*********\n");
				printf(">>> -j/--json            [json file name]\n");
				printf(">>> -s/--signature flag  [0---horizon 1---customer]\n");
				printf(">>> -a/--addr            [load address, use for first hbm]\n");
				return 0;
			}
	}
	if (json_file == NULL) {
		printf("please input json file!!!\n");
		return -1;
	}
	if (sig_flag == -1) {
		printf("please input signature flag!!!\n");
	}

	ret = get_hbm_json(json_file, &hbm_info);
	if (ret < 0) {
		printf("get hbm json failed\n");
		return -1;
	}
	ret = get_bpu_image_header(&tmp_header);
	if (ret < 0) {
		printf("get bpu image header error\n");
		return -1;
	}
	ret = platform_get_hbm_number();
	if (ret >= MAX_HBM) {
		printf("hbm is full\n");
		return -1;
	}

	ret = add_hbm(&tmp_header, &hbm_info, ret,  &mbr_info);
	if (ret < 0) {
		printf("add hbm error\n");
		return -1;
	}
	ret = modify_mbr(&mbr_info);
	if (ret < 0) {
		printf("modify mbr error\n");
		return -1;
	}
	dump_bpu_image_header();
	return 0;
}
