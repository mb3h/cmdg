#ifndef CONFIG_H_INCLUDED__
#define CONFIG_H_INCLUDED__

#include "iface/config_reader.hpp"

struct config {
#ifdef __x86_64__
	uint8_t priv[16]; // gcc's value to x86_64
#else //def __i386__
	uint8_t priv[8]; // gcc's value to i386
#endif
};
struct config *config_get_singlton ();
void config_cleanup();
void config_warmup();
struct config_reader_i *config_query_config_reader_i (struct config *this_);

#endif // CONFIG_H_INCLUDED__
