#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 65111
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

typedef struct {
    struct sockaddr_in address;
    int socket_fd;
    int role;  // 역할: 1, 2, 3
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// 각 클라이언트로부터 받은 최신 메시지를 저장하는 배열
// 인덱스: role-1 (예: role=1 -> latest_values[0])
int latest_values[MAX_CLIENTS] = {-1, -1, -1};

// IP 주소와 역할 매핑 테이블
typedef struct {
    char ip[INET_ADDRSTRLEN];
    int role;
} ip_role_mapping_t;

ip_role_mapping_t ip_role_map[] = {
    {"192.168.83.7", 1},  // 클라이언트 1 (온습도, 압력)
    {"192.168.83.5", 2},  // 클라이언트 2 (카메라)
    {"192.168.83.9", 3}   // 클라이언트 3 (LED, 부저, 이메일)
};
int ip_role_map_size = sizeof(ip_role_map) / sizeof(ip_role_map[0]);

// IP 주소로 역할 확인
int get_role_by_ip(const char *ip) {
    for (int i = 0; i < ip_role_map_size; i++) {
        if (strcmp(ip, ip_role_map[i].ip) == 0) {
            return ip_role_map[i].role;
        }
    }
    return -1;  // 매핑되지 않은 IP
}

// 클라이언트에게 메시지 전송
void send_to_client(int role, int message) {
    pthread_mutex_lock(&clients_mutex);
    client_t *target_client = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->role == role) {
            target_client = clients[i];
            break;
        }
    }
    if (target_client) {
        char buffer[BUFFER_SIZE];
        sprintf(buffer, "%d", message);
        send(target_client->socket_fd, buffer, strlen(buffer), 0);
        printf("클라이언트 [%d]에게 메시지 전송: %d\n", role, message);
    } else {
        printf("역할 [%d]에 해당하는 클라이언트가 연결되어 있지 않습니다.\n", role);
    }
    pthread_mutex_unlock(&clients_mutex);
}

int wait_for_client_response(int role) { 
    // 특정 역할의 클라이언트로부터 '1'을 최대 20초 동안 기다립니다.
    // '1'을 받으면 즉시 1을 반환하고, 20초 내에 받지 못하면 0을 반환합니다.

    // 응답 대기를 시작하기 전에 값을 초기화
    pthread_mutex_lock(&clients_mutex);
    latest_values[role - 1] = -1; // 초기화
    pthread_mutex_unlock(&clients_mutex);

    int timeout_count = 3000; // 3000 * 10ms = 30초 수신 딜레이 고려해서 20초가 아닌 30초로 설정
    for (int i = 0; i < timeout_count; i++) {
        pthread_mutex_lock(&clients_mutex);
        int val = latest_values[role - 1];
        pthread_mutex_unlock(&clients_mutex);

        if (val == 1) {
            // '1'을 받았을 때 처리
            pthread_mutex_lock(&clients_mutex);
            latest_values[role - 1] = -1; // 재사용을 위해 리셋
            pthread_mutex_unlock(&clients_mutex);
            return 1;
        }

        usleep(10000); // 10ms 대기
    }

    // 20초 동안 '1'을 받지 못했을 때 처리
    //printf("역할 [%d] 클라이언트로부터 응답 대기 타임아웃.\n", role);
    return 0;
}


