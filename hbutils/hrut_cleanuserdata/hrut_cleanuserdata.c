/*
 *    COPYRIGHT NOTICE
 *    Copyright 2020 Horizon Robotics, Inc.
 *    All rights reserved.
 * # userdata whitelist: level 1 directory
 * The directory will be save, when clean up userdata partition data
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <dirent.h>
#include <sys/stat.h>

#include "cJSON.h"
// #define _DEBUG_

#ifdef _DEBUG_
#define DEBUG(format,...) printf("FILE: " __FILE__ ", LINE: %d: " format "\n", __LINE__, ##__VA_ARGS__)
#else
#define DEBUG(format,...)
#endif

#define FILE_SIZE (2048)
#define CLEANUSERDATA_LOG "/tmp/hrut_cleanuserdata_log.txt"
static char log[256]={'\0'};
static int protect_all = 0;

// write log into /tmp/veeprom_log.txt
static int cleanuserdata_write_log(const char *write_log)
{
	FILE *cleanuserdata_file;
	if (!(cleanuserdata_file = fopen(CLEANUSERDATA_LOG, "a+"))) {
		perror("fopen faild\n");
		return -1;
	}
	fprintf(cleanuserdata_file, "%s", write_log);
	fclose(cleanuserdata_file);

	memset(log, 0, sizeof(log));
	return 0;
}

#define userdata_whitelist "/usr/bin/userdata_whitelist"

#define USERDATA_DIR ("/userdata/") // "/userdata/"

#define USERDATA_STRLEN (10) // "/userdata/"

//static char userdata_dir_name[256] = {'\0'};

static char *tmp_target_dir;

static bool create_tmp_dir()
{
	char name[] = "/tmp/dirXXXXXX";
    char* tmp_dir = mkdtemp(name);
    if (tmp_dir == NULL) {
        perror("mkdtemp\n");
		goto faild;
	}

	tmp_target_dir = (char *)malloc(strlen(tmp_dir)+1);
	if (!tmp_target_dir) {
		perror("malloc for tmp_target_dir fail\n");
		goto faild;
	}
	memset(tmp_target_dir, 0, strlen(tmp_dir)+1);
	strcat(tmp_target_dir, tmp_dir);

	DEBUG("Create tmp directory:%s success\n", tmp_target_dir);
	sprintf(log, "Create tmp directory:%s success\n", tmp_target_dir);
	cleanuserdata_write_log(log);
	return true;
faild:
	return false;
}
/**
* 递归删除目录(删除该目录以及该目录包含的文件和目录)
* @dir:要删除的目录绝对路径
*/
int remove_dir(const char *dir)
{
	char cur_dir[] = ".";
	char up_dir[] = "..";
	char dir_name[FILE_SIZE] = {'\0'};
	DIR *dirp;
	struct dirent *dp;
	struct stat dir_stat;
	int ret;
	DEBUG("Begin remove dir:%s\n", dir);
	// 参数传递进来的文件不存在，直接返回
	if ( 0 != access(dir, F_OK) ) {
		return 0;
	}
	// lstat will non through
	if((ret = lstat(dir, &dir_stat)) < 0) {
		perror("lstat faild\n");
		return -1;
	}

	if ( S_ISDIR(dir_stat.st_mode) ) {	// 目录文件，递归删除目录中内容
		dirp = opendir(dir);
		while ( (dp=readdir(dirp)) != NULL ) {
			// ignore . && ..
			if ( (0 == strcmp(cur_dir, dp->d_name)) || (0 == strcmp(up_dir, dp->d_name)) ) {
				continue;
			}

			sprintf(dir_name, "%s/%s", dir, dp->d_name);
			remove_dir(dir_name);   // 递归调用
		}
		closedir(dirp);

		rmdir(dir);	// 删除空目录
	} else {
		if (S_ISLNK(dir_stat.st_mode)) {
				unlink(dir);
			} else {
				if((ret = remove(dir)) < 0)
					perror("remove faild");
		}
	}

	return 0;
}

