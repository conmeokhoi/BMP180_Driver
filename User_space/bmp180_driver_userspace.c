#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#define DRIVER_NAME "bmp180_driver"
#define DEVICE_NAME "bmp180"
#define CLASS_NAME "bmp180_class"

#define BMP180_ADDR 0x77
#define BMP180_REG_CONTROL 0xF4
#define BMP180_REG_RESULT 0xF6
#define BMP180_COMMAND_TEMPERATURE 0x2E
#define BMP180_COMMAND_PRESSURE 0x34
#define BMP180_REG_CALIBRATION_START 0xAA
#define BMP180_CALIBRATION_DATA_LENGTH 22

static struct i2c_client *bmp180_client;
static int major_number;
static struct class *bmp180_class = NULL;
static struct device *bmp180_device = NULL;
struct bmp180_calibration_data {
    s16 AC1;
    s16 AC2;
    s16 AC3;
    u16 AC4;
    u16 AC5;
    u16 AC6;
    s16 B1;
    s16 B2;
    s16 MB;
    s16 MC;
    s16 MD;
};
#define BUFFER_SIZE 256
static char data_buffer[BUFFER_SIZE] = "No data\n";
static DEFINE_MUTEX(bmp180_mutex);
static struct bmp180_calibration_data cal_data;
static s32 current_temperature;
static s32 current_pressure;

static ssize_t bmp180_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    ssize_t len = strlen(data_buffer);
    if (*ppos >= len) return 0;
    if (count > len - *ppos) count = len - *ppos;

    if (copy_to_user(buf, data_buffer + *ppos, count)) return -EFAULT;
    *ppos += count;
    return count;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = bmp180_read,
};

static int bmp180_read_calibration_data(struct i2c_client *client) {
    u8 buf[BMP180_CALIBRATION_DATA_LENGTH];
    int ret;

    ret = i2c_smbus_read_i2c_block_data(client, BMP180_REG_CALIBRATION_START,
                                        BMP180_CALIBRATION_DATA_LENGTH, buf);
    if (ret < 0) {
        snprintf(data_buffer, BUFFER_SIZE, "Failed to read calibration data\n");
        return ret;
    }

    cal_data.AC1 = (buf[0] << 8) | buf[1];
    cal_data.AC2 = (buf[2] << 8) | buf[3];
    cal_data.AC3 = (buf[4] << 8) | buf[5];
    cal_data.AC4 = (buf[6] << 8) | buf[7];
    cal_data.AC5 = (buf[8] << 8) | buf[9];
    cal_data.AC6 = (buf[10] << 8) | buf[11];
    cal_data.B1 = (buf[12] << 8) | buf[13];
    cal_data.B2 = (buf[14] << 8) | buf[15];
    cal_data.MB = (buf[16] << 8) | buf[17];
    cal_data.MC = (buf[18] << 8) | buf[19];
    cal_data.MD = (buf[20] << 8) | buf[21];

    printk(KERN_INFO "%s: Calibration data read successfully\n", DRIVER_NAME);
    return 0;
}

static s32 bmp180_read_uncompensated_temperature(struct i2c_client *client) {
    int ret;
    u8 buf[2];
    s32 UT;

    ret = i2c_smbus_write_byte_data(client, BMP180_REG_CONTROL, BMP180_COMMAND_TEMPERATURE);
    if (ret < 0) {
        printk(KERN_ERR "%s: Failed to start temperature measurement\n", DRIVER_NAME);
        return ret;
    }
    msleep(5);

    ret = i2c_smbus_read_i2c_block_data(client, BMP180_REG_RESULT, 2, buf);
    if (ret < 0) {
        printk(KERN_ERR "%s: Failed to read temperature data\n", DRIVER_NAME);
        return ret;
    }

    UT = (buf[0] << 8) | buf[1];
    return UT;
}

static s32 bmp180_read_uncompensated_pressure(struct i2c_client *client) {
    int ret;
    u8 buf[3];
    s32 UP;

    ret = i2c_smbus_write_byte_data(client, BMP180_REG_CONTROL, BMP180_COMMAND_PRESSURE);
    if (ret < 0) {
        printk(KERN_ERR "%s: Failed to start pressure measurement\n", DRIVER_NAME);
        return ret;
    }
    msleep(5);

    ret = i2c_smbus_read_i2c_block_data(client, BMP180_REG_RESULT, 3, buf);
    if (ret < 0) {
        printk(KERN_ERR "%s: Failed to read pressure data\n", DRIVER_NAME);
        return ret;
    }

    UP = ((buf[0] << 16) | (buf[1] << 8) | buf[2]) >> 8;
    return UP;
}

