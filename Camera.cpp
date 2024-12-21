#include <iostream>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <thread>
#include <csignal>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/tcp.h> // TCP KeepAlive 옵션 상수 정의
#include "darknet.h"

// TCP KeepAlive 옵션이 정의되지 않은 경우 정의
#ifndef TCP_KEEPIDLE
#define TCP_KEEPIDLE 4
#endif

#ifndef TCP_KEEPINTVL
#define TCP_KEEPINTVL 5
#endif

#ifndef TCP_KEEPCNT
#define TCP_KEEPCNT 6
#endif

// 소켓 통신 설정
#define SERVER_IP "192.168.83.4"  // 메인 Pi IP 주소
#define SERVER_PORT 65111          // 서버 포트 번호
#define BUFFER_SIZE 1024           // 버퍼 크기

// mat_to_image 함수 선언
image mat_to_image(const cv::Mat &mat);

// 전역 변수
std::queue<cv::Mat> frame_queue;          // 캡처된 프레임을 저장하는 큐
std::mutex frame_mutex;                   // 큐 접근을 위한 뮤텍스
std::condition_variable cv_signal;        // 스레드 간 신호 전달을 위한 조건 변수
bool stop_threads = false;                // 스레드 종료 플래그
bool interrupt_signal = false;            // 메인 파이로부터 "1" 신호 플래그
bool start_signal_received = false;       // 시작 신호 수신 여부
bool detection_mode = false;              // 현재 탐지 모드 여부

const int MAX_QUEUE_SIZE = 3;             // 프레임 큐 최대 크기
const int DETECTION_PERIOD = 6;           // 탐지 주기 (초)
const int DETECTION_DURATION = 20;        // 탐지 지속 시간 (초)

int client_socket;

// 서버에 연결을 시도하는 함수 (최대 50번 재시도)
bool connectNow() {
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) { perror("소켓 생성 실패"); return false; }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) { perror("SERVER_IP 설정 실패"); close(client_socket); return false; }

    int attempt = 0;
    const int MAX_ATTEMPTS = 50;

    while (!stop_threads && attempt < MAX_ATTEMPTS) {
        std::cout << "서버(" << SERVER_IP << ":" << SERVER_PORT << ")에 접속 시도 중..." << std::endl;
        if (connect(client_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == 0) {
            std::cout << "서버(" << SERVER_IP << ":" << SERVER_PORT << ")에 접속 성공" << std::endl;

            // TCP KeepAlive 설정
            int opt = 1;
            if (setsockopt(client_socket, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) { perror("SO_KEEPALIVE 설정 실패"); }

            // KeepAlive 세부 설정
            int idle = 60, intvl = 10, cnt = 5;
            if (setsockopt(client_socket, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) < 0) { perror("TCP_KEEPIDLE 설정 실패"); }
            if (setsockopt(client_socket, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl)) < 0) { perror("TCP_KEEPINTVL 설정 실패"); }
            if (setsockopt(client_socket, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt)) < 0) { perror("TCP_KEEPCNT 설정 실패"); }

            return true;
        } else {
            perror("소켓 연결 실패, 10초 후 재시도합니다...");
            close(client_socket);
            attempt++;
            if (attempt >= MAX_ATTEMPTS) { std::cerr << "최대 재시도 횟수(" << MAX_ATTEMPTS << ")에 도달. 연결 시도 중단." << std::endl; return false; }
            std::this_thread::sleep_for(std::chrono::seconds(10));
            client_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (client_socket == -1) { perror("소켓 생성 실패"); return false; }
        }
    }

    return false;
}

// 소켓 설정 함수
void sockSetup() {
    if (!connectNow()) {
        std::cerr << "서버에 연결할 수 없습니다. 프로그램을 종료합니다." << std::endl;
        exit(1);
    }
}

// 소켓 정리 함수
void cleanSock() { close(client_socket); std::cout << "소켓 종료" << std::endl; }

