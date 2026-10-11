/* TODO: split 'ssh-rsa' logic from user authentication code,
         and add 'ssh-ed25519' logic.
 */
#define _POSIX_C_SOURCE 200809L /* POSIX.1-2008 strdup
#define _POSIX_C_SOURCE 199309L    POSIX.1-2001 nanosleep */
#include <features.h>

#include <stdint.h>
#include "ssh2.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h> // pthread_create
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h> // size_t
#define unixerror strerror
#include <alloca.h>
typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;

#include "queue.hpp"
typedef struct queue queue_s;
#include "ringbuf.hpp"
typedef struct ringbuf ringbuf_s;
#include "mpint.h"
#include "intv.hpp"
typedef struct intv intv_s;
#include "sha2.hpp"
typedef struct sha256 sha256_s;
#include "sha1.hpp"
typedef struct sha1 sha1_s;
#include "cipher_factory.hpp"
#include "mac_factory.hpp"

#include "rand.h"
#include "lock.hpp"
#include "portab.h"
#include "ssh2const.h"

#include "puttykey.h"
#include "cipher_helper.h"

#include "log.h"
#include "vt100.h" // TODO: removing
#include "assert.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.
#define ustrlen(x) (unsigned)strlen(x) // part of 64bit supports.
// CAUTION: for readablity, any of ALIGN must allow x to use assignment
#define PADDING(a,x) ((a) -1 - ((x) +(a) -1)%(a)) // = ALIGN(a,x) - (x)
#define ALIGN(a,x) (((x) +(a) -1) / (a) * (a))
#define ALIGNb(a,x) ((unsigned)-(a) & ((x) +(a) -1)) // CAUTION: useable under a = 2^n only
#define BITUSED8(c) \
	((128 & (c)) ? 8 : \
	 ( 64 & (c)) ? 7 : \
	 ( 32 & (c)) ? 6 : \
	 ( 16 & (c)) ? 5 : \
	 (  8 & (c)) ? 4 : \
	 (  4 & (c)) ? 3 : \
	 (  2 & (c)) ? 2 : \
	 (  1 & (c)) ? 1 : 0)
#define mpint_bitused(body_len, body) \
	(BITUSED8((body)[0]) + ((body_len) -1) * 8)

// conf.h
#define KEX_MODULUS_MAXBITS 4096

#define  INPUT_SPEED 38400 // [bit/s]
#define OUTPUT_SPEED 38400 // [bit/s]
#define      KBD_MAX 4096
#define     ECHO_MAX 0x100000
#define DEFAULT_WINDOW_SIZE 0xFFFFF // 1MB -1
#define          ID_STR_MAX DEFAULT_WINDOW_SIZE

#define MODPOW_PAUSE_SPAN_BITS 64

#define SESSION_ID_LEN 32
#define       SHA1_LEN 20
#define     MACKEY_MAX SHA1_LEN // 'hmac-sha1' PENDING: other support

// login authentication
#define           LOGINNAME_MAX 31
#define AUTH_ALGORYTHM_NAME_MAX 7   // 'ssh-rsa' PENDING: other support
#define   AUTH_RSA_SECRET_P_MAX 768 // prime number (p > q)
#define   AUTH_RSA_SECRET_Q_MAX 768 // prime number
#define   AUTH_RSA_PUBLIC_E_MAX 17  // exponent(public) (supposed 65537 = 17bits)
                                    // (on PUTTYgen, may change to 6 because 37 is choosed instead of 65537)
#define   AUTH_RSA_PUBLIC_N_MAX (AUTH_RSA_SECRET_P_MAX + AUTH_RSA_SECRET_Q_MAX)
                                    // modulus = p * q

#define DH_MIN_SIZE 1024 // [RFC4419]
#define DH_MAX_SIZE 8192 // [RFC4419]

#define SSH_CLIENT_ID "SSH-2.0-Cmdg_0.00" // TODO: -> 'appconst.h'

///////////////////////////////////////////////////////////////////////////////
// readability / portability

#define _ctor                     ssh2_ctor
#define _preconnect               ssh2_preconnect

#define DISCONNECT                SSH2_MSG_DISCONNECT
#define IGNORE                    SSH2_MSG_IGNORE
#define UNIMPLEMENTED             SSH2_MSG_UNIMPLEMENTED
#define DEBUG                     SSH2_MSG_DEBUG
#define SERVICE_REQUEST           SSH2_MSG_SERVICE_REQUEST
#define SERVICE_ACCEPT            SSH2_MSG_SERVICE_ACCEPT
#define KEXINIT                   SSH2_MSG_KEXINIT
#define NEWKEYS                   SSH2_MSG_NEWKEYS
#define KEX_DH_GEX_REQUEST_OLD    SSH2_MSG_KEX_DH_GEX_REQUEST_OLD
#define KEX_DH_GEX_GROUP          SSH2_MSG_KEX_DH_GEX_GROUP
#define KEX_DH_GEX_INIT           SSH2_MSG_KEX_DH_GEX_INIT
#define KEX_DH_GEX_REPLY          SSH2_MSG_KEX_DH_GEX_REPLY
#define KEX_DH_GEX_REQUEST        SSH2_MSG_KEX_DH_GEX_REQUEST
#define USERAUTH_REQUEST          SSH2_MSG_USERAUTH_REQUEST
#define USERAUTH_FAILURE          SSH2_MSG_USERAUTH_FAILURE
#define USERAUTH_SUCCESS          SSH2_MSG_USERAUTH_SUCCESS
#define USERAUTH_PK_OK            SSH2_MSG_USERAUTH_PK_OK
#define GLOBAL_REQUEST            SSH2_MSG_GLOBAL_REQUEST
#define CHANNEL_OPEN              SSH2_MSG_CHANNEL_OPEN
#define CHANNEL_OPEN_CONFIRMATION SSH2_MSG_CHANNEL_OPEN_CONFIRMATION
#define CHANNEL_WINDOW_ADJUST     SSH2_MSG_CHANNEL_WINDOW_ADJUST
#define CHANNEL_DATA              SSH2_MSG_CHANNEL_DATA
#define CHANNEL_EOF               SSH2_MSG_CHANNEL_EOF
#define CHANNEL_CLOSE             SSH2_MSG_CHANNEL_CLOSE
#define CHANNEL_REQUEST           SSH2_MSG_CHANNEL_REQUEST
#define CHANNEL_SUCCESS           SSH2_MSG_CHANNEL_SUCCESS
#define CHANNEL_FAILURE           SSH2_MSG_CHANNEL_FAILURE

#define SECRET_P_MAX              AUTH_RSA_SECRET_P_MAX
#define SECRET_Q_MAX              AUTH_RSA_SECRET_Q_MAX
#define PUBLIC_E_MAX              AUTH_RSA_PUBLIC_E_MAX
#define PUBLIC_N_MAX              AUTH_RSA_PUBLIC_N_MAX

#define SECURITY_ERASE            RINGBUF_SECURITY_ERASE

#define CtoS                      KEY_DIRECTION_CtoS
#define StoC                      KEY_DIRECTION_StoC

///////////////////////////////////////////////////////////////////////////////
// import extend

static int usleep (unsigned usec)
{
struct timespec t = {
	.tv_sec = usec / 1000000,
	.tv_nsec = (usec % 1000000) * 1000
};
int retval
	= nanosleep (&t, NULL);
	return retval;
}

///////////////////////////////////////////////////////////////////////////////
// utility

static void *memcchr (const void *s, int reject, size_t n)
{
const u8 *p, *end;
	p = (const u8 *)s, end = p +n;

	while (p < end && reject == *p)
		++p;
	if (! (p < end))
		return NULL;
#ifdef __cplusplus
	return const_cast<void *>(p);
#else //ndef __cplusplus
	return (void *)p;
#endif
}

