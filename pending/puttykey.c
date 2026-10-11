#include <stdint.h>
#include "puttykey.h"

#include "sha1.hpp"
typedef struct sha1 sha1_s;
typedef struct sha1_hmac sha1_hmac_s;
#include "aes.hpp"
typedef struct aes aes_s;
#include "base64.h"
#include "cipher_helper.h"
#include "portab.h"

#include "vt100.h" // TODO: removing
#include "assert.h"

#include <stdio.h> // fopen feof ...
#include <stdlib.h> // atoi
#include <string.h>
#include <memory.h>
#include <stdbool.h>
#include <alloca.h>

typedef uint32_t u32;
typedef  uint8_t u8;
#define min(a, b) ((a) < (b) ? (a) : (b))
#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.
#define ustrlen(x) (unsigned)strlen(x) // part of 64bit supports.

#define SHA1_LEN               20
#define PPK_PRIVATE_MAC_TAG    "Private-MAC"
#define PPK_PRIVATE_MAC_PREFIX "putty-private-key-file-mac-key"
///////////////////////////////////////////////////////////////////////////////
// import extend

static void sha1_hmac_update_ssh_string (sha1_hmac_s *ctx, const void *data, unsigned len)
{
u8 tmp[4];
	store_be32 (tmp, len);
	sha1_hmac_update (ctx, tmp, usizeof(tmp));
	sha1_hmac_update (ctx, data, len);
}

///////////////////////////////////////////////////////////////////////////////
// readablity / portability

#define E_FILE_CANNOT_OPEN            PPK_E_FILE_CANNOT_OPEN
#define E_AUTHTYPE_ENTRY_NOT_FOUND    PPK_E_AUTHTYPE_ENTRY_NOT_FOUND
#define E_ENCRYPTION_ENTRY_NOT_FOUND  PPK_E_ENCRYPTION_ENTRY_NOT_FOUND
#define E_COMMENT_ENTRY_NOT_FOUND     PPK_E_COMMENT_ENTRY_NOT_FOUND
#define E_PUBLIC_ENTRY_NOT_FOUND      PPK_E_PUBLIC_ENTRY_NOT_FOUND
#define E_PRIVATE_ENTRY_NOT_FOUND     PPK_E_PRIVATE_ENTRY_NOT_FOUND
#define E_PRIVATE_MAC_ENTRY_NOT_FOUND PPK_E_PRIVATE_MAC_ENTRY_NOT_FOUND
#define E_PASSPHRASE_TOO_LONG         PPK_E_PASSPHRASE_TOO_LONG
#define E_UNEXPECTED_ENCRIPTION       PPK_E_UNEXPECTED_ENCRIPTION
#define E_MISMATCH_PRIVATE_MAC        PPK_E_MISMATCH_PRIVATE_MAC

#define PUBLIC_TAG      PPK_PUBLIC_TAG
#define PRIVATE_TAG     PPK_PRIVATE_TAG
#define AUTHTYPE_TAG    "PuTTY-User-Key-File-2"
#define ENCRYPTION_TAG  "Encryption"
#define COMMENT_TAG     "Comment"
#define PRIVATE_MAC_TAG PPK_PRIVATE_MAC_TAG

///////////////////////////////////////////////////////////////////////////////
// public

int ppk_errno = 0;

///////////////////////////////////////////////////////////////////////////////
// readablity / portability

#define _read_private_blob ppk_read_private_blob
#define _parse_public_blob ppk_parse_public_blob

///////////////////////////////////////////////////////////////////////////////
// utility

