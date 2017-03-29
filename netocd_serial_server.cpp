#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/sockets.h"
#include "lwip_tcp_server.h"
#include <FreeRTOS.h>
#include <queue.h>
#include <esp/uart.h>
#include <stdio.h>
#include <debug_stream.h>

#define NETOCD_SERIAL_USE_INTERRUPTS 0

#define NETOCD_SERIAL_PORT 9600

static void net_to_uart(void *pvParameters) {
	uint16_t *cfd = (uint16_t *)pvParameters;
	char data[16];
	int n;
	while(true) {
		n = lwip_read(*cfd, data, sizeof(data));
		if(n == -1) return;
		
		for (int i=0; i<n; ++i) {
			putchar(data[i]);
		}
	}
}

static void client_handler(int cfd) {
	// Create a task for network->uart data flow
	TaskHandle_t tskN2U;
	if (pdTRUE != xTaskCreate(&net_to_uart, "net_to_uart", 1024, &cfd, 2, &tskN2U)) {
		debug_printf("SS: Failed to create net_to_uart task.\n");
		return;
	}
	
	while (true) {
		char ch = getchar();
		int r=0;
		
		do r = lwip_write(cfd, &ch, 1);
		while (r==0);
		
		if (r<0) {
			debug_printf("SS: write returned %d, close.\n", r);
			break;
		}
	}
	
	// Cleanup
	vTaskDelete(tskN2U);
}

static void server_task(void *pvParameters) {
	uint16_t *port = (uint16_t *)pvParameters;
	
	// Create a server
	int srvSocket = lwip_tcp_server(port ? *port : NETOCD_SERIAL_PORT);
	free(port);
	if (srvSocket == -1) return;
	
	// Accept and dispatch loop
	while (true) {
		int cfd = lwip_accept(srvSocket, NULL, 0);
		if (cfd == -1) {
			debug_printf("SS: accept() failed!\n");
			continue;
		}
		
		debug_printf("SS: Client in...\n");
		client_handler(cfd);
		debug_printf("SS: Client out.\n");
		
		lwip_close(cfd);
	}
}

void netocd_serial_server_start(uint16_t port) {
	uint16_t *p = (uint16_t *)malloc(sizeof(port));
	if (!p) return;
	*p = port;
	
	printf("Starting serial server on port %d: ", *p);
	if (pdTRUE != xTaskCreate(&server_task, "serial_server", 2048, p, 2, NULL)) {
		printf("FAILED!\n");
	} else {
		printf("OK!\n");
	}
}
