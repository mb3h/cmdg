#ifndef SSH2CONN_H_INCLUDED__
#define SSH2CONN_H_INCLUDED__

#include "iface/conn.h"
struct ssh2 {
	uint8_t priv[2048]; // TODO cannot set suitable value unless original sha2.c aes.c are implemented.
};
struct conn_i *ssh2_ctor (struct ssh2 *this_, size_t cb);
bool ssh2_preconnect (struct ssh2 *this_, uint16_t port, const char *hostname, const char *ppk_path);

#endif // SSH2CONN_H_INCLUDED__
