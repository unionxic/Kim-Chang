#include "Header.h"
#include "led_buzzer.h"
#include "PWM.h"
#include <stdio.h>

static int led_initialized = 0;
static int buzzer_initialized = 0;

int init_led_buzzer_pins(int led_pin, int buzzer_pin, int frequency) {
    if (init_pwm(buzzer_pin, frequency) == -1) {
        fprintf(stderr, "PWM initialization failed for Buzzer\n");
        return -1;
    }
    buzzer_initialized = 1;
    led_initialized = 1;
    return 0;
}

void control_led_buzzer(int led_pin, int buzzer_pin, int led_state, int buzzer_state) {
    if (led_initialized) {
        if (GPIOWrite(led_pin, led_state) == -1) {
            fprintf(stderr, "GPIO 핀 %d 상태 설정 실패\n", led_pin);
        } else {
            printf("LED 핀 %d 상태 설정 완료: %s\n", led_pin, led_state ? "HIGH" : "LOW");
        }
    } else {
        fprintf(stderr, "LED 핀이 초기화되지 않았습니다.\n");
    }

    if (buzzer_initialized) {
        if (buzzer_state == HIGH) {
            printf("부저를 켭니다. (듀티 사이클 50%%)\n");
            if (set_pwm_duty_cycle(buzzer_pin, 50) == -1) {
                fprintf(stderr, "PWM 듀티 사이클 설정 실패\n");
            } else {
                printf("부저 듀티 사이클 설정 완료\n");
            }
        } else {
            printf("부저를 끕니다. (듀티 사이클 0%%)\n");
            if (set_pwm_duty_cycle(buzzer_pin, 0) == -1) {
                fprintf(stderr, "PWM 듀티 사이클 설정 실패\n");
            } else {
                printf("부저 듀티 사이클 설정 완료\n");
            }
        }
    } else {
        fprintf(stderr, "BUZZER 핀이 초기화되지 않았습니다.\n");
    }
}

void cleanup_led_buzzer_pins(int led_pin, int buzzer_pin) {
    if (buzzer_initialized) {
        cleanup_pwm(buzzer_pin);
        buzzer_initialized = 0;
    }
}
