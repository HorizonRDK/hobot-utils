/*
 *    COPYRIGHT NOTICE
 *    Copyright 2020 Horizon Robotics, Inc.
 *    All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <mtd/ubi-user.h>
#include <mtd/mtd-user.h>
#include <errno.h>

#include "veeprom.h"

#define DEBUG
#define WARN
#define ERROR

#define _STRINGIFY(s) #s

#define DEFAULT_UBI_CTRL "/dev/ubi_ctrl"
#define UBI_NUM 10
#define VEEPROM_UBI(x)	"/dev/ubi"_STRINGIFY(x)"_0"
#define SECTOR_SIZE (512)

#define BUFFER_SIZE 2048

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) sizeof(x) / sizeof(x[0])
#endif /* ARRAY_SIZE */

/* Must align to 64k */
#define NOR_ERASE_LENGTH (VEEPROM_MAX_SIZE / 1024*64 + 1) * 1024*64

#ifndef GPT_ENTRY_MAX
#define GPT_ENTRY_MAX 128
#define GPT_NAME_LEN_BYTES 72
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
    uint16_t part_name[GPT_NAME_LEN_BYTES * sizeof(char) / sizeof(uint16_t)];
};
#endif /* GPT_ENTRY_MAX */

char *device_path = "";
unsigned int start_sector;
unsigned int end_sector;
int fd = -1;
char buffer[BUFFER_SIZE];
int bootmode;

static int sync_flag = SYNC_TO_VEEPROM;

int get_boot_mode(void)
{
	size_t ret = 0;
	int mode = 0;
	char cmd_buf[64] = { 0 };

	/*
	 * some exception list
	 * which using nor+emmc booting, e.g. mono&quad
	 */
	FILE* fd = fopen("/sys/class/socinfo/board_id", "r");
	if(fd) {
		ret = fread(cmd_buf, 1, VEEPROM_X3_BOARD_ID_SIZE, fd);
		fclose(fd);
		if (ret == 0) {
			fprintf(stderr, "Read board_id Failed!\n");
			return -EIO;
		}
	} else {
		WARN("warning: open socinfo/board_id failed, using emmc mode! \n");
		mode = PIN_2ND_EMMC;
		return mode;
	}

	if(cmd_buf[0] == '3' && cmd_buf[1] == '0' && cmd_buf[2] == '0') {
		mode = PIN_2ND_EMMC;
		return mode;
	}
	/* some exception list end */

	memset(cmd_buf, 0, sizeof(cmd_buf));

	fd = fopen("/sys/class/socinfo/boot_mode", "r");
	if(fd) {
		ret = fread(cmd_buf, 1, 1, fd);
		fclose(fd);
		if (ret == 0) {
			fprintf(stderr, "Read board_id Failed!\n");
			return -EIO;
		}
	} else {
		WARN("warning: open socinfo/boot_mode failed, using emmc mode! \n");
		mode = PIN_2ND_EMMC;
	}

	if (cmd_buf[0] == '1')
		mode = PIN_2ND_SPINAND;
	else if (cmd_buf[0] == '5')
		mode = PIN_2ND_SPINOR;
	else
		mode = PIN_2ND_EMMC;

	return mode;
}

int parameter_check(char *c, ssize_t len)
{
	int i = 0;

	for (i = 0; i < len; i++) {
		if(c[i] < '0' || c[i] > 'f')
			return 1;
	}

	return 0;
}

static int is_parameter_valid(int offset, int size)
{
	int offset_left = 0;
	int offset_right = (end_sector - start_sector + 1) * SECTOR_SIZE;
	int max_size = offset_right - offset_left;

	if (offset + size > max_size)
		return 0;

	return 1;
}

