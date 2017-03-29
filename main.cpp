/* http_get - Retrieves a web page over HTTP GET.
 *
 * See http_get_ssl for a TLS-enabled version.
 *
 * This sample code is in the public domain.,
 */
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "esp/gpio.h"

#include <string.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "ssid_config.h"

#include "netocd_debug_server.h"
#include "netocd_serial_server.h"
#include "debug_stream.h"

#include "ota-tftp.h"

extern "C" void user_init(void)
{
	uart_set_baud(0, 115200);
	printf("SDK version:%s\n", sdk_system_get_sdk_version());
	
	sdk_station_config config;
	strcpy((char*)config.ssid, WIFI_SSID);
	strcpy((char*)config.password, WIFI_PASS);
	
	/* required to call wifi_set_opmode before station_set_config */
	sdk_wifi_set_opmode(STATION_MODE);
	sdk_wifi_station_set_config(&config);
	
	// Debug messages server, redirects debug_printf -> TCP.
	debug_stream_setup(3510); // 0xDB6 -> DBG -> DEBUG
	
	// JTAG/SWD debug server
	netocd_debug_server_start(9601);
	
	// Serial bridge server
	netocd_serial_server_start(9600);
	
	// Enable TFTP firmware upload (REMOVE FOR PRODUCTION)
//	ota_tftp_init_server(TFTP_PORT);
}
