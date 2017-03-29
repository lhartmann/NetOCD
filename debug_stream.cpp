#include "debug_stream.h"

#ifdef ENABLE_DEBUG_STREAM

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/sockets.h"
#include "lwip_tcp_server.h"

#include "lwip_tcp_server.h"
#define BUFF_COUNT   4 // Number of messages to be queued
#define BUFF_SIZE  256 // Maximum size of each message

static QueueHandle_t empty, full;
static SemaphoreHandle_t enabled;

int debug_printf(const char *format, ...) {
	// Skip all processing if there are no clients
	if (xSemaphoreTake(enabled, 0) != pdTRUE) {
		return 0;
	}
	xSemaphoreGive(enabled);
	
	// Read variable args
	va_list arglist;
	va_start(arglist, format);
	
	// Retrieve an empty message buffer
	char *buff;
	int r = -1;
	if (xQueueReceive(empty, &buff, portMAX_DELAY) == pdTRUE) {
		// Format the message
		r = vsnprintf(buff, BUFF_SIZE-1, format, arglist);
		
		// Ensure null-terminated (required if buffer size exceeded)
		buff[BUFF_SIZE-1] = 0;
		
		// Queue message for sending
		if (xQueueSend(full, &buff, portMAX_DELAY) != pdTRUE) {
			// Failing here is catastrophic, the buffer is lost!
			// There is nothing we can do... At least release memory.
			free(buff);
			r = -1;
		}
	}
	
	va_end(arglist);
	return r;
}

static void server_task(void *pvParameters) {
	if (!pvParameters) return;
	uint16_t *port = (uint16_t *)pvParameters;
	
	// Create message queues and semaphores
	empty   = xQueueCreate(BUFF_COUNT, sizeof(char*));
	full    = xQueueCreate(BUFF_COUNT, sizeof(char*));
	enabled = xSemaphoreCreateBinary();
	if (!empty || !full || !enabled) {
		if (empty)
			vQueueDelete(empty);
		
		if (full)
			vQueueDelete(full);
		
		if (enabled)
			vSemaphoreDelete(enabled);
		
		return;
	}

	// Allocate the message buffers
	for (int i=0; i<BUFF_COUNT; ++i) {
		char *b = (char *)malloc(BUFF_SIZE);
		if (!b)
			return;
		
		if (xQueueSend(empty, &b, 1) != pdTRUE)
			return;
	}
	
	// Create a server
	int srvSocket = lwip_tcp_server(*port);
	free(port);
	if (srvSocket < 0) {
		vQueueDelete(empty);
		vQueueDelete(full);
		vSemaphoreDelete(enabled);
		return;
	}
	
	// Accept and dispatch loop
	while (true) {
		// Wait for a TCP client
		int cfd = lwip_accept(srvSocket, NULL, NULL);
		if (cfd < 0) continue;
		
		// Say hello:
		if (true) {
			const char *hello = "Welcome to the debug output stream. Good luck!\n";
			lwip_write(cfd, hello, strlen(hello));
		}
		
		// Enable debug_printf
		xSemaphoreGive(enabled);
		
		// Message loop
		char *buff;
		int len = 0;
		while(len == 0) {
			// Retrieve a message from the full buffer
			if (xQueueReceive(full, &buff, portMAX_DELAY) == pdTRUE) {
				// Find out length, then send.
				// len should return to zero on successful write(...).
				len = strlen(buff);
				len -= lwip_write(cfd, buff, len);
			}
			
			// Return bufer to the empty queue
			if (xQueueSend(empty, &buff, portMAX_DELAY) != pdTRUE) {
				// Failing here is catastrophic, the buffer is lost!
				// There is nothing we can do... At least release memory.
				free(buff);
			}
		}
		
		// Disable debug_printf
		xSemaphoreTake(enabled, portMAX_DELAY);
		
		// Discard all pending messages
		while (xQueueReceive(full, &buff, 10))
			xQueueSend(empty, &buff, portMAX_DELAY);
		
		// Client is gone
		lwip_close(cfd);
	}
}

void debug_stream_setup(uint16_t port) {
	uint16_t *p = (uint16_t *)malloc(sizeof(port));
	if (!p) return;
	*p = port;
	
	xTaskCreate(&server_task, "debug_stream", 2048, p, 1, NULL);
}

#endif // ENABLE_DEBUG_STREAM
