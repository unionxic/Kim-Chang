#ifndef PWM_H
#define PWM_H

// PWM 초기화 함수
// pin: PWM 핀 번호
// frequency: PWM 주파수
int init_pwm(int pin, int frequency);

// PWM 듀티 사이클 설정 함수
// pin: PWM 핀 번호
// duty_cycle: 0-100 범위의 듀티 사이클 퍼센트
int set_pwm_duty_cycle(int pin, int duty_cycle);

// PWM 해제 함수
// pin: PWM 핀 번호
int cleanup_pwm(int pin);

#endif // PWM_H
