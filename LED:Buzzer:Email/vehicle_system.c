#include "Header.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <errno.h>

// 전역 변수 선언
int sock;
char buffer[BUFFER_SIZE];
int led_status = 0;
pthread_mutex_t lock;
int terminate_threads = 0;
bool sleep_state = false;  // 슬립 상태 플래그

// 에러 핸들링 함수
void error_handling(const char *message) {
    perror(message);

    if (led_status) {
        control_led_buzzer(LED_PIN, BUZZER_PIN, LOW, LOW);
    }
    cleanup_led_buzzer_pins(LED_PIN, BUZZER_PIN);

    if (sock > 0) {
        close(sock);
    }
    printf("프로그램 종료: %s\n", message);
    fflush(stdout);
    exit(EXIT_FAILURE);
}

// 문자열의 개행 문자 제거 함수
void trim_newline(char *str) {
    size_t len = strcspn(str, "\r\n");
    str[len] = '\0';
}

// 서버에 연결을 시도하는 함수
int try_connect() {
    struct sockaddr_in server_addr;
    int attempt_count = 0;

    while (1) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            perror("Socket creation failed");
            sleep(10);
            attempt_count++;
            if (attempt_count > 50) {
                fprintf(stderr, "50회 이상 연결 재시도 실패. 종료.\n");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        int optval = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
            perror("setsockopt SO_KEEPALIVE 실패");
            close(sock);
            sleep(10);
            attempt_count++;
            if (attempt_count > 50) {
                fprintf(stderr, "50회 이상 연결 재시도 실패. 종료.\n");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        int keepidle = 60;
        int keepintvl = 10;
        int keepcnt = 5;

        if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle)) < 0) {
            perror("setsockopt TCP_KEEPIDLE 실패");
            close(sock);
            sleep(10);
            attempt_count++;
            if (attempt_count > 50) {
                fprintf(stderr, "50회 이상 연결 재시도 실패. 종료.\n");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl)) < 0) {
            perror("setsockopt TCP_KEEPINTVL 실패");
            close(sock);
            sleep(10);
            attempt_count++;
            if (attempt_count > 50) {
                fprintf(stderr, "50회 이상 연결 재시도 실패. 종료.\n");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt)) < 0) {
            perror("setsockopt TCP_KEEPCNT 실패");
            close(sock);
            sleep(10);
            attempt_count++;
            if (attempt_count > 50) {
                fprintf(stderr, "50회 이상 연결 재시도 실패. 종료.\n");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(65111);
        inet_pton(AF_INET, "192.168.83.4", &server_addr.sin_addr);

        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0) {
            printf("Socket connected. Nice.\n");
            fflush(stdout);
            return 1;
        } else {
            perror("Socket connection failed, retrying...");
            close(sock);
            sleep(10);
            attempt_count++;
            if (attempt_count > 50) {
                fprintf(stderr, "50회 이상 연결 재시도 실패. 종료.\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    return 0;
}

// 소켓 설정 함수
void setup_socket() {
    if (!try_connect()) {
        fprintf(stderr, "여러 번의 재시도 후에도 연결 실패. 종료.\n");
        exit(1);
    }
}

// 슬립 상태로 전환하는 함수
void enter_sleep_state() {
    pthread_mutex_lock(&lock);
    
    if (!sleep_state) {
        const char *sleep_message = "99";
        if (send(sock, sleep_message, strlen(sleep_message), 0) < 0) {
            pthread_mutex_unlock(&lock);
            error_handling("슬립 상태 알림 전송 실패");
        } else {
            printf("서버로 슬립 상태 알림 전송 성공\n");
            fflush(stdout);
        }
        sleep_state = true;  // 슬립 상태 플래그 설정
    } else {
        printf("이미 슬립 상태입니다. '99'를 다시 전송하지 않습니다.\n");
        fflush(stdout);
    }

    control_led_buzzer(LED_PIN, BUZZER_PIN, LOW, LOW);
    cleanup_led_buzzer_pins(LED_PIN, BUZZER_PIN);

    terminate_threads = 1;  
    pthread_mutex_unlock(&lock);

    printf("슬립 상태 전환 완료\n");
    fflush(stdout);
}

// 버튼 모니터링 스레드 함수
void *button_monitor_thread(void *arg) {
    while (1) {
        pthread_mutex_lock(&lock);
        if (terminate_threads) {
            pthread_mutex_unlock(&lock);
            break;
        }

        int button_state = is_button_pressed();

        if (button_state) {
            pthread_mutex_unlock(&lock);
            printf("버튼 눌림 감지! 슬립 상태로 전환합니다.\n");
            fflush(stdout);
            enter_sleep_state();
            break;
        }
        pthread_mutex_unlock(&lock);
        usleep(100000);  // 100ms 대기
    }
    return NULL;
}

// 타이머 스레드 함수
void *timer_thread_func(void *arg) {
    printf("타이머 시작: 30분 후 슬립 상태로 전환됩니다.\n");
    fflush(stdout);

    for (int i = 0; i < 1800; i++) {  // 1800초 = 30분
        sleep(1);
        pthread_mutex_lock(&lock);
        if (terminate_threads) {
            pthread_mutex_unlock(&lock);
            printf("타이머 스레드 종료\n");
            fflush(stdout);
            return NULL;
        }
        pthread_mutex_unlock(&lock);
    }

    pthread_mutex_lock(&lock);
    if (!terminate_threads) {
        pthread_mutex_unlock(&lock);
        printf("타이머 만료: 경찰한테 도움 요청 이메일을 다시 보내고 슬립 상태로 전환합니다.\n");
        send_email("ajoupolice@gmail.com", "[긴급 출동] 차량 방치 사고 발생, 차주 대처 불가 상태", "차주 전화번호 : 010-XXXX-XXXX\n 차량 번호 : XX 가 XXXX\n");
        printf("이메일 전송 완료\n");
        fflush(stdout);
        enter_sleep_state();
    } else {
        pthread_mutex_unlock(&lock);
    }

    printf("타이머 스레드 종료\n");
    fflush(stdout);
    return NULL;
}

// 서버로부터 메시지를 수신하는 스레드 함수
void *receive_message(void *arg) {
    int valread;
    pthread_t timer_thread_id, button_thread_id;

    while (1) {
        printf("서버 메시지 수신 스레드가 read() 대기중\n");
        fflush(stdout);
        valread = read(sock, buffer, BUFFER_SIZE - 1);  // 버퍼 오버플로 방지

        if (valread > 0) {
            buffer[valread] = '\0';
            trim_newline(buffer);
            printf("서버 응답: %s\n", buffer);
            fflush(stdout);

            if (strcmp(buffer, "1") == 0) {
                printf("이메일 전송 요청을 받았습니다.\n");
                fflush(stdout);
                send_email("hyukjun021021@gmail.com", "차량 방치 경고", "온도 상태를 확인하세요.");
                send_email("ajoupolice@gmail.com", "차량 방치 사고 발생", "차주 전화번호 : 010-XXXX-XXXX\n 차량 번호 : XX 가 XXXX\n");
                printf("차주, 경찰한테 이메일 전송 완료\n");
                fflush(stdout);
            } else if (strcmp(buffer, "2") == 0) {
                pthread_mutex_lock(&lock);
                if (sleep_state) {
                    // 슬립 상태 해제
                    printf("슬립 상태에서 '2' 메시지를 수신했습니다. 슬립 상태를 해제하고 동작을 수행합니다.\n");
                    fflush(stdout);
                    sleep_state = false;  // 슬립 상태 해제
                    pthread_mutex_unlock(&lock);

                    // LED, 부저 및 이메일 동작 수행
                    printf("LED, 부저 및 이메일 전송 요청을 수행합니다.\n");
                    fflush(stdout);

                    // 주파수를 2000Hz로 수정
                    if (init_led_buzzer_pins(LED_PIN, BUZZER_PIN, 2000) == -1) {
                        fprintf(stderr, "LED/BUZZER 핀 재초기화 실패\n");
                    }

                    button_init();

                    pthread_mutex_lock(&lock);
                    terminate_threads = 0;  
                    led_status = 1;        
                    pthread_mutex_unlock(&lock);

                    control_led_buzzer(LED_PIN, BUZZER_PIN, HIGH, HIGH);
                    printf("LED, BUZZER 켜짐\n");
                    fflush(stdout);

                    send_email("hyukjun021021@gmail.com", "차량 방치 경고", "즉시 차량 내부를 확인하세요.");
                    send_email("ajoupolice@gmail.com", "차량 방치 사고 발생", "차주 전화번호 : 010-XXXX-XXXX\n 차량 번호 : XX 가 XXXX\n");
                    printf("이메일 전송 완료\n");
                    fflush(stdout);

                    if (pthread_create(&timer_thread_id, NULL, timer_thread_func, NULL) != 0) {
                        printf("타이머 스레드 생성 실패\n");
                        fflush(stdout);
                    }

                    if (pthread_create(&button_thread_id, NULL, button_monitor_thread, NULL) != 0) {
                        printf("버튼 모니터 스레드 생성 실패\n");
                        fflush(stdout);
                    }

                    pthread_join(timer_thread_id, NULL);
                    pthread_join(button_thread_id, NULL);

                    enter_sleep_state();
                    printf("슬립 모드 전환 완료\n");
                    fflush(stdout);
                } else {
                    // 슬립 상태가 아닌 경우, 일반적으로 '2' 메시지를 처리
                    pthread_mutex_unlock(&lock);
                    printf("LED, 부저 및 이메일 전송 요청을 받았습니다.\n");
                    fflush(stdout);

                    // 주파수를 2000Hz로 수정
                    if (init_led_buzzer_pins(LED_PIN, BUZZER_PIN, 2000) == -1) {
                        fprintf(stderr, "LED/BUZZER 핀 재초기화 실패\n");
                    }

                    button_init();

                    pthread_mutex_lock(&lock);
                    terminate_threads = 0;  
                    led_status = 1;        
                    pthread_mutex_unlock(&lock);

                    control_led_buzzer(LED_PIN, BUZZER_PIN, HIGH, HIGH);
                    printf("LED, BUZZER 켜짐\n");
                    fflush(stdout);

                    send_email("hyukjun021021@gmail.com", "차량 방치 경고", "즉시 차량 내부를 확인하세요.");
                    send_email("ajoupolice@gmail.com", "차량 방치 사고 발생", "차주 전화번호 : 010-XXXX-XXXX\n 차량 번호 : XX 가 XXXX\n");
                    printf("이메일 전송 완료\n");
                    fflush(stdout);

                    if (pthread_create(&timer_thread_id, NULL, timer_thread_func, NULL) != 0) {
                        printf("타이머 스레드 생성 실패\n");
                        fflush(stdout);
                    }

                    if (pthread_create(&button_thread_id, NULL, button_monitor_thread, NULL) != 0) {
                        printf("버튼 모니터 스레드 생성 실패\n");
                        fflush(stdout);
                    }

                    pthread_join(timer_thread_id, NULL);
                    pthread_join(button_thread_id, NULL);

                    enter_sleep_state();
                    printf("슬립 모드 전환 완료\n");
                    fflush(stdout);
                }
            }
            continue;
        } else if (valread == 0) {
            printf("서버 연결 종료. 재접속 시도.\n");
            fflush(stdout);
            close(sock);
            setup_socket();
            continue;
        } else {
            perror("서버 메시지 수신 실패");
            error_handling("서버 메시지 수신 실패");
            continue;
        }
    }
    return NULL;
}

// 메인 함수
int main() {
    printf("프로그램 시작\n");
    fflush(stdout);
    if (pthread_mutex_init(&lock, NULL) != 0) {
        fprintf(stderr, "뮤텍스 초기화 실패\n");
        exit(1);
    }
    printf("서버와의 연결 시도를 시작합니다.\n");
    setup_socket();
    GPIOSetup();  
    printf("GPIOSetup 완료\n");
    fflush(stdout);
    button_init();
    printf("button_init 완료\n");
    fflush(stdout);

    // 초기 주파수를 2000Hz로 설정
    if (init_led_buzzer_pins(LED_PIN, BUZZER_PIN, 2000) == -1) {
        fprintf(stderr, "LED/BUZZER 초기화 실패\n");
    }
    printf("LED/BUZZER 초기화 완료\n");
    fflush(stdout);

    pthread_t receive_thread;
    if (pthread_create(&receive_thread, NULL, receive_message, NULL) != 0) {
        printf("receive_message 스레드 생성 실패\n");
        fflush(stdout);
        exit(1);
    }

    // 메인 스레드를 무한 루프로 유지하여 프로그램이 종료되지 않도록 함
    while (1) {
        sleep(60);  // 필요 시 다른 작업을 수행할 수 있음
    }

    // 프로그램이 종료되지 않도록 무한 루프를 사용하므로, 아래 코드는 도달하지 않음
    /*
    pthread_join(receive_thread, NULL);
    printf("receive_message 스레드 종료 확인\n");
    fflush(stdout);

    pthread_mutex_lock(&lock);
    terminate_threads = 1;
    pthread_mutex_unlock(&lock);

    pthread_mutex_destroy(&lock);
    printf("뮤텍스 파괴 완료\n");
    fflush(stdout);

    cleanup_led_buzzer_pins(LED_PIN, BUZZER_PIN);
    printf("프로그램 종료\n");
    fflush(stdout);
    return 0;
    */
}