void usage(void)
{
	printf("Usage: hrut_cleanuserdata <image_name>\n");
	printf("Example: \n");
	printf("       hrut_otastatus /userdata/test/all_in_one.zip\n");
	printf("image_name:ota upgrade package absolutely path\n");
}

#if 0
/*
* Maybe whitelist file or directory is not exist.打印输出 目录是否存在.
* 将白名单的 log、结果、信息，输出到 指定文件中.
* solution: 1. 直接去写文件.
*           2. 将信息写入管道，另外开一个守护进程一直读管道信息，然后写入文件.
* 提前帅选出; 避免 symlink() 失败
*/
static int check_source_dir_exist(const char *source_dir)
{
	int ret = 0;
	char *absolute_dir_name = (char *)malloc(strlen(USERDATA_DIR) + strlen(source_dir) + 1);
	if(!absolute_dir_name) {
		sprintf(log, "malloc memory for absolute_dir_name fail\n");
		cleanuserdata_write_log(log);
		return -1;
	}

	strcat(absolute_dir_name, USERDATA_DIR);
	strcat(absolute_dir_name, source_dir);

    if (!access(absolute_dir_name, F_OK)) {
		printf("Directory %s isnot exist\n", absolute_dir_name);
		sprintf(log, "Directory %s isnot exist.\n", absolute_dir_name);
		cleanuserdata_write_log(log);
		ret = 0;
    } else {
		printf("Directory %s exist.\n", absolute_dir_name);
		sprintf(log, "Directory %s is exist.\n", absolute_dir_name);
		cleanuserdata_write_log(log);
		ret = -1;
    }

	free(absolute_dir_name);
	return ret;
}

// 检查destination directory is exist
static int check_dest_dir_exist(const char *dest_dir)
{
	int ret = 0;
	char *absolute_dir_name = (char *)malloc(strlen(tmp_target_dir) + strlen(dest_dir) + 1);
	if(!absolute_dir_name) {
		sprintf(log, "malloc memory for absolute_dir_name fail\n");
		cleanuserdata_write_log(log);
		return -1;
	}

	strcat(absolute_dir_name, tmp_target_dir);
	strcat(absolute_dir_name, dest_dir);

    if (!access(absolute_dir_name, F_OK)) {
		printf("Directory %s isnot exist\n", absolute_dir_name);
		sprintf(log, "Directory %s isnot exist.\n", absolute_dir_name);
		cleanuserdata_write_log(log);
		ret = 0;
    } else {
		printf("Directory %s exist.\n", absolute_dir_name);
		sprintf(log, "Directory %s is exist.\n", absolute_dir_name);
		cleanuserdata_write_log(log);
		ret = -1;
    }

	free(absolute_dir_name);
	return ret;
}
#endif
//创建多级目录 //char* sPathName = /tmp/tmp.0KAIbR/test/xx/xxxx
// ret: 0 success;
int createMultiLevelDir(char* sPathName)
{
	char DirName[FILE_SIZE];
	size_t i, len;

	memset(DirName, 0, sizeof(DirName));
	strcpy(DirName, sPathName);
	len = strlen(DirName);
	if('/' != DirName[len-1]) {
		strcat(DirName, "/");
		len++;
	}

	for(i=1; i<len; i++)
	{
		if('/' == DirName[i]) // step1. 找到 tmp/ 继续往下查找; 最后的结果是正确的创建/tmp/tmp.0KAIbR/test/xx/xxxx
		{
			DirName[i] = '\0';
			if(access(DirName, F_OK) != 0) // step2. 判断 tmp 是否存在.
			{
				if(mkdir(DirName, 0777) == -1)
				{
					perror("mkdir() failed!");
					return -1;
				}
			}
			DirName[i] = '/';
		}
  	}

	return 0;
}


/* 功  能：将str字符串中的oldstr字符串替换为newstr字符串, 仅替换首个对应的字符.
 * 参  数：str：操作目标 oldstr：被替换者 newstr：替换者
 * 返回值：返回替换之后的字符串
 * 版  本： V0.3
 */
