/* MQTT Platform Binding for FreeRTOS with LwIP */

/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander - initial API and implementation and/or initial documentation
 *    Ian Craggs - convert to FreeRTOS
 *
 * The TLS Binding has been written by Sebastian Reichel
 * and is licensed additionally licensed under ISC-License.
 *******************************************************************************/

#include <string.h>
#include "MQTTPlatform.h"
#include "Printf.h"

#include "mbedtls/config.h"
#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/debug.h"

/* ---- Mutex ---- */
void MutexInit(Mutex* mutex) {
	mutex->sem = xSemaphoreCreateMutex();
}

int MutexLock(Mutex* mutex) {
	return xSemaphoreTake(mutex->sem, portMAX_DELAY);
}

int MutexUnlock(Mutex* mutex) {
	return xSemaphoreGive(mutex->sem);
}

/* ---- Thread ---- */
int ThreadStart(Thread* thread, void (*fn)(void*), void* arg) {
	int rc = 0;
	uint16_t usTaskStackSize = (configMINIMAL_STACK_SIZE * 5);
	UBaseType_t uxTaskPriority = uxTaskPriorityGet(NULL); /* set the priority as the same as the calling task*/

	rc = xTaskCreate(fn,	/* The function that implements the task. */
		"mqtt-task",		/* Just a text name for the task to aid debugging. */
		usTaskStackSize,	/* The stack size is defined in FreeRTOSIPConfig.h. */
		arg,				/* The task parameter, not used in this case. */
		uxTaskPriority,		/* The priority assigned to the task is defined in FreeRTOSConfig.h. */
		&thread->task);		/* The task handle is not used. */

	return rc;
}

/* ---- Timer ---- */
void TimerCountdownMS(Timer* timer, unsigned int timeout_ms) {
	timer->xTicksToWait = timeout_ms / portTICK_PERIOD_MS; /* convert milliseconds to ticks */
	vTaskSetTimeOutState(&timer->xTimeOut); /* Record the time at which this function was entered. */
}

void TimerCountdown(Timer* timer, unsigned int timeout) {
	TimerCountdownMS(timer, timeout * 1000);
}

int TimerLeftMS(Timer* timer) {
	xTaskCheckForTimeOut(&timer->xTimeOut, &timer->xTicksToWait); /* updates xTicksToWait to the number left */
	return (timer->xTicksToWait < 0) ? 0 : (timer->xTicksToWait * portTICK_PERIOD_MS);
}

char TimerIsExpired(Timer* timer) {
	return xTaskCheckForTimeOut(&timer->xTimeOut, &timer->xTicksToWait) == pdTRUE;
}

void TimerInit(Timer* timer) {
	timer->xTicksToWait = 0;
	memset(&timer->xTimeOut, '\0', sizeof(timer->xTimeOut));
}

/* ---- Network ---- */

static int LwIP_read(Network* n, unsigned char* buffer, int len, int timeout_ms) {
	setsockopt(n->socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout_ms, sizeof(timeout_ms));

	int bytes = 0;
	while (bytes < len) {
		int rc = recv(n->socket, &buffer[bytes], (size_t)(len - bytes), 0);
		if (rc == -1) {
			if (errno != ENOTCONN && errno != ECONNRESET) {
				bytes = -1;
				break;
			}
		} else {
			bytes += rc;
		}
	}
	return bytes;
}

static int LwIP_write(Network* n, unsigned char* buffer, int len, int timeout_ms) {
	setsockopt(n->socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout_ms, sizeof(timeout_ms));
	return write(n->socket, buffer, len);
}

int NetworkConnect(Network* n, char* addr, int port) {
	int sock;
	int type = SOCK_STREAM;
	struct sockaddr_in address;
	int rc = -1;
	u8_t family = AF_INET;
	struct addrinfo *result = NULL;
	struct addrinfo hints = {0, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, 0, NULL, NULL, NULL};
	static struct timeval tv;

	sock = -1;
	if (addr[0] == '[')
		++addr;

	if ((rc = getaddrinfo(addr, NULL, &hints, &result)) == 0)
	{
		struct addrinfo* res = result;

		/* prefer ip4 addresses */
		while (res)
		{
			if (res->ai_family == AF_INET)
			{
				result = res;
				break;
			}
			res = res->ai_next;
		}

		if (result->ai_family == AF_INET)
		{
			address.sin_port = htons(port);
			address.sin_family = family = AF_INET;
			address.sin_addr = ((struct sockaddr_in*)(result->ai_addr))->sin_addr;
		}
		else
			rc = -1;

		freeaddrinfo(result);
	}

	if (rc == 0)
	{
		sock = socket(family, type, 0);
		if (sock != -1)
		{
#if defined(NOSIGPIPE)
			int opt = 1;

			if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (void*)&opt, sizeof(opt)) != 0)
				Log(TRACE_MIN, -1, "Could not set SO_NOSIGPIPE for socket %d", sock);
#endif

			if (family == AF_INET)
				rc = connect(sock, (struct sockaddr*)&address, sizeof(address));
		}
	}
	if (sock == SO_ERROR)
		return -1;

	tv.tv_sec = 1;  /* 1 second Timeout */
	tv.tv_usec = 0;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

	n->socket = sock;
	n->mqttread = LwIP_read;
	n->mqttwrite = LwIP_write;

	return 0;
}

