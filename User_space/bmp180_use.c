#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("/dev/bmp180", O_RDONLY);
    if (fd < 0) {
        perror("Open failed");
        return 1;
    }

    char buf[256];
    ssize_t ret = read(fd, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        printf("Data from BMP180: %s", buf);
    }

    close(fd);
    return 0;
}