static unsigned hex2bin (const char *s, void *dst_, unsigned len)
{
BUG(s && dst_ && 0 < len)
u8 *dst, *end;
	dst = (u8 *)dst_, end = dst +len;

u8 val; unsigned i;
	for (val = 0x00, i = 0; ! (dst < end && '\0' == s[i]); ++i) {
int c
		= s[i];
		c = ('0' <= c && c <= '9') ? c - '0' :
		    ('A' <= c && c <= 'F') ? c - 'A' +10 :
		    ('a' <= c && c <= 'f') ? c - 'a' +10 : -1;
		if (-1 == c)
			break;
		val = val << 4 | (u8)(unsigned)c;
		if (0 == i % 2)
			continue;
		*dst++ = val, val = 0x00;
	}
	if (1 == i % 2)
		*dst <<= 4;

	return i;
}

///////////////////////////////////////////////////////////////////////////////
// .ppk file operation

// NOTE: (*1)do right-trim into entried value.
static bool _find_entry (FILE *stream, const char *prefix, char *s, unsigned size)
{
BUG(! ('\0' == *prefix || size < strlen (prefix)))
	if ('\0' == *prefix || size < strlen (prefix))
		return 0; // invalid argument
unsigned prefix_len
	= ustrlen (prefix);

	while (!feof (stream)) {
		*s = '\0';
		if (!fgets (s, size -1, stream))
			continue; // error or EOF
		s[size -1] = '\0';
char *tail;
		tail = strchr (s, '\0');
		while (s < tail && (u8)tail[-1] < 0x20)
			*--tail = '\0';
		if (0 == memcmp (prefix, s, prefix_len)) {
			memmove (s, s +prefix_len, tail +1 -(s +prefix_len)); // except for prefix
			return true; // found
		}
	}
	return false; // not found (= EOF)
}

static unsigned _read_blob (FILE *fp, void *dst_, unsigned size, const char *prefix)
{
BUG(fp && (NULL == dst_ || dst_ && 0 < size) && prefix && !('\0' == *prefix))

	fseek (fp, 0, SEEK_SET);
char text[128];
	if (!_find_entry (fp, prefix, text, usizeof(text)))
		return 0; // not found
unsigned retval; u8 *dst;
	retval = 0, dst = (u8 *)dst_;

int parsed, lines;
	parsed = 0, lines = atoi (text);
	while (parsed < lines) {
		if (feof (fp))
			break;
		*text = '\0';
		if (!fgets (text, sizeof(text) -1, fp))
			continue; // error or EOF
		text[sizeof(text) -1] = '\0';
char *tail;
		tail = strchr (text, '\0');
		while (text < tail && (u8)tail[-1] < 0x20)
			*--tail = '\0';

unsigned len;
		len = base64_decode (text, NULL, 0);
		if (dst) {
BUG(retval < size)
			len = base64_decode (text, dst +retval, size - retval);
			if (! (retval +len < size)) {
				retval += len;
				break;
			}
		}
		retval += len;
		++parsed;
		if (text < tail && '=' == tail[-1])
			break;
	}
	return retval;
}

static unsigned _get_cipher_type (FILE *fp)
{
BUG(fp)
unsigned retval = 0;
	do {
		fseek (fp, 0, SEEK_SET);
char text[128];
		if (!_find_entry (fp, ENCRYPTION_TAG ": ", text, usizeof(text))) {
			ppk_errno = E_ENCRYPTION_ENTRY_NOT_FOUND;
			break;
		}
		if (0 == strcmp ("aes256-cbc", text))
			retval = CIPHER_TYPE_AES256_CBC;
		else // PENDING: not 'aes256-cbc' cryption on private-key file
			ppk_errno = E_UNEXPECTED_ENCRIPTION;
	} while (0);

	return retval;
}