int NetworkDisconnect(Network* n) {
	return close(n->socket);
}

static void platform_ssl_debug(void *ctx, int level, const char *file, int line, const char *str) {
	printf("[%d] %s\r\n", str);
}

static int platform_ssl_random(void *p_rng, unsigned char *output, size_t output_len) {
	size_t off = 0;

	for (; output_len > 0; output_len--)
		*(output + off++) = (unsigned char) rand();

	return 0;
}

static int TLS_read(Network* n, unsigned char* buffer, int len, int timeout_ms) {
	int ret;

	ret = mbedtls_ssl_read(&n->ssl, buffer, len);
	if(ret > 0)
		return ret;
	if(ret == MBEDTLS_ERR_SSL_TIMEOUT)
		return -1;

	printf("[TLS-recv] -0x%04x\r\n", -ret);

	if(ret == 0)
		return -2;
	else if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
		return -2;
	else
		return -1;
}

static int TLS_write(Network* n, unsigned char* buffer, int len, int timeout_ms) {
	int ret;

	ret = mbedtls_ssl_write(&n->ssl, buffer, len);

	if(ret >= 0)
		return ret;

	printf("[TLS-send] -0x%04x\r\n", -ret);

	return -1;
}

int TLSConnect(Network* n, const char* addr, int port, const char* cert, const unsigned int cert_len) {
	int ret;
	char portstr[6];
	int verification_result;

	mbedtls_net_init(&n->fd);
	mbedtls_ssl_init(&n->ssl);
	mbedtls_ssl_config_init(&n->conf);
	mbedtls_x509_crt_init(&n->cacert);

	sprintf(portstr, "%hu", port);

	if((ret = mbedtls_net_connect(&n->fd, addr, portstr, MBEDTLS_NET_PROTO_TCP)) != 0) {
		printf("mbedtls_net_connect() returned -0x%x\r\n", -ret);
		return ret;
	}

	if((ret = mbedtls_ssl_config_defaults(&n->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		printf("mbedtls_ssl_config_defaults() returned -0x%x\r\n", -ret);
		return ret;
	}

	mbedtls_ssl_conf_authmode(&n->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
	mbedtls_ssl_conf_ca_chain(&n->conf, &n->cacert, NULL);
	mbedtls_ssl_conf_rng(&n->conf, platform_ssl_random, NULL);
	mbedtls_ssl_conf_dbg(&n->conf, platform_ssl_debug, NULL);
	mbedtls_ssl_conf_read_timeout(&n->conf, 1000); /* 1 second */

	if((ret = mbedtls_ssl_setup(&n->ssl, &n->conf)) != 0) {
		printf("mbedtls_ssl_setup() returned -0x%x\r\n", -ret);
		return ret;
	}

	if((ret = mbedtls_x509_crt_parse(&n->cacert, (uint8_t*)cert, strlen(cert)+1)) != 0) {
		printf("mbedtls_x509_crt_parse() returned -0x%x\r\n", -ret);
		return ret;
	}

	mbedtls_ssl_set_hostname(&n->ssl, addr);
	mbedtls_ssl_set_bio(&n->ssl, &n->fd, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);

	while((ret = mbedtls_ssl_handshake(&n->ssl)) != 0) {
		switch(ret) {
			case MBEDTLS_ERR_SSL_WANT_READ:
			case MBEDTLS_ERR_SSL_WANT_WRITE:
				break;
			default:
				printf("mbedtls_ssl_handshake() returned -0x%x\r\n", -ret);
				return ret;
		}
	}

	verification_result = mbedtls_ssl_get_verify_result(&n->ssl);

	if(verification_result & MBEDTLS_X509_BADCERT_EXPIRED) {
		printf("Certificate expired!\r\n");
		return 1;
	}
	if(verification_result & MBEDTLS_X509_BADCERT_REVOKED) {
		printf("Certificate revoked!\r\n");
		return 1;
	}
	if(verification_result & MBEDTLS_X509_BADCERT_CN_MISMATCH) {
		printf("Certificate CN mismatch!\r\n");
		return 1;
	}
	if(verification_result & MBEDTLS_X509_BADCERT_NOT_TRUSTED) {
		printf("Certificate untrusted!\r\n");
		return 1;
	}

	n->socket = (int)(n->fd.fd);
	n->mqttread = TLS_read;
	n->mqttwrite = TLS_write;
}

int TLSDisconnect(Network* n) {
	mbedtls_ssl_close_notify(&n->ssl);
	mbedtls_ssl_free(&n->ssl);
	mbedtls_ssl_config_free(&n->conf);
	mbedtls_net_free(&n->fd);
	mbedtls_x509_crt_free(&n->cacert);
}