char *strrpc(char *str,char *oldstr,char *newstr){
    char bstr[FILE_SIZE];//转换缓冲区
    memset(bstr, 0, sizeof(bstr));

	int flag = 0;
    for (size_t i = 0; i < strlen(str); i++) {
        if(!strncmp(str+i, oldstr, strlen(oldstr)) && flag == 0){
            strcat(bstr,newstr);
			flag = 1;
            i += strlen(oldstr) - 1;
        }else{
        	strncat(bstr, str + i, 1);
	    }
    }

    strcpy(str, bstr);
    return str;
}

// sizeof(file_path) will least of 2048
// 返回值: 1, 是白名单; 0, 不是白名单.
static int is_in_whitelist(const char *userdata_path)
{
	char tmp_dir_path[FILE_SIZE];  //转换缓冲区 "/userdata" -> "/tmp/dirMY5pxU"
	memset(tmp_dir_path, 0, sizeof(tmp_dir_path));
	strcpy(tmp_dir_path, userdata_path);

	strrpc(tmp_dir_path, "/userdata", tmp_target_dir);
	DEBUG("%s will be %s.\n", userdata_path, tmp_dir_path);

    if (!access(tmp_dir_path, F_OK)) {
		DEBUG("target directory %s is exist\n", tmp_dir_path);
		return 1;
    } else {
		DEBUG("target directory %s isnot exist.\n", tmp_dir_path);
		return 0;
    }
}


// brief: 由 白名单目录文件,创建 /tmp/userdata 下面的软链接.
//		由于软链接不要求源文件是否存在,故可以不考虑 白名单中的文件是否存在。
// Argument: 去掉 "/userdata" 后的目录名
// 返回值: 0 创建成功或文件已存在. others创建失败.
static int create_white_dir(const char *dir_name)
{
	if (!dir_name) {
		return 0;
	}
	int ret = 0;
	size_t point = 0;

	char source_dir_name[FILE_SIZE] = {'\0'};
	char target_dir_name[FILE_SIZE] = {'\0'};

	char copy_file_dir[FILE_SIZE] = {'\0'};
	strcpy(copy_file_dir, dir_name);

	char tmp_file_dir[FILE_SIZE];
	memset(tmp_file_dir, 0, sizeof(tmp_file_dir));

	char file_dir_name[FILE_SIZE];
	memset(file_dir_name, 0, sizeof(file_dir_name));


	memset(source_dir_name, 0, sizeof(source_dir_name));
	// merge /userdata/ & dir_name to source directory name
	strcat(source_dir_name, USERDATA_DIR);
	strcat(source_dir_name, dir_name);
	DEBUG("source_dir_name is %s\n", source_dir_name);

	memset(target_dir_name, 0, sizeof(target_dir_name));
	strcat(target_dir_name, tmp_target_dir);
	strcat(target_dir_name, "/");
	strcat(target_dir_name, dir_name);
	DEBUG("target_dir_name1 is %s\n", target_dir_name);

    if (!access(source_dir_name, F_OK)) {
		DEBUG("source directory %s is exist\n", source_dir_name);
    } else {
		DEBUG("source directory %s isnot exist.\n", source_dir_name);
		goto create_white_dir_out;
    }

    if (!access(target_dir_name, F_OK)) {
		DEBUG("target directory %s is exist\n", target_dir_name);
		goto create_white_dir_out;
    } else {
		DEBUG("target directory %s isnot exist.\n", target_dir_name);
    }

	// 检查子字符串目录是否存在.
	char *ch_point = &copy_file_dir[0];
	int file_dir_flag = 0;
	for(point = strlen(dir_name); point > 0; point--) {
		if ( *(ch_point+point) == '/' ) {
			file_dir_flag = 1;
			break;
		}
	}

	if (file_dir_flag == 1) {
		strncpy(tmp_file_dir, dir_name, point+1);  // '\0' will take one character.
		DEBUG("file %s '/' is location at %d, %s\n", dir_name, point, tmp_file_dir);
	}

	/* dir->string 只有一级目录  */
	if (file_dir_flag == 0) {
		ret = symlink(source_dir_name, target_dir_name);
		if (ret) {
			printf("create directory symbol link %s form %s failed!", target_dir_name, source_dir_name);
			goto create_white_dir_out;
		} else {
			DEBUG("create directory symbol link %s form %s success!", target_dir_name, source_dir_name);
		}
	} else if (file_dir_flag == 1) { /* dir->string 只是多级目录  */
		strcat(file_dir_name, tmp_target_dir);
		strcat(file_dir_name, "/");
		strcat(file_dir_name, tmp_file_dir);
		DEBUG("file at  %s dir \n", file_dir_name);
		// file_dir_name 用于保存多级目录前面的目录；
		if (!access(file_dir_name, F_OK)) {
			DEBUG("target file directory %s is exist.\n", file_dir_name);
			ret = symlink(source_dir_name, target_dir_name);
			if (ret) {
				printf("create symbol link %s form %s failed!\n", target_dir_name, source_dir_name);
				goto create_white_dir_out;
			} else {
				DEBUG("create symbol link %s form %s success!\n", target_dir_name, source_dir_name);
			}
		} else {
			DEBUG("target file directory %s isnot exist.\n", file_dir_name);
			// 1, 创建目标目录 然后建立软链接
			ret = createMultiLevelDir(file_dir_name);
			if (ret) {
				printf("createMultiLevelDir %s failed!\n", file_dir_name);
				goto create_white_dir_out;
			}
			ret = symlink(source_dir_name, target_dir_name);
			if (ret) {
				printf("create symbol link %s form %s failed!\n", target_dir_name, source_dir_name);
				goto create_white_dir_out;
			}
		}
	}


create_white_dir_out:
	return ret;
}

