// led_buzzer.h
#ifndef LED_BUZZER_H
#define LED_BUZZER_H

#define HIGH 1
#define LOW  0

int init_led_buzzer_pins(int led_pin, int buzzer_pin, int frequency);
void control_led_buzzer(int led_pin, int buzzer_pin, int led_state, int buzzer_state);
void cleanup_led_buzzer_pins(int led_pin, int buzzer_pin);

#endif // LED_BUZZER_H
