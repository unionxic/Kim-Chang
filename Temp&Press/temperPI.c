    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <pthread.h>
    #include <pigpio.h>
    #include <wiringPi.h>
    #include <time.h>  
    #include "GPIO.h"
    #include "dht11.h"
    #include <sys/socket.h>  
    #include <arpa/inet.h>   
    #include <errno.h>
    #include <netinet/tcp.h> // TCP_KEEPIDLE, TCP_KEEPINTVL, TCP_KEEPCNT 옵션 위해 필요

    #define BUFFER_SIZE 1024

    // 상수 정의
    #define PRESSURE_SENSOR_PIN 17 
    #define SERVER_SIGNAL_PIN 23   
    #define CLIENT_SIGNAL_PIN 22   
    #define PRESSURE_THRESHOLD_HIGH 1  
    #define PRESSURE_THRESHOLD_LOW 0

    #define PORT 65111       
    #define SERVER_IP "192.168.83.4" 

    volatile int pressure = 0;  
    volatile int stop_flag = 0; 
    volatile int as = 1;
    volatile int set = 0;

    volatile int pressureChanged = 0;
    volatile int serverMessageReceived = 0;
    pthread_mutex_t flagMutex = PTHREAD_MUTEX_INITIALIZER;

    // 디바운싱 변수
    volatile unsigned long last_interrupt_time = 0;
    const unsigned long DEBOUNCE_TIME = 200000;

    int client_socket = -1;

    //-----------------------------------------------------------------------
    // TCP Keep-Alive 기반 소켓 연결 함수
    // 프로그램 시작 시 setup_socket()으로 서버와 연결을 시도하며,
    // 이후 recv()와 Keep-Alive를 통해 연결 상태를 모니터링하고,
    // 끊길 경우 즉시 재연결 시도. 메인 루프에서 주기적으로 확인하지 않음.
    //-----------------------------------------------------------------------
    int try_connect() {
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == -1) {
            perror("[ERROR] Socket creation failed");
            return -1;
        }

        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
            perror("[ERROR] Invalid server IP");
            close(client_socket);
            client_socket = -1;
            return -1;
        }

        if (connect(client_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("[ERROR] Server connection failed");
            close(client_socket);
            client_socket = -1;
            return -1;
        }

        printf("[DEBUG] Connected to server at %s:%d\n", SERVER_IP, PORT);

        // TCP Keep-Alive 설정
        int optval = 1;
        if (setsockopt(client_socket, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
            perror("[ERROR] Failed to enable SO_KEEPALIVE");
            close(client_socket);
            client_socket = -1;
            return -1;
        }

        int keepidle = 10;
        if (setsockopt(client_socket, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle)) < 0) {
            perror("[ERROR] Failed to set TCP_KEEPIDLE");
        }

        int keepintvl = 10;
        if (setsockopt(client_socket, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl)) < 0) {
            perror("[ERROR] Failed to set TCP_KEEPINTVL");
        }

        int keepcnt = 5;
        if (setsockopt(client_socket, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt)) < 0) {
            perror("[ERROR] Failed to set TCP_KEEPCNT");
        }

        return 0;
    }

    void setup_socket() {
        const int MAX_RETRY_COUNT = 50;
        int retry_count = 0;
        while (retry_count < MAX_RETRY_COUNT) {
            if (try_connect() == 0) {
                return; // 연결 성공
            }
            retry_count++;
            printf("[ERROR] Unable to connect. Retrying in 10 seconds... (%d/%d)\n", retry_count, MAX_RETRY_COUNT);
            sleep(10);
        }

        fprintf(stderr, "[ERROR] Exceeded maximum reconnection attempts. Exiting.\n");
        exit(1);
    }

    void cleanup_socket() {
        if (client_socket != -1) {
            close(client_socket);
            client_socket = -1;
        }
        printf("[DEBUG] Socket closed\n");
    }

    //-----------------------------------------------------------------------
    // 기존 로직 (압력 센서, 온도 측정, 상태 모니터링) 변경 없음
    //-----------------------------------------------------------------------
    void pressureInterrupt() {
        unsigned long current_time = gpioTick();

        if (current_time - last_interrupt_time < DEBOUNCE_TIME) {
            return;
        }
        last_interrupt_time = current_time;

        static int last_state = -1;
        int current_state = gpioRead(PRESSURE_SENSOR_PIN);

        if (current_state != last_state) {
            last_state = current_state;
            pthread_mutex_lock(&flagMutex);
            pressureChanged = 1;
            pthread_mutex_unlock(&flagMutex);
        }
    }

    int getPressure() {
        int sensor_pin = PRESSURE_SENSOR_PIN; 
        int level = gpioRead(sensor_pin); 
        if (level >= PRESSURE_THRESHOLD_HIGH) {
            return 1; 
        } else if (level <= PRESSURE_THRESHOLD_LOW) {
            return 0; 
        }
        return pressure; 
    }

    float getTemperatureValue() {
        float temperature = 0.0, humidity = 0.0;
        while(1){
            if (readDHT11(&temperature, &humidity) == 0) {
                return temperature; 
            } 
        }
    }

    void waitForPressureChange() {
        printf("[DEBUG] waitForPressureChange 시작\n");
        set = 0;
        pthread_mutex_lock(&flagMutex);
        serverMessageReceived = 0;  
        pthread_mutex_unlock(&flagMutex);

        while (1) {
            pthread_mutex_lock(&flagMutex);
            if (serverMessageReceived) {
                pthread_mutex_unlock(&flagMutex);
                break;
            }
            pthread_mutex_unlock(&flagMutex);
            usleep(100000); // 0.1초 대기
        }
        printf("위급 상황이 종료되었습니다. 프로그램을 재시작합니다.\n");
    }

    void startTimer(int seconds) {
        printf("%d초 동안 타이머 실행 중...\n", seconds);
        sleep(seconds);
    }

    int checkTemperatureRange(float temperature) {
        if (temperature >= 15.0 && temperature <= 25.0) return 1;
        else if ((temperature >= 5.0 && temperature < 15.0) || (temperature > 25.0 && temperature <= 35.0)) return 2;
        else if ((temperature >= 0.0 && temperature < 5.0) || (temperature > 35.0 && temperature <= 40.0)) return 3;
        else return 4;
    }

    int monitorTemperatureWithShortIntervals(int duration_seconds, int interval_seconds, int bfrange) {
        int elapsed_time = 0;
        int i=0;
        set = 1;
        sleep(interval_seconds);
        while (elapsed_time < duration_seconds && !stop_flag) {
            float temperature = getTemperatureValue();
            i++;
            printf("%d초 후 측정된 온도: %.2f°C\n", i*interval_seconds, temperature);
            
            int range = checkTemperatureRange(temperature);
            if (range == 1 || range == 2) {
                printf("온도가 구간 %d로 복구되었습니다.\n", range);
                return range; 
            }
            
            if (bfrange == 3 && range == 4) {
                printf("온도가 위험온도입니다. 위험 온도에 대응\n");
                return range;
            } else if (bfrange == 4 && range == 3) {
                printf("온도가 경고온도입니다. 경고 온도에 대응\n");
                return range;
            }

            if (elapsed_time + interval_seconds < duration_seconds) {
                sleep(interval_seconds);
                elapsed_time += interval_seconds;
            } else {
                break; 
            }
        }
        return 0;
    }

    void sendToServer(const char *message) {
        printf("서버에 메시지 전송: %s\n", message);
        for (int i = 0; message[i] != '\0'; i++) {
            char bit = message[i];
            if (bit == '1') {
                gpioWrite(SERVER_SIGNAL_PIN, 1); 
            } else {
                gpioWrite(SERVER_SIGNAL_PIN, 0); 
            }
            usleep(500000); 
        }
        gpioWrite(SERVER_SIGNAL_PIN, 0); 
    }

    //-----------------------------------------------------------------------
    // 서버 메시지 수신 스레드
    // recv()를 통해 서버 메시지 및 상태 모니터링
    // 끊김 감지 시 즉시 재연결 시도
    // 메인 함수에서 주기적으로 확인하는 루프 없음
    //-----------------------------------------------------------------------
    void *serverMessageThread(void *arg) {
        (void)arg;
        char buffer[BUFFER_SIZE];

        while (!stop_flag) {
            memset(buffer, 0, sizeof(buffer));
            int bytesRead = recv(client_socket, buffer, sizeof(buffer)-1, 0);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                int serverCode = atoi(buffer);

                if (serverCode == 99) {
                    printf("서버로부터 코드 %d 수신\n", serverCode);
                    printf(" 위급 상황 종료\n");
                    pthread_mutex_lock(&flagMutex);
                    serverMessageReceived = 1;
                    pthread_mutex_unlock(&flagMutex);

                } 
                else if (serverCode == 100) {
                    printf("서버로부터 코드 %d 수신\n", serverCode);
                    printf("객체가 인식되지 않았습니다. 압력이 감지될 때까지 대기합니다.\n");
                    
                    // 압력이 감지될 때까지 대기
                    while (1) {
                        pthread_mutex_lock(&flagMutex);
                        if (pressureChanged) {  // 인터럽트 발생 여부 확인
                            pressureChanged = 0; // 플래그 리셋
                            pthread_mutex_unlock(&flagMutex);

                            if (getPressure() == 1) { // 압력이 감지된 경우
                            serverMessageReceived = 1;
                                printf("압력이 감지되었습니다. 다음 작업으로 진행합니다.\n");
                                break;
                            }
                        } else {
                            pthread_mutex_unlock(&flagMutex);
                        }
                        usleep(100000); // 0.1초 대기
                    }
                }

                

            } else if (bytesRead == 0) {
                // 서버 연결 종료
                printf("[ERROR] Server disconnected. Attempting reconnection...\n");
                cleanup_socket();
                setup_socket();
                serverMessageReceived = 1;
                printf("[INFO] Reconnected to server.\n");
                

            } else {
                // 에러 (Keep-Alive나 네트워크 문제 등)
                perror("[ERROR] recv error");
                cleanup_socket();
                setup_socket();
                serverMessageReceived = 1;
                printf("[INFO] Reconnected to server after error.\n");  
            }

            if (stop_flag) break;
            sleep(1); 
        }
        return NULL;
    }

    // 서버로 메시지 전송 함수
    void clientSend(int sock, const char *message) {
        if (sock == -1) {
            printf("[ERROR] No server connection.\n");
            return;
        }

        if (send(sock, message, strlen(message), 0) == -1) {
            perror("[ERROR] Failed to send message to server");
        } else {
            printf("서버로 메시지 전송: %s\n", message);
        }
    }

    // 압력 작업 스레드
    void *pressureTask(void *arg) {
        (void)arg;

        while (1) {
            pressure = getPressure();
            if (pressure == 1) {
                
                printf("압력 감지됨: 압력 관련 작업 시작\n");
                sleep(1);
                
                float temperature = getTemperatureValue();

                printf("측정된 온도: %.2f°C\n", temperature);

                if (temperature > 35.0 || 0 > temperature ) {
                    printf("비정상 온도 감지! 3초 타이머를 5번 실행하여 확인합니다.\n");

                    int abnormal_count = 0;
                    for (int i = 0; i < 5 && !stop_flag; i++) {
                        startTimer(3); 
                        temperature = getTemperatureValue();
                        printf("%d초 후 측정된 온도: %.2f°C\n", (i+1)*3, temperature);

                        if (temperature > 35.0 || 0 > temperature ) {
                            printf("비정상 온도 감지!\n");
                            abnormal_count++;
                        } else {
                            printf("온도가 정상 범위 내에 있습니다.\n");
                            break;
                        }
                    }

                    if (abnormal_count == 5) {
                        printf("5번 연속 비정상 상태입니다! 서버에 경고를 전송합니다.\n");
                        if (client_socket != -1) {
                            clientSend(client_socket, "1");  
                        }
                        waitForPressureChange();
                        continue;
                    }
                } else {
                    printf("온도가 정상 범위 내에 있습니다. 추가 조치는 필요하지 않습니다.\n");
                    sleep(200);
                }
            } else {
                if (set == 0){
                printf("압력이 감지되지 않았습니다. 온도를 확인합니다.\n");
                }
                sleep(1);
                
                float temperature = getTemperatureValue();
                printf("측정된 온도: %.2f°C\n", temperature);
                int range = checkTemperatureRange(temperature);
                
                switch (range) {
                    case 1:
                        printf("온도는 안전 온도입니다. 10초마다 온도를 확인합니다.\n");
                        while (range == 1 && !stop_flag) {
                            startTimer(10); 
                            set = 1;
                            temperature = getTemperatureValue();
                            printf("10초 후 온도: %.2f°C\n", temperature);
                            range = checkTemperatureRange(temperature);
                        }
                        break;

                    case 2:
                        printf("온도가 주의 온도입니다. 7초마다 온도를 확인합니다.\n");
                        while (range == 2 && !stop_flag) {
                            startTimer(7); 
                            set = 1;
                            temperature = getTemperatureValue();
                            printf("7초 후 온도: %.2f°C\n", temperature);
                            range = checkTemperatureRange(temperature);
                        }
                        break;

                    case 3:
                        printf("온도가 경고 온도입니다. 15초 동안 5초 간격으로 온도를 확인합니다.\n");
                        if (monitorTemperatureWithShortIntervals(15, 5, 3) == 0 && !stop_flag) {
                            printf("15초 동안 온도가 정상으로 복구되지 않았습니다! 서버에 '2' 전송.\n");
                            if (client_socket != -1) {
                                clientSend(client_socket, "2"); 
                            
                            }  
                            waitForPressureChange();
                        } 
                        
                        continue;

                    case 4:
                        printf("온도가 위험온도입니다. 10초 동안 5초 간격으로 온도를 확인합니다.\n");
                        if (monitorTemperatureWithShortIntervals(10, 5, 4) == 0 && !stop_flag) {
                            printf("10초 동안 온도가 정상으로 복구되지 않았습니다! 서버에 '3' 전송.\n");
                            if (client_socket != -1) {
                                clientSend(client_socket, "3");  
                            }
                            waitForPressureChange();
                        }
                        continue;

                    default:
                        printf("알 수 없는 구간입니다.\n");
                        break;
                }
            }

            if (stop_flag) {
                printf("압력 상태 변경으로 작업 중단\n");
                stop_flag = 0; 
            }
        }
        return NULL;
    }

    // 메인 함수: 별도의 연결 상태 주기 확인 루프 없음
    int main() {
        srand((unsigned int)time(NULL)); 

        if (wiringPiSetup() == -1) {
            printf("WiringPi setup failed!\n");
            return EXIT_FAILURE;
        }

        printf("pigpio 초기화 중...\n");
        gpioCfgSetInternals(PI_CFG_NOSIGHANDLER);
        if (gpioInitialise() < 0) {
            printf("pigpio 초기화 실패\n");
            return EXIT_FAILURE;
        }

        gpioSetMode(PRESSURE_SENSOR_PIN, PI_INPUT); 
        gpioSetPullUpDown(PRESSURE_SENSOR_PIN, PI_PUD_DOWN); 
        gpioSetMode(SERVER_SIGNAL_PIN, PI_OUTPUT); 
        gpioSetMode(CLIENT_SIGNAL_PIN, PI_INPUT);  
        gpioSetPullUpDown(CLIENT_SIGNAL_PIN, PI_PUD_DOWN); 

        // 프로그램 시작 시 서버와 즉시 연결 시도
        setup_socket();

        if (gpioSetISRFunc(PRESSURE_SENSOR_PIN, EITHER_EDGE, 100, pressureInterrupt) < 0) {
            printf("인터럽트 설정 실패\n");
            return EXIT_FAILURE;
        }
        
        sleep(1);
        printf("프로그램 실행 중...\n");
        sleep(1);
        printf("압력 센서 상태 감지 시작\n");
        sleep(1);
    
        pthread_t pressureTaskThread;
        if (pthread_create(&pressureTaskThread, NULL, pressureTask, NULL) != 0) {
            perror("압력 작업 스레드 생성 실패");
            return EXIT_FAILURE;
        }

        pthread_t serverThread;
        if (pthread_create(&serverThread, NULL, serverMessageThread, NULL) != 0) {
            perror("서버 메시지 스레드 생성 실패");
            return EXIT_FAILURE;
        }

        // 메인 쓰레드는 여기서 각 쓰레드 종료 대기
        // 별도로 주기적으로 연결 상태를 확인하는 루프가 없음
        pthread_join(pressureTaskThread, NULL);
        pthread_join(serverThread, NULL);

        printf("프로그램 종료\n");
        cleanup_socket();
        gpioTerminate();

        return 0;
    }