// 创建白名单中的文件
// example:
// file:log/log.txt ==> 提取出 "/" 前面的字符.
// mkdir /tmp/tmp.0KAIbR/log;
// ln -s /userdata/log/log1.txt /tmp/tmp.0KAIbR/log/log1.txt
static int create_white_file(const char *file_name)
{
	if (!file_name) {
		return 0;
	}
	int ret = 0;
	size_t point = 0;


	char tmp_file_dir[FILE_SIZE];
	memset(tmp_file_dir, 0, sizeof(tmp_file_dir));
	char file_dir_name[FILE_SIZE];
	memset(file_dir_name, 0, sizeof(file_dir_name));


	char copy_file_dir[FILE_SIZE] = {'\0'};
	strcpy(copy_file_dir, file_name);

	char source_file_name[FILE_SIZE];
	memset(source_file_name, 0, sizeof(source_file_name));

	char target_file_name[FILE_SIZE];
	memset(target_file_name, 0, sizeof(target_file_name));

	strcat(source_file_name, USERDATA_DIR);
	strcat(source_file_name, file_name);

	strcat(target_file_name, tmp_target_dir);
	strcat(target_file_name, "/");
	strcat(target_file_name, file_name);

    if (!access(source_file_name, F_OK)) {
		DEBUG("source file %s is exist.\n", source_file_name);
    } else {
		DEBUG("source file %s isnot exist.\n", source_file_name);
		goto create_white_file_out;
    }

	if (!access(target_file_name, F_OK)) {
		DEBUG("target file %s exist.\n", target_file_name);
		goto create_white_file_out;
	} else {
		DEBUG("target file %s isnot exist.\n", target_file_name);
	}

	// 检查子字符串目录是否存在.
	char *ch_point = &copy_file_dir[0];
	int file_dir_flag = 0;
	for(point = strlen(file_name); point > 0; point--) {
		if ( *(ch_point+point) == '/' ) {
			file_dir_flag = 1;
			break;
		}
	}

	if (file_dir_flag == 1) {
		strncpy(tmp_file_dir, file_name, point+1);  // '\0' will take one character.
		DEBUG("file %s '/' is location at %d, %s\n", file_name, point, tmp_file_dir);
	}

	if (file_dir_flag == 0) {
		ret = symlink(source_file_name, target_file_name);
		if (ret) {
			printf("create symbol link %s form %s failed!\n", target_file_name, source_file_name);
			goto create_white_file_out;
		} else {
			DEBUG("create symbol link %s form %s success!\n", target_file_name, source_file_name);
		}
	} else if (file_dir_flag == 1) {
		strcat(file_dir_name, tmp_target_dir);
		strcat(file_dir_name, "/");
		strcat(file_dir_name, tmp_file_dir);
		DEBUG("file at  %s dir \n", file_dir_name);
		// file_dir_name 用于保存文件前面的目录;
		if (!access(file_dir_name, F_OK)) {
			DEBUG("target file directory %s is exist.\n", file_dir_name);
			ret = symlink(source_file_name, target_file_name);
			if (ret) {
				printf("create symbol link %s form %s failed!\n", target_file_name, source_file_name);
				goto create_white_file_out;
			} else {
				DEBUG("create symbol link %s form %s success!\n", target_file_name, source_file_name);
			}
		} else {
			DEBUG("target file directory %s isnot exist.\n", file_dir_name);
			// 1, 创建目标目录 然后建立软链接
			ret = createMultiLevelDir(file_dir_name);
			if (ret) {
				printf("createMultiLevelDir %s failed!\n", file_dir_name);
				goto create_white_file_out;
			}
			ret = symlink(source_file_name, target_file_name);
			if (ret) {
				printf("create symbol link %s form %s failed!\n", target_file_name, source_file_name);
				goto create_white_file_out;
			}
		}
	}

create_white_file_out:
	return ret;
}

