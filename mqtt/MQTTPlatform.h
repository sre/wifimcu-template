#ifndef __MQTT_PLATFORM_H
#define __MQTT_PLATFORM_H

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/netdb.h"

#include "mbedtls/config.h"
#include "mbedtls/platform.h"
#include "mbedtls/net.h"

#ifndef MQTT_TASK
#define MQTT_TASK
#endif

/* ---- Mutex ---- */
typedef struct Mutex {
	SemaphoreHandle_t sem;
} Mutex;

void MutexInit(Mutex*);
int MutexLock(Mutex*);
int MutexUnlock(Mutex*);

/* ---- Thread ---- */
typedef struct Thread {
	TaskHandle_t task;
} Thread;

int ThreadStart(Thread*, void (*fn)(void*), void* arg);

/* ---- Timer ---- */
typedef struct Timer {
	TickType_t xTicksToWait;
	TimeOut_t xTimeOut;
} Timer;

void TimerInit(Timer*);
char TimerIsExpired(Timer*);
void TimerCountdownMS(Timer*, unsigned int);
void TimerCountdown(Timer*, unsigned int);
int TimerLeftMS(Timer*);

/* ---- Network ---- */
typedef struct Network {
	int socket;
	int (*mqttread) (struct Network*, unsigned char*, int, int);
	int (*mqttwrite) (struct Network*, unsigned char*, int, int);

	/* TLS */
	mbedtls_net_context fd;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
	mbedtls_x509_crt cacert;
} Network;

static void NetworkInit(Network* n) {
	n->socket = 0;
}

int NetworkConnect(Network* n, char* addr, int port);
int NetworkDisconnect(Network* n);

int TLSConnect(Network* n, const char* addr, int port, const char* cert, const unsigned int cert_len);
int TLSDisconnect(Network* n);

#endif
