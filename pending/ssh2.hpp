#ifndef SSH2_HPP_INCLUDED__
#define SSH2_HPP_INCLUDED__

#include "iface/conn.hpp"
struct ssh2 {
	uint8_t priv[3072]; // TODO: cannot set suitable value unless original sha2.c aes.c are implemented.
};
struct conn_i *ssh2_ctor (struct ssh2 *this_, unsigned cb);
void ssh2_preconnect (struct ssh2 *this_, const char *hostname, uint16_t port, const char *ppk_path);

#endif // SSH2_HPP_INCLUDED__
