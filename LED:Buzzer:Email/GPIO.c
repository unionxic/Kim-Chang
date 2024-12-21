#include "GPIO.h"
#include "Header.h"

#define BUFFER_MAX 3
#define DIRECTION_MAX 128
#define VALUE_MAX 40

#define IN 0
#define OUT 1 
#define LOW 0
#define HIGH 1

// GPIO 내보내기 함수
int GPIOExport(int pin) {
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open export for writing!\n");
        perror("Failed to open export for writing"); // 상세한 오류 메시지
        return -1;
    }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return 0;
}

// GPIO 반환 함수
int GPIOUnexport(int pin) {
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open unexport for writing!\n");
        return -1;
    }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return 0;
}

// GPIO 방향 설정 함수
int GPIODirection(int pin, int dir) {
    const char s_directions_str[] = "in\0out";
    char path[DIRECTION_MAX];
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
    fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror("Failed to open gpio direction for writing!\n");
        fprintf(stderr, "GPIO 핀 %d 방향 설정을 위한 파일 열기 실패\n", pin);
        return -1;
    }

    if (write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3) == -1) {
        perror("Failed to set direction");
        fprintf(stderr, "GPIO 핀 %d 방향 설정 실패\n", pin);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

// GPIO 값 읽기 함수
int GPIORead(int pin) {
    char path[VALUE_MAX];
    char value_str[3];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open gpio value for reading!\n");
        return -1;
    }

    if (read(fd, value_str, 3) == -1) {
        fprintf(stderr, "Failed to read value!\n");
        close(fd);
        return -1;
    }

    close(fd);
    return atoi(value_str);
}

// GPIO 값 쓰기 함수
int GPIOWrite(int pin, int value) {
    char path[VALUE_MAX];
    char value_str[3];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open gpio value for writing!\n");
        return -1;
    }

    snprintf(value_str, 3, "%d", value);
    if (write(fd, value_str, 3) == -1) {
        fprintf(stderr, "Failed to write value!\n");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

void GPIOSetup() {
    GPIOExport(LED_PIN);
    //GPIOExport(BUZZER_PIN);
    GPIOExport(BUTTON_PIN);

    GPIODirection(LED_PIN, OUT);
    //GPIODirection(BUZZER_PIN, OUT);
    GPIODirection(BUTTON_PIN, IN);
}
