// TODO support thread safety
#ifdef WIN32
 // TODO
#else
#include <sys/time.h> // timeval
#endif
#include <memory.h> // memcpy
#include "defs.h"
#define arraycountof(x) (sizeof(x)/sizeof((x)[0]))

#include "sha1.h"


typedef uint32_t u32;
typedef  uint8_t u8;

// TODO integrate store_be32()/load_be32().
static u32 load_be32 (const void *src_)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
u32 val;
	val = *(const u32 *)src_;
	return 0xff & val >> 24 | 0xff00 & val >> 8 | 0xff0000 & val << 8 | 0xff000000UL & val << 24;
#else
	return *(const u32 *)src_;
#endif
}
static void store_be32 (void *dst, u32 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(u32 *)dst = 0xffU & val >> 24 | 0xff00U & val >> 8 | 0xff0000U & val << 8 | 0xff000000UL & val << 24;
#else
	*(u32 *)dst = val;
#endif
}

#define MAXPOOL  1280
#define NOISELEN 64
#define SEEDLEN  20
static struct {
	size_t noise_charged;
	size_t pool_used;
	u8 noise[NOISELEN];
	u32 seed[SEEDLEN / sizeof(u32)];
	u8 pool[MAXPOOL];
} this__, *m_ = &this__;

#define DISABLE_STIR 1
#define SEED_UPDATED 1
static void random_stir ();
static int random_add_noise0 (const void *src_, size_t len, int flags)
{
int retval;
	retval = 0;
const u8 *src;
	src = (const u8 *)src_;
	while (! (m_->noise_charged + len < NOISELEN)) {
size_t cb;
		memcpy (m_->noise + m_->noise_charged, src, cb = NOISELEN - m_->noise_charged);
		src += cb;
		len -= cb;
		sha1_core (m_->seed, m_->noise);
		m_->noise_charged = 0;
		retval |= SEED_UPDATED;
u8 xor_mask[SEEDLEN];
size_t i;
		for (i = 0; i < arraycountof(m_->seed); ++i)
			store_be32 (xor_mask +i * sizeof(m_->seed[0]), m_->seed[i]);
		for (i = 0; i < sizeof(xor_mask); ++i) {
			m_->pool[m_->pool_used++] ^= xor_mask[i];
			if (! (m_->pool_used < MAXPOOL))
				m_->pool_used = 0;
		}
		if (m_->pool_used < SEEDLEN && DISABLE_STIR & ~flags)
			random_stir ();
	}
	if (0 < len) {
		memcpy (m_->noise + m_->noise_charged, src, len);
		m_->noise_charged += len;
	}
	return retval;
}

static void random_stir ()
{
	// short noise
struct timeval tv;
	gettimeofday (&tv, NULL);
	if (! (SEED_UPDATED & random_add_noise0 (&tv, sizeof(tv), DISABLE_STIR)))
		sha1_core (m_->seed, m_->noise);
u32 hash_sha1[SEEDLEN / sizeof(u32)];
	memcpy (hash_sha1, m_->seed, SEEDLEN); // iv

	// refresh pool (2 rounds)
u8 fixed_noise[NOISELEN];
int round;
	for (round = 0; round < 2; ++round) {
		memcpy (fixed_noise, m_->pool, NOISELEN);
u32 *p;
		p = (u32 *)(m_->pool + MAXPOOL);
		while ((u32 *)m_->pool < p) {
			p -= SEEDLEN / sizeof(*p);
size_t k;
			for (k = 0; k < arraycountof(hash_sha1); ++k)
				hash_sha1[k] ^= load_be32 (&p[k]);
			sha1_core (hash_sha1, fixed_noise);
			for (k = 0; k < arraycountof(hash_sha1); ++k)
				store_be32 (&p[k], hash_sha1[k]);
		}
	}
	sha1_core (hash_sha1, fixed_noise);
	memcpy (m_->seed, hash_sha1, SEEDLEN);
	m_->pool_used = SEEDLEN;
}

void store_random (void *dst_, size_t cb)
{
u8 *dst;
	dst = (u8 *)dst_;
	while (0 < cb) {
		if (! (m_->pool_used < MAXPOOL))
			random_stir ();
size_t got;
		got = min(cb, MAXPOOL - m_->pool_used);
		memcpy (dst, m_->pool + m_->pool_used, got);
		dst += got;
		m_->pool_used += got;
		cb -= got;
	}
}

void random_add_noise (const void *src_, size_t cb)
{
	random_add_noise0 (src_, cb, 0);
}

void random_init ()
{
	memset (&this__, 0, sizeof(this__));
	random_stir ();
}