static int parse_ota_image_path(const char *ota_image_path)
{
	int ret = 0;
	size_t item = 0; int dir_flag = 0;
	char ota_img[FILE_SIZE];
	memset(ota_img, 0, sizeof(ota_img));
	char *target_file_name = NULL;
	char *file_dir_name = NULL;
	char *ota_image_dir;
	ota_image_dir = (char *)calloc(strlen(ota_image_path)+1, sizeof(char));
	if(ota_image_dir == NULL) {
		printf("calloc faild");
		goto parse_ota_out;
	}
	strcpy(ota_img, ota_image_path);

	//'\0' & '/' total take two characters.
	target_file_name = (char *)malloc(strlen(tmp_target_dir) + strlen(ota_image_path) + 2);
	if(!target_file_name) {
		sprintf(log, "malloc memory for target_file_name fail\n");
		cleanuserdata_write_log(log);
		goto parse_ota_out;
	}
	memset(target_file_name, 0, strlen(tmp_target_dir) + strlen(ota_image_path) + 2);
	strcat(target_file_name, tmp_target_dir);
	strcat(target_file_name, "/");

	//  judge ota_image_path whether need to keep or not.
	if (strncmp(ota_img, "/userdata/", USERDATA_STRLEN) == 0) {
		strcpy(ota_image_dir, &ota_img[USERDATA_STRLEN]);
		DEBUG("ota_image_dir:%s", ota_image_dir);
		strcat(target_file_name, ota_image_dir);

		// 如果 ota 镜像目录已在保护目录下,则直接退出。
		if (!access(target_file_name, F_OK)) {
			DEBUG("target file %s exist.\n", target_file_name);
			goto parse_ota_out;
		}


		// extract ota image file directory.
		for(item=strlen(ota_image_path);  item >= USERDATA_STRLEN; item--) {
			if(ota_img[item] == '/') {
				dir_flag = 1;
				memset(ota_image_dir, 0, strlen(ota_image_path) + 1);
				strncpy(ota_image_dir, &ota_img[USERDATA_STRLEN], item-USERDATA_STRLEN);
				DEBUG("ota_image_dir:%s", ota_image_dir);
				break;
			}
		}

		//  一级目录下ota_image_dir = all_in_one.zip
		if (dir_flag == 0) {
			symlink(ota_image_path, target_file_name);
		} else if (dir_flag == 1) {  // ota_image_dir = /userdata/test/xxx/xxx/xx/all_in_one.zip
			file_dir_name = (char *)malloc(strlen(tmp_target_dir) + strlen(ota_image_dir) + 2);  // '\0' & '/' take two bytes.
			memset(file_dir_name, 0, strlen(tmp_target_dir) + strlen(ota_image_dir) + 2);

			strcat(file_dir_name, tmp_target_dir);
			strcat(file_dir_name, "/");
			strcat(file_dir_name, ota_image_dir);
			DEBUG("%s at %s dir \n", ota_image_path, file_dir_name);

			if (!access(file_dir_name, F_OK)) {
				DEBUG("target file directory %s is exist.\n", file_dir_name);
				ret = symlink(ota_image_path, target_file_name);
			} else {
				ret = createMultiLevelDir(file_dir_name);
				if (ret) {
					printf("createMultiLevelDir %s failed!\n", file_dir_name);
					goto parse_ota_out;
				}
				ret = symlink(ota_image_path, target_file_name);
			}
		}
	}

parse_ota_out:
	if(file_dir_name)
		free(file_dir_name);
	if(ota_image_dir)
		free(ota_image_dir);
	if(target_file_name)
		free(target_file_name);
	return ret;
}