static int sockopen (const char *hostname, u16 port)
{
int server;
bool is_err;
	is_err = true;
	do {
		// new connection
		if (-1 == (server = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
// [reason] missing internet, driver problem [ignore] impossible [recovery] user side
log_ (VTRR "LAN/WAN error" VTO ": !socket() @err=%d'%s'" "\n", errno, unixerror (errno));
			break;
		}
		// active open (CLOSED -> SYN_SENT -> ESTABLISHED)
struct in_addr in_addr;
		memset (&in_addr, 0, sizeof(in_addr));
		if (INADDR_NONE == (in_addr.s_addr = inet_addr (hostname))) {
// [reason] invalid hostname, DNS problem [ignore] impossible [recovery] user side
log_ (VTRR "DNS error" VTO ": !inet_addr('%s') @err=%d'%s'" "\n", hostname, errno, unixerror (errno));
			break;
		}
struct sockaddr_in server_addr;
		memset (&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family      = AF_INET;
		server_addr.sin_port        = htons (port);
		server_addr.sin_addr.s_addr = in_addr.s_addr;
		if (-1 == connect (server, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
// [reason] TCP/IP problem, server problem [ignore] impossible [recovery] user side
log_ (VTRR "TCP/IP error" VTO ": %d.%d.%d.%d:%d !connect() @err=%d'%s'" "\n", ((u8 *)&in_addr.s_addr)[0], ((u8 *)&in_addr.s_addr)[1], ((u8 *)&in_addr.s_addr)[2], ((u8 *)&in_addr.s_addr)[3], port, errno, unixerror (errno));
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
// console subsystem

// @param cb  0: = strlen(src)
static unsigned echo_back (ringbuf_s *dst, Lock *lock, const char *src, unsigned cb)
{
BUG(lock && dst && src)
	if (! (0 < cb))
		cb = ustrlen (src);

unsigned written;
lock_lock (lock);
	written = ringbuf_write (dst, src, cb);
lock_unlock (lock);
	if (written < cb)
// [reason] sweep not enough (receiver) [ignore] impossible [recovery] wait for sweeping
log_ (VTRR "warning" VTO ": echo chunk overflow. (disposed %d bytes)" "\n", cb - written);

	return written;
}

static unsigned ringbuf_gets_streaming (
	  char *s, unsigned size
	, const char *kbd_data, unsigned kbd_len
	, ringbuf_s *echo_post, Lock *echo_lock
)
{
BUG(s && 0 < size && kbd_data && 0 < kbd_len)
BUG(memchr (s, '\0', size))
char *p;
	if (! (p = (char *)memchr (s, '\0', size)))
		p = s +size; // treat as buffer full

char chunk[128]; unsigned echo_len;
	echo_len = 0;
unsigned parsed;
	for (parsed = 0; parsed < kbd_len;) {
char c;
		c = kbd_data[parsed++];
		if ('\r' == c) {
			--parsed;
			chunk[echo_len++] = '\r';
			chunk[echo_len++] = '\n';
		}
		else if (0x08 == c) {
			if (! (s < p))
				continue;
			*--p = '\0';
			if (echo_post) {
				chunk[echo_len++] = 0x08;
				chunk[echo_len++] = ' ';
				chunk[echo_len++] = 0x08;
			}
		}
		else if (c < 0x20 || 0x7e < c)
			continue;
		else if (! (p +1 < s +size))
			continue;
		else {
			*p++ = (char)c;
			*p = '\0';
			if (echo_post)
				chunk[echo_len++] = c;
		}
		if (echo_len +3 <= sizeof(chunk) && parsed < kbd_len && !('\r' == kbd_data[parsed]))
			continue;
		if (echo_post && 0 < echo_len)
			echo_back (echo_post, echo_lock, chunk, echo_len);
		echo_len = 0;
		if (parsed < kbd_len && '\r' == kbd_data[parsed])
			break;
	}
BUG(0 == echo_len)
	memset (chunk, 0, sizeof(chunk)); // security erase
	return parsed;
}

static void ringbuf_gets (
	  char *s, unsigned size
	, ringbuf_s *kbd_recv, Lock *kbd_lock
	, ringbuf_s *echo_post, Lock *echo_lock
)
{
BUG(s && 0 < size && kbd_recv && kbd_lock /*&& echo_post && echo_lock*/)
	*s = '\0';

bool is_eol;
	is_eol = false;
	do {
const u8 *buf; unsigned len1, pos1, len2;
lock_lock (kbd_lock);
		if (! (buf = ringbuf_peek (kbd_recv, KBD_MAX, &len1, &pos1, &len2))) {
lock_unlock (kbd_lock);
			usleep (1000000 / 60);
			continue;
		}
unsigned kbd_len; const char *kbd_data;
		if (0 < len1)
			kbd_len = len1, kbd_data = (const char *)(buf +pos1);
		else
			kbd_len = len2, kbd_data = (const char *)(buf +0);
unsigned kbd_parsed
		= ringbuf_gets_streaming (
			  s, size
			, kbd_data, kbd_len
			, echo_post, echo_lock
			);
		if (kbd_parsed < kbd_len && '\r' == kbd_data[kbd_parsed])
			is_eol = true, ++kbd_parsed;
		if (0 < kbd_parsed)
			ringbuf_remove (kbd_recv, kbd_parsed);
lock_unlock (kbd_lock);
	} while (!is_eol);
}

#define INPUT_CLOSE   1
#define INPUT_REQUEST 2
#define INPUT_ACCEPT  3

#define INPUT_ECHO_OFF 0x01

typedef struct {
	queue_s *msg_post; Lock *msg_lock; unsigned input_max;
	bool boot_complete;
	ringbuf_s *kbd_recv; Lock *kbd_lock;
	ringbuf_s *echo_post; Lock *echo_lock;
} interrupt_input_s;

// PENDING: fix blocking to non-blocking, and integrate to _mainloop().
static void interrupt_input (interrupt_input_s *m_)
{
	// copy parameter (for multi-thread)
interrupt_input_s copied;
	memcpy (&copied, m_, sizeof(copied));
	m_->boot_complete = true, m_ = &copied;
char *input;
	alloca_chk(input, = (char *), m_->input_max, );

BUG(m_->msg_post && m_->msg_lock && 0 < m_->input_max
 && m_->kbd_recv && m_->kbd_lock && m_->echo_post && m_->echo_lock)
	// internal warmup
char *msg_copy; unsigned msg_max;
bool end_of_interrupt;
	msg_copy = (char *)malloc (msg_max = 128);
	end_of_interrupt = false;

	// message pump
	do {
		// wait message
lock_lock (m_->msg_lock);
const u8 *msg_orig; unsigned msg_len;
		if (! (msg_orig = queue_peek (m_->msg_post, 0, &msg_len))) {
lock_unlock (m_->msg_lock);
			usleep (1000000 / 60);
			continue;
		}

		// copy message (for multi-thread)
unsigned need = msg_max;
		while (need < msg_len)
			need *= 2;
		if (msg_max < need)
			msg_copy = (u8 *)realloc (msg_copy, msg_max = need);
		memcpy (msg_copy, msg_orig, msg_len);
lock_unlock (m_->msg_lock);

		// dispatch message
		switch (msg_copy[0]) {
		case INPUT_CLOSE:
			end_of_interrupt = true;
			break;
		case INPUT_REQUEST:
BUG(1 < msg_len)
			if (! (2 < msg_len))
				break;
u8 accept_id; bool echo_off;
			accept_id = msg_copy[1], echo_off = (INPUT_ECHO_OFF & msg_copy[2]) ? true : false;
			if (2 < msg_len)
				echo_back (m_->echo_post, m_->echo_lock, msg_copy +3, msg_len -3);
			ringbuf_gets (
				  input, m_->input_max
				, m_->kbd_recv, m_->kbd_lock
				, (echo_off) ? NULL : m_->echo_post, m_->echo_lock
			);
			if (echo_off)	
				echo_back (m_->echo_post, m_->echo_lock, "\r\n", 2);
lock_lock (m_->msg_lock);
			queue_write_u8 (m_->msg_post, INPUT_ACCEPT);
			queue_write_u8 (m_->msg_post, accept_id);
			if (! ('\0' == input[0])) {
BUG(memchr (input, '\0', m_->input_max))
				queue_write (m_->msg_post, input, ustrlen (input));
			}
			queue_set_boundary (m_->msg_post);
lock_unlock (m_->msg_lock);
			break;
		default:
			continue;
		}

		// sweep message
lock_lock (m_->msg_lock);
		queue_remove (m_->msg_post, msg_len);
lock_unlock (m_->msg_lock);
	} while (!end_of_interrupt);
	free (msg_copy);
}
static void *interrupt_input_thunk (void *priv)
{
	interrupt_input ((interrupt_input_s *)priv);
	return NULL;
}

static void input_request_post (queue_s *input_subsystem, u8 accept_id, u8 flags, const char *cstr)
{
	queue_write_u8 (input_subsystem, INPUT_REQUEST);
	queue_write_u8 (input_subsystem, accept_id);
	queue_write_u8 (input_subsystem, flags);
	queue_write (input_subsystem, cstr, ustrlen (cstr));
	queue_set_boundary (input_subsystem);
}

static u8 input_accept_wait (
  queue_s *m_
, unsigned span_usec
, Lock *lock
, unsigned *pkt_len
)
{
BUG(m_ && pkt_len)
const u8 *pkt; bool not_found;
	not_found = true;
	while (1) {
if (lock) lock_lock (lock);
		pkt = queue_peek (m_, 0, pkt_len);
if (lock) lock_unlock (lock);
		if (pkt && INPUT_ACCEPT == pkt[0]) {
			not_found = false;
			break;
		}
		if (! (0 < span_usec))
			break;
		usleep (span_usec);
	}
	if (not_found)
		return 0;
	return pkt[1]; // accept_id
}

static unsigned input_accept_get (
  queue_s *m_
, Lock *lock
, void *dst
, unsigned dst_max
)
{
unsigned dst_len; const u8 *pkt; unsigned pkt_len;
	dst_len = 0;

lock_lock (lock);
	do {
		if (! (pkt = queue_peek (m_, 0, &pkt_len)))
			break;
BUG(pkt && 2 < pkt_len && INPUT_ACCEPT == pkt[0])
		if (! (pkt && 2 < pkt_len && INPUT_ACCEPT == pkt[0]))
			break;
		memcpy (dst, pkt +2, dst_len = min(pkt_len -2, dst_max));
		queue_remove (m_, pkt_len);
	} while (0);
lock_unlock (lock);

	return dst_len;
}

///////////////////////////////////////////////////////////////////////////////
// module extend

static void queue_write_ssh_string (queue_s *msg_out, const char *cstr)
{
unsigned len;
	queue_write_be32 (msg_out, len = ustrlen (cstr));
	queue_write (msg_out, cstr, len);
}

///////////////////////////////////////////////////////////////////////////////
/* EMSA-PKCS1-v1_5-ENCODE  [RFC3447] [RFC4252]

   mpint(EM) = 0x01 || PS(min 8 octets) || 0x00 || T
 */

#define EMSA_TYPE_SHA1 1

// RSASSA-PKCS1-v1_5 [RFC3447]
static const u8 SHA1_DER_HEADER[] = {
	  0x30, 0x21 // (SEQUENCE) len=33
	, 0x30, 0x09 // (SEQUENCE) len=9
	, 0x06, 0x05 // (OID) len=5
	, 0x2B, 0x0E, 0x03, 0x02, 0x1A // 1.3.14.3.2.26 (SHA-1)
	, 0x05, 0x00 // (NULL) len=0
	, 0x04, 0x14 // (OCTET STRING) len=20
};

static unsigned emsa_decode (
  const u8 *emsa, unsigned emsa_len
, unsigned *hash
)
{
BUG(emsa)

// 0x01 || PS(min 8 octets) || 0x00
const u8 *ps_end, *emsa_end;
	emsa_end = emsa +emsa_len;
	if (emsa_end < emsa +1 +8 +1
	 || !(0x01 == emsa[0])
	 || NULL == (ps_end = memcchr (emsa +1, 0xff, emsa_end -(emsa +1)))
	 || ps_end < emsa +1 +8
	 || !(0x00 == *ps_end)
	)
		return 0; // format error

const u8 *der = ps_end +1;
	if (der +sizeof(SHA1_DER_HEADER) +SHA1_LEN == emsa_end
	 && 0 == memcmp (SHA1_DER_HEADER, der, sizeof(SHA1_DER_HEADER))) {
// T = DER(SHA1)
		if (hash)
			*hash = (unsigned)(der +sizeof(SHA1_DER_HEADER) -emsa);
		return EMSA_TYPE_SHA1;
	}
	// PENDING: other type support

	return 0; // unexpected type
}

static bool emsa_encode (
  unsigned hash_type
, const void *hash_target, unsigned hash_target_len
, u8 *emsa, unsigned emsa_len
)
{
BUG(EMSA_TYPE_SHA1 == hash_type) // PENDING: other type support
	if (! (EMSA_TYPE_SHA1 == hash_type))
		return 0;

BUG(hash_target && 0 < hash_target_len && emsa && 0 < emsa_len)

ASSERTE(1 +8 +1 +sizeof(SHA1_DER_HEADER) +SHA1_LEN <= emsa_len)
	if (! (1 +8 +1 +sizeof(SHA1_DER_HEADER) +SHA1_LEN <= emsa_len))
		return false;

// 0x01 || PS(min 8 octets) || 0x00
u8 *der
	= emsa +emsa_len -(sizeof(SHA1_DER_HEADER) +SHA1_LEN);
	emsa[0] = 0x01;
	memset (emsa +1, 0xff, (der -1) - (emsa +1));
	der[-1] = 0x00;

// T = DER(SHA1)
	memcpy (der, SHA1_DER_HEADER, sizeof(SHA1_DER_HEADER));
sha1_s sha1;
	sha1_init (&sha1, usizeof(sha1));
	sha1_update (&sha1, hash_target, hash_target_len);
	sha1_finish (&sha1, der +sizeof(SHA1_DER_HEADER));

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// generate random

// x = rand(p/2)
static unsigned mpint_random_half (
	  const u8 *p_, unsigned p_len // kex_modulus
	,       u8 *x_, unsigned x_max
)
{
BUG(p_ && 0 < p_len && x_ && 0 < x_max)
	while (1 < p_len && 0 == *p_)
		++p_, --p_len; // safety
unsigned p_bitused, x_len;
	p_bitused = mpint_bitused(p_len, p_);
	x_len = ALIGNb(8, 1 +p_bitused -1) / 8;
BUG(x_len <= x_max)

// p/2
u8 *pd2; unsigned pd2_len;
	alloca_chk(pd2, = (u8 *), pd2_len, = p_len);
ASSERTE(pd2)
	memcpy (pd2, p_, p_len);
u16 shift; unsigned i;
	for (shift = 0, i = 0; i < pd2_len; ++i) {
		shift |= (u16)pd2[i] << 7;
		pd2[i] = (u8)(shift >> 8);
		shift <<= 8;
	}
	if (0 == *pd2)
		++pd2, --pd2_len;
BUG(0 < pd2_len && 0 < *pd2)

// 1 < a < p/2
u8 mask0 // bitused(p/2) mod 8 = 0:ff 1:01 2:03 .. 6:3f 7:7f
	= 0xff >> (7 - (p_bitused +6)) % 8;
	do {
		store_random (x_, x_len);
		x_[0] &= mask0;
	} while (! (u8be_cmp (x_, x_len, pd2, pd2_len) < 0));

// with any 00 prefix -> mpint
u8 *nz = x_;
	while (nz < x_ +x_len -1 && 0 == *nz)
		++nz; // non-zero seek
	if (127 < *nz)
		--nz; // top as mpint
BUG(nz +1 == x_ || x_ <= nz && nz < x_ +x_len)

	if (nz +1 == x_) // x_[0]=80..ff
		memmove (x_ +1, nz +1, x_len), *x_ = 0x00, ++x_len;
	else if (x_ < nz) { // x_[1 < N]=01..7f or x_[2 < N]=80..ff
		memmove (x_, nz, x_ +x_len -nz);
		memset (x_ +x_len, 0, nz -x_); // security erase
		x_len -= nz -x_;
	}

BUG(0 < x_len && 0 < *x_ && *x_ < 128 || 1 < x_len && 0 == *x_ && 127 < *(x_ +1))
	return x_len;
}

///////////////////////////////////////////////////////////////////////////////
// key exchange hash (H)

static void kexhash_input_finish (sha256_s *ctx, void *dst)
{
BUG(ctx && dst)
	sha256_finish (ctx, dst);
}
static void kexhash_input_start (sha256_s *ctx, unsigned len)
{
BUG(ctx && 0 < len)
	sha256_init (ctx, len);
}
static void kexhash_input_raw (sha256_s *ctx, const void *raw, unsigned len)
{
BUG(ctx && raw && 0 < len)
	sha256_update (ctx, raw, len);
}
static void kexhash_input_be32 (sha256_s *ctx, u32 val)
{
u8 be32[4];
	store_be32 (be32, val);
	kexhash_input_raw (ctx, be32, usizeof(be32));
}
static void kexhash_finish (
	  sha256_s *ctx
	, bool is_need_pbits_minmax
	, unsigned pbits
	, const u8 *p_, unsigned p_len
	, const u8 *g_, unsigned g_len
	, const u8 *e_, unsigned e_len
	, const u8 *f_, unsigned f_len
	, const u8 *k_, unsigned k_len
	, u8 kexhash[32]
)
{
	// kex hash #6 uint32  min, minimal size in bits of an acceptable group
	// kex hash #6 uint32  n, preferred size in bits of the group the server will send
	// kex hash #6 uint32  max, maximal size in bits of an acceptable group
	if (!is_need_pbits_minmax)
		kexhash_input_be32 (ctx, pbits);
	else /*if (is_need_pbits_minmax)*/ {
		kexhash_input_be32 (ctx, DH_MIN_SIZE);
		kexhash_input_be32 (ctx, pbits);
		kexhash_input_be32 (ctx, DH_MAX_SIZE);
	}
	// kex hash #7 p = safe prime
	kexhash_input_be32 (ctx, p_len);
	kexhash_input_raw (ctx, p_, p_len);
	// kex hash #8 g = generator for subgroup
	kexhash_input_be32 (ctx, g_len);
	kexhash_input_raw (ctx, g_, g_len);
	// kex hash #9 e = g ^ secret(client) mod p
	kexhash_input_be32 (ctx, e_len);
	kexhash_input_raw (ctx, e_, e_len);
	// kex hash #10 f = g ^ secret(server) mod p
	kexhash_input_be32 (ctx, f_len);
	kexhash_input_raw (ctx, f_, f_len);
	// kex hash #11 K = g ^ (secret(client) * secret(server)) mod p
	kexhash_input_be32 (ctx, k_len);
	kexhash_input_raw (ctx, k_, k_len);
	// 
	kexhash_input_finish (ctx, kexhash);
}

///////////////////////////////////////////////////////////////////////////////
// crypt key / MAC key [RFC4253] 7.2

#define SHA256_LEN (256 / 8)

struct ssh2keygen {
	u8 *seed; unsigned seed_len;
	u8 *sid; unsigned sid_len;
};

static void ssh2key_dtor (void *this_)
{
BUG(this_)
struct ssh2keygen *m_;
	m_ = (struct ssh2keygen *)this_;

	if (m_->seed) {
		memset (m_->seed, 0, m_->seed_len); // security erase
		free (m_->seed), m_->seed = NULL;
	}
	if (m_->sid) {
		memset (m_->sid, 0, m_->sid_len); // security erase
		free (m_->sid), m_->sid = NULL;
	}
}

static bool ssh2key_ctor (
	  void *this_
	, const u8 *k_, unsigned k_len
	, const u8 *h, unsigned h_len
	, const u8 *sid, unsigned sid_len
)
{
BUG(this_ && k_ && 0 < k_len && h && 0 < h_len && sid && 0 < sid_len)
struct ssh2keygen *m_;
	m_ = (struct ssh2keygen *)this_;
	if (NULL == (m_->seed = (u8 *)malloc (m_->seed_len = 4 +k_len +h_len)))
		return false;
	if (NULL == (m_->sid = (u8 *)malloc (m_->sid_len = sid_len))) {
		memset (m_->seed, 0, m_->seed_len); // security erase
		free (m_->seed); m_->seed = NULL;
		return false;
	}
u8 *dst;
	dst = m_->seed;
// K (mpint) || H
	while (1 < k_len && 0 == k_[0] && k_[1] < 128) ++k_, --k_len; // safety
	store_be32 (dst, k_len);
	memcpy (dst +4, k_, k_len);
	memcpy (dst +4 +k_len, h, h_len);
// session_id
	memcpy (m_->sid, sid, sid_len);
	return true;
}

static void ssh2key_generate (void *this_, int x, void *dst_, unsigned dst_len)
{
BUG(this_ && dst_)
struct ssh2keygen *m_;
	m_ = (struct ssh2keygen *)this_;
u8 *dst;
	dst = (u8 *)dst_;
unsigned total; sha256_s state;

// K1 = HASH(K || H || X || session_id)  (X is e.g., "A") [RFC4253] 7.2
	sha256_init (&state, usizeof(state));
	sha256_update (&state, m_->seed, m_->seed_len);
	sha256_update (&state, &x, 1);
	sha256_update (&state, m_->sid, m_->sid_len);
// key = K1
	if (dst_len < SHA256_LEN) {
u8 tmp[SHA256_LEN];
		sha256_finish (&state, tmp);
		memcpy (dst +0, tmp, dst_len), total = dst_len;
	}
	else {
		sha256_finish (&state, dst +0), total = SHA256_LEN;
	}

	while (total < dst_len) {
// K2 = HASH(K || H || K1)
// K3 = HASH(K || H || K1 || K2)
// ...
		sha256_init (&state, usizeof(state));
		sha256_update (&state, m_->seed, m_->seed_len);
		sha256_update (&state, dst +0, total);
// key = K1 || K2 || K3 || ...
		if (dst_len < total +SHA256_LEN) {
u8 tmp[SHA256_LEN];
			sha256_finish (&state, tmp);
			memcpy (dst +0, tmp, dst_len - total), total = dst_len;
		}
		else {
			sha256_finish (&state, dst +total), total += SHA256_LEN;
		}
	}

BUG(total == dst_len)
}

///////////////////////////////////////////////////////////////////////////////
// SSH-2 negotiation control

typedef struct tcp_phase_ {
	u8 is_need_decrypt      :1;
	u8 is_need_encrypt      :1;
	u8 is_idstr_received    :1;
	u8 reserved             :5;
} tcp_phase;

typedef struct ssh2_ ssh2_s;
struct ssh2_ {
// thread parameter
	u16 ws_col; u16 ws_row;
	u16 port;
	char *hostname;
	char *ppk_path;

// authentication
	unsigned pbits;
	u8 *p_; unsigned p_len;
	u8 *g_; unsigned g_len;
	u8 *x_; unsigned x_len;
	u8 *e_; unsigned e_len;
	u8 session_id[SESSION_ID_LEN];
	char login_name[LOGINNAME_MAX +1];
	sha256_s kexhash_ctx;
	pthread_t auth_subsystem_id;
	Lock auth_subsystem_lock; queue_s auth_subsystem;

// encrypt / decrypt
	struct cipher_i *enc; struct cipher enc_this_;
	struct cipher_i *dec; struct cipher dec_this_;

// checksum
	struct mac_i *emac; struct mac emac_this_; u32 epkt_num;
	struct mac_i *dmac; struct mac dmac_this_; u32 dpkt_num;

// communication
	u32 recv_window_max;
	u32 recv_window_used;
	tcp_phase tcp;
	u8 is_need_pbits_minmax :1;
	u8 is_kbd_to_channel    :1;
	u8 reserved             :6;
	ringbuf_s tcp_in; void *tcp_in_;
	queue_s pkt_in;
	queue_s msg_out;
	queue_s tcp_out;

// console input / output
	Lock  kbd_lock; ringbuf_s  kbd_buf; void * kbd_buf_;
	Lock echo_lock; ringbuf_s echo_buf; void *echo_buf_;
};

///////////////////////////////////////////////////////////////////////////////
// post SSH client message

// PENDING: UI or .conf setting

static const char kex_algs_cstr[] =
 "diffie-hellman-group-exchange-sha256"
//",diffie-hellman-group-exchange-sha1"
//",diffie-hellman-group14-sha1"
//",diffie-hellman-group1-sha1"
;
static const char servauth_algs_cstr[] =
 "ssh-rsa"
//",ssh-dss"
;

// 'aes256-ctr' PENDING: other crypt support
static const char crypt_algs_cstr[] =
 "aes256-ctr"
//",aes256-cbc"
//",rijndael-cbc@lysator.liu.se"
//",aes192-ctr"
//",aes192-cbc"
//",aes128-ctr"
//",aes128-cbc"
//",blowfish-ctr"
//",blowfish-cbc"
//",3des-ctr"
//",3des-cbc"
//",arcfour256"
//",arcfour128"
;

// 'hmac-sha1' PENDING: other MAC support
static const char mac_algs_cstr[] =
 "hmac-sha1"
//",hmac-sha1-96"
//",hmac-md5"
;
static const char pack_algs_cstr[] =
 "none"
//",zlib"
;

// #20 KEXINIT
static void kexinit_post (queue_s *msg_out)
{
	queue_write_u8 (msg_out, KEXINIT);
u8 cookie[16];
	store_random (cookie, sizeof(cookie));
	queue_write (msg_out, cookie, sizeof(cookie));
	queue_write_ssh_string (msg_out, kex_algs_cstr);
	queue_write_ssh_string (msg_out, servauth_algs_cstr);
	queue_write_ssh_string (msg_out, crypt_algs_cstr);
	queue_write_ssh_string (msg_out, crypt_algs_cstr);
	queue_write_ssh_string (msg_out, mac_algs_cstr);
	queue_write_ssh_string (msg_out, mac_algs_cstr);
	queue_write_ssh_string (msg_out, pack_algs_cstr);
	queue_write_ssh_string (msg_out, pack_algs_cstr);
	queue_write_be32 (msg_out, 0);
	queue_write_be32 (msg_out, 0);
	queue_write (msg_out, "\0\0\0\0\0", 5);
	queue_set_boundary (msg_out);
}

// #30 KEX_DH_GEX_REQUEST_OLD
static void kex_dh_gex_request_old_post (queue_s *msg_out, unsigned pbits)
{
	queue_write_u8 (msg_out, KEX_DH_GEX_REQUEST_OLD);
	queue_write_be32 (msg_out, pbits);
	queue_set_boundary (msg_out);
}

// #34 KEX_DH_GEX_REQUEST
static void kex_dh_gex_request_post (queue_s *msg_out, unsigned pbits)
{
	queue_write_u8 (msg_out, KEX_DH_GEX_REQUEST);
	queue_write_be32 (msg_out, DH_MIN_SIZE);
	queue_write_be32 (msg_out, pbits);
	queue_write_be32 (msg_out, DH_MAX_SIZE);
	queue_set_boundary (msg_out);
}

// #5 SERVICE_REQUEST
static void service_request_post (queue_s *msg_out)
{
	queue_write_u8 (msg_out, SERVICE_REQUEST);
	queue_write_ssh_string (msg_out, "ssh-userauth");
	queue_set_boundary (msg_out);
}

// #21 NEWKEYS
static void newkeys_post (queue_s *msg_out)
{
	queue_write_u8 (msg_out, NEWKEYS);
	queue_set_boundary (msg_out);
}

// #32 KEX_DH_GEX_INIT
static unsigned kex_dh_gex_init_post (
	queue_s *msg_out
	, const u8 *g_, unsigned g_len // generator for subgroup
	, const u8 *x_, unsigned x_len // secret(client) (= random value of 1 < x < p/2)
	, const u8 *p_, unsigned p_len // safe prime
	,       u8 *e_, unsigned e_max // exchange value sent by client (= g ^ secret(client) mod p)
)
{
BUG(g_ && 0 < g_len && x_ && 0 < x_len && p_ && 0 < p_len && e_ && 0 < e_max)
	// e = g^x mod p [RFC4419]
unsigned e_len =
	mpint_modpow (
		  g_, g_len
		, x_, x_len
		, p_, p_len
		, e_, e_max
		, NULL, NULL, 0
		); // Heavy

	queue_write_u8 (msg_out, KEX_DH_GEX_INIT);
	queue_write_be32 (msg_out, e_len);
	queue_write (msg_out, e_, e_len);
	queue_set_boundary (msg_out);
	return e_len;
}

// #50 USERAUTH_REQUEST
/* (*1)
   signature is not 'mpint' but 'string' of unsigned integer  without
  '00' padding. [RFC4253]

 >  6.6.  Public Key Algorithms
 ...
 >    string    "ssh-rsa"
 >    string    rsa_signature_blob
 >
 > The value for 'rsa_signature_blob' is encoded as a string containing
 > s (which is an integer, without lengths or padding, unsigned, and in
                           ~~~~~~~~~~~~~~~~~~~~~~~~~~  ~~~~~~~~
 > network byte order).
 */
static void userauth_request_post (
	  queue_s *msg_out
	, const char *login_name
	, const u8 *userkey, unsigned userkey_len
	, const u8 *session_id
	, const u8 *priv_userexp_, unsigned priv_userexp_len
)
{
	queue_write_u8 (msg_out, USERAUTH_REQUEST);
	queue_write_ssh_string (msg_out, login_name);
	queue_write_ssh_string (msg_out, "ssh-connection");

	if (! (userkey && 0 < userkey_len)) {
		queue_write_ssh_string (msg_out, "none");
		queue_set_boundary (msg_out);
		return;
	}

bool post_with_sign
	= (session_id && priv_userexp_ && 0 < priv_userexp_len) ? true : false;
	queue_write_ssh_string (msg_out, "publickey");
	queue_write_u8 (msg_out, (post_with_sign) ? 0x01 : 0x00); // TRUE : FALSE
	queue_write_ssh_string (msg_out, "ssh-rsa"); // PENDING: other type support
	queue_write_be32 (msg_out, userkey_len);
	queue_write (msg_out, userkey, userkey_len);
	if (!post_with_sign) {
		queue_set_boundary (msg_out);
		return;
	}

// session identifier || USERAUTH_REQUEST message [RFC4252]
const u8 *msg; unsigned msg_len;
	msg = queue_peek (msg_out, -1, &msg_len);
u8 *sign_target; unsigned sign_target_len;
	alloca_chk(sign_target, = (u8 *), sign_target_len, = 4 +SESSION_ID_LEN +msg_len);
	store_be32 (sign_target +0, SESSION_ID_LEN);
	memcpy (sign_target +4, session_id, SESSION_ID_LEN);
	memcpy (sign_target +4 +SESSION_ID_LEN, msg, msg_len);

userkey_s r;
	ppk_parse_public_blob (userkey, userkey_len, &r);
unsigned usermod_bitused
	= mpint_bitused(r.usermod_len, userkey +r.usermod);
u8 *emsa; unsigned emsa_len;
	alloca_chk(emsa, = (u8 *), emsa_len, = ALIGNb(8, usermod_bitused) / 8 -1);
bool result
	= emsa_encode (
		  EMSA_TYPE_SHA1
		, sign_target, sign_target_len
		, emsa, emsa_len
		);
BUG(true == result) // TODO: error handling
u8 *usersign; unsigned usersign_max;
	alloca_chk(usersign, = (u8 *), usersign_max, = ALIGNb(8, 1 +usermod_bitused) / 8);
BUG(r.usermod_len <= usersign_max)
unsigned usersign_len =
	mpint_modpow (
		  emsa, emsa_len
		, priv_userexp_, priv_userexp_len
		, userkey +r.usermod, r.usermod_len
		, usersign, usersign_max
		, NULL, NULL, 0
		);
	while (0 < usersign_len && 0x00 == *usersign)
		++usersign, --usersign_len; // mpint -> non-padding uint(b.e.) (*1)
BUG(0 < usersign_len) // TODO: error handling

	queue_write_be32 (msg_out, 4 +7 +4 +usersign_len);
	queue_write_ssh_string (msg_out, "ssh-rsa");
	queue_write_be32 (msg_out, usersign_len);
	queue_write (msg_out, usersign, usersign_len);
	queue_set_boundary (msg_out);
}

// #90 CHANNEL_OPEN
static void channel_open_post (queue_s *msg_out, u32 recv_window, u32 recv_packet)
{
	queue_write_u8 (msg_out, CHANNEL_OPEN);
	queue_write_ssh_string (msg_out, "session");
	queue_write_be32 (msg_out, 0x100); // recepient channel
	queue_write_be32 (msg_out, recv_window); // initial window size
	queue_write_be32 (msg_out, recv_packet); // maximum packet size
	queue_set_boundary (msg_out);
}

// #93 CHANNEL_WINDOW_ADJUST
static unsigned channel_window_adjust_post (queue_s *msg_out, u32 add_len)
{
	queue_write_u8 (msg_out, CHANNEL_WINDOW_ADJUST);
	queue_write_be32 (msg_out, 0); // sender channel
	queue_write_be32 (msg_out, add_len); // bytes to add
	queue_set_boundary (msg_out);
}

// #94 CHANNEL_DATA
static void channel_data_post (queue_s *msg_out, const u8 *src, unsigned cb)
{
	queue_write_u8 (msg_out, CHANNEL_DATA);
	queue_write_be32 (msg_out, 0); // sender channel
	queue_write_be32 (msg_out, cb);
	queue_write (msg_out, src, cb);
	queue_set_boundary (msg_out);
}

// #97 CHANNEL_CLOSE
static void channel_close_post (queue_s *msg_out)
{
	queue_write_u8 (msg_out, CHANNEL_CLOSE);
	queue_write_be32 (msg_out, 0); // sender channel
	queue_set_boundary (msg_out);
}

// #98 CHANNEL_REQUEST
static void channel_request_post (queue_s *msg_out, u8 chanreq_stage, u16 ws_col, u16 ws_row)
{
	queue_write_u8 (msg_out, CHANNEL_REQUEST);
	queue_write_be32 (msg_out, 0); // recepient channel
BUG(chanreq_stage < 2)
	switch (chanreq_stage) {
	case 0:
		queue_write_ssh_string (msg_out, "pty-req"); // request type
		queue_write_u8 (msg_out, 0x01); // want reply
		queue_write_ssh_string (msg_out, "xterm"); // other 'vt100' ...
		queue_write_be32 (msg_out, ws_col); // ws_col
		queue_write_be32 (msg_out, ws_row); // ws_row
		queue_write_be32 (msg_out, 0); // xpixels
		queue_write_be32 (msg_out, 0); // ypixels

queue_s tmp;
		queue_ctor (&tmp, usizeof(tmp), 96, 32, 0);
		queue_write_u8 (&tmp, 3);
		queue_write_be32 (&tmp, 127); // VERASE(3) 127
		queue_write_u8 (&tmp, 128);
		queue_write_be32 (&tmp, INPUT_SPEED); // TTY_OP_ISPEED(128) [bit/s] input
		queue_write_u8 (&tmp, 129);
		queue_write_be32 (&tmp, OUTPUT_SPEED); // TTY_OP_OSPEED(129) [bit/s] output
		queue_write_u8 (&tmp, 0);
		queue_set_boundary (&tmp);
const u8 *pty_req; unsigned pty_req_len;
		pty_req = queue_peek (&tmp, 0, &pty_req_len);

		queue_write_be32 (msg_out, pty_req_len);
		queue_write (msg_out, pty_req, pty_req_len); // body of 'pty-req'

		queue_dtor (&tmp);
		break;
//	case 1:
	default:
		queue_write_ssh_string (msg_out, "shell"); // request type
		queue_write_u8 (msg_out, 0x01); // want reply
		break;
	}
	queue_set_boundary (msg_out);
}

///////////////////////////////////////////////////////////////////////////////
// parse SSH message

// #31 KEX_DH_GEX_GROUP
typedef struct {
	unsigned p; unsigned p_len;
	unsigned g; unsigned g_len;
} kex_dh_gex_group_s;
static bool parse_kex_dh_gex_group (
	  const void *msg_, unsigned msg_len
	, kex_dh_gex_group_s *retval
)
{
BUG(msg_ && 0 < msg_len && retval)
const u8 *msg, *msg_end;
	msg = (const u8 *)msg_, msg_end = msg +msg_len;

const u8 *p_, *g_; unsigned p_len, g_len;
	if (msg_end < (p_ = msg +1 +4) || msg_end < p_ +(p_len = load_be32 (p_ -4))
	 || msg_end < (g_ = p_ +p_len +4) || msg_end < g_ +(g_len = load_be32 (g_ -4))
	 || g_ +g_len < msg_end
	) {
// [reason] server sent invalid KEX_DH_GEX_GROUP (#31) [ignore] impossible [recovery] impossible
log_ (VTRR "SSH error" VTO ": KEX_DH_GEX_GROUP: unexpected layout data." "\n");
		return false;
	}
	if (! (5 == g_len && 0x00 == g_[0] || g_len < 5))
// [reason] server sent bigger g(generator for subgroup) [ignore] possible [recovery] user accept long waiting
log_ (VTRR "SSH warning" VTO ": KEX_DH_GEX_GROUP: 33bits over GF(p) is used. (may be slowly)" "\n");

// safe prime (modulus)
	retval->p = (unsigned)(p_ -msg); retval->p_len = p_len;

// generator for subgroup in GF(p)
	retval->g = (unsigned)(g_ -msg); retval->g_len = g_len;

	return true;
}

// #33 KEX_DH_GEX_REPLY
/* - the host key   -> exponent, modulus
   - f              = g ^ secret(server) mod p
   - signature of H (RSA2)
 */
typedef struct {
	unsigned hostkey; unsigned hostkey_len;
	unsigned hostmod; unsigned hostmod_len;
	unsigned hostexp; unsigned hostexp_len;
	unsigned f; unsigned f_len;
	unsigned kex_sign; unsigned kex_sign_len;
} kex_dh_gex_reply_s;
static bool parse_kex_dh_gex_reply (
	  const void *msg_, unsigned msg_len
	, kex_dh_gex_reply_s *retval
)
{
BUG(msg_ && 0 < msg_len && retval)
const u8 *msg, *msg_end;
	msg = (const u8 *)msg_, msg_end = msg +msg_len;

const u8 *hostkey_, *f_, *sign_;
unsigned hostkey_len_, f_len_, sign_len_;
	if (msg_end < (hostkey_ = msg +1 +4)
	 || msg_end < hostkey_ +(hostkey_len_ = load_be32 (hostkey_ -4))
	 || msg_end < (f_ = hostkey_ +hostkey_len_ +4)
	 || msg_end < f_ +(f_len_ = load_be32 (f_ -4))
	 || msg_end < (sign_ = f_ +f_len_ +4)
	 || msg_end < sign_ +(sign_len_ = load_be32 (sign_ -4))
	 || sign_ +sign_len_ < msg_end
	) {
// [reason] server sent invalid KEX_DH_GEX_REPLY (#33) [ignore] impossible [recovery] impossible
log_ (VTRR "SSH error" VTO ": KEX_DH_GEX_REPLY: unexpected layout data." "\n");
		return false;
	}

const u8 *hostkey_type_, *hostexp_, *hostmod_, *hostkey_end;
unsigned hostkey_type_len_, hostexp_len_, hostmod_len_;
	hostkey_end = hostkey_ +hostkey_len_;
	if (hostkey_end < (hostkey_type_ = hostkey_ +4)
	 || hostkey_end < hostkey_type_ +(hostkey_type_len_ = load_be32 (hostkey_type_ -4))
	 || !(7 == hostkey_type_len_)
	 || !(0 == memcmp ("ssh-rsa", hostkey_type_, 7)) // PENDING: other type support
	 || hostkey_end < (hostexp_ = hostkey_type_ +hostkey_type_len_ +4)
	 || hostkey_end < hostexp_ +(hostexp_len_ = load_be32 (hostexp_ -4))
	 || hostkey_end < (hostmod_ = hostexp_ +hostexp_len_ +4)
	 || hostkey_end < hostmod_ +(hostmod_len_ = load_be32 (hostmod_ -4))
	) {
// [reason] sever sent invalid host public key (#33) [ignore] impossible [recovery] impossible
log_ (VTRR "SSH error" VTO ": KEX_DH_GEX_REPLY: unexpected layout public host key." "\n");
		return false;
	}

const u8 *sign_type_, *sign_body_, *sign_end;
unsigned sign_type_len_, sign_body_len_;
	sign_end = sign_ +sign_len_;
	if (sign_end < (sign_type_ = sign_ +4)
	 || sign_end < sign_type_ +(sign_type_len_ = load_be32 (sign_type_ -4))
	 || !(7 == sign_type_len_ && 0 == memcmp ("ssh-rsa", sign_type_, 7)) // PENDING: other type support
	 || sign_end < (sign_body_ = sign_type_ +sign_type_len_ +4)
	 || sign_end < sign_body_ +(sign_body_len_ = load_be32 (sign_body_ -4))
	) {
// [reason] server sent invalid signature of host public key (#33) [ignore] impossible [recovery] impossible
log_ (VTRR "SSH error" VTO ": KEX_DH_GEX_REPLY: unexpected layout signature of H." "\n");
		return false;
	}

	retval->hostkey = (unsigned)(hostkey_ -msg); retval->hostkey_len =  hostkey_len_;
	retval->hostmod = (unsigned)(hostmod_ -msg); retval->hostmod_len =  hostmod_len_;
	retval->hostexp = (unsigned)(hostexp_ -msg); retval->hostexp_len =  hostexp_len_;
	retval->f = (unsigned)(f_ -msg); retval->f_len = f_len_;
	retval->kex_sign = (unsigned)(sign_body_ -msg); retval->kex_sign_len = sign_body_len_;
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// decode protocol (version string, plain/crypted message, MAC)

static const char *decode_version_string (
	  ringbuf_s *tcp_in
	, unsigned *idstr_len, unsigned *crlf_len
)
{
const u8 *tcp_ringbuf; unsigned len1, pos1;
	if (! (tcp_ringbuf = ringbuf_peek (tcp_in, ID_STR_MAX, &len1, &pos1, NULL)))
		return NULL;
BUG(0 == pos1)
const u8 *src, *tail;
	if (NULL == (tail = (const u8 *)memchr (src = tcp_ringbuf +pos1, '\n', len1)))
		return NULL;
unsigned len;
	len = (unsigned)(tail - src);
	++tail;
	while (0 < len && src[len -1] < 0x20)
		--len;
ASSERTE(src +len == tail -2 && '\r' == tail[-2])
	if (idstr_len)
		*idstr_len = len;
	if (crlf_len)
		*crlf_len = (tail - src) - len;

	return (const char *)src;
}

static u8 decode_plain_message (ringbuf_s *tcp_in, queue_s *pkt_in)
{
const u8 *prev; unsigned len0;
	prev = queue_peek (pkt_in, -1, &len0);
const u8 *tcp_ringbuf; unsigned len1, len2, pos1;
	tcp_ringbuf = ringbuf_peek (tcp_in, 0, &len1, &pos1, &len2);
	if (len0 +len1 +len2 < 4)
		return 0;

u8 msg_type // Must do before queue_write(pkt_in, )
	= (5 < len0) ? prev[5]
	: 0 /* not reached, or too short */;

u8 be32[4];
	if (0 < len0)
		memcpy (be32 +0, prev, min(len0, 4));
	if (len0 < 4) {
		ringbuf_read (tcp_in, be32 +len0, 4 -len0);
		queue_write (pkt_in, be32 +len0, 4 -len0);
		len0 = 4;
		ringbuf_peek (tcp_in, 0, &len1, &pos1, &len2);
	}
unsigned pkt_len, add1, add2;
	pkt_len = 4 +load_be32 (be32);
	if (0 < (add1 = min(len1, pkt_len -len0)))
		queue_write (pkt_in, tcp_ringbuf +pos1, add1);
	if (0 < (add2 = min(len2, pkt_len -(len0 +add1))))
		queue_write (pkt_in, tcp_ringbuf +0, add2);

	msg_type // Must do before ringbuf_remove(tcp_in, )
	= (5 < len0            ) ? msg_type
	: (5 < len0 +add1      ) ? tcp_ringbuf[pos1 +(5 -len0)]
	: (5 < len0 +add1 +add2) ? tcp_ringbuf[5 -(add1 +add2)]
	: 0 /* not reached, or too short */;

	if (0 < add1 +add2)
		ringbuf_remove (tcp_in, add1 +add2);
	if (pkt_len < len0 +add1 +add2)
		return 0; // not completed

	queue_set_boundary (pkt_in);
	return msg_type;
}

static u8 decode_crypt_message (
	  ringbuf_s *tcp_in
	, u32 dpkt_num
	, struct cipher_i *dec, struct cipher *dec_this_
	, struct mac_i *dmac, struct mac *dmac_this_
	, queue_s *pkt_in
)
{
const u8 *prev; unsigned len0;
	prev = queue_peek (pkt_in, -1, &len0);
u8 *tcp_ringbuf; unsigned len1, len2, pos1;
	tcp_ringbuf = ringbuf_enter_modifying (tcp_in, &len1, &pos1, &len2);
	if (len0 +len1 +len2 < 4) {
		ringbuf_leave_modifying (tcp_in);
		return 0; // not completed
	}
		
unsigned bs
	= dec->get_block_length (dec_this_);
BUG(0 == (bs -1 & len0))
u8 *dst, *tmp;
	alloca_chk(dst, = (u8 *), bs, );
	alloca_chk(tmp, = (u8 *), bs, );
u32 pkt_len; unsigned pos, done;
	pkt_len = (0 == len0) ? (u32)-1 : 4 +load_be32 (prev +0);
	pos = pos1;
	for (done = 0; len0 +done < pkt_len && !(len1 +len2 < done + bs); done += bs) {
		if (! (len1 < done +bs))
			dec->decrypt (dec_this_, tcp_ringbuf +pos, bs, dst), pos += bs;
		else {
			if (done < len1)
				memcpy (tmp +0, tcp_ringbuf +pos, len1 -done);
			pos = bs -(len1 -done);
			memcpy (tmp +len1, tcp_ringbuf +0, pos);
			dec->decrypt (dec_this_, tmp, bs, dst);
		}
		queue_write (pkt_in, dst, bs);
		if (0 == len0 +done)
			pkt_len = 4 +load_be32 (dst);
	}
	memset (dst, 0, bs); // security erase
	ringbuf_leave_modifying (tcp_in);
	if (0 < done)
		ringbuf_remove (tcp_in, done);

unsigned dmac_len
	= dmac->get_length (dmac_this_);
	if (len0 +len1 +len2 < pkt_len +dmac_len)
		return 0; // not completed
const u8 *pkt; u8 msg_type;
	pkt = queue_peek (pkt_in, -1, &pkt_len);
BUG(pkt_len == len0 + done)
	msg_type = (5 < pkt_len) ? pkt[5] : 0;

// MAC validate
u8 *expected, *computed;
	alloca_chk(expected, = (u8 *), dmac_len, );
	alloca_chk(computed, = (u8 *), dmac_len, );

	ringbuf_read (tcp_in, expected, dmac_len);
	dmac->compute (dmac_this_, dpkt_num, pkt, pkt_len, computed);

	if (! (0 == memcmp (expected, computed, dmac_len))) {
// [reason] drop packet, any missing protocol, server problem [ignore] impossible [recovery] impossible
log_ (VTRR "SSH error" VTO ": encrypted packet MAC mismatch." "\n");
		; // TODO: MAC error recovery
	}

	queue_set_boundary (pkt_in);
	return msg_type;
}

static bool decode_message (
	  ringbuf_s *tcp_in
	, tcp_phase *tcp, u32 dpkt_num
	, void (*idstr_callback)(const char *idstr, unsigned idstr_len, void *priv), void *priv
	, struct cipher_i *dec, struct cipher *dec_this_
	, struct mac_i *dmac, struct mac *dmac_this_
	, queue_s *pkt_in
)
{
// V_S, the server's version string
	if (!tcp->is_idstr_received) {
const char *idstr; unsigned idstr_len, crlf_len;
		if (NULL == (idstr = decode_version_string (tcp_in, &idstr_len, &crlf_len)))
			return false;
		tcp->is_idstr_received = 1;
		idstr_callback (idstr, idstr_len, priv);
		ringbuf_remove (tcp_in, idstr_len +crlf_len);
	}

// server SSH message (plain)
	if (!tcp->is_need_decrypt) {
unsigned msg_type;
		if (0 == (msg_type = decode_plain_message (tcp_in, pkt_in)))
			return false;
		if (NEWKEYS == msg_type)
			tcp->is_need_decrypt = 1;
		return true;
	}

// server SSH message (crypted)
unsigned msg_type;
	if (0 == (msg_type = decode_crypt_message (tcp_in, dpkt_num, dec, dec_this_, dmac, dmac_this_, pkt_in)))
		return false;
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// encode protocol (version string, plain/crypted message, MAC)

static void encode_version_string (const char *idstr, queue_s *tcp_out)
{
BUG(idstr)
	queue_write (tcp_out, idstr, ustrlen (idstr));
	queue_write (tcp_out, "\r\n", 2);
	queue_set_boundary (tcp_out);
}

static void encode_plain_message (
	  const u8 *msg, unsigned msg_len
	, queue_s *tcp_out
)
{
unsigned pkt_align, padding, pkt_len;
	pkt_align = 8; // [RFC4253] 6.
	if ((padding = PADDING(pkt_align, 4 +1 +msg_len)) < 4) // [RFC4253] 6.
		padding += pkt_align;
BUG(4 <= padding)
	pkt_len = 4 +1 +msg_len +padding;

	// prefix header
u8 *pkt;
	alloca_chk(pkt, = (u8 *), pkt_len +SHA1_LEN, );
	store_be32 (pkt, 1 + msg_len + padding);
	pkt[4] = (u8)padding;
	memcpy (pkt +4 +1, msg, msg_len);

	// postfix padding
	memset (pkt +4 +1 +msg_len, 0, padding); // [RFC4253]
	queue_write (tcp_out, pkt, pkt_len);
}

// 'aes256-ctr' PENDING: other crypt support
// 'hmac-sha1' PENDING: other MAC support
static void encode_crypt_message (
	  const u8 *msg, unsigned msg_len
	, u32 epkt_num
	, struct cipher_i *enc, struct cipher *enc_this_
	, struct mac_i *emac, struct mac *emac_this_
	, queue_s *tcp_out
)
{
	// length of SSH packet, padding and MAC [RFC4253] 6.
unsigned pkt_align, padding, pkt_len;
	if ((pkt_align = enc->get_block_length (enc_this_)) < 8)
		pkt_align = 8;
	if ((padding = PADDING(pkt_align, 4 +1 +msg_len)) < 4)
		padding += pkt_align;
BUG(4 <= padding)
	pkt_len = 4 +1 +msg_len +padding;
unsigned emac_len
	= emac->get_length (emac_this_);

	// work stack frame
u8 *pkt;
	alloca_chk(pkt, = (u8 *), pkt_len +emac_len, );

	// header & body
	store_be32 (pkt, 1 + msg_len + padding);
	pkt[4] = (u8)padding;
	memcpy (pkt +4 +1, msg, msg_len);
	// padding
	store_random (pkt +pkt_len -padding, padding); // [RFC4253]
	// MAC
	emac->compute (emac_this_, epkt_num, pkt, pkt_len, pkt +pkt_len);
	// encrypt
	enc->encrypt (enc_this_, pkt, pkt_len, pkt);

	// post
	queue_write (tcp_out, pkt, pkt_len +emac_len);
}

static bool encode_message (
	  queue_s *msg_out
	, tcp_phase *tcp, u32 epkt_num
	, struct cipher_i *enc, struct cipher *enc_this_
	, struct mac_i *emac, struct mac *emac_this_
	, queue_s *tcp_out
)
{
const u8 *msg; unsigned msg_len;
	if (NULL == (msg = queue_peek (msg_out, 0, &msg_len)))
		return false;

	if (!tcp->is_need_encrypt) {
		encode_plain_message (msg, msg_len, tcp_out);
		if (0 < msg_len && NEWKEYS == msg[0])
			tcp->is_need_encrypt = 1;
	}
	else
		encode_crypt_message (msg, msg_len, epkt_num, enc, enc_this_, emac, emac_this_, tcp_out);
	queue_remove (msg_out, msg_len);
	queue_set_boundary (tcp_out);
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// event handler

#define LOGINNAME_INPUT  1
#define PASSPHRASE_INPUT 2

// #-- the server's version string
static void _on_idstr (ssh2_s *m_, const char *idstr, unsigned idstr_len)
{
BUG(0 < idstr_len)
	// TODO: parse ssh version etc.

	// kex hash #2 string  V_S, the server's version string (CR and LF excluded)
	kexhash_input_be32 (&m_->kexhash_ctx, idstr_len);
	kexhash_input_raw (&m_->kexhash_ctx, idstr, idstr_len);

	kexinit_post (&m_->msg_out);

const u8 *kexinit; unsigned kexinit_len;
	kexinit = queue_peek (&m_->msg_out, -2, &kexinit_len);
	// kex hash #3 string  I_C, the payload of the client's SSH_MSG_KEXINIT
	kexhash_input_be32 (&m_->kexhash_ctx, kexinit_len);
	kexhash_input_raw (&m_->kexhash_ctx, kexinit, kexinit_len);
}

// #20 KEXINIT
static void _on_kexinit (ssh2_s *m_, const u8 *msg, unsigned msg_len)
{
	// PENDING: parse KEXINIT message, and decide each algorithms [RFC4253] 7.1
	m_->enc = cipher_factory (&m_->enc_this_, "aes256-ctr");
	m_->dec = cipher_factory (&m_->dec_this_, "aes256-ctr");
	m_->emac = mac_factory (&m_->emac_this_, "hmac-sha1");
	m_->dmac = mac_factory (&m_->dmac_this_, "hmac-sha1");

	// kex hash #4 string  I_S, the payload of the server's SSH_MSG_KEXINIT
	kexhash_input_be32 (&m_->kexhash_ctx, msg_len);
	kexhash_input_raw (&m_->kexhash_ctx, msg, msg_len);

	kex_dh_gex_request_post (&m_->msg_out, m_->pbits);
	m_->is_need_pbits_minmax = 1;
}

// #31 KEX_DH_GEX_GROUP
static void _on_kex_dh_gex_group (ssh2_s *m_, const u8 *msg, unsigned msg_len)
{
kex_dh_gex_group_s r;
	if (!parse_kex_dh_gex_group (msg, msg_len, &r))
		return;
	if (m_->x_) { // safety
		memset (m_->x_, 0, m_->x_len); // security erase
		free (m_->x_), m_->x_ = NULL;
	}
unsigned p_bitused;
	p_bitused = mpint_bitused(r.p_len, msg +r.p);
unsigned x_max;
	m_->x_ = (u8 *)malloc (x_max = ALIGNb(8, 1 +(p_bitused -1)) / 8);
BUG(ALIGNb(8, 1 +(p_bitused -1)) / 8 <= x_max) // storable space of 'mpint' less than p/2
	m_->x_len = mpint_random_half (
		  msg +r.p, r.p_len
		, m_->x_, x_max
	);
	if (m_->e_) { // safety
		memset (m_->e_, 0, m_->e_len); // security erase
		free (m_->e_), m_->e_ = NULL;
	}
BUG(NULL == m_->e_)
unsigned e_max;
	m_->e_ = (u8 *)malloc (e_max = ALIGNb(8, 1 +p_bitused * 2) / 8);
	m_->e_len = kex_dh_gex_init_post (
		  &m_->msg_out
		, msg +r.g, r.g_len
		, m_->x_, m_->x_len
		, msg +r.p, r.p_len
		, m_->e_, e_max
		);
	if (m_->p_) // safety
		free (m_->p_), m_->p_ = NULL;
	memcpy (m_->p_ = (u8 *)malloc (r.p_len), msg +r.p, m_->p_len = r.p_len);
	if (m_->g_) // safety
		free (m_->g_), m_->g_ = NULL;
	memcpy (m_->g_ = (u8 *)malloc (r.g_len), msg +r.g, m_->g_len = r.g_len);
}

// #33 KEX_DH_GEX_REPLY
static void _on_kex_dh_gex_reply (ssh2_s *m_, const u8 *msg, unsigned msg_len)
{
kex_dh_gex_reply_s r;
	if (!parse_kex_dh_gex_reply (msg, msg_len, &r))
		return; // PENDING: error handling
	// kex hash #5 string  K_S, the host key
	kexhash_input_raw (&m_->kexhash_ctx, msg +r.hostkey -4, 4 +r.hostkey_len);
unsigned mod_bitused
	= mpint_bitused(r.hostmod_len, msg +r.hostmod);
u8 *emsa; unsigned emsa_max;
	alloca_chk(emsa, = (u8 *), emsa_max, = ALIGNb(8, 1 +mod_bitused) / 8);

	// decrypt: sign(H) -> EMSA(H)
unsigned emsa_len =
	mpint_modpow (
		  msg +r.kex_sign, r.kex_sign_len
		, msg +r.hostexp, r.hostexp_len
		, msg +r.hostmod, r.hostmod_len
		, emsa, emsa_max
		, NULL, NULL, 0
		);

	// parse EMSA
unsigned r_sha1, kexhash_type;
	kexhash_type = emsa_decode (emsa, emsa_len, &r_sha1);
BUG(0 < r_sha1 && EMSA_TYPE_SHA1 == kexhash_type) // TODO: error handling

	// K = f^x mod p [RFC4419]
BUG(m_->p_ && 0 < m_->p_len)
u8 *k_; unsigned k_max;
	alloca_chk(k_, = (u8 *), k_max, = ALIGNb(8, 1 + 2 * mpint_bitused(m_->p_len, m_->p_)) / 8);
unsigned k_len =
	mpint_modpow (
		  msg +r.f, r.f_len
		, m_->x_, m_->x_len
		, m_->p_, m_->p_len
		, k_, k_max
		, NULL, NULL, 0
		); // Heavy
	memset (m_->x_, 0, m_->x_len); // security erase
	free (m_->x_), m_->x_ = NULL;

BUG(m_->g_ && 0 < m_->g_len)
BUG(0 < r.f_len && msg[r.f] < 128 || 1 < r.f_len && 0 == msg[r.f] && 127 < msg[r.f +1])
u8 kexhash[32];
	kexhash_finish (
		 &m_->kexhash_ctx
		, m_->is_need_pbits_minmax
		, m_->pbits
		, m_->p_, m_->p_len
		, m_->g_, m_->g_len
		, m_->e_, m_->e_len
		, msg +r.f, r.f_len
		, k_, k_len
		, kexhash
		);
	memset (m_->e_, 0, m_->e_len); // security erase
	free (m_->p_), m_->p_ = NULL;
	free (m_->g_), m_->g_ = NULL;
	free (m_->e_), m_->e_ = NULL;
u8 kexhash_sha1[SHA1_LEN];
	sha1 (kexhash, usizeof(kexhash), kexhash_sha1);
	if (! (0 == memcmp (kexhash_sha1, emsa +r_sha1, SHA1_LEN)))
		; // TODO: error handling server auth
	memcpy (m_->session_id, kexhash, SESSION_ID_LEN); // session id

struct ssh2keygen seed;
	ssh2key_ctor (&seed, k_, k_len, kexhash, usizeof(kexhash), m_->session_id, SESSION_ID_LEN);
	m_->enc->pushkey (&m_->enc_this_, ssh2key_generate, &seed, CtoS);
	m_->dec->pushkey (&m_->dec_this_, ssh2key_generate, &seed, StoC);
	m_->emac->pushkey (&m_->emac_this_, ssh2key_generate, &seed, CtoS);
	m_->dmac->pushkey (&m_->dmac_this_, ssh2key_generate, &seed, StoC);
	ssh2key_dtor (&seed);
}

// #21 NEWKEYS
static void _on_newkeys (ssh2_s *m_, const u8 *msg, unsigned msg_len)
{
	newkeys_post (&m_->msg_out);
	service_request_post (&m_->msg_out);
}

// #6 SERVICE_ACCEPT
static void _on_service_accept (ssh2_s *m_, const u8 *msg, unsigned msg_len)
{
	userauth_request_post (
		 &m_->msg_out
		, m_->login_name
		, NULL, 0
		, NULL, NULL, 0
		);
}

// #51 USERAUTH_FAILURE
static void _on_userauth_failure (ssh2_s *m_, const u8 *msg, unsigned msg_len)
{
	// PENDING: recognize auth ways.
u8 *userkey; unsigned userkey_len;
	do {
		if (0 == (userkey_len = ppk_read_public_blob (m_->ppk_path, NULL, 0)))
			break;
		alloca_chk(userkey, = (u8 *), userkey_len, );
		if (! (userkey_len == ppk_read_public_blob (m_->ppk_path, userkey, userkey_len)))
			break;
	} while (0);

	switch (ppk_errno) {
	case PPK_E_PUBLIC_ENTRY_NOT_FOUND:
	default: // any error
log_ (VTRR "%s" VTO ": '" PPK_PUBLIC_TAG "': cannot read." "\n", m_->ppk_path);
		exit (-1); // [reason] private key file problem [ignore] impossible [recovery] user side

	case 0: // no error
		userauth_request_post (
			 &m_->msg_out
			, m_->login_name
			, userkey, userkey_len
			, NULL, NULL, 0
			);
		break;
	}
}

// #52 USERAUTH_SUCCESS
static void _on_userauth_success (ssh2_s *m_, const u8 *msg, unsigned msg_len)
{
ASSERTE(1 == msg_len)
	channel_open_post (&m_->msg_out, m_->recv_window_max, m_->recv_window_max);
}

// #60 USERAUTH_PK_OK
static void _on_userauth_pk_ok (ssh2_s *m_, const u8 *msg, unsigned msg_len)
{
	// PENDING: recognize auth ways.
// input passphrase (request)
lock_lock (&m_->auth_subsystem_lock);
	input_request_post (
		 &m_->auth_subsystem
		, PASSPHRASE_INPUT, INPUT_ECHO_OFF
		, "Passphrase: "
	);
lock_unlock (&m_->auth_subsystem_lock);
}

// #91 CHANNEL_OPEN_CONFIRMATION
static void _on_channel_open_confirmation (ssh2_s *m_, const u8 *msg, unsigned msg_len)
{
	// TODO: parse
	 // (4)recepient channel (=256) (*)defined by CHANNEL_OPEN
	 // (4)sender channel (= 0)
	 // (4)remote window size (= 0)
	 // (4)remote max pkt size (= 0x8000)
	channel_request_post (&m_->msg_out, 0, m_->ws_col, m_->ws_row);
}

// #97 CHANNEL_CLOSE
static void _on_channel_close (ssh2_s *m_, const u8 *msg, unsigned msg_len)
{
	// TODO: parse
	 // (4)recepient channel (=256) (*)defined by CHANNEL_OPEN
	channel_close_post (&m_->msg_out);
}

// #99 CHANNEL_SUCCESS
static void _on_channel_success (ssh2_s *m_, const u8 *msg, unsigned msg_len)
{
	// TODO: parse
	 // (4)recepient channel (=256) (*)defined by CHANNEL_OPEN
	channel_request_post (&m_->msg_out, 1, m_->ws_col, m_->ws_row);
	m_->is_kbd_to_channel = 1;
}

// #94 CHANNEL_DATA
static void _on_channel_data (ssh2_s *m_, const u8 *msg, unsigned msg_len)
{
	// (4)recepient channel
	// (str)console out
	// TODO: accept recepient channel.
	if (msg_len < 1 +4 +4)
		return;
unsigned data_len;
	if (0 == (data_len = load_be32 (msg +1 +4)))
		return;
	if (msg_len < 1 +4 +4 + data_len)
		return;
	m_->recv_window_used += data_len; // TODO: error log
	echo_back (&m_->echo_buf, &m_->echo_lock, msg +1 +4 +4, data_len);
}

static bool _dispatch_message (ssh2_s *m_, const u8 *msg, unsigned msg_len)
{
BUG(m_ && msg && 0 < msg_len)
	switch (msg[0]) {
	case KEXINIT: // #20
		_on_kexinit (m_, msg, msg_len);
		break;
	case KEX_DH_GEX_GROUP: // #31
		_on_kex_dh_gex_group (m_, msg, msg_len);
		break;
	case KEX_DH_GEX_REPLY: // #33
		_on_kex_dh_gex_reply (m_, msg, msg_len);
		break;
	case NEWKEYS: // #21
		_on_newkeys (m_, msg, msg_len);
		break;
	case SERVICE_ACCEPT: // #6
		_on_service_accept (m_, msg, msg_len);
		break;
	case USERAUTH_FAILURE: // #51
		_on_userauth_failure (m_, msg, msg_len);
		break;
	case USERAUTH_SUCCESS: // #52
		_on_userauth_success (m_, msg, msg_len);
		break;
	case USERAUTH_PK_OK: // #60
		_on_userauth_pk_ok (m_, msg, msg_len);
		break;
	case CHANNEL_OPEN_CONFIRMATION: // #91
		_on_channel_open_confirmation (m_, msg, msg_len);
		break;
	case CHANNEL_CLOSE: // #97
		_on_channel_close (m_, msg, msg_len);
		break;
	case CHANNEL_REQUEST: // #98
		// TODO: implements
		 // (4)recepient channel (=256) (*)defined by CHANNEL_OPEN
		 // (str, args)
		 // -> 'exit-status' (1)FALSE (4)exit_status
		break;
	case CHANNEL_FAILURE: // #100
		// TODO: implements
		break;
	case CHANNEL_SUCCESS: // #99
		_on_channel_success (m_, msg, msg_len);
		break;
	case CHANNEL_WINDOW_ADJUST: // #93
		// TODO: implements
		break;
	case CHANNEL_DATA: // #94
		_on_channel_data (m_, msg, msg_len);
		break;
	case CHANNEL_EOF: // #96
		// TODO: implements
		 // (4)recepient channel (=256) (*)defined by CHANNEL_OPEN
		break;
	case GLOBAL_REQUEST: // #80
		// PENDING: implements
		break;
	default:
		break;
	}
	return true;
}

static void _on_passphrase_accept (ssh2_s *m_, unsigned msg_len)
{
// subsystem access
char *passphrase; unsigned got;
	alloca_chk(passphrase, = (char *), msg_len +1, );
	got = input_accept_get (
		 &m_->auth_subsystem
		, &m_->auth_subsystem_lock
		, passphrase
		, msg_len
	);
BUG(msg_len == got)
	passphrase[got] = '\0';

u8 *userkey; unsigned userkey_len;
u8 *priv_userexp; unsigned priv_userexp_len;
	priv_userexp = NULL;
	ppk_errno = 0;
	do {
		if (0 == (userkey_len = ppk_read_public_blob (m_->ppk_path, NULL, 0)))
			break;
		alloca_chk(userkey, = (u8 *), userkey_len, );
		if (! (userkey_len == ppk_read_public_blob (m_->ppk_path, userkey, userkey_len)))
			break;
unsigned priv_userexp_max;
		if (0 == (priv_userexp_max = ppk_read_private_blob (m_->ppk_path, NULL, NULL, 0)))
			break;
		alloca_chk(priv_userexp, = (u8 *), priv_userexp_max, );
		if (0 == (priv_userexp_len = ppk_read_private_blob (m_->ppk_path, passphrase, priv_userexp, priv_userexp_max)))
			break;

		userauth_request_post (
			 &m_->msg_out
			, m_->login_name
			, userkey, userkey_len
			, m_->session_id, priv_userexp, priv_userexp_len
		);
	} while (0);
	if (priv_userexp && 0 < priv_userexp_len)
		memset (priv_userexp, 0, priv_userexp_len); // security erase
	if (0 == ppk_errno)
		return; // success

// what error ?
	switch (ppk_errno) {
	case PPK_E_FILE_CANNOT_OPEN:
	case PPK_E_PUBLIC_ENTRY_NOT_FOUND:
log_ (VTRR "%s" VTO ": '" PPK_PUBLIC_TAG "' cannot read." "\n", m_->ppk_path);
		break; // [reason] private key file problem ('Public-Lines') [ignore] impossible [recovery] user side
	case PPK_E_PRIVATE_ENTRY_NOT_FOUND:
log_ (VTRR "%s" VTO ": '" PPK_PRIVATE_TAG "' cannot read." "\n", m_->ppk_path);
		break; // [reason] private key file problem ('Private-Lines') [ignore] impossible [recovery] user side
	case PPK_E_PRIVATE_MAC_ENTRY_NOT_FOUND:
log_ (VTRR "%s" VTO ": '" PPK_PRIVATE_MAC_TAG "' cannot read." "\n", m_->ppk_path);
		break; // [reason] private key file problem ('Private-MAC') [ignore] impossible [recovery] user side
	case PPK_E_PASSPHRASE_TOO_LONG:
	case PPK_E_ENCRYPTION_ENTRY_NOT_FOUND:
	case PPK_E_UNEXPECTED_ENCRIPTION:
	default:
log_ (VTRR "%s" VTO ": '" PPK_PRIVATE_TAG "' cannot decrypt." "\n", m_->ppk_path);
		break;
	case PPK_E_MISMATCH_PRIVATE_MAC:
		break; // recoverable error
	}
	if (! (PPK_E_MISMATCH_PRIVATE_MAC == ppk_errno))
		exit (-1); // fatal error

// re-input passphrase (request)
lock_lock (&m_->auth_subsystem_lock);
	input_request_post (
		 &m_->auth_subsystem
		, PASSPHRASE_INPUT, INPUT_ECHO_OFF
		, "Wrong passphrase" "\r\n"
		  "Passphrase: "
	);
lock_unlock (&m_->auth_subsystem_lock);
}

static void _dispatch_input_accept (ssh2_s *m_, unsigned accept_id, unsigned pkt_len)
{
	switch (accept_id) {
	case PASSPHRASE_INPUT:
		_on_passphrase_accept (m_, pkt_len -2);
		break;
	default:
		break;
	}
}

///////////////////////////////////////////////////////////////////////////////
// network socket

#define E_SUCCESS               0
#define E_IO_ERROR             -1
#define E_IO_PENDING           -2
#define E_SOCKET_CLOSED        -3
#define E_PACKET_NOT_COMPLETED -4

static int send_from_queue (int fd, queue_s *tcp_out)
{
const u8 *pkt; unsigned pkt_len;
	if (NULL == (pkt = queue_peek (tcp_out, 0, &pkt_len)))
		return E_PACKET_NOT_COMPLETED;

	errno = 0;
int sent
	= (int)write (fd, pkt, pkt_len);
	if (0 == sent || sent < 0 && EWOULDBLOCK == errno)
		return E_IO_PENDING;
	if (sent < 0)
		return E_IO_ERROR;

	queue_remove (tcp_out, sent);
	return 0; // success
}

static int recv_to_ringbuf (
	  int server
	, ringbuf_s *tcp_in
)
{
	errno = 0;
u8 *buf; unsigned len1, pos1, len2;
	buf = ringbuf_enter_appending (tcp_in, &len1, &pos1, &len2);
BUG(buf)
unsigned add; int retcode;
	add = 0;
	if ((retcode = (int)read (server, buf +pos1, len1)) < 0)
		add = 0;
	else if (len1 == (add = (unsigned)retcode) && 0 < len2) {
		if (0 < (retcode = (int)read (server, buf +0, len2)))
			add += retcode;
	}
	ringbuf_leave_appending (tcp_in, add);

	if (0 == retcode) // RST received
		return E_SOCKET_CLOSED;
	if (-1 == retcode && EWOULDBLOCK == errno) // wait data arrival
		return E_IO_PENDING;
	if (-1 == retcode) // unexpected error
		return E_IO_ERROR;

	return E_SUCCESS; // one more bytes exists
}

///////////////////////////////////////////////////////////////////////////////
// message loop

static void _on_idstr_thunk (const char *idstr, unsigned idstr_len, void *priv)
{
	_on_idstr ((ssh2_s *)priv, idstr, idstr_len);
}

static void _mainloop (
  ssh2_s *m_
, int server
, const char *hostname
, u16 port
, const char *login_name
, unsigned input_max
)
{
int fdflags;
	if ((fdflags = fcntl (server, F_GETFL)) < 0) {
// [reason] unknown (socket problem) [ignore] impossible [recovery] impossible
log_ (VTRR "socket error" VTO ": !fcntl(F_GETFL)=%d @err=%d'%s'" "\n", fdflags, errno, unixerror (errno));
		return;
	}
	if ((fdflags = fcntl (server, F_SETFL, O_NONBLOCK | fdflags)) < 0) {
// [reason] unknown (socket problem) [ignore] impossible [recovery] impossible
log_ (VTRR "socket error" VTO ": !fcntl(F_SETFL)=%d @err=%d'%s'" "\n", fdflags, errno, unixerror (errno));
		return;
	}
	m_->pbits = KEX_MODULUS_MAXBITS;
	m_->epkt_num = m_->dpkt_num = 0;
BUG(strlen (login_name) <= sizeof(m_->login_name) -1)
	strncpy (m_->login_name, login_name, sizeof(m_->login_name) -1);
	m_->login_name[sizeof(m_->login_name) -1] = '\0';

char *msg_copy;
	alloca_chk(msg_copy, = (char *), 2 +input_max, );
	while (1) {
		// TCP receive
		switch (recv_to_ringbuf (server, &m_->tcp_in)) {
		case E_IO_ERROR: // [reason] unknown problem (socket read) [ignore] impossible [recovery] impossible
log_ (VTRR "TCP error" VTO ": !read() @err=%d'%s'" "\n", errno, unixerror (errno));
			return;
		case E_SOCKET_CLOSED: // [reason] socket disconnected [ignore] impossible [recovery] impossible
log_ (VTYY "Disconnected." VTO "\n");
			return;
		case E_IO_PENDING:
		case E_SUCCESS:
			break;
		default:
BUG(0)
		}

		// server SSH2 packetize
		if (true == decode_message (&m_->tcp_in
			, &m_->tcp,  m_->dpkt_num
			, _on_idstr_thunk, m_
			,  m_->dec, &m_->dec_this_
			,  m_->dmac, &m_->dmac_this_
			, &m_->pkt_in
		))
			++m_->dpkt_num;

		// server SSH2 message
const u8 *pkt; unsigned pkt_len;
		if (pkt = queue_peek (&m_->pkt_in, 0, &pkt_len)) {
			if (!(pkt[4] < pkt_len) || _dispatch_message (m_, pkt +4 +1, pkt_len -(pkt[4] +4 +1)))
				queue_remove (&m_->pkt_in, pkt_len);
		}

		// key-in data
lock_lock (&m_->kbd_lock);
unsigned len1, len2;
		if (m_->is_kbd_to_channel && ringbuf_peek (&m_->kbd_buf, KBD_MAX, &len1, NULL, &len2)) {
u8 *kbd;
			alloca_chk(kbd, = (u8 *), len1 +len2, );
unsigned kbd_len
			= ringbuf_read (&m_->kbd_buf, kbd, len1 +len2);
lock_unlock (&m_->kbd_lock);
			channel_data_post (&m_->msg_out, kbd, kbd_len);
			continue;
		}
lock_unlock (&m_->kbd_lock);

		// TCP send
		switch (send_from_queue (server, &m_->tcp_out)) {
		case E_IO_ERROR: // [reason] unknown problem (socket write) [ignore] impossible [recovery] impossible
log_ (VTRR "TCP error" VTO ": !write() @err=%d'%s'" "\n", errno, unixerror (errno));
			continue;
		case E_PACKET_NOT_COMPLETED:
			break;
		case E_IO_PENDING:
			usleep (1000);
			continue;
		case E_SUCCESS: // sent one packet.
			continue;
		default:
BUG(0)
		}

		// client SSH2 message
		if (true == encode_message (&m_->msg_out
			, &m_->tcp,  m_->epkt_num
			,  m_->enc, &m_->enc_this_
			,  m_->emac, &m_->emac_this_
			, &m_->tcp_out
		)) {
			++m_->epkt_num;
			continue;
		}

		// input passphrase (response)
unsigned accept_id/*, pkt_len*/;
		accept_id = input_accept_wait (
			 &m_->auth_subsystem
			, 0
			, &m_->auth_subsystem_lock
			, &pkt_len
			);
		if (0 < accept_id) {
BUG(2 < pkt_len && PASSPHRASE_INPUT == accept_id)
			_dispatch_input_accept (m_, accept_id, pkt_len);
		}
		// cannot dispatch both send and recv.
		usleep (1000);
	}
}

static void *_thread (void *param)
{
BUG(param)
ssh2_s *m_;
	m_ = (ssh2_s *)param;

	// input interrupt start
interrupt_input_s ii = {
	.boot_complete = false,
	.msg_lock      = &m_->auth_subsystem_lock,
	.msg_post      = &m_->auth_subsystem,
	.input_max     =  max(LOGINNAME_MAX, PPK_PASSPHRASE_MAX),
	.kbd_lock      = &m_->kbd_lock,
	.kbd_recv      = &m_->kbd_buf,
	.echo_lock     = &m_->echo_lock,
	.echo_post     = &m_->echo_buf,
};
	if (! (0 == pthread_create (&m_->auth_subsystem_id, NULL, interrupt_input_thunk, (void *)&ii))) {
// [reason] unknown failure (thread creation) [ignore] impossible [recovery] impossible
log_ (VTRR "internal error" VTO ": !pthread_create(interrupt_input) @err=%d'%s'" "\n", errno, unixerror (errno));
		return NULL;
	}
	while (!ii.boot_complete)
		usleep (1);

	// input login name (request)
lock_lock (&m_->auth_subsystem_lock);
	input_request_post (
		 &m_->auth_subsystem
		, LOGINNAME_INPUT, 0
		, "login as: "
	);
lock_unlock (&m_->auth_subsystem_lock);
	//                  (response)
unsigned accept_id, msg_len;
	accept_id = input_accept_wait (
		 &m_->auth_subsystem
		, 1000000 / 60
		, &m_->auth_subsystem_lock
		, &msg_len
		);
BUG(2 < msg_len && LOGINNAME_INPUT == accept_id)
char *login_name;
	alloca_chk(login_name, = (char *), msg_len -2 +1, );
unsigned got
	= input_accept_get (
		 &m_->auth_subsystem
		, &m_->auth_subsystem_lock
		, login_name
		, msg_len -2
	);
BUG(msg_len -2 == got)
	login_name[got] = '\0';

	// the client's version string (send request)
	encode_version_string (SSH_CLIENT_ID, &m_->tcp_out);
const u8 *client_id; unsigned client_id_len;
	client_id = queue_peek (&m_->tcp_out, -2, &client_id_len);

	// kex hash #1 string  V_C, the client's version string (CR and LF excluded)
	kexhash_input_start (&m_->kexhash_ctx, usizeof(m_->kexhash_ctx));
	kexhash_input_be32 (&m_->kexhash_ctx, client_id_len -2);
	kexhash_input_raw (&m_->kexhash_ctx, client_id, client_id_len -2);

int server;
	if (-1 == (server = sockopen (m_->hostname, m_->port)))
		return NULL;
	_mainloop (m_, server, m_->hostname, m_->port, login_name, ii.input_max);
	return NULL;
}

void _preconnect (struct ssh2 *this_, const char *hostname, u16 port, const char *ppk_path)
{
BUG(this_ && hostname && 0 < port && ppk_path)
ssh2_s *m_;
	m_ = (ssh2_s *)this_;

	if (m_->hostname) free (m_->hostname);
	m_->hostname = strdup (hostname);
	if (m_->ppk_path) free (m_->ppk_path);
	m_->ppk_path = strdup (ppk_path);
	m_->port     = port;
}

static bool _connect (void *this_, u16 ws_col, u16 ws_row)
{
ssh2_s *m_;
	m_ = (ssh2_s *)this_;
BUG(m_)

	if (! (0 < m_->port || m_->hostname || m_->ppk_path))
		return false;
	m_->ws_col = ws_col;
	m_->ws_row = ws_row;
pthread_t term_id;
	if (! (0 == pthread_create (&term_id, NULL, _thread, (void *)m_)))
		return false;
	return true;
}
static int _write (void *this_, const void *buf, unsigned cb)
{
ssh2_s *m_;
	m_ = (ssh2_s *)this_;
BUG(m_ && buf && 0 < cb)

int retval;
lock_lock (&m_->kbd_lock);
	retval = ringbuf_write (&m_->kbd_buf, buf, cb);
lock_unlock (&m_->kbd_lock);
	return retval;
}
static int _read (void *this_, void *buf, unsigned cb)
{
ssh2_s *m_;
	m_ = (ssh2_s *)this_;
BUG(m_ && buf && 0 < cb)
int retval;
lock_lock (&m_->echo_lock);
	if (0 == (retval = ringbuf_read (&m_->echo_buf, buf, cb))) {
		retval = -1, errno = EWOULDBLOCK;
	}
lock_unlock (&m_->echo_lock);
	return retval;
}

static void _dtor (void *this_)
{
ssh2_s *m_;
	m_ = (ssh2_s *)this_;

	if (m_->hostname) free (m_->hostname);
	if (m_->ppk_path) free (m_->ppk_path);

lock_lock (&m_->auth_subsystem_lock);
	queue_write_u8 (&m_->auth_subsystem, INPUT_CLOSE);
	queue_set_boundary (&m_->auth_subsystem);
lock_unlock (&m_->auth_subsystem_lock);
	pthread_join (m_->auth_subsystem_id, NULL);
	queue_dtor (&m_->auth_subsystem);
lock_dtor (&m_->auth_subsystem_lock);

	queue_dtor (&m_->tcp_out);
	queue_dtor (&m_->msg_out);

	queue_dtor (&m_->pkt_in);
	ringbuf_dtor (&m_->tcp_in);
	free (m_->tcp_in_);

lock_lock (&m_->kbd_lock);
	ringbuf_dtor (&m_->kbd_buf);
	free (m_->kbd_buf_);
lock_unlock (&m_->kbd_lock);
lock_dtor (&m_->kbd_lock);

lock_lock (&m_->echo_lock);
	ringbuf_dtor (&m_->echo_buf);
	free (m_->echo_buf_);
lock_unlock (&m_->echo_lock);
lock_dtor (&m_->echo_lock);
	memset (m_, 0, sizeof(ssh2_s));
}

static struct conn_i ssh2;
struct conn_i *_ctor (struct ssh2 *this_, unsigned cb)
{
ssh2_s *m_;
	m_ = (ssh2_s *)this_;
BUG(m_ && 0 < cb)

#ifndef __cplusplus // for C++ convert tool
BUGc(sizeof(ssh2_s) <= cb, " %d <= " VTRR "%d" VTO, usizeof(ssh2_s), cb)
	memset (this_, 0, sizeof(ssh2_s));
#else
// thread parameter
	m_->hostname = m_->ppk_path = NULL;
// authentication
	m_->p_ = m_->g_ = m_->x_ = m_->e_ = NULL;
	m_->login_name[0] = '\0';
#endif //ndef __cplusplus

	m_->recv_window_max = DEFAULT_WINDOW_SIZE;
	m_->recv_window_used = 0;
	// heap request
bool is_error;
	is_error = true;
	do {
// communication
		if (NULL == (m_->tcp_in_ = malloc (m_->recv_window_max +1)))
			break;
// console input / output
		if (NULL == (m_->kbd_buf_ = malloc (KBD_MAX)))
			break;
		if (NULL == (m_->echo_buf_ = malloc (ECHO_MAX)))
			break;
		is_error = false;
	} while (0);
	if (is_error) {
		_dtor (this_);
		return NULL;
	}
	// 
	random_init ();
	ringbuf_ctor (&m_->kbd_buf, usizeof(m_->kbd_buf), m_->kbd_buf_, KBD_MAX, SECURITY_ERASE);
	ringbuf_ctor (&m_->echo_buf, usizeof(m_->echo_buf), m_->echo_buf_, ECHO_MAX, SECURITY_ERASE);
lock_ctor (&m_->kbd_lock, usizeof(m_->kbd_lock));
lock_ctor (&m_->echo_lock, usizeof(m_->echo_lock));
	ringbuf_ctor (&m_->tcp_in, usizeof(m_->tcp_in), m_->tcp_in_, m_->recv_window_max +1, 0);
	queue_ctor (&m_->pkt_in, usizeof(m_->pkt_in), 16, 16, SECURITY_ERASE);
	queue_ctor (&m_->msg_out, usizeof(m_->msg_out), 16, 16, SECURITY_ERASE);
	queue_ctor (&m_->tcp_out, usizeof(m_->tcp_out), 16, 16, 0);
lock_ctor (&m_->auth_subsystem_lock, usizeof(m_->auth_subsystem_lock));
	queue_ctor (&m_->auth_subsystem, usizeof(m_->auth_subsystem), 16, 16, SECURITY_ERASE);
	return &ssh2;
}

static struct conn_i ssh2 = {
	_dtor,
	_connect,
	_write,
	_read,
};