static void bmp180_calculate_values(s32 UT, s32 UP) {
    s32 X1, X2, B5, B6, X3, B3, p;
    u32 B4, B7;
    s32 temperature;

    // Calculate true temperature (in 0.1 °C)
    X1 = ((UT - (s32)cal_data.AC6) * (s32)cal_data.AC5) >> 15;
    X2 = ((s32)cal_data.MC << 11) / (X1 + (s32)cal_data.MD);
    B5 = X1 + X2;
    temperature = (B5 + 8) >> 4;
    current_temperature = temperature;

    // Calculate true pressure (in Pa)
    B6 = B5 - 4000;
    X1 = ((s32)cal_data.B2 * (B6 * B6) >> 12) >> 11;
    X2 = ((s32)cal_data.AC2 * B6) >> 11;
    X3 = X1 + X2;
    B3 = ((((s32)cal_data.AC1 * 4 + X3) << 0) + 2) / 4;
    X1 = ((s32)cal_data.AC3 * B6) >> 13;
    X2 = (((s32)cal_data.B1) * ((B6 * B6) >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    B4 = ((u32)cal_data.AC4 * (u32)(X3 + 32768)) >> 15;
    B7 = ((u32)UP - B3) * 50000;
    if (B7 < 0x80000000) {
        p = (B7 * 2) / B4;
    } else {
        p = (B7 / B4) * 2;
    }
    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;
    current_pressure = p + ((X1 + X2 + 3791) >> 4);
}

static int bmp180_read_data(struct i2c_client *client) {
    s32 UT, UP;

    UT = bmp180_read_uncompensated_temperature(client);
    if (UT < 0) {
        return UT;
    }

    UP = bmp180_read_uncompensated_pressure(client);
    if (UP < 0) {
        return UP;
    }

    bmp180_calculate_values(UT, UP);

    snprintf(data_buffer, BUFFER_SIZE, "Temperature: %d.%d °C\nPressure: %d Pa\n",
             current_temperature / 10, current_temperature % 10, current_pressure);

    return 0;
}

static int bmp180_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    int ret;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C | I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_I2C_BLOCK)) {
        printk(KERN_ERR "%s: Required I2C functionality not available\n", DRIVER_NAME);
        return -EIO;
    }

    bmp180_client = client;

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ERR "%s: Failed to register character device\n", DRIVER_NAME);
        return major_number;
    }
    printk(KERN_INFO "%s: Registered with major number %d\n", DRIVER_NAME, major_number);

    bmp180_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(bmp180_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "%s: Failed to create class\n", DRIVER_NAME);
        return PTR_ERR(bmp180_class);
    }

    bmp180_device = device_create(bmp180_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(bmp180_device)) {
        class_destroy(bmp180_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "%s: Failed to create device\n", DRIVER_NAME);
        return PTR_ERR(bmp180_device);
    }
    printk(KERN_INFO "%s: Device created at /dev/%s\n", DRIVER_NAME, DEVICE_NAME);

    mutex_init(&bmp180_mutex);

    ret = bmp180_read_calibration_data(client);
    if (ret < 0) {
        device_destroy(bmp180_class, MKDEV(major_number, 0));
        class_destroy(bmp180_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        mutex_destroy(&bmp180_mutex);
        return ret;
    }

    // Đọc dữ liệu ban đầu để có giá trị hiển thị khi device được tạo
    bmp180_read_data(client);

    return 0;
}

static void bmp180_remove(struct i2c_client *client) {
    device_destroy(bmp180_class, MKDEV(major_number, 0));
    class_unregister(bmp180_class);
    class_destroy(bmp180_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    mutex_destroy(&bmp180_mutex);
    printk(KERN_INFO "%s: Driver removed\n", DRIVER_NAME);
}

static const struct i2c_device_id bmp180_id[] = {
    { "bmp180", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, bmp180_id);

static struct i2c_driver bmp180_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
    },
    .probe = bmp180_probe,
    .remove = bmp180_remove,
    .id_table = bmp180_id,
};

module_i2c_driver(bmp180_driver);

MODULE_AUTHOR("Nguyen Huynh Khoi_22146024 & Huynh Hai Dang_22146010");
MODULE_DESCRIPTION("BMP180 I2C Client Driver");
MODULE_LICENSE("GPL");