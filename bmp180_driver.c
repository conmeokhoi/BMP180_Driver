#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#define DRIVER_NAME "bmp180_driver"

#define BMP180_ADDR 0x77
#define BMP180_REG_CONTROL 0xF4
#define BMP180_REG_RESULT 0xF6
#define BMP180_COMMAND_TEMPERATURE 0x2E
#define BMP180_COMMAND_PRESSURE 0x34
#define BMP180_REG_CALIBRATION_START 0xAA
#define BMP180_CALIBRATION_DATA_LENGTH 22

static struct i2c_client *bmp180_client;

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

static struct bmp180_calibration_data cal_data;

static int bmp180_read_calibration_data(struct i2c_client *client)
{
    u8 buf[BMP180_CALIBRATION_DATA_LENGTH];
    int ret;

    // Read calibration data
    ret = i2c_smbus_read_i2c_block_data(client, BMP180_REG_CALIBRATION_START, 
                                        BMP180_CALIBRATION_DATA_LENGTH, buf);
    if (ret < 0) {
        printk(KERN_ERR "Failed to read BMP180 calibration data\n");
        return ret;
    }

    // Convert calibration data from bytes to values
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

    printk(KERN_INFO "BMP180: Calibration data read successfully\n");
    return 0;
}

static int bmp180_read_uncompensated_temperature(struct i2c_client *client)
{
    int ret;
    u8 buf[2];
    s32 UT;

    // Start temperature measurement
    ret = i2c_smbus_write_byte_data(client, BMP180_REG_CONTROL, BMP180_COMMAND_TEMPERATURE);
    if (ret < 0) {
        printk(KERN_ERR "Failed to start temperature measurement\n");
        return ret;
    }

    // Wait for conversion to complete (standard 5ms)
    msleep(5);

    // Read uncompensated temperature
    ret = i2c_smbus_read_i2c_block_data(client, BMP180_REG_RESULT, 2, buf);
    if (ret < 0) {
        printk(KERN_ERR "Failed to read temperature data\n");
        return ret;
    }

    UT = (buf[0] << 8) | buf[1];
    return UT;
}

static int bmp180_read_uncompensated_pressure(struct i2c_client *client)
{
    int ret;
    u8 buf[3];
    s32 UP;

    // Start pressure measurement (OSS = 0 for lowest resolution but fastest measurement)
    ret = i2c_smbus_write_byte_data(client, BMP180_REG_CONTROL, BMP180_COMMAND_PRESSURE);
    if (ret < 0) {
        printk(KERN_ERR "Failed to start pressure measurement\n");
        return ret;
    }

    // Wait for conversion to complete (standard 5ms for OSS=0)
    msleep(5);

    // Read uncompensated pressure
    ret = i2c_smbus_read_i2c_block_data(client, BMP180_REG_RESULT, 3, buf);
    if (ret < 0) {
        printk(KERN_ERR "Failed to read pressure data\n");
        return ret;
    }

    UP = ((buf[0] << 16) | (buf[1] << 8) | buf[2]) >> 8;
    return UP;
}

static void bmp180_calculate_values(s32 UT, s32 UP)
{
    s32 X1, X2, X3, B3, B5, B6, p;
    u32 B4, B7;
    s32 temperature, pressure;

    // Calculate true temperature
    X1 = ((UT - cal_data.AC6) * cal_data.AC5) >> 15;
    X2 = (cal_data.MC << 11) / (X1 + cal_data.MD);
    B5 = X1 + X2;
    temperature = (B5 + 8) >> 4;

    // Calculate true pressure
    B6 = B5 - 4000;
    X1 = (cal_data.B2 * ((B6 * B6) >> 12)) >> 11;
    X2 = (cal_data.AC2 * B6) >> 11;
    X3 = X1 + X2;
    B3 = (((cal_data.AC1 * 4 + X3) << 0) + 2) / 4;
    X1 = (cal_data.AC3 * B6) >> 13;
    X2 = (cal_data.B1 * ((B6 * B6) >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    B4 = (cal_data.AC4 * (u32)(X3 + 32768)) >> 15;
    B7 = ((u32)UP - B3) * (50000 >> 0);
    
    if (B7 < 0x80000000) {
        p = (B7 * 2) / B4;
    } else {
        p = (B7 / B4) * 2;
    }
    
    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;
    pressure = p + ((X1 + X2 + 3791) >> 4);

    // Print temperature and pressure data
    printk(KERN_INFO "BMP180: Temperature = %d.%d °C, Pressure = %d Pa\n", 
           temperature/10, temperature%10, pressure);
}

static int bmp180_read_data(struct i2c_client *client)
{
    s32 UT, UP;

    // Read uncompensated temperature
    UT = bmp180_read_uncompensated_temperature(client);
    if (UT < 0) {
        return UT;m nộp tất cả file lên github và gửi đường link cho thầy qua folder tạo sẵn trên classroom.
    }

    // Read uncompensated pressure
    UP = bmp180_read_uncompensated_pressure(client);
    if (UP < 0) {
        return UP;
    }

    // Calculate and print values
    bmp180_calculate_values(UT, UP);

    return 0;
}

static int bmp180_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret;

    // Read calibration data
    ret = bmp180_read_calibration_data(client);
    if (ret < 0) {
        printk(KERN_ERR "Failed to read BMP180 calibration data\n");
        return ret;
    }

    bmp180_client = client;

    // Read data from BMP180 sensor
    ret = bmp180_read_data(client);
    if (ret < 0) {
        return ret;
    }

    printk(KERN_INFO "BMP180 driver installed\n");

    return 0;
}

static void bmp180_remove(struct i2c_client *client)
{
    printk(KERN_INFO "BMP180 driver removed\n");
    // Clean up
}

static const struct i2c_device_id bmp180_id[] = {
    { "bmp180", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, bmp180_id);

static struct i2c_driver bmp180_driver = {
    .driver = {
        .name   = DRIVER_NAME,
        .owner  = THIS_MODULE,
    },
    .probe      = bmp180_probe,
    .remove     = bmp180_remove,
    .id_table   = bmp180_id,
};

static int __init bmp180_init(void)
{
    printk(KERN_INFO "Initializing BMP180 driver\n");
    return i2c_add_driver(&bmp180_driver);
}

static void __exit bmp180_exit(void)
{
    printk(KERN_INFO "Exiting BMP180 driver\n");
    i2c_del_driver(&bmp180_driver);
}

module_init(bmp180_init);
module_exit(bmp180_exit);

MODULE_AUTHOR("Nguyen Huynh Khoi_22146024 & Huynh Hai Dang_22146010");
MODULE_DESCRIPTION("BMP180 I2C Client Driver");
MODULE_LICENSE("GPL");