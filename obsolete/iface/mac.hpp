#ifndef MAC_HPP_INCLUDED__
#define MAC_HPP_INCLUDED__

#include "iface/sshkey_const.h"

struct mac {
	uint8_t priv[512];
};

struct mac_i {
	// IUnknown
	void (*release) (struct mac *this_);
	// IKey
	void (*pushkey) (
		  struct mac *this_
		, void (*generator) (void *priv, int x, void *key, unsigned key_len)
		, void *priv
		, int direction
	);
	// IMac
	unsigned (*get_length) (struct mac *this_);
	void (*compute) (struct mac *this_, uint32_t seq, const void *src, unsigned src_len, void *dst);
};

#endif // MAC_HPP_INCLUDED__