static bool _decrypt_private_blob (unsigned encrypt_type, const char *passphrase, void *encrypted, unsigned cb)
{
BUG(encrypted && passphrase && 0 < cb)
u8 seed[4 + PPK_PASSPHRASE_MAX]; // TODO: expand without stack-frame crushing
unsigned len;
	if (sizeof(seed) < 4 + (len = ustrlen (passphrase))) {
		ppk_errno = E_PASSPHRASE_TOO_LONG;
		return false;
	}
	memcpy (seed +4, passphrase, len);
	store_be32 (seed, 0);
u8 key320[40];
	sha1 (seed, 4 +len, key320);
	store_be32 (seed, 1);
	sha1 (seed, 4 +len, key320 +20);
	memset (seed, 0, sizeof(seed)); // security erase
	switch (encrypt_type) {
	case CIPHER_TYPE_AES256_CBC:
	{
aes_s ctx;
		aes_init (&ctx, usizeof(ctx));
		aes_setkey (&ctx, key320, 256);
		memset (key320, 0, sizeof(key320)); // security erase
	void *plain; u8 iv[16];
		plain = encrypted;
		memset (iv, 0, sizeof(iv));
		aes_setiv (&ctx, iv);
		aes_cbc_decrypt (&ctx, encrypted, cb, plain);
		memset (&ctx, 0, sizeof(ctx)); // security erase
		break;
	}
	default:
		ppk_errno = E_UNEXPECTED_ENCRIPTION;
		return false; // PENDING: not 'aes256-cbc' cryption on private-key file
	}
	return true;
}

static struct {
	const char *name;
	int errno_;
} list[] = {
	  {   AUTHTYPE_TAG ": ", E_AUTHTYPE_ENTRY_NOT_FOUND   }
	, { ENCRYPTION_TAG ": ", E_ENCRYPTION_ENTRY_NOT_FOUND }
	, {    COMMENT_TAG ": ", E_COMMENT_ENTRY_NOT_FOUND    }
	, { NULL, 0 }
};