static int delete_nonwhitelist_file(const char *dir)
{
	int ret = 0;
	char cur_dir[] = ".";
	char up_dir[] = "..";
	char dir_name[FILE_SIZE];
	memset(dir_name, 0, sizeof(dir_name));
	DIR *dirp;
	struct dirent *dp;
	struct stat dir_stat;

	//  参数传递进来的目录不存在，直接返回
	if ( 0 != access(dir, F_OK) ) {
		return 0;
	}

	if((ret = lstat(dir, &dir_stat)) < 0)
		perror("lstat faild\n");

	if (S_ISDIR(dir_stat.st_mode)) {
		if(!strcmp(dir, "/userdata")) {
			dirp = opendir(dir);
			while ( (dp=readdir(dirp)) != NULL ) {
				// ignore . && ..
				if ( (0 == strcmp(cur_dir, dp->d_name)) || (0 == strcmp(up_dir, dp->d_name)) ) {
					continue;
				}
				sprintf(dir_name, "%s/%s", dir, dp->d_name);
				delete_nonwhitelist_file(dir_name);   // 递归调用
			}
			closedir(dirp);
		} else {  // 非 /userdata 目录, 是不是白名单.
			if(!is_in_whitelist(dir)) {
				remove_dir(dir);
			} else {  //  是白名单，查看里面文件是否全部都是白名单，删掉掉里面非白名单的目录 & 文件.
				dirp = opendir(dir);
				while ( (dp=readdir(dirp)) != NULL ) {
				//  ignore . && ..
					if ( (0 == strcmp(cur_dir, dp->d_name)) || (0 == strcmp(up_dir, dp->d_name)) ) {
						continue;
					}
					sprintf(dir_name, "%s/%s", dir, dp->d_name);
					delete_nonwhitelist_file(dir_name);   // 递归调用
				}
				closedir(dirp);
			}
		}
	} else {  // 非目录文件.
		if(!is_in_whitelist(dir)) {
			if (S_ISLNK(dir_stat.st_mode)) {
				unlink(dir);
			} else {
				if((ret = remove(dir)) < 0) {
					perror("remove faild\n");
					return ret;
				}
			}
		}
	}

	return ret;
}

//  将此字符串中p2从第m个字符开始的全部子字符串复制成另一个字符串p1; 字符串从m 后截取
void copy_m(char *from, char *to, int m)
{
	char *from_tmp = from;
	for(; *(from_tmp + m) != '\0'; from_tmp++, to++) {
		*to = *(from_tmp + m);
	}
	*to = '\0';
}

