#include <stdint.h>
#include "mac_factory.hpp"

#include "sha1.hpp"
#include "portab.h"
#include "log.h"
#include "vt100.h" // TODO: removing
#include "assert.h"

#include <stdlib.h> // exit
#include <string.h>
#include <memory.h>
typedef uint32_t u32;
typedef  uint8_t u8;

#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.

///////////////////////////////////////////////////////////////////////////////
// readability / portability

#define CtoS KEY_DIRECTION_CtoS
#define StoC KEY_DIRECTION_StoC

///////////////////////////////////////////////////////////////////////////////

#define SHA1_LEN (160 / 8)

typedef struct hmacsha1 {
	unsigned mac_len;
	u8 mac_key[SHA1_LEN];
} hmacsha1_s;

// [RFC4253] 7.2
// 'hmac-sha1' PENDING: other crypt support
static void hmacsha1_pushkey (
	  struct mac *this_
	, void (*key_generator) (void *priv, int x, void *key, unsigned key_len)
	, void *priv
	, int direction
)
{
hmacsha1_s *m_;
	m_ = (hmacsha1_s *)this_->priv;
BUG(m_)
unsigned mac_keylen
	= usizeof(m_->mac_key);

	key_generator (priv, (CtoS == direction) ? 'E' : 'F', m_->mac_key, mac_keylen);
}

static unsigned hmacsha1_get_length (struct mac *this_)
{
hmacsha1_s *m_;
	m_ = (hmacsha1_s *)this_->priv;
BUG(m_)
	return m_->mac_len;
}

static void hmacsha1_compute (struct mac *this_, u32 seq, const void *src, unsigned src_len, void *dst)
{
hmacsha1_s *m_;
	m_ = (hmacsha1_s *)this_->priv;
BUG(m_)
struct sha1_hmac mac_ctx;
	sha1_hmac_init (&mac_ctx, usizeof(mac_ctx), m_->mac_key, usizeof(m_->mac_key));
u8 be32[4];
	store_be32 (be32, seq);
	sha1_hmac_update (&mac_ctx, be32, usizeof(be32));
	sha1_hmac_update (&mac_ctx, src, src_len);
	sha1_hmac_finish (&mac_ctx, dst);
}

static void hmacsha1_dtor (struct mac *this_)
{
hmacsha1_s *m_;
	m_ = (hmacsha1_s *)this_->priv;
BUG(m_)
	memset (m_->mac_key, 0, usizeof(m_->mac_key)); // security erase
	m_->mac_len = 0;
}

static struct mac_i hmacsha1_mac_i = {
	.release    = hmacsha1_dtor,
	.pushkey    = hmacsha1_pushkey,
	.compute    = hmacsha1_compute,
	.get_length = hmacsha1_get_length,
};

static struct mac_i *hmacsha1_ctor (struct mac *this_)
{
hmacsha1_s *m_;
	m_ = (hmacsha1_s *)this_->priv;
BUG(m_)
	memset (m_->mac_key, 0, usizeof(m_->mac_key));
	m_->mac_len = SHA1_LEN;
	return &hmacsha1_mac_i;
}

///////////////////////////////////////////////////////////////////////////////

// 'hmac-sha1' PENDING: other crypt support
struct mac_i *mac_factory (struct mac *this_, const char *name)
{
BUG(0 == strcmp ("hmac-sha1", name))
	return hmacsha1_ctor (this_);
}