static bool _calc_decrypting_checksum (
  FILE *fp
, const char *passphrase
, const u8 *pubkey, unsigned pubkey_len
, const u8 *privkey, unsigned privkey_len
,       u8 *mac, unsigned mac_len
)
{
BUG(fp && passphrase && mac && 0 < mac_len)
static const char header[]
	= PPK_PRIVATE_MAC_PREFIX;
sha1_s sha1;
	sha1_init (&sha1, usizeof(sha1));
	sha1_update (&sha1, header, usizeof(header) -1);
	sha1_update (&sha1, passphrase, ustrlen (passphrase));
u8 mackey[SHA1_LEN];
	sha1_finish (&sha1, mackey);
	memset (&sha1, 0, usizeof(sha1)); // security erase
sha1_hmac_s hmac;
	sha1_hmac_init (&hmac, usizeof(hmac), mackey, usizeof(mackey));
	memset (&mackey, 0, usizeof(mackey)); // security erase

	fseek (fp, 0, SEEK_SET);

char text[128]; int i;
	for (i = 0; list[i].name; ++i) {
//		fseek (fp, 0, SEEK_SET);
		if (!_find_entry (fp, list[i].name, text, usizeof(text)))
			break;
		sha1_hmac_update_ssh_string (&hmac, text, ustrlen (text));
	}
	if (0 == list[i].errno_) {
		sha1_hmac_update_ssh_string (&hmac, pubkey, pubkey_len);
		sha1_hmac_update_ssh_string (&hmac, privkey, privkey_len);
		sha1_hmac_finish (&hmac, mac);
	}
	memset (&hmac, 0, usizeof(hmac)); // security erase

	if (list[i].errno_) {
		ppk_errno = list[i].errno_;
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// public/private blob (user authentication key)

bool _parse_public_blob (
	  const void *blob_, unsigned blob_len
	, userkey_s *retval
)
{
BUG(blob_ && 0 < blob_len && retval)
const u8 *blob, *blob_end;
	blob = (const u8 *)blob_, blob_end = blob +blob_len;

const u8 *blob_type, *userexp, *usermod;
unsigned blob_type_len, userexp_len, usermod_len;
	if (blob_end < (blob_type = blob +4)
	 || blob_end < blob_type +(blob_type_len = load_be32 (blob_type -4))
	 || !(7 == blob_type_len)
	 || !(0 == memcmp ("ssh-rsa", blob_type, 7)) // 'ssh-rsa' PENDING: other support
	 || blob_end < (userexp = blob_type +blob_type_len +4)
	 || blob_end < userexp +(userexp_len = load_be32 (userexp -4))
	 || blob_end < (usermod = userexp +userexp_len +4)
	 || blob_end < usermod +(usermod_len = load_be32 (usermod -4))
	)
		return false;

	retval->usermod = (unsigned)(usermod -blob); retval->usermod_len =  usermod_len;
	retval->userexp = (unsigned)(userexp -blob); retval->userexp_len =  userexp_len;
	return true;
}

typedef struct {
	unsigned userexp; unsigned userexp_len; // inverse of pub_exp mod (p -1)(q -1)
	unsigned p; unsigned p_len;             // prime number (p > q, modulus = p * q)
	unsigned q; unsigned q_len;             // prime number
	unsigned iqmp; unsigned iqmp_len;       // inverse of q mod p
} priv_userkey_s;
static bool _parse_private_blob (
	  const void *blob_, unsigned blob_len
	, priv_userkey_s *retval
)
{
BUG(blob_ && 0 < blob_len && retval)
const u8 *blob, *blob_end;
	blob = (const u8 *)blob_, blob_end = blob +blob_len;

const u8 *userexp, *p, *q, *iqmp;
unsigned userexp_len, p_len, q_len, iqmp_len;
	if (blob_end < (userexp = blob_ +4)
	 || blob_end < userexp +(userexp_len = load_be32 (userexp -4))
	 || blob_end < (p = userexp +userexp_len +4)
	 || blob_end < p +(p_len = load_be32 (p -4))
	 || blob_end < (q = p +p_len +4)
	 || blob_end < q +(q_len = load_be32 (q -4))
	 || blob_end < (iqmp = q +q_len +4)
	 || blob_end < iqmp +(iqmp_len = load_be32 (iqmp -4))
	)
		return false;

	retval->userexp = (unsigned)(userexp -blob); retval->userexp_len =  userexp_len;
	retval->p = (unsigned)(p -blob); retval->p_len =  p_len;
	retval->q = (unsigned)(q -blob); retval->q_len =  q_len;
	retval->iqmp = (unsigned)(iqmp -blob); retval->iqmp_len =  iqmp_len;
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// public

#if 0 // PENDING: should notify warning here ?
static unsigned USERKEY_MAX
	= 4 + AUTH_ALGORYTHM_NAME_MAX
	+ 4 + ALIGNb(8, 1 +PUBLIC_E_MAX) / 8               // mpint: pub_exp
	+ 4 + ALIGNb(8, 1 +PUBLIC_N_MAX) / 8               // mpint: modulus
	;
static unsigned PRIV_USEREXP_MAX
	= 4 + ALIGNb(8, 1 +SECRET_P_MAX +SECRET_Q_MAX) / 8 // mpint: inverse of pub_exp mod (p -1)(q -1)
	;
static unsigned PRIV_USERKEY_MAX
	= 4 + ALIGNb(8, 1 +SECRET_P_MAX +SECRET_Q_MAX) / 8 // mpint: inverse of pub_exp mod (p -1)(q -1)
	+ 4 + ALIGNb(8, 1 +SECRET_P_MAX) / 8               // mpint: prime number (p > q, modulus = p * q)
	+ 4 + ALIGNb(8, 1 +SECRET_Q_MAX) / 8               // mpint: prime number
	+ 4 + ALIGNb(8, 1 +SECRET_P_MAX) / 8               // mpint: inverse of q mod p
	;
#endif

unsigned _read_public_blob (FILE *fp, void *dst, unsigned size)
{
unsigned retval = 0;
	do {
unsigned userkey_len;
		if (0 == (userkey_len = _read_blob (fp, dst, size, PPK_PUBLIC_TAG ": "))) {
			ppk_errno = E_PUBLIC_ENTRY_NOT_FOUND;
			break;
		}
		retval = userkey_len;
	} while (0);

	return retval;
}

unsigned _read_private_blob (
  const char *ppk_path
, const char *passphrase
, void *priv_userexp, unsigned priv_userexp_max
)
{
unsigned retval; FILE *fp; unsigned ppk_priv_len; u8 *ppk_priv;
	retval = 0, fp = NULL, ppk_priv_len = 0, ppk_priv = NULL;
	do {
		if (NULL == (fp = fopen (ppk_path, "r"))) {
			ppk_errno = E_FILE_CANNOT_OPEN;
			break;
		}
		if (0 == (ppk_priv_len = _read_blob (fp, NULL, 0, PPK_PRIVATE_TAG ": "))) {
			ppk_errno = E_PRIVATE_ENTRY_NOT_FOUND;
			break;
		}
		if (! (passphrase && priv_userexp && 0 < priv_userexp_max)) {
			retval = ppk_priv_len;
			break;
		}

		alloca_chk(ppk_priv, = (u8 *), ppk_priv_len, );
		if (0 == (ppk_priv_len = _read_blob (fp, ppk_priv, ppk_priv_len, PPK_PRIVATE_TAG ": "))) {
			ppk_errno = E_PRIVATE_ENTRY_NOT_FOUND;
			break;
		}

unsigned cipher_type;
		if (0 == (cipher_type = _get_cipher_type (fp)))
			break;
		if (! (true == _decrypt_private_blob (cipher_type, passphrase, ppk_priv, ppk_priv_len)))
			break;

u8 *userkey; unsigned userkey_len;
		if (0 == (userkey_len = _read_public_blob (fp, NULL, 0)))
			break;
		alloca_chk(userkey, = (u8 *), userkey_len, );
		if (! (userkey_len == _read_public_blob (fp, userkey, userkey_len)))
			break;

u8 checking_mac[SHA1_LEN];
		if (! (true == _calc_decrypting_checksum (
			  fp
			, passphrase
			, userkey, userkey_len
			, ppk_priv, ppk_priv_len
			, checking_mac, usizeof(checking_mac)
		)))
			break;
char text[128];
		if (!_find_entry (fp, PRIVATE_MAC_TAG ": ", text, usizeof(text))) {
			ppk_errno = E_PRIVATE_MAC_ENTRY_NOT_FOUND;
			break;
		}
u8 correct_mac[SHA1_LEN];
		if (! (2 * SHA1_LEN == strspn (text, "0123456789abcdefABCDEF")
		    && 2 * SHA1_LEN == hex2bin (text, correct_mac, usizeof(correct_mac))
		    && 0 == memcmp (checking_mac, correct_mac, SHA1_LEN)
		)) {
			ppk_errno = PPK_E_MISMATCH_PRIVATE_MAC;
			break;
		}

priv_userkey_s r;
		if (!_parse_private_blob (ppk_priv, ppk_priv_len, &r))
			break;
		retval = min(r.userexp_len, priv_userexp_max);
		memcpy (priv_userexp, ppk_priv +r.userexp, retval);
	} while (0);

	if (ppk_priv && 0 < ppk_priv_len)
		memset (ppk_priv, 0, ppk_priv_len); // security erase
	if (fp)
		fclose (fp);

	return retval;
}

///////////////////////////////////////////////////////////////////////////////
// public (glue)

unsigned ppk_read_public_blob (const char *ppk_path, void *dst, unsigned size)
{
FILE *fp;
	if (NULL == (fp = fopen (ppk_path, "r"))) {
		ppk_errno = E_FILE_CANNOT_OPEN;
		return 0;
	}
unsigned retval
	= _read_public_blob (fp, dst, size);
	fclose (fp);
	return retval;
}
