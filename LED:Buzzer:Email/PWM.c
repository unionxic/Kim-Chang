#include "Header.h"

#define PWM_CHIP_PATH "/sys/class/pwm/pwmchip0"

// Helper 함수: 파일에 쓰기
static int write_to_file(const char *path, const char *value) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        perror("Failed to open file");
        return -1;
    }
    fprintf(fp, "%s", value);
    fclose(fp);
    return 0;
}

// GPIO 핀을 PWM 채널로 매핑
static int gpio_to_pwm_channel(int pin) {
    switch(pin) {
        case 18:
            return 0;
        case 19:
            return 1;
        default:
            fprintf(stderr, "GPIO pin %d does not support PWM\n", pin);
            return -1;
    }
}

// PWM 초기화 함수
int init_pwm(int pin, int frequency) {
    char path[64];
    int channel = gpio_to_pwm_channel(pin);
    if (channel == -1) {
        return -1;
    }

    // PWM export 확인
    snprintf(path, sizeof(path), "%s/pwm%d", PWM_CHIP_PATH, channel);
    if (access(path, F_OK) == -1) {
        // PWM export
        snprintf(path, sizeof(path), "%s/export", PWM_CHIP_PATH);
        char channel_str[3];
        snprintf(channel_str, sizeof(channel_str), "%d", channel);
        if (write_to_file(path, channel_str) < 0) {
            perror("PWM export failed");
            return -1;
        }

        // 설정이 적용될 때까지 대기
        usleep(100000); // 100ms
    }

    // PWM 주기(period) 설정
    snprintf(path, sizeof(path), "%s/pwm%d/period", PWM_CHIP_PATH, channel);
    char period_ns[16];
    snprintf(period_ns, sizeof(period_ns), "%ld", 1000000000 / frequency); // 주기(ns) 계산
    if (write_to_file(path, period_ns) < 0) {
        perror("PWM period setting failed");
        return -1;
    }

    // 초기 듀티 사이클 설정 (0%)
    snprintf(path, sizeof(path), "%s/pwm%d/duty_cycle", PWM_CHIP_PATH, channel);
    if (write_to_file(path, "0") < 0) {
        perror("PWM duty_cycle initialization failed");
        return -1;
    }

    // PWM 활성화
    snprintf(path, sizeof(path), "%s/pwm%d/enable", PWM_CHIP_PATH, channel);
    if (write_to_file(path, "1") < 0) {
        perror("PWM enable failed");
        return -1;
    }

    printf("PWM 핀 %d 초기화 완료 (채널 %d)\n", pin, channel);
    return 0;
}

// PWM 듀티 사이클 설정 함수
int set_pwm_duty_cycle(int pin, int duty_cycle) {
    char path[64];
    int channel = gpio_to_pwm_channel(pin);
    if (channel == -1) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s/pwm%d/duty_cycle", PWM_CHIP_PATH, channel);

    // duty_cycle을 퍼센트로 설정
    // period를 읽어서 duty_cycle 계산
    char period_path[64];
    snprintf(period_path, sizeof(period_path), "%s/pwm%d/period", PWM_CHIP_PATH, channel);
    FILE *fp = fopen(period_path, "r");
    if (!fp) {
        perror("Failed to open period file");
        return -1;
    }
    long period;
    if (fscanf(fp, "%ld", &period) != 1) {
        perror("Failed to read period");
        fclose(fp);
        return -1;
    }
    fclose(fp);

    long duty = (period * duty_cycle) / 100;

    char duty_cycle_ns[16];
    snprintf(duty_cycle_ns, sizeof(duty_cycle_ns), "%ld", duty);
    if (write_to_file(path, duty_cycle_ns) < 0) {
        perror("PWM duty_cycle setting failed");
        return -1;
    }

    printf("PWM 핀 %d 듀티 사이클 %d%%로 설정 (채널 %d)\n", pin, duty_cycle, channel);
    return 0;
}

// PWM 해제 함수
int cleanup_pwm(int pin) {
    char path[64];
    int channel = gpio_to_pwm_channel(pin);
    if (channel == -1) {
        return -1;
    }

    // PWM 비활성화
    snprintf(path, sizeof(path), "%s/pwm%d/enable", PWM_CHIP_PATH, channel);
    if (write_to_file(path, "0") < 0) {
        perror("PWM disable failed");
        return -1;
    }

    // PWM unexport
    snprintf(path, sizeof(path), "%s/unexport", PWM_CHIP_PATH);
    char channel_str[3];
    snprintf(channel_str, sizeof(channel_str), "%d", channel);
    if (write_to_file(path, channel_str) < 0) {
        perror("PWM unexport failed");
        return -1;
    }

    printf("PWM 핀 %d 해제 완료 (채널 %d)\n", pin, channel);
    return 0;
}