int get_mtdnum_partname(char* target_nm) {
	int target_mtd_nm = -1;
	int ret = 0;
	size_t rw_ret = 0;
	char tmp_path[512], mtd_name_buf[128], mtd_num_tmp[16];
	char *name_ptr;
	FILE* mtd_fd;
	struct dirent *mtdx;
	/* use mtd sysfs node for finding the correct partition */
	DIR *mtd_sys = opendir("/sys/class/mtd/");
	if (mtd_sys == NULL) {
		ERROR("MTD system does not exist or is too old!\n");
		return -1;
	}
	while ((mtdx = readdir(mtd_sys)) != NULL) {
		memset(tmp_path, 0, sizeof(tmp_path));
		memset(mtd_name_buf, 0, sizeof(mtd_name_buf));
		snprintf(tmp_path, sizeof(tmp_path),
				"/sys/class/mtd/%s/name", mtdx->d_name);
		mtd_fd = fopen(tmp_path, "r");
		if(mtd_fd) {
			rw_ret = fread(mtd_name_buf, sizeof(char), sizeof(mtd_name_buf), mtd_fd);
			fclose(mtd_fd);
			if (rw_ret == 0) {
				fprintf(stderr, "Read mtd name from %s failed!\n", tmp_path);
				ret = closedir(mtd_sys);
				if (ret) {
					fprintf(stderr, "%s: closedir failed!\n", __func__);
					return ret;
				}
				return -EIO;
			}
			/* Got the partition, extract mtd number */
			if (!strncmp(mtd_name_buf, target_nm,
							strlen(mtd_name_buf) - 1)) {
				memset(mtd_num_tmp, 0, sizeof(mtd_num_tmp));
				name_ptr = mtdx->d_name;
				while (*name_ptr) {
					if (isdigit(*name_ptr)) {
						snprintf(mtd_num_tmp, sizeof(mtd_num_tmp),
								"%ld",
								strtol(name_ptr, &name_ptr, 10));
					} else {
						name_ptr++;
					}
				}
				target_mtd_nm = atoi(mtd_num_tmp);
				break;
			}
		}
	}
	ret = closedir(mtd_sys);
	if (ret) {
		fprintf(stderr, "%s: closedir failed!\n", __func__);
		return ret;
	}
	return target_mtd_nm;
}

/*
* Convert str from gpt part_name to normal string
*/
static void convert_partname(uint16_t *str, char *result) {
	unsigned label_max, label_count = 0;
	/* Naively convert UTF16-LE to 7 bits. */
	label_max = fmin(GPT_NAME_LEN_BYTES - 1, sizeof(str));
	while (label_count < label_max) {
		uint8_t c = (uint8_t)(str[label_count] & 0xff);
		if (c && !isprint(c))
			c = '!';
		result[label_count] = c;
		label_count++;
	}
}

int veeprom_init(void)
{
	fd = open(device_path, O_RDWR | O_SYNC);
	if (fd < 0) {
		bootmode = get_boot_mode();

		if (bootmode == PIN_2ND_SPINAND) {
			if (access(VEEPROM_UBI(UBI_NUM), F_OK) == -1) {
				struct ubi_attach_req req;
				int ubi_ctrl_fd, ret;
				memset(&req, 0, sizeof(struct ubi_attach_req));
				req.mtd_num = -1;
				/* Try env partition first*/
				req.mtd_num = get_mtdnum_partname("env");

				if (req.mtd_num < 0)
					req.mtd_num = get_mtdnum_partname("boot");

				if (req.mtd_num < 0) {
					ERROR("Boot/Env partition not found!\n");
					return -1;
				}
				req.ubi_num = UBI_NUM;
				ubi_ctrl_fd = open(DEFAULT_UBI_CTRL, O_RDONLY);
				if (ubi_ctrl_fd == -1) {
					ERROR("UBI Control %s not present in system!\n", DEFAULT_UBI_CTRL);
					return -1;
				}
				ret = ioctl(ubi_ctrl_fd, UBI_IOCATT, &req);
				if (ret < 0) {
					ERROR("Attach mtd%d as ubi device failed!\n", req.mtd_num);
					close(ubi_ctrl_fd);
					return -1;
				}
				close(ubi_ctrl_fd);
			}
			device_path = VEEPROM_UBI(UBI_NUM);
		} else if (bootmode == PIN_2ND_SPINOR) {
			char nor_veeprom_path[128];
			int nor_veeprom_mtdnm;

			memset(&nor_veeprom_path, 0, sizeof(nor_veeprom_path));

			nor_veeprom_mtdnm = get_mtdnum_partname("veeprom");

			snprintf(nor_veeprom_path, sizeof(nor_veeprom_path),
					 "/dev/mtd%d", nor_veeprom_mtdnm);
			device_path = nor_veeprom_path;
		} else {
			/* Assume using emmc veeprom */
			char *gpt_dev = "/dev/mmcblk0";
			char mmc_veeprom_path[128];
			int ret = 0;
			size_t rw_ret = 0;
			char name_translated[GPT_ENTRY_MAX][GPT_NAME_LEN_BYTES] = { 0 };
    		struct gpt_entry gpt_entries[GPT_ENTRY_MAX] = { 0 };
			FILE *gpt_on_disk = fopen(gpt_dev, "r");
			/* Read gpt from mmc */
			/* first lba is mbr, second is gpt header, total 1024 bytes */
			if (!gpt_on_disk) {
				printf("mmc:%s device not found!\n", gpt_dev);
				return -1;
			}
			ret = fseek(gpt_on_disk, 1024, SEEK_SET);
			if (ret) {
				fprintf(stderr, "fseek to gpt offset failed!\n");
				fclose(gpt_on_disk);
				return ret;
			}
			rw_ret = fread(&gpt_entries, sizeof(struct gpt_entry),
						GPT_ENTRY_MAX, gpt_on_disk);
			if (rw_ret != GPT_ENTRY_MAX) {
				printf("read short: %ld\n", rw_ret);
				fclose(gpt_on_disk);
				return -1;
			}
			for (int i = 0; i < ARRAY_SIZE(gpt_entries); i++) {
				convert_partname(gpt_entries[i].part_name, name_translated[i]);
				if (!strcmp(name_translated[i], "veeprom")) {
					snprintf(mmc_veeprom_path, sizeof(mmc_veeprom_path),
							"/dev/mmcblk0p%d", i + 1);
					device_path = mmc_veeprom_path;
				}
			}
			fclose(gpt_on_disk);
		}
		start_sector = VEEPROM_START_SECTOR;
		end_sector = VEEPROM_END_SECTOR;
		DEBUG("dev_path:%s, start_sector=%d, end_sector=%d\n",
			  device_path, start_sector, end_sector);

		fd = open(device_path, O_RDWR | O_SYNC);

		if (fd < 0) {
			ERROR("Error: open %s fail\n", device_path);
			return -1;
		}
	}

	return 0;
}

