#ifndef BUTTON_H
#define BUTTON_H

#define BUTTON_PIN 20 // 버튼이 연결된 GPIO 핀 번호

// 버튼 초기화
int button_init(void);

// 버튼 상태 읽기
int button_read(void);

// 버튼 상태 초기화 함수
void initialize_button_state(void);

// 버튼이 눌렸는지 확인하는 함수
int is_button_pressed(void);

#endif // BUTTON_H
