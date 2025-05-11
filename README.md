# BMP180_Driver
Bosch BMP180 Sensor Driver for Raspberry Pi 3b+ | Measure Temperature in C & Pressure in Pa

# HOW TO USE THIS DRIVER 
# 1.Wiring,make sure you've connected your BMP180 correctly to :

VCC ----> 3.3V power (Pin 1)|
GND ----> ground (Pin 6)

SCL ----> I2C1_SCL (Pin 5)|
SDA ----> I2C1_SDA (Pin 3)

# 2.Check if BMP180 connected with code:
```bash
i2cdetect -y 1
```
Make sure it show address as 77. When it show adress as "UU" mean that your sensor in use by another modules, use "sudo rmmod" to remove old driver which using that address.
EX:
```bash
sudo rmmod bmp280_i2c bmp280_spi bmp280
```
# 3.Install driver

In terminal, change to folder which hold source code then install driver:

# 3.1.Compile the Device Tree Overlay

Install the device tree compiler if you haven't already:
```bash
sudo apt-get update
sudo apt-get install device-tree-compiler
```
Compile the overlay file:
```bash
dtc -@ -I dts -O dtb -o bmp180.dtbo bmp180_overlay.dts
```
Move the compiled overlay to the overlays directory:
```bash
sudo cp bmp180.dtbo /boot/overlays/
```
# 3.2.Enable the Overlay

Edit the config.txt file:
```bash
sudo nano /boot/config.txt
```
Add the following lines to enable the I2C interface and load your overlay:
```bash
#Enable I2C
dtparam=i2c_arm=on

#Load BMP180 overlay
dtoverlay=bmp180
```
Save the file and exit then reboot:
```bash
sudo reboot
```
Once rebooted, check if the I2C device is detected with step 2.


# 3.3.Load your driver module (if it's built as a module):
```bash
make
sudo insmod bmp180_driver.ko
```


After installing, check if driver is working correctly:
```bash
sudo dmesg
```
If it show as picture, mean modules is working.

![image](https://github.com/user-attachments/assets/6daaeebf-730a-4ac7-88a3-8eba9a0f07e1)


# 4. Remove the driver 

To remove this driver, type:
```bash
sudo sudo rmmod bmp180_driver
make clean
```
![image](https://github.com/user-attachments/assets/6fb3ff38-4a63-45b9-b0b8-c54cefbca00b)

Driver removed.

## Author 

Nguyen Huynh Khoi 22146024

 Huynh Hai Dang   22146010

 ## Video demo code
 >https://youtu.be/AksHJeWDkb0
