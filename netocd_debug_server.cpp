#include "espressif/esp_common.h"
#include "esp/uart.h"

#include <string.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/sockets.h"
#include "lwip_tcp_server.h"
#include "debug_stream.h"

void usleep(unsigned us) {
	return;
	static volatile uint16_t delay_counter;
	while (us--) {
		delay_counter = 40;
		while (delay_counter--) asm(" nop");
	}
}

// Communication protocol:
//   Bits [7:6] select command set:
#define NOCD_CMD_SET_M  0xC0
#define NOCD_CMD_SET(i) (((i)&3)<<6)
//   SET0 uses [5:0] as raw GPIO bits:
#define NOCD0_TMS         0x01
#define NOCD0_TCK         0x02
#define NOCD0_TDI         0x04
#define NOCD0_TDO         0x08
#define NOCD0_SWCLK       0x10
#define NOCD0_SWDIO       0x20

// SET1 uses [1:0] as raw GPIO bits:
#define NOCD1_TRST        0x01
#define NOCD1_SRST        0x02

// SET3 uses [5:0] as individual commands
#define NOCD3_READ0       0xC0 // Read back set 0
#define NOCD3_READ1       0xC1 // Read back set 1
#define NOCD3_LED_OFF     0xC2
#define NOCD3_LED_ON      0xC3
#define NOCD3_SWDIO_OUT   0xC4
#define NOCD3_SWDIO_IN    0xC5
#define NOCD3_JTAG_XCH_W  0xC8
#define NOCD3_JTAG_XCH_R  0xC9
#define NOCD3_SWD_XCH_W   0xCA
#define NOCD3_SWD_XCH_R   0xCB

#define PIN_LED   16 // Node D4, ESP12's LED
#define PIN_TMS    0 // Node D3
#define PIN_TCK    4 // Node D2
#define PIN_TDI    5 // Node D1
#define PIN_TDO   12 // Node D6
#define PIN_SWCLK 13 // Node D7
#define PIN_SWDIO 14 // Node D5
#define PIN_TRST  15 // Node D8
#define PIN_SRST   2 // Node D0

static uint8_t last_write0;
static void write0(uint8_t byte) {
	last_write0 = byte;
	
	gpio_write(PIN_TDI,   byte & NOCD0_TDI);
	gpio_write(PIN_TMS,   byte & NOCD0_TMS);
	gpio_write(PIN_TCK,   byte & NOCD0_TCK);
	gpio_write(PIN_SWDIO, byte & NOCD0_SWDIO);
	gpio_write(PIN_SWCLK, byte & NOCD0_SWCLK);
}

static void write1(uint8_t byte) {
	gpio_write(PIN_TRST,   byte & NOCD1_TRST);
	gpio_write(PIN_SRST,   byte & NOCD1_SRST);
}

static uint8_t read0() {
	uint8_t r = NOCD_CMD_SET(0);
	if (gpio_read(PIN_TDI))   r |= NOCD0_TDI;
	if (gpio_read(PIN_TDO))   r |= NOCD0_TDO;
	if (gpio_read(PIN_TMS))   r |= NOCD0_TMS;
	if (gpio_read(PIN_TCK))   r |= NOCD0_TCK;
	if (gpio_read(PIN_SWCLK)) r |= NOCD0_SWCLK;
	if (gpio_read(PIN_SWDIO)) r |= NOCD0_SWDIO;
	return r;
}

static uint8_t read1() {
	uint8_t r = NOCD_CMD_SET(1);
	if (gpio_read(PIN_TRST)) r |= NOCD1_TRST;
	if (gpio_read(PIN_SRST)) r |= NOCD1_SRST;
	return r;
}

static bool JTAG_bit(bool tms, bool tdi) {
	uint8_t base = last_write0 & ~(NOCD0_TCK | NOCD0_TMS | NOCD0_TDI);
	if (tms) base |= NOCD0_TMS;
	if (tdi) base |= NOCD0_TDI;
	
	write0(base);
	bool r = gpio_read(PIN_TDO);
	write0(base | NOCD0_TCK);
	
	return r;
}

