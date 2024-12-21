#ifndef VEHICLE_SAFETY_SYSTEM_H
#define VEHICLE_SAFETY_SYSTEM_H

#define _GNU_SOURCE  // Feature Test Macro 정의

// GPIO 제어 모듈 헤더
#include "GPIO.h"
#include "gpio_pins.h"
#include "led_buzzer.h"
#include "button.h"
#include "email.h"
#include "PWM.h"
//#include "dht11.h"

// WiringPi 라이브러리 (GPIO 및 SPI, I2C 제어)
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <wiringPiI2C.h>

// 네트워크 통신 라이브러리
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <netinet/tcp.h>  // TCP_KEEPIDLE, TCP_KEEPINTVL, TCP_KEEPCNT

// 멀티스레드 라이브러리
#include <pthread.h>

// 시스템 호출 및 유틸리티 라이브러리
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

// 신호 처리 및 시간 관리 라이브러리
#include <signal.h>
#include <time.h>

// cURL 라이브러리 사용 (SMTP Protocol)
#include <curl/curl.h>

// 매크로 정의
#define HIGH 1
#define LOW 0

#define IN 0
#define OUT 1

// 버퍼 크기 정의
#define BUFFER_SIZE 1024

// 전역 변수 선언
extern int sock;                  // 서버 소켓
extern char buffer[BUFFER_SIZE];  // 버퍼
extern int led_status;            // LED와 부저 상태
extern pthread_mutex_t lock;      // 멀티스레드 동기화를 위한 뮤텍스
extern int terminate_threads;     // 스레드 종료 플래그

#endif // VEHICLE_SAFETY_SYSTEM_H
