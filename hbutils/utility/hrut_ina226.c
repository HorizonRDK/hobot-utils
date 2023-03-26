/*
 * Copyright (c) 2019 Horizon Robotics
 */

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

/* common register definitions */
#define INA2XX_CONFIG 0x00
#define INA2XX_SHUNT_VOLTAGE 0x01 /* readonly */
#define INA2XX_BUS_VOLTAGE 0x02   /* readonly */
#define INA2XX_POWER 0x03         /* readonly */
#define INA2XX_CURRENT 0x04       /* readonly */
#define INA2XX_CALIBRATION 0x05

/* INA226 register definitions */
#define INA226_MASK_ENABLE 0x06
#define INA226_ALERT_LIMIT 0x07
#define INA226_DIE_ID 0xFF

#define ERROR_CODE_TRUE 0
#define ERROR_CODE_FALSE 1
#define ERROR_CODE_WRITE_ADDR 10
#define ERROR_CODE_WRITE_DATA 20
#define ERROR_CODE_READ_ADDR 30
#define ERROR_CODE_READ_DATA 40
#define ERROR_CODE_START_BIT 50
#define ERROR_CODE_APROCESS 60
#define ERROR_CODE_DENY 70

#define I2C_M_RD 0x0001
#define I2C_RETRIES 0x0701
#define I2C_TIMEOUT 0x0702
#define I2C_RDWR 0x0707

struct i2c_msg
{
    u_int16_t addr;
    u_int16_t flags;
    u_int16_t len;
    u_int8_t *buf;
};

struct i2c_ioctl_data
{
    struct i2c_msg *msgs;
    unsigned int nmsgs;
};

float shunt_resis = 5.0f;
u_int8_t dev_addr = 0x45;
int average_times = 50;
int interval_time = 20;

void print_usage(void)
{
    puts(
        "Usage: hrut_ina226 -A [device address] -R [resistance value]\n"
        "  -A --address       I2C address of ina226(Hex),\n"
        "                     default is 0x45\n"
        "                     X3-DVB && X3-CVB: 0x40\n"
        "                      e.g: 0x45\n"
        "  -R --resistance    value of shunt resistance(milli-ohm),\n"
        "                     default is 5.0\n"
        "                      e.g: 5.0\n"
        "  -c --count         average count\n"
        "                     default is 50\n"
        "  -i --interval-time interva tims(millisecond)\n"
        "                     default is 20\n"
        "  -h --help\n"
        "Example: \n"
        "multimode S202:\n"
        "\thrut_ina226 -A 0x45 -R 5.0\n"
        "X3-DVB && X3-CVB\n"
        "\thrut_ina226 -A 0x40 -R 5.0\n");
}
int parse_opt(int argc, char *argv[])
{
    int ret = 0;
    while (1)
    {
        int cmd_ret;
        int tmp_avg_times = 50;
        int tmp_interval_time = 20;
        static const char short_options[] = "A:R:h:c:i:";
        static const struct option long_options[] = {
            {"address", 1, 0, 'A'},
            {"resistance", 1, 0, 'R'},
            {"count", 1, 0, 'c'},
            {"interval-time", 1, 0, 'i'},
            {"help", 0, 0, 'h'},
            {NULL, 0, 0, 0},
        };

        cmd_ret = getopt_long(argc, argv, short_options, long_options, NULL);

        if (cmd_ret == -1)
            break;
        switch (cmd_ret) {
        case 'A':
            sscanf(optarg, "0x%c", &dev_addr);
            break;
        case 'R':
            sscanf(optarg, "%f", &shunt_resis);
            break;
        case 'c':
            sscanf(optarg, "%d", &tmp_avg_times);
            average_times = tmp_avg_times > 1 ? tmp_avg_times : 1;
            break;
        case 'i':
            sscanf(optarg, "%d", &tmp_interval_time);
            interval_time = tmp_interval_time > 1 ? tmp_interval_time : 1;
            break;
        case 'h':
        case '?':
            print_usage();
            ret = -1;
            break;
        default:
            break;
        }
    }
    return ret;
}

