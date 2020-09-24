#ifdef WIN32
# define uint32_t DWORD
# define  uint8_t BYTE
# define uint16_t WORD
 // TODO
#else
# define _XOPEN_SOURCE 500 // usleep (>= 500)
# include <features.h>
# include <fcntl.h>
# include <unistd.h>
# include <pthread.h> // pthread_create
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "defs.h"
#include <alloca.h>
#define RUPDIV(d, src) (((src) +(d) -1) / (d))

#include "ssh2.h"
#include "intv.h"
typedef struct intv intv_s;
#include "sha2.h"
typedef struct sha256 sha256_s;
#include "sha1.h"
typedef struct sha1 sha1_s;
typedef struct sha1_hmac sha1_hmac_s;
#include "aes.h"
typedef struct aes aes_s;
#include "base64.h"
#include "rand.h"
#include "lock.h"

typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;

#define ALLOCA_ENV(cb_alloca) ASSERTE((cb_alloca) <= 4096)
        // set suitable to stack frame on platform.

// conf.h
#define KEX_MODULUS_MAXBITS 2048
#define CIPHER_BLOCK_MAXLEN 16
#define          MAC_MAXLEN 20
#define     PACKET_MAXALIGN 16
#define      PADDING_MAXLEN (PACKET_MAXALIGN +3)

#define  INPUT_SPEED 38400 // [bit/s]
#define OUTPUT_SPEED 38400 // [bit/s]
#define CHANNEL_DATA_MAXLEN 0x100000
#define       WINDOW_MINLEN CHANNEL_DATA_MAXLEN
#define        STDIN_MAXBUF 4096
#define         SEND_MAXBUF (6 +4 +4 + STDIN_MAXBUF \
		                               + PADDING_MAXLEN + MAC_MAXLEN)
#define         RECV_MAXBUF (6 +4 +4 + CHANNEL_DATA_MAXLEN \
		                               + PADDING_MAXLEN + MAC_MAXLEN)
#define       STDOUT_MAXBUF CHANNEL_DATA_MAXLEN

#define MODPOW_PAUSE_SPAN_BITS 64

#define   HOSTNAME_MAXLEN 63
#define   USERNAME_MAXLEN 31
#define PASSPHRASE_MAXLEN 63
#define   PPK_PATH_MAXLEN 127

#define PPK_BASE64_MAXBYTE_PER_LINE 48 // (= 64 chars)
#define  USERPRIV_MAXBITS 1024
#define    RSA2_P_MAXBITS 512
#define    RSA2_Q_MAXBITS 512
#define RSA2_IQMP_MAXBITS 1024
#define RSA2_PRIVKEY_MAXLEN ( \
	4 +1 + RUPDIV(8, USERPRIV_MAXBITS) + \
	4 +1 + RUPDIV(8, RSA2_P_MAXBITS) + \
	4 +1 + RUPDIV(8, RSA2_Q_MAXBITS) + \
	4 +1 + RUPDIV(8, RSA2_IQMP_MAXBITS) )

#include "vt100.h"
extern FILE *stderr_old;
#define HI0 VTO
#define HIERR VTRR

#define ERR0(fmt                                          ) do { fprintf (stderr_old, HIERR "%s"HI0 "\n", fmt                                     ); } while (0);
#define ERR1(fmt, arg1                                    ) do { fprintf (stderr_old, HIERR fmt HI0 "\n", arg1                                    ); } while (0);
#define ERR2(fmt, arg1, arg2                              ) do { fprintf (stderr_old, HIERR fmt HI0 "\n", arg1, arg2                              ); } while (0);
#define ERR3(fmt, arg1, arg2, arg3                        ) do { fprintf (stderr_old, HIERR fmt HI0 "\n", arg1, arg2, arg3                        ); } while (0);
#define ERR4(fmt, arg1, arg2, arg3, arg4                  ) do { fprintf (stderr_old, HIERR fmt HI0 "\n", arg1, arg2, arg3, arg4                  ); } while (0);
#define ERR7(fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7) do { fprintf (stderr_old, HIERR fmt HI0 "\n", arg1, arg2, arg3, arg4, arg5, arg6, arg7); } while (0);
#define WARN0 ERR0
#define WARN1 ERR1
#define WARN2 ERR2
#define WARN3 ERR3
#define WARN4 ERR4

#define SSH2_MSG_SERVICE_REQUEST           5
#define SSH2_MSG_SERVICE_ACCEPT            6
#define SSH2_MSG_KEXINIT                   20
#define SSH2_MSG_NEWKEYS                   21
#define SSH2_MSG_KEX_DH_GEX_REQUEST_OLD    30
#define SSH2_MSG_KEX_DH_GEX_GROUP          31
#define SSH2_MSG_KEX_DH_GEX_INIT           32
#define SSH2_MSG_KEX_DH_GEX_REPLY          33
#define SSH2_MSG_USERAUTH_REQUEST          50
#define SSH2_MSG_USERAUTH_FAILURE          51
#define SSH2_MSG_USERAUTH_SUCCESS          52
#define SSH2_MSG_USERAUTH_PK_OK            60
#define SSH2_MSG_CHANNEL_OPEN              90
#define SSH2_MSG_CHANNEL_OPEN_CONFIRMATION 91
#define SSH2_MSG_CHANNEL_WINDOW_ADJUST     93
#define SSH2_MSG_CHANNEL_DATA              94
#define SSH2_MSG_CHANNEL_EOF               96
#define SSH2_MSG_CHANNEL_CLOSE             97
#define SSH2_MSG_CHANNEL_REQUEST           98
#define SSH2_MSG_CHANNEL_SUCCESS           99

static const u8 ASN1_TRUE[] = {
    0x00, 0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2B,
    0x0E, 0x03, 0x02, 0x1A, 0x05, 0x00, 0x04, 0x14,
};

#define     SSH_CLIENT_ID "SSH-2.0-Cmdg_0.00"

///////////////////////////////////////////////////////////////////////////////
// portability

// TODO integrate load_be32()/store_be32().
static u32 load_be32 (const void *src_)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
u32 val;
	val = *(const u32 *)src_;
	return 0xff & val >> 24 | 0xff00 & val >> 8 | 0xff0000 & val << 8 | 0xff000000UL & val << 24;
#endif
}
static size_t store_be32 (void *dst, u32 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(u32 *)dst = 0xffU & val >> 24 | 0xff00U & val >> 8 | 0xff0000U & val << 8 | 0xff000000UL & val << 24;
#endif
	return sizeof(u32);
}

///////////////////////////////////////////////////////////////////////////////
// utility

#define SECURITY_ERASE 1
struct ringbuf {
	int flags;
	size_t size;
	size_t used;
	size_t stored;
	u8 *begin;
};
typedef struct ringbuf ringbuf_s;

static size_t ringbuf_write (ringbuf_s *m_, const void *src_, size_t cb)
{
	if (0 == cb)
		cb = strlen ((const char *)src_);
const u8 *src;
	src = (const u8 *)src_;
size_t put0;
	put0 = 0;
	if (m_->used <= m_->stored && 0 < (put0 = min(cb, (0 == m_->used ? m_->size -1 : m_->size) - m_->stored))) {
		memcpy (m_->begin + m_->stored, src, put0);
		src += put0;
		cb -= put0;
		m_->stored += put0;
		if (! (m_->stored < m_->size))
			m_->stored = 0;
	}
size_t put;
	put = 0;
	if (m_->stored < m_->used && 0 < (put = min(cb, m_->used - m_->stored))) {
		memcpy (m_->begin + m_->stored, src, put);
		src += put;
		cb -= put;
		m_->stored += put;
	}
	return put0 + put;
}
static size_t ringbuf_read (ringbuf_s *m_, void *dst_, size_t cb)
{
u8 *dst;
	dst = (u8 *)dst_;
size_t used;
	used = m_->used;
size_t got0;
	got0 = 0;
	if (m_->stored < used && 0 < (got0 = min(cb, m_->size - used))) {
		if (dst) {
			memcpy (dst, m_->begin + used, got0);
			if (SECURITY_ERASE & m_->flags)
				memset (m_->begin + used, 0, got0); // security erase
			dst += got0;
		}
		cb -= got0;
		used += got0;
		if (! (used < m_->size))
			used = 0;
	}
size_t got;
	if (0 < (got = min(cb, m_->stored - used))) {
		if (dst) {
			memcpy (dst, m_->begin + used, got);
			if (SECURITY_ERASE & m_->flags)
				memset (m_->begin + used, 0, got); // security erase
			dst += got;
		}
		cb -= got;
		used += got;
	}
	if (dst)
		m_->used = used;
	return got0 + got;
}
static void ringbuf_dtor (ringbuf_s *m_)
{
	if (SECURITY_ERASE & m_->flags) {
		if (m_->stored < m_->used /* && m_->used < m_->size */) {
			m_->used = 0;
			memset (m_->begin + m_->used, 0, m_->size - m_->used); // security erase
		}
		if (m_->used < m_->stored) {
			m_->used = m_->stored; // safety
			memset (m_->begin + m_->used, 0, m_->stored - m_->used); // security erase
		}
	}
	memset (m_, 0, sizeof(ringbuf_s));
}
static void ringbuf_ctor (ringbuf_s *m_, void *buf_, size_t cb, int flags)
{
	memset (m_, 0, sizeof(ringbuf_s));
	m_->begin = (u8 *)buf_;
	m_->size = cb;
	m_->flags = flags;
}