void veeprom_exit(void)
{
	sync();
	close(fd);
	fd = -1;
}

void veeprom_setsync(int flag)
{
	sync_flag = flag;
}

int veeprom_format(void)
{
	return veeprom_clear(0, 256);
}

int veeprom_read(int offset, char *buf, int size)
{
	unsigned int sector_left = 0;
	unsigned int sector_right = 0;
	unsigned int sector_count = 0;
	unsigned int offset_inner = 0;
	unsigned int remain_inner = 0;
	unsigned int i = 0;
	int ret = 0;
	ssize_t rw_ret = 0;

	if (get_boot_mode() == PIN_2ND_SPINAND) {
		memset(buffer, 0, BUFFER_SIZE);
		if (fd == -1) {
			ret = veeprom_init();
			if (ret || fd == -1) {
				fprintf(stderr, "Veeprom Init Failed!\n");
				return -1;
			}
		}
		rw_ret = read(fd, buffer, BUFFER_SIZE);
		if (rw_ret < 0) {
			ERROR("Read %d bytes failed!\n", BUFFER_SIZE);
			return -1;
		}
		if (rw_ret != BUFFER_SIZE)
			WARN("%s: short read: %ld, targeted:%d!\n", __func__,
						rw_ret, BUFFER_SIZE);
		memcpy(buf, buffer + offset, size);
		veeprom_exit();
		return 0;
	}

	if (!is_parameter_valid(offset, size)) {
		ERROR("Error: parameters invalid\n");
		return -1;
	}

	/* compute sector count */
	sector_left = start_sector + (offset / SECTOR_SIZE);
	sector_right = start_sector + ((offset + size - 1) / SECTOR_SIZE);
	sector_count = sector_right - sector_left + 1;
	DEBUG("sector_left = %d\n", sector_left);
	DEBUG("sector_right = %d\n", sector_right);

	for (i = 0; i < sector_count; ++i) {
		int operate_count = 0;
		memset(buffer, 0, BUFFER_SIZE);

		if (lseek(fd, (sector_left + i) * SECTOR_SIZE, SEEK_SET) < 0) {
			ERROR("Error: lseek sector %d fail\n", sector_left + i);
			return -1;
		}
		rw_ret = read(fd, buffer, BUFFER_SIZE);
		if (rw_ret < 0) {
			ERROR("Error: read sector %d fail\n", sector_left + i);
			return -1;
		}

		if (rw_ret != BUFFER_SIZE)
			WARN("%s: short read: %ld, targeted:%d!\n", __func__,
						rw_ret, BUFFER_SIZE);

		/* sector number: sector_left + i - start_sector */
		offset_inner = offset - (sector_left + i - start_sector) * SECTOR_SIZE;
		remain_inner = SECTOR_SIZE - offset_inner;
		operate_count = (remain_inner >= size ? size : remain_inner);
		size -= operate_count;
		offset += operate_count;
		DEBUG("%s:offset_inner = %d\n", __FUNCTION__, offset_inner);
		DEBUG("%s:operate_count = %d\n", __FUNCTION__, operate_count);
		memcpy(buf, buffer + offset_inner, operate_count);
		buf += operate_count;
	}

	return 0;
}

