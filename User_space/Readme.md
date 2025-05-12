# HOW TO USE THIS DRIVER ON USER-SPACE

After connect and test on kernel-space sucessfully, you can try this code on user space following these step

## 1. Install the driver
```bash
make
sudo insmod bmp180_driver_userspace.ko
```
## 2. Compile the demo code with gcc
```bash
gcc  bmp180_use.c -o bmp180_use
```
## 3. Run the code after compile
```bash
sudo ./bmp180_use
```

If you do it correctly, the result will show as under:

![image](https://github.com/user-attachments/assets/cfa70b3e-d11a-46b1-b289-543694bf2ec4)


# To remove the code, type
```
sudo rmmod bmp180_driver_userspace
make clean
```

# Author 
Nguyen Huynh Khoi 22146024

 Huynh Hai Dang   22146010

# Video demo code

