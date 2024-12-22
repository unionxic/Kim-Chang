#include <wiringPi.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "dht11.h"

#define MAX_TIME 100

// DHT11 데이터를 읽는 함수
int readDHT11(float *temperature, float *humidity) {
    uint8_t lststate = HIGH;
    uint8_t counter = 0;
    uint8_t j = 0, i;
    int dht11_val[5] = {0, 0, 0, 0, 0};

    pinMode(DHT11PIN, OUTPUT);
    digitalWrite(DHT11PIN, 0);
    delay(18);
    digitalWrite(DHT11PIN, 1);
    delayMicroseconds(40);
    pinMode(DHT11PIN, INPUT);

    for (i = 0; i < MAX_TIME; i++) {
        counter = 0;
        while (digitalRead(DHT11PIN) == lststate) {
            counter++;
            delayMicroseconds(1);
            if (counter == 255) break;
        }
        lststate = digitalRead(DHT11PIN);

        if (counter == 255) break;

        if ((i >= 4) && (i % 2 == 0)) {
            dht11_val[j / 8] <<= 1;
            if (counter > 26) {
                dht11_val[j / 8] |= 1;
            }
            j++;
        }
    }

    // 데이터 유효성 검사
    if ((j >= 40) && (dht11_val[4] == ((dht11_val[0] + dht11_val[1] + dht11_val[2] + dht11_val[3]) & 0xFF))) {
        *humidity = dht11_val[0] + dht11_val[1] / 10.0;
        *temperature = dht11_val[2] + dht11_val[3] / 10.0;
        return 0; // 성공
    } else {
      
        return -1; // 실패
    }
}
