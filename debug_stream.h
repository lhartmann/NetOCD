#ifndef DEBUG_STREAM_H
#define DEBUG_STREAM_H

#include <stdint.h>

//#define ENABLE_DEBUG_STREAM

#ifdef ENABLE_DEBUG_STREAM
void debug_stream_setup(uint16_t port);
int debug_printf(const char *format, ...);
#else
inline void debug_stream_setup(uint16_t port) {}
inline int debug_printf(const char *format, ...) { return 0; }
#endif
	
#endif