/* parse userdata whitelist and create /tmp/userdata/ tree.
 * filename: must be file absolutely path.
*/
static size_t parse_whitelist_file(char *whitelist_file,
	const char *ota_image_path) {
	FILE *f; long len; char *data;
	int item = 0;
	size_t ret = 0; char userdata_dir_token[] = "./";
	char *dir_path = NULL; char *file_path = NULL;

	if (!(f = fopen(whitelist_file, "rb"))) {
		perror("fopen faild\n");
		return -1;
	}

	fseek(f, 0, SEEK_END);
	if((len = ftell(f)) < 0) {
		fclose(f);
		return -1;
	}
	fseek(f, 0, SEEK_SET);
	data = (char*)malloc(len+1);
	if (!data) {
		perror("malloc for data fail\n");
		fclose(f);
		return -1;
	}
	ret = fread(data,  1, len, f);
	if (ret != len) {
		perror("fread faild\n");
		fclose(f);
		ret = -1;
		goto parse_out;
	}

	fclose(f);

	if(!create_tmp_dir())
		goto parse_out;

	/*
	* interpretation JSON: DIRECTORY
	*/
	cJSON *root = cJSON_Parse(data);
	cJSON *directory = cJSON_GetObjectItem(root, "DIRECTORY");
	if(directory) {
		int dir_number = cJSON_GetArraySize(directory);
		DEBUG("The %s have %d dirctorys\n", directory->string, dir_number);

		for(item = 0; item < dir_number; item++) {
			cJSON *dir_item = cJSON_GetArrayItem(directory, item);
			if (!dir_item) {
				printf("get %s   %d  failed!\n", directory->string, item);
				ret = -1;
				goto parse_out;
			}
			DEBUG("dir: %d name is %s\n", item, dir_item->valuestring);

			if ((strcmp(dir_item->valuestring, userdata_dir_token) == 0) || (strcmp(dir_item->valuestring, "/userdata") == 0) ||
				(strcmp(dir_item->valuestring, "/userdata/") == 0)) {
				// when DIRCTORY element is "./";Will protect /userdata all files.
				protect_all = 1;
			}
			// 白名单解析字符 dir_item->valuestring case1. "./" 表示:"/userdata" 当前路径;
			// 2. "/" 表示: 绝对路径
			// 3. 字符串; 同 case1
			// 创建白名单目录.
			dir_path = (char *)calloc(strlen(dir_item->valuestring) + 1, sizeof(char));
			if (!dir_path) {
				perror("calloc faild\n");
				ret = -1;
				goto parse_out;
			}
			if (strncmp(dir_item->valuestring, userdata_dir_token, strlen(userdata_dir_token)) == 0) {
				copy_m(dir_item->valuestring, dir_path, (int)strlen(userdata_dir_token));
				DEBUG("dir_path %s\n", dir_path);
			} else if (strncmp(dir_item->valuestring, USERDATA_DIR, strlen(USERDATA_DIR)) == 0) {
				copy_m(dir_item->valuestring, dir_path, (int)strlen(USERDATA_DIR));
				DEBUG("dir_path %s\n", dir_path);
			} else if ((strncmp(dir_item->valuestring, "/", strlen("/")) == 0) &&
				strncmp(dir_item->valuestring, USERDATA_DIR, strlen(USERDATA_DIR))) {
				free(dir_path);
				dir_path = NULL;
				continue;
			} else {
				strcpy(dir_path, dir_item->valuestring);
				DEBUG("dir_path %s\n", dir_path);
			}
			ret = create_white_dir(dir_path);
			if (ret) {
				printf("create whitedir %s fail\n", dir_item->valuestring);
				goto parse_out_r;
			}
			free(dir_path);
			dir_path = NULL;
		}
	}
	/*
	* interpretation JSON: FILE
	*/
	cJSON *file = cJSON_GetObjectItem(root, "FILE");
	if(file) {
		int file_number = cJSON_GetArraySize(file);
		DEBUG("The %s have %d files\n", file->string, file_number);

		for(item = 0; item < file_number; item++) {
			cJSON *file_item = cJSON_GetArrayItem(file, item);
			if (!file_item) {
				printf("get file_item %d  failed!\n", item);
				ret = -1;
				free(dir_path);
				goto parse_out;
			}
			DEBUG("file: %d name is %s\n", item, file_item->valuestring);
			file_path = (char *)calloc(strlen(file_item->valuestring) + 1, sizeof(char));
			if (!file_path) {
				perror("calloc faild\n");
				ret = -1;
				free(dir_path);
				goto parse_out;
			}
			if (strncmp(file_item->valuestring, userdata_dir_token, strlen(userdata_dir_token)) == 0) {
				copy_m(file_item->valuestring, file_path,  (int)strlen(userdata_dir_token));
				DEBUG("file_path %s\n", file_path);
			} else if (strncmp(file_item->valuestring, USERDATA_DIR, strlen(USERDATA_DIR)) == 0) {
				copy_m(file_item->valuestring, file_path, strlen(USERDATA_DIR));
				DEBUG("file_path %s\n", file_path);
			} else if ((strncmp(file_item->valuestring, "/", strlen("/")) == 0) &&
				strncmp(file_item->valuestring, USERDATA_DIR, strlen(USERDATA_DIR))) {
				free(file_path);
				file_path = NULL;
				continue;
			} else {
				strcpy(file_path, file_item->valuestring);
				DEBUG("file_path %s\n", file_path);
			}

			ret = create_white_file(file_path);
			if (ret) {
				printf("create whitefile %s fail\n", file_item->valuestring);
				goto parse_out_r;
			}
			free(file_path);
			file_path = NULL;
		}
	}

	/*
	* interpretation OTA image path.
	*/
	if (ota_image_path != NULL) {
		DEBUG("OTA image path: %s\n", ota_image_path);
		ret = parse_ota_image_path(ota_image_path);
		if (ret) {
			printf("create %s fail\n", ota_image_path);
			goto parse_out_r;
		}
	}

parse_out_r:
	if (dir_path)
		free(dir_path);
	if (file_path)
		free(file_path);
	if (data)
		free(data);
	return ret;
parse_out:
	if (data)
		free(data);
	return ret;
}

