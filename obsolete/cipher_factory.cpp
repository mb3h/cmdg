#include <stdint.h>
#include "cipher_factory.hpp"

#include "aes.hpp"
#include "log.h"
#include "vt100.h" // TODO: removing
#include "assert.h"

#include <stdlib.h> // exit
#include <string.h>
#include <memory.h>
typedef uint8_t u8;

#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.

///////////////////////////////////////////////////////////////////////////////
// readability / portability

#define CtoS KEY_DIRECTION_CtoS
#define StoC KEY_DIRECTION_StoC

///////////////////////////////////////////////////////////////////////////////

typedef struct aes256ctr {
	unsigned bs; // block size
	struct aes ctx;
} aes256ctr_s;

// [RFC4253] 7.2
// 'aes256-ctr' PENDING: other crypt support
static void aes256ctr_pushkey (
	  struct cipher *this_
	, void (*key_generator) (void *priv, int x, void *key, unsigned key_len)
	, void *priv
	, int direction
)
{
aes256ctr_s *m_;
	m_ = (aes256ctr_s *)this_->priv;
BUG(m_)
u8 key512[64];

	key_generator (priv, (CtoS == direction) ? 'C' : 'D', key512, 256 / 8);
	aes_setkey (&m_->ctx, key512, 256);

	key_generator (priv, (CtoS == direction) ? 'A' : 'B', key512, 128 / 8);
	aes_setiv (&m_->ctx, key512); // use first 128-bits

	memset (key512, 0, sizeof(key512)); // security erase
}

static void aes256ctr_decrypt (struct cipher *this_, const void *src, unsigned src_len, void *dst)
{
aes256ctr_s *m_;
	m_ = (aes256ctr_s *)this_->priv;
BUG(m_)
BUG(src_len / m_->bs * m_->bs == src_len) // cannot use '%' in macro.
	aes_ctr_decrypt (&m_->ctx, src, src_len, dst);
}

static void aes256ctr_encrypt (struct cipher *this_, const void *src, unsigned src_len, void *dst)
{
aes256ctr_s *m_;
	m_ = (aes256ctr_s *)this_->priv;
BUG(m_)
BUG(src_len / m_->bs * m_->bs == src_len) // cannot use '%' in macro.
	aes_ctr_encrypt (&m_->ctx, src, src_len, dst);
}

static unsigned aes256ctr_get_block_length (struct cipher *this_)
{
aes256ctr_s *m_;
	m_ = (aes256ctr_s *)this_->priv;
BUG(m_)
	return m_->bs;
}

static void aes256ctr_dtor (struct cipher *this_)
{
aes256ctr_s *m_;
	m_ = (aes256ctr_s *)this_->priv;
BUG(m_)
	memset (&m_->ctx, 0, usizeof(m_->ctx)); // security erase PENDING: aes_dtor()
	m_->bs = 0;
}

static struct cipher_i aes256ctr_cipher_i = {
	.release          = aes256ctr_dtor,
	.pushkey          = aes256ctr_pushkey,
	.encrypt          = aes256ctr_encrypt,
	.decrypt          = aes256ctr_decrypt,
	.get_block_length = aes256ctr_get_block_length,
};

static struct cipher_i *aes256ctr_ctor (struct cipher *this_)
{
aes256ctr_s *m_;
	m_ = (aes256ctr_s *)this_->priv;
BUG(m_)
	aes_init (&m_->ctx, usizeof(m_->ctx)); // PENDING: aes_ctor()
	m_->bs = 128 / 8; // block size = 128 bits
	return &aes256ctr_cipher_i;
}

///////////////////////////////////////////////////////////////////////////////

// 'aes256-ctr' PENDING: other crypt support
struct cipher_i *cipher_factory (struct cipher *this_, const char *name)
{
BUG(0 == strcmp ("aes256-ctr", name))
	return aes256ctr_ctor (this_);
}