static int sockopen (const char *hostname, u16 port)
{
int server;
bool is_err;
	is_err = true;
	do {
		// new connection
		if (-1 == (server = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
ERR2("!socket() @err=%d'%s'", errno, unixerror (errno))
			break;
		}
		// active open (CLOSED -> SYN_SENT -> ESTABLISHED)
struct in_addr in_addr;
		memset (&in_addr, 0, sizeof(in_addr));
		if (INADDR_NONE == (in_addr.s_addr = inet_addr (hostname))) {
ERR3("!inet_addr('%s') @err=%d'%s'", hostname, errno, unixerror (errno))
			break;
		}
struct sockaddr_in server_addr;
		memset (&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family      = AF_INET;
		server_addr.sin_port        = htons (port);
		server_addr.sin_addr.s_addr = in_addr.s_addr;
		if (-1 == connect (server, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
ERR7("!connect %d.%d.%d.%d:%d @err=%d'%s'", ((u8 *)&in_addr.s_addr)[0], ((u8 *)&in_addr.s_addr)[1], ((u8 *)&in_addr.s_addr)[2], ((u8 *)&in_addr.s_addr)[3], port, errno, unixerror (errno))
			break;
		}
		is_err = false;
	} while (0);
	if (is_err && !(-1 == server)) {
		close (server);
		server = -1;
	}
	return server;
}

///////////////////////////////////////////////////////////////////////////////
// independent

static size_t ssh2_store_string (u8 *dst, const char *cstr)
{
size_t len;
	store_be32 (dst, len = strlen (cstr));
	memcpy (dst +4, cstr, len);
	return 4 + len;
}

///////////////////////////////////////////////////////////////////////////////
// .ppk file operation

static ssize_t ppk_read_blob (const char *ppk_path, void *dst_, size_t size, const char *prefix)
{
u8 *dst;
	dst = (u8 *)dst_;
FILE *fp;
	if (NULL == (fp = fopen (ppk_path, "r"))) {
WARN1("%s: file cannot open.", ppk_path)
		return 0;
	}
char text[128];
int stage, lines, parsed;
	stage = lines = parsed = 0;
	while (!feof (fp) && stage < 2) {
		*text = '\0';
		fgets (text, sizeof(text), fp);
		text[sizeof(text) -1] = '\0';
char *tail;
		tail = strchr (text, '\0');
		while (text < tail && (u8)tail[-1] < 0x20)
			*--tail = '\0';
size_t len, max_cb;
		switch (stage) {
		case 0:
			if (! (0 == memcmp (prefix, text, len = strlen (prefix))))
				continue;
			lines = atoi (text +len);
			++stage;
			break;
		case 1:
			if ((max_cb = (u8 *)dst_ + size - dst) < PPK_BASE64_MAXBYTE_PER_LINE)
				return -1;
			dst += base64_decode (text, dst, max_cb);
			++parsed;
			if (parsed < lines && !(text < tail && '=' == tail[-1]))
				continue;
			if (parsed < lines || !(text < tail && '=' == tail[-1]))
WARN3("%s" "defined %d lines, but %d lines.", prefix, lines, parsed)
			++stage;
			break;
		}
	}
	return dst - (u8 *)dst_;
}
static ssize_t ppk_read_public_blob (const char *ppk_path, void *dst, size_t size)
{
	return ppk_read_blob (ppk_path, dst, size, "Public-Lines: ");
}
static intv_s *ssh2_public_blob_get_modulus (const void *src_, size_t cb)
{
const u8 *src, *end;
	src = (const u8 *)src_;
	end = (const u8 *)src_ +cb;
size_t len;
	// 'ssh-rsa'
	if (end < src +4 + (len = load_be32 (src)))
		return NULL;
	if (! (7 == len && 0 == memcmp ("ssh-rsa", src +4, len)))
		return NULL;
	src += 4 + len;
	// exponent
	if (end < src +4 + (len = load_be32 (src)))
		return NULL;
	src += 4 + len;
	// modulus
	if (end < src +4 + (len = load_be32 (src)))
		return NULL;
intv_s *pub_mod;
	intv_load_u8be (pub_mod = intv_ctor (8 * len), src +4, len);
	return pub_mod;
}
static ssize_t ppk_read_private_blob (const char *ppk_path, void *dst, size_t size)
{
	return ppk_read_blob (ppk_path, dst, size, "Private-Lines: ");
}
static bool ppk_decrypt_private_blob (void *encrypted, size_t cb, const char *passphrase)
{
u8 seed[4 + PASSPHRASE_MAXLEN];
size_t len;
	if (sizeof(seed) < 4 + (len = strlen (passphrase)))
		return false;
	memcpy (seed +4, passphrase, len);
	store_be32 (seed, 0);
u8 key320[40];
	sha1 (seed, 4 +len, key320);
	store_be32 (seed, 1);
	sha1 (seed, 4 +len, key320 +20);
	memset (seed, 0, sizeof(seed)); // security erase
bool aes_init_ok;
aes_s ctx;
	aes_init_ok = aes_init (&ctx, sizeof(aes_s));
ASSERTE(aes_init_ok)
	aes_setkey (&ctx, key320, 256);
	memset (key320, 0, sizeof(key320)); // security erase
void *plain;
	plain = encrypted;
u8 iv[16];
	memset (iv, 0, sizeof(iv));
	aes_setiv (&ctx, iv);
	aes_cbc_decrypt (&ctx, encrypted, cb, plain);
	memset (&ctx, 0, sizeof(ctx)); // security erase
	// TODO verify MAC
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// generate random

static size_t ssh2_kexinit_cookie_create (void *dst, size_t cb)
{
	store_random (dst, cb);
	return cb;
}

// 1 < return < kex_modulus / 2
static size_t ssh2_kex_privkey_create (const intv_s *kex_modulus, void *u8be, size_t cb_max)
{
u8 *buf;
	buf = (u8 *)u8be;
size_t cb;
	if (cb_max < (cb = RUPDIV(8, kex_modulus->bitused -1)))
		return 0; // caller bug (not enough buffer)
size_t half_cb;
u8 *half;
	half = (u8 *)malloc (half_cb = RUPDIV(8, kex_modulus->bitused));
ASSERTE(half)
	intv_store_u8be (kex_modulus, half);
u16 shift;
	shift = 0;
size_t i;
	for (i = 0; i < half_cb; ++i) {
		shift |= (u16)half[i] << 7;
		half[i] = (u8)(shift >> 8);
		shift <<= 8;
	}
u8 mask0;
	mask0 = (1 << (kex_modulus->bitused -2) % 8 +1) -1;
	do {
		store_random (buf, cb);
		buf[0] &= mask0;
	} while (! (u8be_cmp (buf, cb, half, half_cb) < 0));
	free (half);
	return cb;
}

static void ssh2_pkt_padding (void *dst, size_t cb)
{
	store_random (dst, cb);
}

///////////////////////////////////////////////////////////////////////////////
// key exchange hash (H)

static void kexhash_input_finish (sha256_s *state, void *dst)
{
	sha256_finish (state, dst);
}
static bool kexhash_input_start (sha256_s *state, size_t len)
{
bool sha256_init_ok;
	sha256_init_ok = sha256_init (state, len);
ASSERTE(sha256_init_ok)
	return sha256_init_ok;
}
static void kexhash_input_raw (sha256_s *state, const void *raw, size_t len)
{
	sha256_update (state, raw, len);
}
static void kexhash_input_be32 (sha256_s *state, u32 val)
{
u8 src[4];
	store_be32 (src, val);
	kexhash_input_raw (state, src, sizeof(src));
}
static void kexhash_finish (sha256_s *kexhash_work, size_t pbits, const intv_s *p, u32 g, const intv_s *e, const intv_s *f, const intv_s *k, u8 kexhash[32])
{
	kexhash_input_be32 (kexhash_work, pbits);
u8 tmp[512 +1];
	tmp[0] = 0x00;
size_t len, skip;
	// kex hash #7 p = safe prime
	len = 1 + intv_store_u8be (p, tmp +1);
	len -= skip = (tmp[1] < 0x80) ? 1 : 0;
	kexhash_input_be32 (kexhash_work, len);
	kexhash_input_raw (kexhash_work, tmp +skip, len);
	// kex hash #8 g = generator for subgroup
	store_be32 (tmp +1, g);
	for (len = 5; 1 < len && 0x00 == tmp[5 - len] && tmp[6 - len] < 0x80;)
		--len;
	kexhash_input_be32 (kexhash_work, len);
	kexhash_input_raw (kexhash_work, tmp +5 -len, len);
	// kex hash #9 e = g ^ secret(client) mod p
	len = 1 + intv_store_u8be (e, tmp +1);
	len -= skip = (tmp[1] < 0x80) ? 1 : 0;
	kexhash_input_be32 (kexhash_work, len);
	kexhash_input_raw (kexhash_work, tmp +skip, len);
	// kex hash #10 f = g ^ secret(server) mod p
	len = 1 + intv_store_u8be (f, tmp +1);
	len -= skip = (tmp[1] < 0x80) ? 1 : 0;
	kexhash_input_be32 (kexhash_work, len);
	kexhash_input_raw (kexhash_work, tmp +skip, len);
	// kex hash #11 K = g ^ (secret(client) * secret(server)) mod p
	len = 1 + intv_store_u8be (k, tmp +1);
	len -= skip = (tmp[1] < 0x80) ? 1 : 0;
	kexhash_input_be32 (kexhash_work, len);
	kexhash_input_raw (kexhash_work, tmp +skip, len);
	// 
	kexhash_input_finish (kexhash_work, kexhash);
}

///////////////////////////////////////////////////////////////////////////////
// encrypt/decrypt/MAC key

struct ssh2keygen {
	u8 *seed;
	size_t seed_len;
	size_t variable;
};

static void *ssh2key_security_erase (void *this__)
{
struct ssh2keygen *this_;
	this_ = (struct ssh2keygen *)this__;
	memset (this_->seed, 0, this_->seed_len);
}

static void *ssh2key_dtor (void *this__)
{
struct ssh2keygen *this_;
	this_ = (struct ssh2keygen *)this__;
	if (this_->seed)
		free (this_->seed);
	free (this_);
}
static void *ssh2key_ctor ()
{
	return malloc (sizeof(struct ssh2keygen));
}

static bool ssh2key_init (void *this__, const intv_s *k, const u8 *h, size_t h_len, const u8 *sid, size_t sid_len)
{
struct ssh2keygen *this_;
	this_ = (struct ssh2keygen *)this__;
size_t k_len;
	k_len = (k->bitused +7 +1) / 8;
	if (NULL == (this_->seed = (u8 *)malloc (this_->seed_len = 4 +k_len +h_len +1 +sid_len)))
		return false;
u8 *dst;
	dst = this_->seed;
	store_be32 (dst, k_len);
	dst += 4;
	*dst = 0x00;
	intv_store_u8be (k, dst +(0 == k->bitused % 8 ? 1 : 0));
	dst += k_len;
	memcpy (dst, h, h_len);
	dst += h_len;
	this_->variable = dst - this_->seed;
	++dst;
	memcpy (dst, sid, sid_len);
	return true;
}
static void ssh2key_store_be512 (void *this__, int c, void *dst_)
{
struct ssh2keygen *this_;
	this_ = (struct ssh2keygen *)this__;
u8 *dst;
	dst = (u8 *)dst_;
	this_->seed[this_->variable] = (u8)c;
bool sha256_init_ok;
sha256_s state;
	sha256_init_ok = sha256_init (&state, sizeof(state));
ASSERTE(sha256_init_ok)
	sha256_update (&state, this_->seed, this_->seed_len);
	sha256_finish (&state, dst +0);

	sha256_init_ok = sha256_init (&state, sizeof(state));
ASSERTE(sha256_init_ok)
	sha256_update (&state, this_->seed, this_->variable);
	sha256_update (&state, dst +0, 32);
	sha256_finish (&state, dst +32);
}
static void ssh2keys_generate (const intv_s *k, const u8 *kexhash, size_t kexhash_len, const u8 *session_id, size_t session_id_len, aes_s *enc_ctx, aes_s *dec_ctx, u8 *emac_key, u8 *dmac_key)
{
struct ssh2keygen *seed;
	ssh2key_init (seed = ssh2key_ctor (), k, kexhash, kexhash_len, session_id, session_id_len);
u8 key512[64];
bool aes_init_ok;
	// client encryption
	aes_init_ok = aes_init (enc_ctx, sizeof(aes_s));
ASSERTE(aes_init_ok)
	ssh2key_store_be512 (seed, 'C', key512);
	aes_setkey (enc_ctx, key512, 256);
	ssh2key_store_be512 (seed, 'A', key512);
	aes_setiv (enc_ctx, key512); // use first 128-bits
	ssh2key_store_be512 (seed, 'E', key512);
	memcpy (emac_key, key512, 20);
	// server decryption
	aes_init_ok = aes_init (dec_ctx, sizeof(aes_s));
ASSERTE(aes_init_ok)
	ssh2key_store_be512 (seed, 'D', key512);
	aes_setkey (dec_ctx, key512, 256);
	ssh2key_store_be512 (seed, 'B', key512);
	aes_setiv (dec_ctx, key512); // use first 128-bits
	ssh2key_store_be512 (seed, 'F', key512);
	memcpy (dmac_key, key512, 20);
	ssh2key_security_erase (seed);
	ssh2key_dtor (seed);
	seed = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// SSH-2 negotiation control

enum ssh2_msg {
	SSHVER_LIBID       = 1,
	KEXINIT            = 2,
	DH_GEX_REQUEST_OLD = 4,
	DH_GEX_GROUP       = 4,
	DH_GEX_INIT        = 8,
	DH_GEX_REPLY       = 8,
	NEWKEYS            = 16,
	SERVICE_REQUEST    = 32,
	SERVICE_ACCEPT     = 32,
	USERAUTH_REQUEST   = 64,
	USERAUTH_SUCCESS   = 64,
	CHANNEL_OPEN              = 128,
	CHANNEL_OPEN_CONFIRMATION = 128,
	CHANNEL_REQUEST    = 256,
	CHANNEL_SUCCESS    = 256,
	CHANNEL_CLOSE      = 512,
};
typedef struct ssh2_ ssh2_s;
struct ssh2_ {
	u32 server_window_size;
	u16 ws_col;
	u16 ws_row;
	u16 port;
	char hostname[HOSTNAME_MAXLEN +1];
	char ppk_path[PPK_PATH_MAXLEN +1];
	size_t cb_client_kexinit;
	const u8 *client_kexinit;
	enum ssh2_msg sent;
	enum ssh2_msg received;
	size_t done;
	size_t pbits;
	size_t pkt_align;
	intv_s *p;
	u32 g;
	intv_s *a;
	intv_s *e;
	intv_s *f;
	intv_s *k;
	u8 kexhash[32];
	u8 kexhash0[32];
	aes_s enc_ctx;
	u32 emac_seq;
	u8 emac_key[20];
	aes_s dec_ctx;
	u32 dmac_seq;
	u8 dmac_key[20];
	u8 recv0[CIPHER_BLOCK_MAXLEN];
	u8 user_inputstate;
	u8 user_authstate;
	char login_name[USERNAME_MAXLEN +1];
	char passphrase[PASSPHRASE_MAXLEN +1];
	u8 user_privexp[4 +1 + RUPDIV(8, USERPRIV_MAXBITS)];
	u8 chanreq_stage;
	u8 kexhash_rsa2_low20_plus1[1 +20];
	sha256_s kexhash_work;
	Lock read_lock;
	Lock write_lock;
	void *readbuf_;
	void *writebuf_;
	ringbuf_s readbuf;
	ringbuf_s writebuf;
};

static bool is_enough_window_size (ssh2_s *m_, size_t expected)
{
	return (m_->server_window_size < expected
		&& CHANNEL_OPEN_CONFIRMATION & m_->received) ? false : true;
}

///////////////////////////////////////////////////////////////////////////////
// create SSH client message

static size_t store_kexinit (void *dst_, size_t size)
{
static const char kex_algs_cstr[] = "diffie-hellman-group-exchange-sha256"
//",diffie-hellman-group-exchange-sha1,diffie-hellman-group14-sha1,diffie-hellman-group1-sha1"
;
static const char servauth_algs_cstr[] = "ssh-rsa"
//",ssh-dss"
;
static const char crypt_algs_cstr[] = "aes256-ctr"
//",aes256-cbc,rijndael-cbc@lysator.liu.se,aes192-ctr,aes192-cbc,aes128-ctr,aes128-cbc,blowfish-ctr,blowfish-cbc,3des-ctr,3des-cbc,arcfour256,arcfour128"
;
static const char mac_algs_cstr[] = "hmac-sha1"
//",hmac-sha1-96,hmac-md5"
;
static const char pack_algs_cstr[] = "none"
//",zlib"
;
u8 *dst;
	dst = (u8 *)dst_;
	*dst++ = SSH2_MSG_KEXINIT;
	ssh2_kexinit_cookie_create (dst, 16);
	dst += 16;
	store_be32 (dst, strlen (kex_algs_cstr));
	strcpy ((char *)dst +4, kex_algs_cstr);
	dst += 4 + strlen (kex_algs_cstr);
	store_be32 (dst, strlen (servauth_algs_cstr));
	strcpy ((char *)dst +4, servauth_algs_cstr);
	dst += 4 + strlen (servauth_algs_cstr);
	store_be32 (dst, strlen (crypt_algs_cstr));
	strcpy ((char *)dst +4, crypt_algs_cstr);
	dst += 4 + strlen (crypt_algs_cstr);
	store_be32 (dst, strlen (crypt_algs_cstr));
	strcpy ((char *)dst +4, crypt_algs_cstr);
	dst += 4 + strlen (crypt_algs_cstr);
	store_be32 (dst, strlen (mac_algs_cstr));
	strcpy ((char *)dst +4, mac_algs_cstr);
	dst += 4 + strlen (mac_algs_cstr);
	store_be32 (dst, strlen (mac_algs_cstr));
	strcpy ((char *)dst +4, mac_algs_cstr);
	dst += 4 + strlen (mac_algs_cstr);
	store_be32 (dst, strlen (pack_algs_cstr));
	strcpy ((char *)dst +4, pack_algs_cstr);
	dst += 4 + strlen (pack_algs_cstr);
	store_be32 (dst, strlen (pack_algs_cstr));
	strcpy ((char *)dst +4, pack_algs_cstr);
	dst += 4 + strlen (pack_algs_cstr);
	store_be32 (dst +0, 0);
	store_be32 (dst +4, 0);
	memset (dst +8, 0, 5);
	dst += 8 +5;
	return dst - (u8 *)dst_;
}
static size_t store_dh_gex_request_old (void *dst_, size_t size, size_t pbits)
{
	if (size < 5)
		return 0;
u8 *dst;
	dst = (u8 *)dst_;
	*dst = SSH2_MSG_KEX_DH_GEX_REQUEST_OLD;
	store_be32 (dst +1, pbits);
	return 5;
}
static size_t store_dh_gex_init (void *dst_, size_t size, const intv_s *e)
{
u8 *dst;
	dst = (u8 *)dst_;
size_t cb;
	cb = RUPDIV(8, e->bitused);
size_t prefix;
	prefix = 3 - (cb +3) % 4;
	prefix += (0 == e->bitused % 32) ? 1 : 0;
	if (size < 5 +prefix +cb)
		return 0;
	*(dst +0) = SSH2_MSG_KEX_DH_GEX_INIT;
	if (0 < prefix)
		memset (dst +5, 0x00, prefix);
	dst += 5 +prefix + intv_store_u8be (e, dst +5 +prefix);
	store_be32 ((u8 *)dst_ +1, dst - (u8 *)dst_ -5);
	return dst - (u8 *)dst_;
}

static size_t create_sign (void *dst_, size_t size, const void *session_id_v2, const void *src, size_t src_len, const intv_s *priv_exp, const intv_s *modulus)
{
size_t asn1_cb;
	asn1_cb = RUPDIV(8, modulus->bitused) -1;
ASSERTE(1 + 1 + 20 <= asn1_cb) // 01 ff+ sha1
u8 *work;
	work = (u8 *)malloc (asn1_cb);
ASSERTE(work) // out of memory
	work[0] = 0x01;
	memset (work +1, 0xff, asn1_cb -20 -sizeof(ASN1_TRUE) -1);
	memcpy (work +asn1_cb -20 -sizeof(ASN1_TRUE), ASN1_TRUE, sizeof(ASN1_TRUE));
	sha1 (src, src_len, work +asn1_cb -20);
bool sha1_init_ok;
sha1_s sha1;
	sha1_init_ok = sha1_init (&sha1, sizeof(sha1_s));
ASSERTE(sha1_init_ok)
u8 tmp[4];
	store_be32 (tmp, 32);
	sha1_update (&sha1, tmp, sizeof(tmp));
	sha1_update (&sha1, session_id_v2, 32);
	sha1_update (&sha1, src, src_len);
	sha1_finish (&sha1, work +asn1_cb -20);
intv_s *asn1_hash;
	intv_load_u8be (asn1_hash = intv_ctor (max (8 * asn1_cb +64, 2 * modulus->bitused)), work, asn1_cb);
	free (work);
intv_s *signature;
	vmodpow (signature = intv_ctor (2 * modulus->bitused), asn1_hash, priv_exp, modulus, NULL, NULL, 0);
	intv_dtor (asn1_hash);
	if (size < RUPDIV(8, signature->bitused +1)) {
		intv_dtor (signature);
		return 0; // TODO fatal: output buffer overflow
	}
u8 *dst;
	dst = (u8 *)dst_;
	if (0 == signature->bitused % 8)
		*dst++ = 0x00;
	dst += intv_store_u8be (signature, dst);
	intv_dtor (signature);
	return dst - (u8 *)dst_;
}

static size_t echo_back (ssh2_s *m_, const char *src, size_t cb)
{
size_t written;
lock_lock (&m_->read_lock);
	written = ringbuf_write (&m_->readbuf, src, cb);
lock_unlock (&m_->read_lock);
	if (written < cb)
fprintf (stderr_old, VTRR "WARN" VTO " stdout buffer overflow (MAX %d bytes), disposed %d bytes." "\n", STDOUT_MAXBUF, cb - written);
	return written;
}
#define ECHO_OFF 1
static size_t easy_line_input (ssh2_s *m_, const char *input, size_t len_i, char *output, size_t maxlen_o, size_t got_o, int flags)
{
size_t size;
char echo_[128], *echo;
	echo = echo_;
const char *p, *end;
	p = input;
	end = p + len_i;
	while (p < end) {
char c;
		c = *p++;
		if ('\r' == c) {
			--p;
			*echo++ = c;
			*echo++ = '\n';
		}
		else if (0x08 == c) {
			if (! (0 < got_o))
				continue;
			output[--got_o] = '\0';
			if (ECHO_OFF & ~flags) {
				*echo++ = c;
				*echo++ = ' ';
				*echo++ = 0x08;
			}
		}
		else if (c < 0x20 || 0x7e < c)
			continue;
		else {
			if (got_o +1 < maxlen_o) {
				output[got_o++] = (char)c;
				output[got_o] = '\0';
			}
			if (ECHO_OFF & ~flags)
				*echo++ = c;
		}
		if (echo +3 <= echo_ + sizeof(echo_) && p < end && !('\r' == *p) || echo_ == echo)
			continue;
		echo_back (m_, echo_, echo - echo_);
		echo = echo_;
		if ('\r' == *p)
			break;
	}
	memset (echo_, 0, sizeof(echo_)); // security erase
	return p - input;
}
static void pump_input_userauth (ssh2_s *m_)
{
	if (! (m_->user_inputstate < 7))
		return;
size_t cb;
u8 *src, *end;
	src = NULL;
lock_lock (&m_->read_lock);
	if (0 < (cb = ringbuf_read (&m_->writebuf, NULL, STDIN_MAXBUF))) {
ALLOCA_ENV(cb)
		src = (u8 *)alloca (cb);
ASSERTE(src)
		cb = ringbuf_read (&m_->writebuf, src, cb);
		end = src + cb;
	}
lock_unlock (&m_->read_lock);

u8 ppk_priv[RSA2_PRIVKEY_MAXLEN];
size_t ppk_priv_len;
bool do_jump;
	do { do_jump = false; switch (m_->user_inputstate) {
	case 0:
		echo_back (m_, "login as: ", 0);
		++m_->user_inputstate;
	case 1:
		if (! (src && src < end))
			return;
		src += easy_line_input (m_, src, end - src, m_->login_name, sizeof(m_->login_name), strlen (m_->login_name), 0);
		if (! ('\r' == *src))
			break;
		++src;
		m_->user_inputstate = 2 +1;
		do_jump = true;
		continue;
#define LOGIN_NAME_CORRECT 2
	case 2: // passphrase input retry
		echo_back (m_, "Wrong passphrase" "\r\n", 0);
		++m_->user_inputstate;
	case 3:
		echo_back (m_, "Passphrase: ", 0);
		++m_->user_inputstate;
	case 4:
		if (! (src && src < end))
			return;
		src += easy_line_input (m_, src, end - src, m_->passphrase, sizeof(m_->passphrase), strlen (m_->passphrase), ECHO_OFF);
		if (! ('\r' == *src))
			break;
		++src;
		++m_->user_inputstate;
	case 5:
		ppk_priv_len = 0;
		do {
#if 0
#else
			if ((ssize_t)(ppk_priv_len = (size_t)ppk_read_private_blob (m_->ppk_path, ppk_priv, sizeof(ppk_priv))) < 1) {
ERR0("cannot read user-auth public key.")
				m_->user_inputstate = 6; // unrecoverable error
				break;
			}
			ppk_decrypt_private_blob (ppk_priv, ppk_priv_len, m_->passphrase);
			if (ppk_priv_len < 4 + load_be32 (ppk_priv)) {
WARN2("rsa2 private exponent too long. (max %d bytes but %d bytes)", min(ppk_priv_len, sizeof(m_->user_privexp)) -4, load_be32 (ppk_priv))
				m_->user_inputstate = 2; // passphrase input retry
				break;
			}
			if (sizeof(m_->user_privexp) < 4 + load_be32 (ppk_priv)) {
				m_->user_inputstate = 2; // passphrase input retry
				break;
			}
			memcpy (m_->user_privexp, ppk_priv, 4 + load_be32 (ppk_priv));
			m_->user_inputstate = 7;
#endif
		} while (0);
		memset (m_->passphrase, 0, strlen (m_->passphrase)); // security erase
		memset (ppk_priv, 0, ppk_priv_len); // security erase
		if (! (6 == m_->user_inputstate)) {
			do_jump = true;
			continue;
		}
	case 6: // unrecoverable error
		exit (-1); // TODO legal exit process.
#define PASSPHRASE_CORRECT 7
	case 7: // right passphrase
	default:
		break;
	} } while (do_jump);
}
static void pump_input_userauth_thunk (void *this_)
{
	pump_input_userauth ((ssh2_s *)this_);
}

static size_t pump_send_queue0 (ssh2_s *m_, void *dst_, size_t size)
{
	if (SSHVER_LIBID & ~m_->sent) {
		strcpy ((char *)dst_, SSH_CLIENT_ID "\r\n");
size_t len;
		len = strlen ((char *)dst_);
		kexhash_input_be32 (&m_->kexhash_work, len -2);
		kexhash_input_raw (&m_->kexhash_work, dst_, len -2);
		m_->sent |= SSHVER_LIBID;
		return len;
	}
	if (SSHVER_LIBID & ~m_->received)
		return 0;

u8 *dst;
	dst = (u8 *)dst_;
size_t msg_size;
	if (CHANNEL_CLOSE & ~m_->sent && CHANNEL_CLOSE & m_->received) {
		dst[5] = SSH2_MSG_CHANNEL_CLOSE;
		dst += 6;
		dst += store_be32 (dst, 0); // remote id
		msg_size = dst - ((u8 *)dst_ +5);
		m_->sent |= CHANNEL_CLOSE;
	}
	else if (!is_enough_window_size (m_, WINDOW_MINLEN / 2)) {
		dst[5] = SSH2_MSG_CHANNEL_WINDOW_ADJUST;
		dst += 6;
		dst += store_be32 (dst, 0); // remote id
		dst += store_be32 (dst, WINDOW_MINLEN - m_->server_window_size);
		msg_size = dst - ((u8 *)dst_ +5);
		m_->server_window_size = WINDOW_MINLEN;
	}
	else if (CHANNEL_SUCCESS & m_->received) {
size_t cb;
u8 *src;
		src = NULL;
lock_lock (&m_->read_lock);
		if (0 < (cb = ringbuf_read (&m_->writebuf, NULL, STDIN_MAXBUF))) {
ALLOCA_ENV(cb)
			src = (u8 *)alloca (cb);
ASSERTE(src)
			cb = ringbuf_read (&m_->writebuf, src, cb);
		}
lock_unlock (&m_->read_lock);
		if (! (0 < cb /*&& src*/))
			return 0; // no queue
		dst[5] = SSH2_MSG_CHANNEL_DATA;
		dst += 6;
		dst += store_be32 (dst, 0); // remote id
		if (size < 6 +4 +4 +cb)
WARN3("write buffer over: max %d bytes but %d input, disposed %d bytes", size -(6 +4 +4), cb, cb - size +6 +4 +4)
		cb = min(cb, size -(6 +4 +4));
		dst += store_be32 (dst, cb);
		memcpy (dst, src, cb);
		dst += cb;
		msg_size = dst - ((u8 *)dst_ +5);
	}
	else if (CHANNEL_REQUEST & ~m_->sent && CHANNEL_OPEN_CONFIRMATION & m_->received) {
		dst[5] = SSH2_MSG_CHANNEL_REQUEST;
		dst += 6;
u8 *enc_term_modes_begin;
		switch (m_->chanreq_stage) {
		case 0:
			dst += store_be32 (dst, 0);
			dst += ssh2_store_string (dst, "pty-req");
			*dst++ = 0x01; // want_reply (TRUE)
			dst += ssh2_store_string (dst, "xterm"); // other 'vt100' ...
			dst += store_be32 (dst, m_->ws_col); // ws_col
			dst += store_be32 (dst, m_->ws_row); // ws_row
			dst += store_be32 (dst, 0); // xpixels
			dst += store_be32 (dst, 0); // ypixels
			dst += 4; // (4)len
			enc_term_modes_begin = dst;
			*dst++ = 3;
			dst += store_be32 (dst, 127); // VERASE(3) 127
			*dst++ = 128;
			dst += store_be32 (dst, INPUT_SPEED); // TTY_OP_ISPEED(128) [bit/s] input
			*dst++ = 129;
			dst += store_be32 (dst, OUTPUT_SPEED); // TTY_OP_OSPEED(129) [bit/s] output
			*dst++ = 0;
			store_be32 (enc_term_modes_begin -4, dst - enc_term_modes_begin);
			break;
		case 1:
			dst += store_be32 (dst, 0);
			dst += ssh2_store_string (dst, "shell");
			*dst++ = 0x01; // TRUE
			break;
		default:
			break;
		}
		msg_size = dst - ((u8 *)dst_ +5);
		m_->sent |= CHANNEL_REQUEST;
	}
	else if (CHANNEL_OPEN & ~m_->sent && USERAUTH_SUCCESS & m_->received) {
		dst[5] = SSH2_MSG_CHANNEL_OPEN;
		dst += 6;
		dst += ssh2_store_string (dst, "session");
		dst += store_be32 (dst, 0x100); // local id
		dst += store_be32 (dst, WINDOW_MINLEN); // local window size
		dst += store_be32 (dst, CHANNEL_DATA_MAXLEN); // local max pkt size
		msg_size = dst - ((u8 *)dst_ +5);
		m_->sent |= CHANNEL_OPEN;
		m_->server_window_size = WINDOW_MINLEN;
	}
	else if (USERAUTH_REQUEST & ~m_->sent && SERVICE_ACCEPT & m_->received) {
		dst[5] = SSH2_MSG_USERAUTH_REQUEST;
u8 *end;
		end = dst +size;
		dst += 6;
size_t len;
intv_s *priv_exp;
		priv_exp = NULL;
u8 *signed_begin, *signed_end;
intv_s *pub_mod;
		switch (m_->user_authstate) {
		case 0:
			if (m_->user_inputstate < LOGIN_NAME_CORRECT)
				break;
			dst += ssh2_store_string (dst, m_->login_name);
			dst += ssh2_store_string (dst, "ssh-connection");
			dst += ssh2_store_string (dst, "none");
			break;
		case 1:
			if (m_->user_inputstate < LOGIN_NAME_CORRECT)
				break;
			dst += ssh2_store_string (dst, m_->login_name);
			dst += ssh2_store_string (dst, "ssh-connection");
			dst += ssh2_store_string (dst, "publickey");
			*dst++ = 0x00; // FALSE
			dst += ssh2_store_string (dst, "ssh-rsa");
			dst += 4;
			if ((ssize_t)(len = (size_t)ppk_read_public_blob (m_->ppk_path, dst, end - dst)) < 1) {
ERR0("cannot read user-auth public key.")
				exit (-1); // TODO error handling
			}
			dst += len;
			store_be32 (dst -(4 +len), len);
			break;
		case 2:
			if (m_->user_inputstate < PASSPHRASE_CORRECT)
				break;
			signed_begin = dst -1;
			dst += ssh2_store_string (dst, m_->login_name);
			dst += ssh2_store_string (dst, "ssh-connection");
			dst += ssh2_store_string (dst, "publickey");
			*dst++ = 0x01; // TRUE
			dst += ssh2_store_string (dst, "ssh-rsa");
			dst += 4;
			if ((ssize_t)(len = (size_t)ppk_read_public_blob (m_->ppk_path, dst, end - dst)) < 1) {
ERR0("cannot read user-auth public key.")
				memset (m_->user_privexp, 0, sizeof(m_->user_privexp)); // security erase
				exit (-1); // TODO error handling
			}
			dst += len;
			pub_mod = ssh2_public_blob_get_modulus (dst -len, len);
			store_be32 (dst -(4 +len), len);
			signed_end = dst;
			dst += 4;
			dst += ssh2_store_string (dst, "ssh-rsa");
			dst += 4;
			intv_load_u8be (priv_exp = intv_ctor (8 * load_be32 (m_->user_privexp)), m_->user_privexp +4, load_be32 (m_->user_privexp));
			memset (m_->user_privexp, 0, sizeof(m_->user_privexp)); // security erase
			dst += (len = create_sign (dst, end - dst, m_->kexhash0, signed_begin, signed_end - signed_begin, priv_exp, pub_mod)); // TODO error handling
			intv_security_erase (priv_exp);
			intv_dtor (priv_exp);
			intv_dtor (pub_mod);
			store_be32 (dst -(4 +len), len);
			store_be32 (signed_end, dst - (signed_end +4));
			break;
		}
		if ((msg_size = dst - ((u8 *)dst_ +5)) < 2)
			return 0; // passphrase input not finished.
		m_->sent |= USERAUTH_REQUEST;
	}
	else if (SERVICE_REQUEST & ~m_->sent && NEWKEYS & m_->received && NEWKEYS & m_->sent) {
		dst[5] = SSH2_MSG_SERVICE_REQUEST;
		strcpy (dst +6 +4, "ssh-userauth");
size_t len;
		store_be32 (dst +6, len = strlen (dst +6 +4));
		msg_size = 1 +4 +len;
		m_->sent |= SERVICE_REQUEST;
	}
	else if (NEWKEYS & ~m_->sent && NEWKEYS & m_->received && DH_GEX_REPLY & m_->received) {
		dst[5] = SSH2_MSG_NEWKEYS;
		msg_size = 1;
		m_->sent |= NEWKEYS;
		m_->pkt_align = 16;
	}
	else if (DH_GEX_INIT & ~m_->sent && DH_GEX_GROUP & m_->received) {
size_t cb_max;
u8 *buf;
		buf = (u8 *)malloc (cb_max = RUPDIV(8, m_->p->bitused));
ASSERTE(buf)
size_t cb;
		cb = ssh2_kex_privkey_create (m_->p, buf, cb_max);
		intv_load_u8be (m_->a = intv_ctor (m_->p->bitused +64), buf, cb);
		m_->e = intv_ctor (2 * m_->p->bitused);
		vmodpow32 (m_->e, m_->g, m_->a, m_->p, pump_input_userauth_thunk, m_, MODPOW_PAUSE_SPAN_BITS); // Heavy
		msg_size = store_dh_gex_init (dst +5, sizeof(dst) -5, m_->e);
		m_->sent |= DH_GEX_INIT;
	}
	else if (DH_GEX_REQUEST_OLD & ~m_->sent && KEXINIT & m_->received && KEXINIT & m_->sent) {
		msg_size = store_dh_gex_request_old (dst +5, sizeof(dst) -5, m_->pbits);
		m_->sent |= DH_GEX_REQUEST_OLD;
	}
	else if (KEXINIT & ~m_->sent) {
		msg_size = store_kexinit (dst +5, sizeof(dst) -5);
		kexhash_input_be32 (&m_->kexhash_work, msg_size);
		kexhash_input_raw (&m_->kexhash_work, dst +5, msg_size);
		// WARNING Must need one features.
		// 1. pump_send_queue0() and it's caller never crush
		//    memory of ssh2::client_kexinit unless KEXINIT flag
		//    of ssh2::reserved is set.
		// ref) pump_recv_queue0() ssh2_thread()
		m_->client_kexinit = dst +5;
		m_->cb_client_kexinit = msg_size;
		m_->sent |= KEXINIT;
	}
	else
		return 0; // no queue
size_t padding;
	padding = m_->pkt_align +3 - (5 + msg_size +3) % m_->pkt_align;
	dst = (char *)dst_;
	store_be32 (dst, 1 + msg_size + padding);
	dst[4] = (char)(u8)padding;
	if (0 < padding)
		if (NEWKEYS & m_->received)
			ssh2_pkt_padding (dst +5 +msg_size, padding);
		else
			memset (dst +5 +msg_size, 0, padding);
	return 5 +msg_size +padding;
}
static size_t pump_send_queue (ssh2_s *m_, void *dst_, size_t size)
{
size_t stored;
	if (0 == (stored = pump_send_queue0 (m_, dst_, size)))
		return 0;
	if (SERVICE_REQUEST & m_->sent) {
u8 *dst;
		dst = (u8 *)dst_;
		// SSH2 client MAC generate
bool sha1_hmac_init_ok;
sha1_hmac_s emac_ctx;
		sha1_hmac_init_ok = sha1_hmac_init (&emac_ctx, sizeof(sha1_hmac_s), m_->emac_key, 20);
ASSERTE(sha1_hmac_init_ok)
u8 tmp[4];
		store_be32 (tmp, m_->emac_seq);
		sha1_hmac_update (&emac_ctx, tmp, sizeof(tmp));
		sha1_hmac_update (&emac_ctx, dst, stored);
		sha1_hmac_finish (&emac_ctx, dst +stored);
		// SSH2 client encrypt
		aes_ctr_encrypt (&m_->enc_ctx, dst, stored, dst);
		stored += 20;
	}
	if (KEXINIT & m_->sent)
		++m_->emac_seq;
	return stored;
}

///////////////////////////////////////////////////////////////////////////////
// parse SSH server message

// #20 KEXINIT
static size_t on_server_kexinit (size_t body_size, const void *src_, size_t count, size_t done)
{
	if (count < body_size)
		return 0;
const u8 *src;
	src = (const u8 *)src_;
	return body_size;
}
// #21 NEWKEYS
static size_t on_newkeys (size_t body_size, const void *src_, size_t count, size_t done)
{
	if (count < body_size)
		return 0;
	return body_size;
}
// #31 KEX_DH_GEX_GROUP
static ssize_t on_kex_dh_gex_group (size_t body_size, const void *src_, size_t cb, size_t done, intv_s **p, u32 *g)
{
	if (cb < body_size)
		return 0;
const u8 *src, *end;
	src = (const u8 *)src_, end = src +cb;
	do {
size_t len;
		// safe prime (modulus)
		if (end < src +4 || end < src +4 +(len = load_be32 (src)))
			break;
		intv_load_u8be (*p = intv_ctor (8 * len +64), src +4, len);
		src += 4 +len;
		// generator for subgroup in GF(p)
		if (end < src +4 || end < src +4 +(len = load_be32 (src)))
			break;
# if 0
		intv_load_u8be (*g = intv_ctor (max(8 * len +64, 2 * (*p)->bitused)), src +4, len);
		src += 4 +len;
# else // G = 1..32bits
		if (! (len < 5 || 5 == len && 0 == src[4]))
			break;
		src += 4;
		if (5 == len)
			++src, --len;
		*g = 0;
size_t i;
		for (i = 0; i < len; ++i)
			*g = *g << 8 | *src++;
# endif
ASSERTE(end == src)
		return src - (const u8 *)src_;
	} while (0);
ERR1("!#%d(KEX_DH_GEX_GROUP) invalid packet received.", SSH2_MSG_KEX_DH_GEX_GROUP)
	return -1;
}
// #33 KEX_DH_GEX_REPLY
static ssize_t on_kex_dh_gex_reply (ssh2_s *m_, size_t body_size, const void *src_, size_t cb, size_t done, const intv_s *p, const intv_s *a, intv_s **f, intv_s **k, void *kexhash_work, void *verify_)
{
	if (cb < body_size)
		return 0;
u8 *verify;
	verify = (u8 *)verify_;
const u8 *src, *end;
	src = (const u8 *)src_, end = src +cb;
	do {
size_t len;
		// server public key
			// 'ssh-rsa'
		if (end < src +4 || end < src +4 +(len = load_be32 (src)))
			break;
		src += 4;
		if (end < src +4 || end < src +4 +(len = load_be32 (src)))
			break;
		if (! (7 == len && 0 == memcmp ("ssh-rsa", src +4, 7))) // TODO
			break;
		src += 4 +len;
			// KEX exponent
		if (end < src +4 || end < src +4 +(len = load_be32 (src)))
			break;
intv_s *exponent;
		intv_load_u8be (exponent = intv_ctor (8 * len +64), src +4, len);
		src += 4 +len;
			// KEX modulus
		if (end < src +4 || end < src +4 +(len = load_be32 (src)))
			break;
intv_s *modulus;
		intv_load_u8be (modulus = intv_ctor (8 * len +64), src +4, len);
		if (! (modulus->bitused <= KEX_MODULUS_MAXBITS))
			return 0;
		src += 4 +len;
ASSERTE(4 + load_be32 (src_) == src - (const u8 *)src_)
		kexhash_input_raw (kexhash_work, src_, src - (const u8 *)src_);
		// KEX value(server) f = g ^ secret(server) mod p
		if (end < src +4 || end < src +4 +(len = load_be32 (src)))
			break;
		intv_load_u8be (*f = intv_ctor (8 * len +64), src +4, len);
intv_s *f_;
		intv_load_u8be (f_ = intv_ctor (max (8 * len +64, 2 * p->bitused)), src +4, len);
		src += 4 +len;
		// KEX server-signature of H (RSA2)
			// 'ssh-rsa'
		if (end < src +4 || end < src +4 +(len = load_be32 (src)))
			break;
		src += 4;
		if (end < src +4 || end < src +4 +(len = load_be32 (src)))
			break;
		if (! (7 == len && 0 == memcmp ("ssh-rsa", src +4, 7))) // TODO
			break;
		src += 4 +len;
			// base of exponent
		if (end < src +4 || end < src +4 +(len = load_be32 (src)))
			break;
intv_s *signature;
		intv_load_u8be (signature = intv_ctor (max(8 * len +64, 2 * modulus->bitused)), src +4, len);
		src += 4 +len;
ASSERTE(end == src)
		// KEX server-signature verification
intv_s *result;
		vmodpow (result = intv_ctor (2 * modulus->bitused), signature, exponent, modulus, NULL, NULL, 0);
u8 tmp[RUPDIV(8, KEX_MODULUS_MAXBITS)];
		if (verify && RUPDIV(8, result->bitused) <= sizeof(tmp)) {
			len = intv_store_u8be (result, tmp);
			verify[0] = 0;
			do {
				if (! (9 + 8 * (sizeof(ASN1_TRUE) + 20) <= result->bitused))
					break;
const u8 *asn1_verify;
				asn1_verify = tmp +len -20 -sizeof(ASN1_TRUE);
				if (! (0 == memcmp (ASN1_TRUE, asn1_verify, sizeof(ASN1_TRUE))))
					break;
				verify[0] = 0xfe ^ tmp[0]; // safety (should be 0x01 by spec of intv::bitused)
size_t i;
				for (i = 1; tmp +i < asn1_verify; ++i)
					verify[0] &= tmp[i];
			} while (0);
			memcpy (verify +1, tmp +len -20, 20);
		}
		vmodpow (*k = intv_ctor (2 * p->bitused), f_, a, p, pump_input_userauth_thunk, m_, MODPOW_PAUSE_SPAN_BITS); // Heavy
		intv_dtor (f_);
		intv_dtor (result);
		intv_dtor (signature);
		intv_dtor (modulus);
		intv_dtor (exponent);
		return src - (const u8 *)src_;
	} while (0);
ERR1("!#%d(KEX_DH_GEX_REPLY) invalid packet received.", SSH2_MSG_KEX_DH_GEX_REPLY)
	return -1;
}
struct algorythms_ {
	  u16 kex_cb;
	char *kex_csv; // not 'const' for '\0' given support.
	  u16 auth_cb;
	char *auth_csv;
	  u16 cli_enc_cb;
	char *cli_enc_csv;
	  u16 srv_enc_cb;
	char *srv_enc_csv;
	  u16 cli_mac_cb;
	char *cli_mac_csv;
	  u16 srv_mac_cb;
	char *srv_mac_csv;
	  u16 cli_cmprs_cb;
	char *cli_cmprs_csv;
	  u16 srv_cmprs_cb;
	char *srv_cmprs_csv;
	  u16 cli_lang_cb;
	char *cli_lang_csv;
	  u16 srv_lang_cb;
	char *srv_lang_csv;
};
typedef struct algorythms_ algorythms_s;
static bool kexinit_parse (const void *src_, size_t cb, algorythms_s *dst)
{
const u8 *src;
	src = (const u8 *)src_;
const char *end;
	end = (const char *)(src +cb);
	if (end < (const char *)(src +16 +4))
		return false;
	src += 16;
	if (end < (dst->kex_csv = (char *)(src +4)) + (dst->kex_cb = load_be32 (src)) +4)
		return false;
	src += 4 + dst->kex_cb;
	if (end < (dst->auth_csv = (char *)(src +4)) + (dst->auth_cb = load_be32 (src)) +4)
		return false;
	src += 4 + dst->auth_cb;
	if (end < (dst->cli_enc_csv = (char *)(src +4)) + (dst->cli_enc_cb = load_be32 (src)) +4)
		return false;
	src += 4 + dst->cli_enc_cb;
	if (end < (dst->srv_enc_csv = (char *)(src +4)) + (dst->srv_enc_cb = load_be32 (src)) +4)
		return false;
	src += 4 + dst->srv_enc_cb;
	if (end < (dst->cli_mac_csv = (char *)(src +4)) + (dst->cli_mac_cb = load_be32 (src)) +4)
		return false;
	src += 4 + dst->cli_mac_cb;
	if (end < (dst->srv_mac_csv = (char *)(src +4)) + (dst->srv_mac_cb = load_be32 (src)) +4)
		return false;
	src += 4 + dst->srv_mac_cb;
	if (end < (dst->cli_cmprs_csv = (char *)(src +4)) + (dst->cli_cmprs_cb = load_be32 (src)) +4)
		return false;
	src += 4 + dst->cli_cmprs_cb;
	if (end < (dst->srv_cmprs_csv = (char *)(src +4)) + (dst->srv_cmprs_cb = load_be32 (src)) +4)
		return false;
	src += 4 + dst->srv_cmprs_cb;
	if (end < (dst->cli_lang_csv = (char *)(src +4)) + (dst->cli_lang_cb = load_be32 (src)) +4)
		return false;
	src += 4 + dst->cli_lang_cb;
	if (end < (dst->srv_lang_csv = (char *)(src +4)) + (dst->srv_lang_cb = load_be32 (src)) +5)
		return false;
	//dst-> = *src; // boolean first_kex_packet_follows
	//dst-> = load_be32 (src +1); // reserved for future extension
	return true;
}
typedef struct memblk_ {
	size_t cb;
	const char *data;
} memblk_s;
static size_t csv_split (const char *csv, size_t csv_cb, memblk_s *dst_)
{
size_t retval;
	retval = 0;
memblk_s *dst;
	dst = (memblk_s *)dst_;
const char *src, *end;
	src = csv;
	end = src + csv_cb;
	while (src < end) {
const char *tail;
		if (NULL == (tail = (const char *)memchr (src, ',', end - src)))
			tail = end;
		if (dst) {
			dst->cb = tail - src;
			dst->data = src;
			++dst;
		}
		++retval;
		src = tail +1;
	}
	return retval;
}
static char *choose_with_priority (const char *super_csv,
	size_t super_cb, const char *infer_csv, size_t infer_cb,
	const char *error_prefix,
	const char *super_name, const char *infer_name)
{
	do {
size_t super_count;
		if (! (0 < (super_count = csv_split (super_csv, super_cb, NULL))))
			break;
ALLOCA_ENV(sizeof(memblk_s) * super_count)
memblk_s *super_list;
		super_list = (memblk_s *)alloca (sizeof(memblk_s) * super_count);
ASSERTE(super_list)
		csv_split (super_csv, super_cb, super_list);
size_t infer_count;
		if (! (0 < (infer_count = csv_split (infer_csv, infer_cb, NULL))))
			break;
ALLOCA_ENV(sizeof(memblk_s) * infer_count)
memblk_s *infer_list;
		infer_list = (memblk_s *)alloca (sizeof(memblk_s) * infer_count);
ASSERTE(infer_list)
		csv_split (infer_csv, infer_cb, infer_list);
memblk_s *lhs, *rhs;
		for (lhs = super_list; lhs < super_list +super_count; ++lhs)
			for (rhs = infer_list; rhs < infer_list +infer_count; ++rhs)
				if (lhs->cb == rhs->cb && 0 < lhs->cb &&
				    0 == memcmp (lhs->data, rhs->data, lhs->cb))
					return (char *)lhs->data;
	} while (0);
ALLOCA_ENV(super_cb +1 + infer_cb +1)
char *work;
	work = (char *)alloca (super_cb +1 + infer_cb +1);
ASSERTE(work)
	memcpy (work +0, super_csv, super_cb);
	(work +0)[super_cb] = '\0';
	memcpy (work + super_cb +1, infer_csv, infer_cb);
	(work + super_cb +1)[infer_cb] = '\0';
ERR1("%s missmatch.", error_prefix)
fprintf (stderr_old, "%s: " VTGG "%s" VTO "\n", super_name, work +0);
fprintf (stderr_old, "%s: " VTYY "%s" VTO "\n", infer_name, work +super_cb +1);
	return NULL;
}
static bool choose_algorythm (ssh2_s *m_, const void *client_kexinit, size_t client_cb, const void *server_kexinit, size_t server_cb)
{
algorythms_s cli, srv;
	kexinit_parse (client_kexinit, client_cb, &cli);
	if (! (true == kexinit_parse (server_kexinit, server_cb, &srv))) {
WARN0("KEXINIT: server sent broken data (anywhere of many 32bits b.e. sizes).")
		return false;
	}
const char *token; // TODO apply choosed token
	if (NULL == (token = choose_with_priority (cli.kex_csv,
	    cli.kex_cb, srv.kex_csv, srv.kex_cb,
	    "KEXINIT 'key-exchange'",
	    "client", "server")))
		return false;
	if (NULL == (token = choose_with_priority (cli.auth_csv,
	    cli.auth_cb, srv.auth_csv, srv.auth_cb,
	    "KEXINIT 'authentication'",
	    "client", "server")))
		return false;
	if (NULL == (token = choose_with_priority (cli.cli_enc_csv,
	    cli.cli_enc_cb, srv.cli_enc_csv, srv.cli_enc_cb,
	    "KEXINIT 'client encrypt'",
	    "client", "server")))
		return false;
	if (NULL == (token = choose_with_priority (cli.srv_enc_csv,
	    cli.srv_enc_cb, srv.srv_enc_csv, srv.srv_enc_cb,
	    "KEXINIT 'server encrypt'",
	    "client", "server")))
		return false;
	if (NULL == (token = choose_with_priority (cli.cli_mac_csv,
	    cli.cli_mac_cb, srv.cli_mac_csv, srv.cli_mac_cb,
	    "KEXINIT 'client MAC'",
	    "client", "server")))
		return false;
	if (NULL == (token = choose_with_priority (cli.srv_mac_csv,
	    cli.srv_mac_cb, srv.srv_mac_csv, srv.srv_mac_cb,
	    "KEXINIT 'server MAC'",
	    "client", "server")))
		return false;
	if (NULL == (token = choose_with_priority (cli.cli_cmprs_csv,
	    cli.cli_cmprs_cb, srv.cli_cmprs_csv, srv.cli_cmprs_cb,
	    "KEXINIT 'client compress'",
	    "client", "server")))
		return false;
	if (NULL == (token = choose_with_priority (cli.srv_cmprs_csv,
	    cli.srv_cmprs_cb, srv.srv_cmprs_csv, srv.srv_cmprs_cb,
	    "KEXINIT 'server compress'",
	    "client", "server")))
		return false;
	return true;
}
static ssize_t pump_recv_queue0 (ssh2_s *m_, size_t body_size, const void *src_, size_t count, size_t done_)
{
const u8 *src;
	src = (const u8 *)src_;
	if (SSHVER_LIBID & ~m_->received) {
const u8 *tail;
		if (NULL == (tail = (const u8 *)memchr (src, '\n', count)))
			return 0;
size_t len;
		len = tail - src;
		++tail;
		while (0 < len && src[len -1] < 0x20)
			--len;
ASSERTE(src +len == tail -2 && '\r' == tail[-2])
		// TODO parse ssh version etc.
		kexhash_input_be32 (&m_->kexhash_work, len);
		kexhash_input_raw (&m_->kexhash_work, src, len);
		m_->received |= SSHVER_LIBID;
		return tail - src;
	}
size_t done;
	done = max(1, done_);
ssize_t parsed;
	switch (src[0]) {
	case SSH2_MSG_KEXINIT:
		if (KEXINIT & ~m_->sent)
			return 0;
		parsed = on_server_kexinit (body_size -1, src +done, min(body_size, count) - done, done -1);
		if (! (done + parsed < body_size)) {
			kexhash_input_be32 (&m_->kexhash_work, body_size);
			kexhash_input_raw (&m_->kexhash_work, src, body_size);
			// WARNING Must need two features.
			// 1. memory of ssh2::client_kexinit never crushed
			//    unless KEXINIT flag of ssh2::reserved is set.
			// 2. ssh2::client_kexinit is set before flow this
			//    code.
			// ref) pump_send_queue0() ssh2_thread()
			if (! (true == choose_algorythm (m_,
			       m_->client_kexinit +1, m_->cb_client_kexinit -1,
			       src +1, body_size -1)))
				exit (-1); // TODO suitable process shutdown.
			m_->cb_client_kexinit = 0;
			m_->client_kexinit = NULL;
			m_->received |= KEXINIT;
		}
		break;
	case SSH2_MSG_KEX_DH_GEX_GROUP:
		if (-1 == (parsed = on_kex_dh_gex_group (body_size -1, src +done, min(body_size, count) - done, done -1, &m_->p, &m_->g)))
			return -1;
		if (! (done + parsed < body_size))
			m_->received |= DH_GEX_GROUP;
		break;
	case SSH2_MSG_KEX_DH_GEX_REPLY:
		if (-1 == (parsed = on_kex_dh_gex_reply (m_, body_size -1, src +done, min(body_size, count) - done, done -1, m_->p, m_->a, &m_->f, &m_->k, &m_->kexhash_work, m_->kexhash_rsa2_low20_plus1)))
			return -1;
		if (! (done + parsed < body_size)) {
			intv_security_erase (m_->a);
			intv_dtor (m_->a);
			m_->a = NULL;
			m_->received |= DH_GEX_REPLY;
		}
		break;
	case SSH2_MSG_NEWKEYS:
		if (-1 == (parsed = on_newkeys (body_size -1, src +done, min(body_size, count) - done, done -1)))
			return -1;
		if (! (done + parsed < body_size))
			m_->received |= NEWKEYS;
		break;
	case SSH2_MSG_SERVICE_ACCEPT:
		if (count < body_size)
			return -1;
		parsed = body_size;
		m_->received |= SERVICE_ACCEPT;
		break;
	case SSH2_MSG_USERAUTH_FAILURE:
		if (count < body_size)
			return -1;
		// TODO recognize auth ways.
		parsed = body_size;
		if (0 == m_->user_authstate) {
			m_->user_authstate = 1;
			m_->sent &= ~USERAUTH_REQUEST;
		}
		break;
	case SSH2_MSG_USERAUTH_SUCCESS:
		if (count < body_size)
			return -1;
ASSERTE(1 == body_size)
		parsed = body_size;
		m_->received |= USERAUTH_SUCCESS;
		break;
	case SSH2_MSG_USERAUTH_PK_OK:
		if (count < body_size)
			return -1;
		// TODO recognize auth ways.
		parsed = body_size;
		if (1 == m_->user_authstate) {
			m_->user_authstate = 2;
			m_->sent &= ~USERAUTH_REQUEST;
		}
		break;
	case SSH2_MSG_CHANNEL_OPEN_CONFIRMATION:
		if (count < body_size)
			return -1;
		// TODO parse
		 // (4)local id (=256) (*)defined by CHANNEL_OPEN
		 // (4)remote id (= 0)
		 // (4)remote window size (= 0)
		 // (4)remote max pkt size (= 0x8000)
		parsed = body_size;
		m_->received |= CHANNEL_OPEN_CONFIRMATION;
		break;
	case SSH2_MSG_CHANNEL_CLOSE:
		if (count < body_size)
			return -1;
		// TODO parse
		 // (4)local id (=256) (*)defined by CHANNEL_OPEN
		parsed = body_size;
		m_->received |= CHANNEL_CLOSE;
		break;
	case SSH2_MSG_CHANNEL_REQUEST:
		if (count < body_size)
			return -1;
		// TODO parse
		 // (4)local id (=256) (*)defined by CHANNEL_OPEN
		 // (str, args)
		 // -> 'exit-status' (1)FALSE (4)exit_status
		parsed = body_size;
		break;
	case SSH2_MSG_CHANNEL_SUCCESS:
		if (count < body_size)
			return -1;
		// TODO parse
		parsed = body_size;
		if (++m_->chanreq_stage < 2) {
			m_->sent &= ~CHANNEL_REQUEST;
			break;
		}
		m_->received |= CHANNEL_SUCCESS;
		break;
	case SSH2_MSG_CHANNEL_WINDOW_ADJUST:
		if (count < body_size)
			return -1;
		// TODO parse
		parsed = body_size;
		break;
	case SSH2_MSG_CHANNEL_DATA:
		if (count < body_size)
			return -1;
		// (4)local id
		// (str)console out
		// TODO accept local id.
		if (body_size < 5 +4)
			return -1;
size_t data_len;
		if (0 == (data_len = load_be32 (src +5)))
			return 5 +4;
		if (body_size < 5 +4 + data_len)
			return -1;
		m_->server_window_size -= data_len;
		echo_back (m_, src +5 +4, data_len);
		parsed = body_size;
		break;
	default:
		parsed = min(body_size, count) - done;
		break;
	}
	return done + parsed - done_;
}
static ssize_t pump_recv_queue (ssh2_s *m_, const void *src_, size_t got, size_t done_, void *buf_, size_t cb)
{
const size_t bs = 16;
u8 *buf;
	buf = (u8 *)buf_;
	// raw block
	if (SSHVER_LIBID & ~m_->received)
		return pump_recv_queue0 (m_, 0, buf, got, 0);
const u8 *recv0;
	if (NEWKEYS & m_->received) {
		// SSH2 server decrypt (1st)
		if (got < bs)
			return 0;
		if (0 == load_be32 (m_->recv0))
			aes_ctr_decrypt (&m_->dec_ctx, src_, bs * 1, m_->recv0);
		recv0 = m_->recv0;
	}
	else if (3 < got && 0 == load_be32 (src_)) {
WARN0("[+0,4]=00000000 NULL packet received." HI0)
		return 4;
	}
	else
		recv0 = (const u8 *)src_;
ssize_t retval;
	retval = -1; // unrecoverable error
	do {
		// packet size check
		if (got < 4)
			return 0;
u32 pkt_size;
		if (cb -1 < (pkt_size = 4 + load_be32 (recv0))) {
ERR2("[+0,4]=%08x packet length too long. (MAX %d bytes)" HI0, pkt_size -4, cb -1)
			break;
		}
		if (NEWKEYS & m_->received && 0 < pkt_size % 16) {
ERR1("[+0,4]=%08x (encrypted) packet length not aligned." HI0, pkt_size -4)
			break;
		}
		// padding size check
		if (got < 5)
			return 0;
u8 padding;
		padding = recv0[4];
		if (pkt_size < 4 +1 +1 + padding) {
ERR2("[+0,4]=%08x packet length mismatch. (padding=%d)" HI0, pkt_size -4, padding)
			break;
		}
const u8 *src;
		src = (const u8 *)src_;
		if (NEWKEYS & m_->received) {
			// TODO streaming
			if (got < pkt_size +20)
				return 0;
			// SSH2 server decrypt (2nd later)
size_t cnt;
			if (0 < (cnt = min(got, pkt_size) / bs -1))
				aes_ctr_decrypt (&m_->dec_ctx, src +bs, bs * cnt, buf +bs);
			memcpy (buf, recv0, bs);
			// SSH2 server MAC validate
bool sha1_hmac_init_ok;
sha1_hmac_s dmac_ctx;
			sha1_hmac_init_ok = sha1_hmac_init (&dmac_ctx, sizeof(sha1_hmac_s), m_->dmac_key, 20);
ASSERTE(sha1_hmac_init_ok)
u8 tmp[4];
			store_be32 (tmp, m_->dmac_seq);
			sha1_hmac_update (&dmac_ctx, tmp, sizeof(tmp));
			sha1_hmac_update (&dmac_ctx, buf, pkt_size);
u8 computed[20];
			sha1_hmac_finish (&dmac_ctx, computed);
			if (! (0 == memcmp (src +pkt_size, computed, 20))) {
WARN0("SSH2 packet MAC error, packet ignored.")
				retval = pkt_size +20;
				break;
			}
			src = buf;
		}
		else if (got < pkt_size)
			return 0;
		retval = pkt_size + (NEWKEYS & m_->received ? 20 : 0);
		// parse message
ssize_t parsed;
		if ((parsed = pump_recv_queue0 (m_, pkt_size - padding -5, src +5, pkt_size -5, 0)) < 1)
			return parsed;
		parsed += 5;
		if (KEXINIT & m_->received)
			++m_->dmac_seq;
		if (0 == memcmp ("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", m_->kexhash, sizeof(m_->kexhash)) && DH_GEX_REPLY & m_->received) {
			kexhash_finish (&m_->kexhash_work, m_->pbits, m_->p, m_->g, m_->e, m_->f, m_->k, m_->kexhash);
u8 kexhash_sha1[20];
			sha1 (m_->kexhash, sizeof(m_->kexhash), kexhash_sha1);
			if (! (0xff == m_->kexhash_rsa2_low20_plus1[0] && 0 == memcmp (kexhash_sha1, m_->kexhash_rsa2_low20_plus1 +1, 20)))
				; // TODO error handling server auth
			if (0 == memcmp ("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", m_->kexhash0, sizeof(m_->kexhash0)))
				memcpy (m_->kexhash0, m_->kexhash, sizeof(m_->kexhash0));
			ssh2keys_generate (m_->k, m_->kexhash, sizeof(m_->kexhash), m_->kexhash0, sizeof(m_->kexhash0), &m_->enc_ctx, &m_->dec_ctx, m_->emac_key, m_->dmac_key);
		}
	} while (0);
	store_be32 (m_->recv0, 0);
	return retval;
}

static void *ssh2_thread (void *param)
{
ssh2_s *m_;
	m_ = (ssh2_s *)param;
int server;
	if (-1 == (server = sockopen (m_->hostname, m_->port)))
		return NULL;
u8 *dst, *buf;
	dst = buf = NULL;
size_t maxlen_send, maxlen_recv;
	maxlen_send = maxlen_recv = 0;
	do {
int fdflags;
		if ((fdflags = fcntl (server, F_GETFL)) < 0) {
ERR3("!fcntl(F_GETFL)=%d @err=%d'%s'", fdflags, errno, unixerror (errno))
			break;
		}
		if ((fdflags = fcntl (server, F_SETFL, O_NONBLOCK | fdflags)) < 0) {
ERR3("!fcntl(F_SETFL)=%d @err=%d'%s'", fdflags, errno, unixerror (errno))
			break;
		}
		m_->pbits = 4096;
		m_->pkt_align = 8;
		m_->emac_seq = m_->dmac_seq = 0;
		kexhash_input_start (&m_->kexhash_work, sizeof(m_->kexhash_work));
		dst = (u8 *)malloc (maxlen_send = SEND_MAXBUF);
size_t stored, sent;
		// SSH2 client message dispatch (for kex hash #1)
		stored = pump_send_queue (m_, dst, maxlen_send);
		sent = 0;
		buf = (u8 *)malloc (maxlen_recv = RECV_MAXBUF);
ASSERTE(buf)
size_t got;
		got = 0;
ssize_t parsed;
		parsed = 0;
size_t ofs_dummy = 0;
u8 work[16];
		while (1) {
			// user input
			pump_input_userauth (m_);
			// socket recv
			if (! (got < maxlen_recv -1)) {
ERR1("!receive overflow (MAX %d bytes)", maxlen_recv -1)
				break;
			}
ssize_t got_new;
			got_new = 0;
			errno = 0;
			if ((0 < parsed && 0 < got)
				|| is_enough_window_size (m_, WINDOW_MINLEN / 2)
				&& 0 < (got_new = read (server, buf +got, maxlen_recv -1 -got))) {
ASSERTE(is_enough_window_size (m_, got_new))
				// SSH2 server message dispatch
				got += got_new;
				if (0 < (parsed = pump_recv_queue (m_, buf, got, 0, buf, maxlen_recv))) {
					if (parsed < got)
						memmove (buf, buf + parsed, got - parsed);
					got -= parsed;
					// SSH2 client message dispatch (for kex hash #2 -> #3 -> #4)
					// (*)If server ID (kex hash #2) received,
					//    prepare client KEXINIT (kex hash #3) immediately.
					if (KEXINIT & ~m_->sent && SSHVER_LIBID & m_->received)
						stored += pump_send_queue (m_, dst +stored, maxlen_send -stored);
				}
			}
			else if (is_enough_window_size (m_, WINDOW_MINLEN / 2)
				&& 0 == got_new) {
ERR0("Disconnected.");
				break;
			}
			else if (is_enough_window_size (m_, WINDOW_MINLEN / 2)
				&& /*got_new < 0 &&*/ !(EWOULDBLOCK == errno)) {
ERR3("!read(server)=%d @err=%d'%s'", got_new, errno, unixerror (errno))
				break;
			}
			// SSH2 client message dispatch
			else if (0 < stored || 0 < (stored = pump_send_queue (m_, dst, maxlen_send))) {
int sent_new;
				if ((sent_new = write (server, dst + sent, stored - sent)) < 0 && !(EWOULDBLOCK == errno)) {
ERR3("!write(server)=%d @err=%d'%s'", sent_new, errno, unixerror (errno))
					break;
				}
				if (sent_new < 0 /*&& EWOULDBLOCK == errno*/ || 0 == sent_new)
					usleep (1000);
				else if (/* 0 < sent_new && */ stored <= (sent = sent + sent_new))
					sent = stored = 0;
			}
			else // cannot dispatch both send and recv.
				usleep (1000);
		}
	} while (0);
	if (dst)
		free (dst);
	if (buf)
		free (buf);
	return NULL;
}
bool ssh2_preconnect (struct ssh2 *this_, u16 port, const char *hostname, const char *ppk_path)
{
ssh2_s *m_;
	m_ = (ssh2_s *)this_;
	m_->port = port;
	if (sizeof(m_->hostname) < strlen (hostname) +1)
		return false;
	strcpy (m_->hostname, hostname);
	if (sizeof(m_->ppk_path) < strlen (ppk_path) +1)
		return false;
	strcpy (m_->ppk_path, ppk_path);
	return true;
}
static bool ssh2_connect (void *this_, u16 ws_col, u16 ws_row)
{
ssh2_s *m_;
	m_ = (ssh2_s *)this_;
	m_->ws_col = ws_col;
	m_->ws_row = ws_row;
pthread_t term_id;
	if (! (0 == pthread_create (&term_id, NULL, ssh2_thread, (void *)m_)))
		return false;
	return true;
}
static ssize_t ssh2_write (void *this_, const void *buf, size_t cb)
{
ssh2_s *m_;
	m_ = (ssh2_s *)this_;
int retval;
lock_lock (&m_->write_lock);
	retval = ringbuf_write (&m_->writebuf, buf, cb);
lock_unlock (&m_->write_lock);
ALLOCA_ENV(cb * 3)
u8 *log_;
	log_ = (u8 *)alloca (cb * 3);
ASSERTE(log_)
	return retval;
}
static ssize_t ssh2_read (void *this_, void *buf, size_t cb)
{
ssh2_s *m_;
	m_ = (ssh2_s *)this_;
int retval;
lock_lock (&m_->read_lock);
	retval = ringbuf_read (&m_->readbuf, buf, cb);
lock_unlock (&m_->read_lock);
	return retval;
}

static void ssh2_dtor (void *this_)
{
ssh2_s *m_;
	m_ = (ssh2_s *)this_;
lock_lock (&m_->write_lock);
	ringbuf_dtor (&m_->writebuf);
	free (m_->writebuf_);
lock_unlock (&m_->write_lock);
lock_dtor (&m_->write_lock);

lock_lock (&m_->read_lock);
	ringbuf_dtor (&m_->readbuf);
	free (m_->readbuf_);
lock_unlock (&m_->read_lock);
lock_dtor (&m_->read_lock);
	memset (m_, 0, sizeof(ssh2_s));
}

static struct conn_i ssh2;
struct conn_i *ssh2_ctor (struct ssh2 *this_, size_t cb)
{
ASSERTE(sizeof(ssh2_s) <= cb)
	if (cb < sizeof(ssh2_s))
		return NULL;
ssh2_s *m_;
	m_ = (ssh2_s *)this_;
	memset (m_, 0, sizeof(ssh2_s));
	// heap request
bool is_error;
	is_error = true;
	do {
		if (NULL == (m_->writebuf_ = malloc (STDIN_MAXBUF)))
			break;
		if (NULL == (m_->readbuf_ = malloc (STDOUT_MAXBUF)))
			break;
		is_error = false;
	} while (0);
	if (is_error) {
		ssh2_dtor (this_);
		return NULL;
	}
	// 
	random_init ();
	ringbuf_ctor (&m_->writebuf, m_->writebuf_, STDIN_MAXBUF, SECURITY_ERASE);
	ringbuf_ctor (&m_->readbuf, m_->readbuf_, STDOUT_MAXBUF, SECURITY_ERASE);
lock_ctor (&m_->write_lock);
lock_ctor (&m_->read_lock);
	return &ssh2;
}

static struct conn_i ssh2 = {
	ssh2_dtor,
	ssh2_connect,
	ssh2_write,
	ssh2_read,
};

