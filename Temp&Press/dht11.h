#ifndef DHT11_H
#define DHT11_H

// DHT11 핀 번호
#define DHT11PIN 7

// 함수 선언
int readDHT11(float *temperature, float *humidity);

#endif // DHT11_H