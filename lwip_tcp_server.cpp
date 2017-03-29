#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/sockets.h"
#include <string.h>

int lwip_tcp_server(uint16_t port) {
	int s = -1;
	
	// Create a server port and listen
	s = lwip_socket(PF_INET, SOCK_STREAM, 0);
	if (s < 0) return -1;
	
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_len = sizeof(addr);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	
	if (lwip_bind(s, (sockaddr*)&addr, sizeof(addr)) < 0) {
		lwip_close(s);
		return -1;
	}
	
	if (lwip_listen(s, 5) < 0) {
		lwip_close(s);
		return -1;
	}
	
	return s;
}
