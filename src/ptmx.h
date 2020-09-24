#ifndef PTMX_H_INCLUDED__
#define PTMX_H_INCLUDED__

#include "iface/conn.h"
struct ptmx {
	uint8_t priv[4]; // gcc's value to i386
};
struct conn_i *ptmx_ctor (void *this_, size_t cb);

#endif // PTMX_H_INCLUDED__