static bool SWD_bit(bool swdio) {
	uint8_t base = last_write0 & ~(NOCD0_SWCLK | NOCD0_SWDIO);
	if (swdio) base |= NOCD0_SWDIO;
	
	write0(base);
	usleep(1);
	bool r = gpio_read(PIN_SWDIO);
	write0(base | NOCD0_SWCLK);
	usleep(1);
	
	return r;
}

// true on error
static bool JTAG_exchange(int cfd, bool rnw) {
	int tdi;
	
	// First field: Offset, byte
	uint8_t offset;
	if (lwip_read(cfd, &offset, 1) != 1)
		return true;
	
	// Second field: bit count, u16
	uint16_t bit_cnt;
	if (lwip_read(cfd, &bit_cnt, 2) != 2) 
		return true;
	
	// Calculate data size
	uint16_t bytes = (offset+bit_cnt+7) / 8;
	if (bytes > 128)
		return true; // Too big
		
	// Read data
	uint8_t buf[128];
	if (lwip_read(cfd, buf, bytes) != bytes)
		return true;
	
	debug_printf("jtag_exchange: rnw=%d off=%d, bits=%d ", rnw, offset, bit_cnt);
	
	gpio_write(PIN_TMS, 0);
	for (unsigned int i = offset; i < bit_cnt + offset; i++) {
		int bytec = i/8;
		int bcval = 1 << (i % 8);
		tdi = !rnw && (buf[bytec] & bcval);
		
		bool bit = JTAG_bit(0, tdi);
		
		if (rnw) {
			if (bit)
				buf[bytec] |= bcval;
			else
				buf[bytec] &= ~bcval;
		}
	}
	
	// Send response
	if (rnw) if (lwip_write(cfd, buf, bytes) != bytes)
		return true;
	
	// All OK
	last_write0 = read0();
	return false;
}

// true on error
static bool SWD_exchange(int cfd, bool rnw) {
	debug_printf("swd_exchange: ");
	int tdi;
	
	// First field: Offset, byte
	uint8_t offset;
	if (lwip_read(cfd, &offset, 1) != 1) {
		debug_printf("offset read fail");
		return true;
	}
	
	// Second field: bit count, u16
	uint16_t bit_cnt;
	if (lwip_read(cfd, &bit_cnt, 2) != 2) {
		debug_printf("cnt read fail");
		return true;
	}
	
	// Calculate data size
	uint16_t bytes = (offset+bit_cnt+7) / 8;
	if (bytes > 128) {
		debug_printf("byte size too targe (%d)", bytes);
		return true; // Too big
	}
	
	// Read data
	int i = 0;
	uint8_t buf[128];
	while (i < bytes) {
		int r = lwip_read(cfd, buf+i, bytes-i);
		if (r<0) {
			debug_printf("bad data read");
			return true;
		}
		i += r;
	}
	
	debug_printf("rnw=%d,  off=%d, bits=%d, ", rnw, offset, bit_cnt);
	
	if (rnw) {
		for (unsigned int i = offset; i < bit_cnt + offset; i++) {
			int bytec = i/8;
			int bcval = 1 << (i % 8);
			
			if (SWD_bit(0))
				buf[bytec] |= bcval;
			else
				buf[bytec] &= ~bcval;
		}
		if (lwip_write(cfd, buf, bytes) != bytes)
			return true;
	} else {
		for (unsigned int i = offset; i < bit_cnt + offset; i++) {
			int bytec = i/8;
			int bcval = 1 << (i % 8);
			tdi = buf[bytec] & bcval;
			
			SWD_bit(tdi);
		}
	}
	
	debug_printf("data=[ ");
	for (i=0; i<bytes; i++) {
		debug_printf("%02X ", buf[i]);
	}
	debug_printf("] ");
	
	// All ok
	return false;
}