// assume ota_image path: /userdata/test/all_in_one.zip
int main(int argc,char *argv[])
{
	int ret = 0;
	char *ota_image = NULL;
	if (argc > 2) {
		usage();
		return -1;
	} else if (argc == 2) {
		ota_image = argv[1];
    	if (!(access(ota_image, F_OK))) {
			DEBUG("file ota_image exist.\n");
		} else {
			printf("file ota_image not exist\n");
			cleanuserdata_write_log("file ota_image not exist\n");
			usage();
			return -1;
    	}

		if ( *ota_image != '/' ) {
			printf(" %s, please use absolutely path...\n", ota_image);
			snprintf(log, sizeof(log), " %s, please use absolutely path...\n",
				ota_image);
			cleanuserdata_write_log(log);
			return -1;
		}
	}
    if (!(access(userdata_whitelist, F_OK))) {
		DEBUG("file userdata_whitelist exist.\n");
    } else {
		printf("file userdata_whitelist not exist\n");
		cleanuserdata_write_log("file userdata_whitelist not exist\n");
		return -1;
    }

	// step1: parse_whitelist
	ret = (int)parse_whitelist_file(userdata_whitelist, ota_image);
	if (ret) {
		printf("parse_file failed!\n");
		cleanuserdata_write_log("parse_file failed!\n");
		goto main_out;
	}
	DEBUG("parse_file successful\n");

	if (protect_all == 0) {
		// step2: traversal /userdata file then delete non whitelist file.
		ret = delete_nonwhitelist_file("/userdata");
		if (ret) {
			printf("delete_nonwhitelist_file\n");
			cleanuserdata_write_log("delete_nonwhitelist_file\n");
			goto main_out;
		}
	}
	// step3: clean tmp_target_dir
	ret = remove_dir(tmp_target_dir);
	if (ret) {
		printf("clean_tmp_target_dir fail\n");
		cleanuserdata_write_log("clean_tmp_target_dir fail\n");
		goto main_out;
	}

main_out:
	if(tmp_target_dir)
		free(tmp_target_dir);
	return ret;
}
