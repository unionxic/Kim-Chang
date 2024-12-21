#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include "button.h"

// 버튼 초기화
int button_init(void) {
    // WiringPi 초기화
    if (wiringPiSetupGpio() == -1) {  // GPIO 번호 체계를 사용
        fprintf(stderr, "WiringPi 초기화 실패!\n");
        exit(1);
    }

    // BUTTON_PIN을 입력 모드로 설정하고 풀업 저항 활성화
    pinMode(BUTTON_PIN, INPUT);
    pullUpDnControl(BUTTON_PIN, PUD_UP);
    
    return 0;
}

// 버튼 상태 읽기
int button_read(void) {
    return digitalRead(BUTTON_PIN);
}

// 버튼 상태 초기화 함수 구현
void initialize_button_state(void) {
    if (button_read() == LOW) {
        printf("프로그램 시작 시 버튼이 눌려 있습니다. 초기화합니다.\n");
    }
}

// 버튼이 눌렸는지 확인하는 함수 구현
int is_button_pressed(void) {
    if (button_read() == LOW) {
        delay(50); // 디바운싱 처리 (50ms 대기)
        if (button_read() == LOW) {
            return 1; // 버튼이 눌린 상태로 확인됨
        }
    }
    return 0;
}
