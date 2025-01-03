# 차량 방치 사고 방지 시스템 (김앤장 시프사무소)

## 프로젝트 개요
- **목표**: 차량 내 영유아 및 반려동물 방치 사고를 방지하기 위해 온습도 센서, 압력 센서, 카메라 등을 활용하여 차량 내부 상황을 모니터링하고, 위험 상황 발생 시 경고 및 알림을 제공.
- **필요성**: 차량 내부 온도 급상승으로 인한 사고 예방을 목적으로 하며, 긴급 상황 발생 시 구조를 신속히 요청할 수 있도록 설계.
- **특징**: 
  - 온습도, 압력 센서, 카메라를 활용한 상황 분석.
  - IoT 시스템 기반의 실시간 모니터링 및 경고 시스템 구현.
  - 낮은 비용으로 상용 시스템 일부 기능을 구현.

---

## 주요 기술
- **센서 활용**:
  - 온도/습도 센서: 차량 내부 환경 모니터링.
  - 압력 센서: 운전자 유무 감지.
  - 카메라: 객체(사람, 반려동물) 탐지.
- **알림 시스템**:
  - LED 및 부저로 즉각적 경고 제공.
  - 이메일을 통해 사용자 및 보호자에게 알림.
- **네트워크**: 
  - 소켓 통신을 활용한 Raspberry Pi 간 데이터 송수신.
  - TCP KeepAlive로 안정적인 연결 유지.

---

## 시스템 구조
1. **하드웨어 구성**:
   - **Main Pi**: 온습도/압력, 카메라, LED/부저 파이와 통신하며 전체 로직 제어.
   - **Slave Pi 1**: 온습도 및 압력 센싱.
   - **Slave Pi 2**: 카메라를 통해 객체 감지.
   - **Slave Pi 3**: LED/부저 제어 및 이메일 발신.
2. **작동 흐름**:
   - 온습도/압력 상태 확인 → 위험 상황 감지 시 메인 파이에 알림.
   - 객체 감지 필요 시 카메라 파이 활성화.
   - LED/부저 작동 및 이메일 알림 전송.

---

## 주요 알고리즘
- **온습도/압력 파이**:
  - 온도 범위에 따라 주기적으로 데이터 측정.
  - 비정상 온도 지속 시 메인 파이에 알림.
- **카메라 파이**:
  - 메인 파이 신호에 따라 객체 탐지 수행.
  - YOLO-tiny 모델로 20초 동안 6초 간격으로 탐지.
- **LED/부저 파이**:
  - 경고 및 위험 상태 시 LED, 부저 작동 및 이메일 발송.
  - 버튼 입력 또는 타이머 만료 시 작동 종료.

---

## 실행 방법
1. **컴파일 명령어**:
   - 온습도/압력 파이:
     ```bash
     gcc -o temperPI temperPI.c dht11.c -lwiringPi -lpigpio -lpthread
     ```
   - LED/부저 파이:
     ```bash
     gcc -o vehicle_system vehicle_system.c GPIO.c led_buzzer.c button.c email.c PWM.c -I/usr/local/include -L/usr/local/lib -lwiringPi -lpthread -lcurl
     ```
   - 카메라 파이:
     ```bash
     g++ -std=c++17 -O3 -Wall -Wextra -pthread -o test Camera.cpp GPIO.c -I./darknet/include -L./darknet -ldarknet $(pkg-config --cflags --libs opencv4) -lpthread
     ```
   - 메인 파이:
     ```bash
     gcc -o final_main final_main.c -lpthread
     ```
2. **실행 순서**:
   - 메인 파이를 서버 모드로 실행.
   - 각 Slave Pi를 메인 파이에 연결 후 실행.

---

## 성과 및 확장 가능성
- **성과**:
  - 차량 내 방치 사고 방지를 위한 IoT 시스템 성공 구현.
  - 효율적 자원 사용을 위한 주기 제어 및 인터럽트 기반 설계.
  - 낮은 비용으로 상용 시스템 일부 기능을 대체.
- **확장 가능성**:
  - GPS 모듈 추가로 위치 기반 알림 시스템 구축 가능.
  - 초음파 센서 등 추가 센서를 통해 정확도 향상.

---

## 참고 자료
- 질병관리청 온도별 인체 영향 보고서
- OpenCV & YOLO-tiny 공식 문서
- Raspberry Pi I/O 매뉴얼