// 메인 Pi로 메시지를 전송하는 함수
void sendMsg(const std::string& msg) {
    if (send(client_socket, msg.c_str(), msg.length(), 0) == -1) { perror("서버로 메시지 전송 실패"); }
    else { std::cout << "서버로 메시지 전송: " << msg << std::endl; }
}

// 소켓 입력을 모니터링하는 함수
void sockMonitor() {
    char buffer[BUFFER_SIZE];
    while (!stop_threads) {
        memset(buffer, 0, sizeof(buffer));
        int recvBytes = recv(client_socket, buffer, sizeof(buffer)-1, 0);

        if (recvBytes > 0) {
            buffer[recvBytes] = '\0';
            std::string msg(buffer);
            if (msg == "1") {
                {
                    std::lock_guard<std::mutex> lock(frame_mutex);
                    interrupt_signal = true;
                    start_signal_received = true;
                    std::cout << "메인 파이의 '1' 신호 수신. 프로그램 시작." << std::endl;
                }
                cv_signal.notify_all();
            }
        }
        else if (recvBytes == 0) {
            std::cerr << "서버와의 연결 끊김. 재연결 시도..." << std::endl;
            close(client_socket);
            if (!connectNow()) { std::cerr << "여러 번의 시도 끝에 재연결 실패. 스레드 정지." << std::endl; stop_threads = true; }
        }
        else { perror("소켓 수신 에러"); stop_threads = true; }

        if (stop_threads) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

// 프레임을 캡처하는 함수 (스레드 실행)
void frameCapture(cv::VideoCapture &cap) {
    {
        std::unique_lock<std::mutex> lock(frame_mutex);
        cv_signal.wait(lock, [] { return start_signal_received || stop_threads; });
    }

    if (stop_threads) return;

    while (!stop_threads) {
        cv::Mat frm;
        cap >> frm;
        if (!frm.empty()) {
            if (frm.type() != CV_8UC3) { std::cout << "프레임을 CV_8UC3 포맷으로 변환합니다." << std::endl; cv::cvtColor(frm, frm, cv::COLOR_GRAY2BGR); }

            std::lock_guard<std::mutex> lock(frame_mutex);
            if (frame_queue.size() >= MAX_QUEUE_SIZE) { frame_queue.pop(); }
            frame_queue.push(frm.clone());
        }
        else { std::cerr << "카메라 오류 발생" << std::endl; std::this_thread::sleep_for(std::chrono::milliseconds(500)); }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// 프레임을 처리하고 객체를 탐지하는 함수 (스레드 실행)
void frameProcess(network *net, const std::vector<std::string> &classes, const std::vector<std::string> &targets) {
    std::cout << "메인 파이의 시작 신호를 기다리는 중..." << std::endl;

    {
        std::unique_lock<std::mutex> lock(frame_mutex);
        cv_signal.wait(lock, [] { return start_signal_received || stop_threads; });
    }

    if (stop_threads) return;

    while (!stop_threads) {
        {
            std::unique_lock<std::mutex> lock(frame_mutex);
            cv_signal.wait(lock, [] { return interrupt_signal || stop_threads; });
            if (stop_threads) break;
            std::cout << "신호 수신. 탐지를 시작합니다." << std::endl;
            interrupt_signal = false;
            detection_mode = true;
        }

        auto start_t = std::chrono::steady_clock::now();
        bool found_obj = false;

        while (!stop_threads && !found_obj) {
            auto elapsed = std::chrono::steady_clock::now() - start_t;
            auto secs = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

            if (secs >= DETECTION_DURATION) { std::cout << "20초 경과. 대기 상태로 돌아갑니다." << std::endl; break; }

            std::this_thread::sleep_for(std::chrono::seconds(DETECTION_PERIOD));

            cv::Mat frm;
            {
                std::lock_guard<std::mutex> lock(frame_mutex);
                if (!frame_queue.empty()) { frm = frame_queue.front(); frame_queue.pop(); }
                else { continue; }
            }

            if (!frm.empty()) {
                std::cout << "프레임에서 객체를 탐지 중..." << std::endl;
                image im = mat_to_image(frm);
                image resized = letterbox_image(im, net->w, net->h);
                network_predict_ptr(net, resized.data);

                int nboxes = 0;
                detection *dets = get_network_boxes(net, im.w, im.h, 0.7, 0.5, 0, 1, &nboxes, 0);
                do_nms_sort(dets, nboxes, net->layers[net->n - 1].classes, 0.45);

                bool tgt_found = false;
                for (int i = 0; i < nboxes; ++i) {
                    for (int j = 0; j < net->layers[net->n - 1].classes; ++j) {
                        if (dets[i].prob[j] > 0.6) {
                            std::string cls = classes[j];
                            if (std::find(targets.begin(), targets.end(), cls) != targets.end()) { tgt_found = true; break; }
                        }
                    }
                    if (tgt_found) break;
                }

                free_detections(dets, nboxes);
                free_image(im);
                free_image(resized);

                if (tgt_found) {
                    sendMsg("1");
                    std::cout << "객체 감지됨! 즉시 대기 상태로 돌아갑니다." << std::endl;
                    found_obj = true;
                }
            }
        }

        detection_mode = false;
        if (!found_obj) { std::cout << "대기 상태로 돌아갑니다." << std::endl; }
    }
}

int main() {
    sockSetup();

    try {
        // Darknet 설정 파일 및 가중치 파일 경로
        char *cfg = const_cast<char*>("/home/pi/Desktop/fuck/darknet/cfg/yolov4-tiny.cfg");
        char *weight = const_cast<char*>("/home/pi/Desktop/fuck/darknet/yolov4-tiny.weights");
        network *net = load_network_custom(cfg, weight, 0, 1); // 네트워크 로드
        set_batch_network(net, 1); // 배치 사이즈 설정

        // 클래스 이름 로드
        std::vector<std::string> cls_names;
        std::ifstream cls_file("/home/pi/Desktop/fuck/darknet/cfg/coco.names");
        std::string ln;
        while (std::getline(cls_file, ln)) cls_names.push_back(ln);
        std::vector<std::string> tgt_classes = {"person", "dog", "cat"}; // 탐지할 타겟 클래스

        // 카메라 초기화 (웹캠 0번)
        cv::VideoCapture cap(0, cv::CAP_V4L2);
        if (!cap.isOpened()) {
            std::cerr << "웹캠을 열 수 없습니다." << std::endl;
            cleanSock();
            return -1;
        }

        // 카메라 설정
        cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));

        // 시그널 처리: Ctrl+C로 프로그램 종료
        std::signal(SIGINT, [](int) {
            std::cout << "Ctrl+C 감지. 스레드를 정지하고 종료합니다..." << std::endl;
            stop_threads = true;
            cv_signal.notify_all();
        });

        // 스레드 시작
        std::thread cap_thr(frameCapture, std::ref(cap)); // 프레임 캡처 스레드
        std::thread det_thr(frameProcess, net, cls_names, tgt_classes); // 탐지 스레드
        std::thread sock_thr(sockMonitor); // 소켓 모니터링 스레드

        // 스레드 종료 대기
        cap_thr.join();
        det_thr.join();
        sock_thr.join();

        // 자원 정리
        cleanSock();
        cap.release();
        cv::destroyAllWindows();
    }
    catch (...) {
        cleanSock();
        return -1;
    }

    return 0;
}

// OpenCV Mat를 Darknet 이미지로 변환하는 함수 정의
image mat_to_image(const cv::Mat &mat) {
    int h = mat.rows, w = mat.cols, c = mat.channels();
    if (mat.depth() != CV_8U) { throw std::runtime_error("[오류] Mat 깊이가 CV_8U가 아닙니다."); }

    // Darknet 이미지 생성
    image im = make_image(w, h, c);
    unsigned char *data = mat.data;
    int step = mat.step;

    // 채널별로 데이터 복사 및 정규화
    for(int y=0;y<h;y++) for(int k=0;k<c;k++) for(int x=0;x<w;x++) {
        im.data[k*w*h + y*w + x] = data[y*step + x*c + k] / 255.0;
    }

    return im;
}