int ina226_i2c_read(int fd, u_int8_t dev_addr, u_int8_t sub_addr,
                    u_int16_t *data)
{
    int ret = ERROR_CODE_TRUE;

    struct i2c_ioctl_data i2c_data;
    u_int8_t write_buff = sub_addr;
    u_int8_t buff[2] = {0};

    ioctl(fd, I2C_TIMEOUT, 2);  // set I2C timeout
    ioctl(fd, I2C_RETRIES, 1);  // set retries times

    i2c_data.msgs = malloc(2 * sizeof(struct i2c_msg));

    i2c_data.nmsgs = 2;
    (i2c_data.msgs[0]).len = 1;
    (i2c_data.msgs[0]).addr = dev_addr;
    (i2c_data.msgs[0]).flags = 0;
    (i2c_data.msgs[0]).buf = &write_buff;

    (i2c_data.msgs[1]).len = 2;
    (i2c_data.msgs[1]).addr = dev_addr;
    (i2c_data.msgs[1]).flags = I2C_M_RD;
    (i2c_data.msgs[1]).buf = buff;

    ret = ioctl(fd, I2C_RDWR, (u_int64_t)&i2c_data);
    if (ret < 0) {
        printf("ioctl read error, ret = 0x%x\n", ret);
    } else {
        *data = (u_int16_t)((buff[0] << 8) + buff[1]);
    }

    free(i2c_data.msgs);
    return ret;
}

int ina226_i2c_write(int fd, u_int8_t dev_addr, u_int8_t sub_addr,
                     u_int16_t data)
{
    int ret = ERROR_CODE_TRUE;
    struct i2c_ioctl_data i2c_data;

    ioctl(fd, I2C_TIMEOUT, 2);  // set I2C timeout
    ioctl(fd, I2C_RETRIES, 1);  // set retries times

    i2c_data.nmsgs = 1;
    i2c_data.msgs = malloc(i2c_data.nmsgs * sizeof(struct i2c_msg));

    (i2c_data.msgs[0]).len = 2 + 1;
    (i2c_data.msgs[0]).addr = dev_addr;
    (i2c_data.msgs[0]).flags = 0;
    (i2c_data.msgs[0]).buf = malloc(2 + 1);

    (i2c_data.msgs[0]).buf[0] = sub_addr;
    (i2c_data.msgs[0]).buf[1] = (u_int8_t)((data >> 8) & 0xff);
    (i2c_data.msgs[0]).buf[2] = (u_int8_t)(data & 0xff);

    ret = ioctl(fd, I2C_RDWR, (u_int64_t)&i2c_data);
    if (ret < 0) {
        printf("ioctl write error, ret = 0x%x \n", ret);
    }
    free((i2c_data.msgs[0]).buf);
    free(i2c_data.msgs);

    return ret;
}

int main(int argc, char *argv[])
{
    int fd = -1;
    int i = 0;
    u_int16_t data;
    float voltage_total = 0.0;
    float current_total = 0.0;
    float power_total = 0.0;

    if (parse_opt(argc, argv) < 0)
        return -1;
    float current_lsb = 2e-4f;
    float voltage_lsb = 1.25e-3f;
    float power_lsb = 25.0f * current_lsb;
    u_int16_t cal_value =
        (u_int16_t)(5.12f / (current_lsb * (float)shunt_resis));

    fd = open("/dev/i2c-0", O_RDWR);
    if (fd < 0) {
        printf("open /dev/i2c-0 failed\n");
        return -1;
    }

    if (ina226_i2c_read(fd, dev_addr, INA226_DIE_ID, &data) < 0) {
        printf("read Die ID failed\n");
        goto error;
    }
    if (data != 0x2260) {
        printf("device is't INA226\n");
        goto error;
    }

    if (ina226_i2c_write(fd, dev_addr, INA2XX_CALIBRATION, cal_value) < 0) {
        printf("write Calibration register failed\n");
        goto error;
    }
    usleep(10000);
    for (i = 0; i < average_times; ++i) {
        if (ina226_i2c_read(fd, dev_addr, INA2XX_BUS_VOLTAGE, &data) < 0) {
            printf("read bus voltage failed\n");
            goto error;
        }
        voltage_total += (float)data * voltage_lsb;

        if (ina226_i2c_read(fd, dev_addr, INA2XX_CURRENT, &data) < 0) {
            printf("read current failed\n");
            goto error;
        }
        current_total += (float)fabs((float)data * current_lsb);

        if (ina226_i2c_read(fd, dev_addr, INA2XX_POWER, &data) < 0) {
            printf("read power failed\n");
            goto error;
        }
        power_total += (float)data * power_lsb;
        usleep((uint32_t)interval_time*1000);
    }
    printf("Voltage: %.4f V\n", (float)voltage_total / (float)average_times);
    printf("Current: %.4f A\n",
    	fabs((float)current_total / (float)average_times));
    printf("Power  : %.4f W\n", (float)power_total / (float)average_times);
error:
    close(fd);
    return 0;
}
