#ifndef CIPHER_H_INCLUDED__
#define CIPHER_H_INCLUDED__

#include "iface/sshkey_const.h"

struct cipher {
	uint8_t priv[512];
};

struct cipher_i {
	// IUnknown
	void (*release) (struct cipher *this_);
	// IKey
	void (*pushkey) (
		  struct cipher *this_
		, void (*generator) (void *priv, int x, void *key, unsigned key_len)
		, void *priv
		, int direction
	);
	// ICipher
	unsigned (*get_block_length) (struct cipher *this_);
	void (*decrypt) (struct cipher *this_, const void *src, unsigned src_len, void *dst);
	void (*encrypt) (struct cipher *this_, const void *src, unsigned src_len, void *dst);
};

#endif // CIPHER_H_INCLUDED__
