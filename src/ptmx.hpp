#ifndef PTMX_HPP_INCLUDED__
#define PTMX_HPP_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "iface/conn.hpp"
struct ptmx {
	uint8_t priv[4]; // gcc's value to i386/x86_64
};
struct conn_i *ptmx_ctor (void *this_, unsigned cb);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // PTMX_HPP_INCLUDED__
