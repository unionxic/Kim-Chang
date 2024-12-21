#include "Header.h"
#include <string.h>
#include <curl/curl.h>
#include <stdlib.h> // getenv 사용을 위해 추가
#include <stdio.h>

// 이메일 전송 함수
void send_email(const char *receiver_email, const char *subject, const char *body) {
    CURL *curl;
    CURLcode res;

    // 환경 변수로 이메일과 비밀번호 불러오기
    const char *email_username = getenv("EMAIL_USERNAME");
    const char *email_password = getenv("EMAIL_PASSWORD");

    // 환경 변수가 설정되지 않았을 때 경고 메시지 출력
    if (email_username == NULL || email_password == NULL) {
        fprintf(stderr, "환경 변수 EMAIL_USERNAME 또는 EMAIL_PASSWORD가 설정되지 않았습니다.\n");
        return;
    }

    curl = curl_easy_init();
    if (curl) {
        struct curl_slist *recipients = NULL;

        // 디버그 정보 출력 (서버와 통신 흐름 보기)
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        // SMTP 서버 설정
        curl_easy_setopt(curl, CURLOPT_URL, "smtp://smtp.gmail.com:587");
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        curl_easy_setopt(curl, CURLOPT_PORT, 587);
        curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");

        // 사용자 인증 정보 설정
        curl_easy_setopt(curl, CURLOPT_USERNAME, email_username);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, email_password);

        // 메일 전송자 설정
        char mail_from[256];
        snprintf(mail_from, sizeof(mail_from), "<%s>", email_username);
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, mail_from);

        // 메일 수신자 설정
        recipients = curl_slist_append(recipients, receiver_email);
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        // 이메일 메시지 구성
        char message[2048];
        snprintf(message, sizeof(message),
                 "To: %s\r\n"
                 "From: %s\r\n"
                 "Subject: %s\r\n"
                 "\r\n"
                 "%s\r\n",
                 receiver_email, email_username, subject, body);

        // 메시지를 메모리에서 파일처럼 다루기 위해 fmemopen 사용
        FILE *message_stream = fmemopen(message, strlen(message), "r");
        if (message_stream == NULL) {
            fprintf(stderr, "메모리 파일 열기 실패\n");
            curl_slist_free_all(recipients);
            curl_easy_cleanup(curl);
            return;
        }

        // 메시지 설정
        curl_easy_setopt(curl, CURLOPT_READDATA, message_stream);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        // 이메일 전송
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "이메일 전송 실패: %s\n", curl_easy_strerror(res));
        } else {
            printf("이메일 전송 성공!\n");
        }

        // 자원 해제
        fclose(message_stream);
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
    } else {
        fprintf(stderr, "CURL 초기화 실패\n");
    }
}