static void netocd_loop(int cfd) {
	// Set led on to indicate connection
	gpio_write(PIN_LED,1);
	
	// Message loop
	uint8_t byte;
	while (lwip_read(cfd, &byte, 1) == 1) {
//		usleep(1);
		debug_printf("CMD[0x%02X]: ", byte);
		
		// Classify by command sets
		switch (byte & NOCD_CMD_SET_M) {
			// Set 0-1 are mask writes
			case NOCD_CMD_SET(0):
				debug_printf("WR0[0x%02X] ", byte);
				write0(byte);
				break;
				
			case NOCD_CMD_SET(1):
				debug_printf("WR1[0x%02X] ", byte);
				write1(byte);
				vTaskDelay(100 / portTICK_PERIOD_MS);
				break;
				
			// Set 2 is unimplemented, ignore
			case NOCD_CMD_SET(2):
				break;
				
			// Set 3 is individual commands
			case NOCD_CMD_SET(3):
				// Handle each command separately
				switch (byte) {
					case NOCD3_READ0:
						byte = read0();
						debug_printf("RD0[0x%02X] ", byte);
						if (lwip_write(cfd, &byte, 1) != 1)
							return;
						break;
					
					case NOCD3_READ1:
						byte = read1();
						debug_printf("RD1[0x%02X] ", byte);
						if (lwip_write(cfd, &byte, 1) != 1)
							return;
						break;
						
					case NOCD3_LED_OFF:
						debug_printf("led ");
						gpio_write(PIN_LED, 1); // YES, backwards!
						break;
						
					case NOCD3_LED_ON:
						debug_printf("LED ");
						gpio_write(PIN_LED, 0);
						break;
						
					case NOCD3_SWDIO_IN:
						debug_printf("SWDIN ");
						gpio_enable(PIN_SWDIO, GPIO_INPUT);
						break;
						
					case NOCD3_SWDIO_OUT:
						debug_printf("SWDOUT ");
						gpio_enable(PIN_SWDIO, GPIO_OUTPUT);
						break;
						
					case NOCD3_JTAG_XCH_W:
						debug_printf("JTAG_W ");
						if (JTAG_exchange(cfd, false))
							return;
						break;
						
					case NOCD3_JTAG_XCH_R:
						debug_printf("JTAG_R ");
						if (JTAG_exchange(cfd, true))
							return;
						break;
						
					case NOCD3_SWD_XCH_W:
						debug_printf("SWD_W ");
						if (SWD_exchange(cfd, false))
							return;
						break;
						
					case NOCD3_SWD_XCH_R:
						debug_printf("SWD_R ");
						if(SWD_exchange(cfd, true))
							return;
						break;
				}
				break;
		}
		debug_printf("\n");
	}
}

inline void disable_nagle(int s) {
	int flag = 1;
	flag = setsockopt(s, /* socket affected */
		IPPROTO_TCP,     /* set option at TCP level */
		TCP_NODELAY,     /* name of option */
		(char *) &flag,  /* the cast is historical cruft */
		sizeof(int));    /* length of option value */
}

static void debug_server_task(void *pvParameters) {
	if (!pvParameters) return;
	uint16_t *port = (uint16_t *)pvParameters;
	
	// Create a server
	int srvSocket = lwip_tcp_server(*port);
	free(port);
	if (srvSocket < 0) return;
	
	// Accept and dispatch loop
	while (true) {
		int cfd = lwip_accept(srvSocket, NULL, NULL);
		if (cfd < 0) continue;
		
		disable_nagle(cfd);
		
		debug_printf("Got debug client!\n");
		netocd_loop(cfd);
		debug_printf("Client gone.\n");
		
		lwip_close(cfd);
	}
}

void netocd_debug_server_start(uint16_t port) {
	// Change all non-flash pins to GPIO
	gpio_enable(PIN_LED,   GPIO_OUTPUT);
	gpio_enable(PIN_TMS,   GPIO_OUTPUT);
	gpio_enable(PIN_TDI,   GPIO_OUTPUT);
	gpio_enable(PIN_TDO,   GPIO_INPUT);
	gpio_enable(PIN_TCK,   GPIO_OUTPUT);
	gpio_enable(PIN_SWDIO, GPIO_OUTPUT);
	gpio_enable(PIN_SWCLK, GPIO_OUTPUT);
	gpio_enable(PIN_TRST,  GPIO_OUTPUT);
	gpio_enable(PIN_SRST,  GPIO_OUTPUT);
	
	write0(NOCD0_TMS);
	write1(NOCD1_SRST | NOCD1_TRST);
	
	uint16_t *p = (uint16_t *)malloc(sizeof(port));
	if (!p) return;
	*p = port;
	
	xTaskCreate(&debug_server_task,  "debug_server",  2048, p, 2, NULL);
}