// 클라이언트 처리 스레드 함수
void *handle_client(void *arg) {
    client_t *cli = (client_t *)arg;
    char buffer[BUFFER_SIZE];

    printf("클라이언트 [역할: %d] 연결됨 (IP: %s)\n", cli->role, inet_ntoa(cli->address.sin_addr));

    while (1) {
        int valread = read(cli->socket_fd, buffer, BUFFER_SIZE);
        if (valread <= 0) break;

        buffer[valread] = '\0';
        int received_value = atoi(buffer);
        printf("클라이언트 [역할: %d]로부터 메시지 수신: %d\n", cli->role, received_value);

        // 수신 값 latest_values에 저장
        pthread_mutex_lock(&clients_mutex);
        latest_values[cli->role - 1] = received_value;
        pthread_mutex_unlock(&clients_mutex);

        // 역할 1(온습도,압력) 클라이언트에서의 로직 처리
        if (cli->role == 1) {
            if (received_value == 1) {
                printf("운전자 의식 불명. LED 파이에게 LED,부저,이메일 지시\n");
                send_to_client(3, 2);
            } else if (received_value == 2) {

                pthread_mutex_lock(&clients_mutex);
                latest_values[2 - 1] = -1; // 미리 -1로 세팅
                pthread_mutex_unlock(&clients_mutex);

                printf("케이스 2, 카메라 파이에게 작동 지시\n");
                send_to_client(2, 1);
                int response = wait_for_client_response(2); // 클라이언트 2 응답 대기
                if (response == 1) {
                    printf("객체 확인. LED 파이에게 이메일 지시\n");
                    send_to_client(3, 1);
                } else if (response == 0) {
                    //send_to_client(1, 99);
                    printf("객체 없음. 메인파이를 슬립 상태로 전환.\n");
                    send_to_client(1,100);
                    //break; //12/12 22:18 에 수정함. 클라이언트 1이 여러개 생기는 것에 대한 대책으로 해봄
                } else if (response == -1) {
                    printf("카메라 파이 응답 실패. 계속 대기 또는 에러 처리.\n");
                }
            } else if (received_value == 3) {

                pthread_mutex_lock(&clients_mutex);
                latest_values[2 - 1] = -1; // 미리 -1로 세팅
                pthread_mutex_unlock(&clients_mutex);

                printf("케이스 3 카메라 파이에게 작동 지시\n");
                send_to_client(2, 1);
                int response = wait_for_client_response(2); // 클라이언트 2 응답 대기
                if (response == 1) {
                    printf("객체 확인. LED 파이에게 LED,부저,이메일 지시\n");
                    send_to_client(3, 2);
                } else if (response == 0) {
                    //send_to_client(1, 99);
                    printf("객체 없음. 메인파이를 슬립 상태로 전환.\n");
                    send_to_client(1,100);
                    //break; // 12/12 22:18 에 수정함. 클라이언트 1이 여러개 생기는 것에 대한 대책으로 해봄
                } else if (response == -1) {
                    printf("카메라 파이 응답 실패. 계속 대기 또는 에러 처리.\n");
                }
            } else {
                printf("알 수 없는 메시지 수신: %d\n", received_value);
            }
        }

        //3번 파이에서 종료 신호를 보내면 그걸 1번 파이로 전달시켜주는 역할
        if (cli->role == 3) {
            if (received_value == 99) {
                printf("상황종료\n");
                send_to_client(1, 99);
            }
        }
    }

    close(cli->socket_fd);
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == cli) {
            clients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    printf("클라이언트 [역할: %d] 연결 종료\n", cli->role);
    free(cli);
    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("소켓 생성 실패");
        exit(EXIT_FAILURE);
    }

    // 소켓 재사용 옵션 설정
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("소켓 재사용 설정 실패");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("바인딩 실패");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("리스닝 실패");
        exit(EXIT_FAILURE);
    }
    printf("서버가 포트 %d에서 실행 중입니다...\n", PORT);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket < 0) {
            perror("클라이언트 연결 실패");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
        int role = get_role_by_ip(client_ip);

        if (role == -1) {
            printf("알 수 없는 클라이언트 연결: IP %s\n", client_ip);
            close(new_socket);
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        int assigned = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] == NULL) {
                client_t *cli = (client_t *)malloc(sizeof(client_t));
                cli->address = address;
                cli->socket_fd = new_socket;
                cli->role = role;
                clients[i] = cli;

                pthread_t tid;

                //send_to_client(2,1); //owejfoijaoivjsdlvnsaodifiohowefjowiefoiasvisvbodbvcowenonwfowoiimoivsdas 이거 카메라 파이 테스트용으로 넣은거 나중에 빼야됨
                pthread_create(&tid, NULL, handle_client, (void *)cli);
                pthread_detach(tid); // 스레드 분리
                assigned = 1;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (!assigned) {
            // 클라이언트 수용 불가 시 소켓 종료
            printf("최대 클라이언트 수 초과. 연결 거부.\n");
            close(new_socket);
        }
    }

    close(server_fd);
    return 0;
}