int veeprom_write(int offset, const char *buf, int size)
{
	int ret;
	ssize_t rw_ret = 0;
	if (get_boot_mode() == PIN_2ND_SPINAND) {
		unsigned int bytes = BUFFER_SIZE;
		memset(buffer, 0, BUFFER_SIZE);
		if (fd == -1) {
			ret = veeprom_init();
			if (ret || fd == -1) {
				fprintf(stderr, "Veeprom Init Failed!\n");
				return -1;
			}
		}

		rw_ret = read(fd, buffer, bytes);
		if (rw_ret < 0) {
			ERROR("Read %d bytes failed!\n", BUFFER_SIZE);
			return -1;
		}

		if(rw_ret != bytes)
			WARN("%s: short read: %ld, targeted:%d!\n", __func__,
						rw_ret, BUFFER_SIZE);
		memcpy(buffer + offset, buf, size);

		if (ioctl(fd, UBI_IOCVOLUP, &bytes)) {
			ERROR("Start update volume %s failed, %s!\n",
				   VEEPROM_UBI(UBI_NUM), strerror(errno));
			return -1;
		}

		rw_ret = write(fd, buffer, bytes);
		if (rw_ret < 0) {
			ERROR("Write %d bytes failed!\n", BUFFER_SIZE);
			return -1;
		}

		if(rw_ret != bytes)
			WARN("%s: short write: %ld, targeted:%d!\n", __func__,
						rw_ret, BUFFER_SIZE);

		veeprom_exit();
		return 0;
	}

	unsigned int sector_left = 0;
	unsigned int sector_right = 0;
	unsigned int sector_count = 0;
	unsigned int offset_inner = 0;
	unsigned int remain_inner = 0;
	unsigned int i = 0;

	if (!is_parameter_valid(offset, size)) {
		ERROR("Error: parameters invalid\n");
		return -1;
	}

	sector_left = start_sector + (offset / SECTOR_SIZE);
	sector_right = start_sector + ((offset + size - 1) / SECTOR_SIZE);
	sector_count = sector_right - sector_left + 1;

	for (i = 0; i < sector_count; ++i) {
		int operate_count = 0;
		memset(buffer, 0, BUFFER_SIZE);

		if (lseek(fd, (sector_left + i) * SECTOR_SIZE, SEEK_SET) < 0) {
			DEBUG("Error: lseek sector %d fail\n", sector_left + i);
			return -1;
		}

		rw_ret = read(fd, buffer, BUFFER_SIZE);
		if (rw_ret < 0) {
			DEBUG("Error: read sector %d fail\n", sector_left + i);
			return -1;
		}

		if (rw_ret != BUFFER_SIZE)
			WARN("%s: short read: %ld, targeted:%d!\n", __func__,
						rw_ret, BUFFER_SIZE);

		if (bootmode == PIN_2ND_SPINOR) {
			struct erase_info_user argp;
			argp.start = 0;
			argp.length = NOR_ERASE_LENGTH;
			ret = ioctl(fd, MEMERASE, &argp);
			if (ret < 0) {
				ERROR("Erase NOR for %d bytes failed!\n", NOR_ERASE_LENGTH);
				return ret;
			}
		}

		offset_inner = offset - (sector_left + i - start_sector) * SECTOR_SIZE;
		remain_inner = SECTOR_SIZE - offset_inner;
		operate_count = (remain_inner >= size ? size : remain_inner);
		size -= operate_count;
		offset += operate_count;
		DEBUG("%s:offset_inner = %d\n", __FUNCTION__, offset_inner);
		DEBUG("%s:operate_count = %d\n", __FUNCTION__, operate_count);
		memcpy(buffer + offset_inner, buf, operate_count);
		buf += operate_count;

		if (lseek(fd, (sector_left + i) * SECTOR_SIZE, SEEK_SET) < 0) {
			DEBUG("Error: lseek sector %d fail\n", sector_left + i);
			return -1;
		}

		if ((rw_ret = write(fd, buffer, BUFFER_SIZE)) < 0) {
			DEBUG("Error: write sector %d fail\n", sector_left + i);
			return -1;
		}
		if (rw_ret != BUFFER_SIZE)
			WARN("%s: short write: %ld, targeted:%d!\n", __func__,
						rw_ret, BUFFER_SIZE);
	}
	return 0;
}

