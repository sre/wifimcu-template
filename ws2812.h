#ifndef __WS2812_H
#define __WS2812_H

#include <stdbool.h>
#include <stdint.h>

typedef union {
	struct __attribute__ ((__packed__)) {
		uint8_t _unused;
		uint8_t b;
		uint8_t r;
		uint8_t g;
	} colors;
	uint32_t grbu;
} ws2812_led_t;

void ws2812_init(void);
void ws2812_send(ws2812_led_t *leds, int led_count);
bool ws2812_is_sending(void);

#endif