int veeprom_clear(int offset, int size)
{
	int ret = 0;
	ssize_t rw_ret;
	if (get_boot_mode() == PIN_2ND_SPINAND) {
		char *buf;
		int buf_len = size * sizeof(char);
		buf = (char*) malloc(buf_len);
		memset(buf, 0, buf_len);
		ret = veeprom_write(offset, buf, size);
		free(buf);
		return ret;
	}
	unsigned int sector_left = 0;
	unsigned int sector_right = 0;
	unsigned int sector_count = 0;
	unsigned int offset_inner = 0;
	unsigned int remain_inner = 0;
	unsigned int i = 0;

	if (!is_parameter_valid(offset, size)) {
		ERROR("Error: parameters invalid\n");
		return -1;
	}

	sector_left = start_sector + (offset / SECTOR_SIZE);
	sector_right = start_sector + ((offset + size - 1) / SECTOR_SIZE);
	sector_count = sector_right - sector_left + 1;

	for (i = 0; i < sector_count; ++i) {
		int operate_count = 0;
		memset(buffer, 0, BUFFER_SIZE);

		if (lseek(fd, (sector_left + i) * SECTOR_SIZE, SEEK_SET) < 0) {
			DEBUG("Error: lseek sector %d fail\n", sector_left + i);
			return -1;
		}
		rw_ret = read(fd, buffer, BUFFER_SIZE);
		if (rw_ret < 0) {
			DEBUG("Error: read sector %d fail\n", sector_left + i);
			return -1;
		}
		if (rw_ret != BUFFER_SIZE)
			WARN("%s: short read: %ld, targeted:%d!\n", __func__,
						rw_ret, BUFFER_SIZE);

		offset_inner = offset - (sector_left + i - start_sector) * SECTOR_SIZE;
		remain_inner = SECTOR_SIZE - offset_inner;
		operate_count = (remain_inner >= size ? size : remain_inner);
		size -= operate_count;
		offset += operate_count;
		DEBUG("%s:offset_inner = %d\n", __FUNCTION__, offset_inner);
		DEBUG("%s:operate_count = %d\n", __FUNCTION__, operate_count);
		memset(buffer + offset_inner, 0, operate_count);

		if (lseek(fd, (sector_left + i) * SECTOR_SIZE, SEEK_SET) < 0) {
			DEBUG("Error: lseek sector %d fail\n", sector_left + i);
			return -1;
		}

		if ((rw_ret = write(fd, buffer, BUFFER_SIZE)) < 0) {
			DEBUG("Error: write sector %d fail\n", sector_left + i);
			return -1;
		}
		if (rw_ret != BUFFER_SIZE)
			WARN("%s: short write: %ld, targeted:%d!\n", __func__,
						rw_ret, BUFFER_SIZE);
	}

	return 0;
}

int veeprom_dump(void)
{
	ssize_t rw_ret = 0;
	unsigned int j = 0;
	unsigned int start_byte = start_sector * SECTOR_SIZE;

	if (lseek(fd, start_byte, SEEK_SET) < 0) {
		ERROR("Error: lseek sector %d fail\n", start_byte);
		return -1;
	}

	printf("veeprom:\n");

	memset(buffer, 0, BUFFER_SIZE);
	rw_ret = read(fd, buffer, VEEPROM_MAX_SIZE);
	if (rw_ret < 0) {
		ERROR("Error: read fail\n");
		return -1;
	}
	if (rw_ret != BUFFER_SIZE)
			WARN("%s: short read: %ld, targeted:%d!\n", __func__,
						rw_ret, BUFFER_SIZE);

	for (j = 0; j < VEEPROM_MAX_SIZE; ++j) {
		printf("%02x ", buffer[j]);

		if (!((j + 1) % 16))
			printf("\n");
	}

	return 0;
}
