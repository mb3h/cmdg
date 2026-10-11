#ifndef PUTTYKEY_H_INCLUDED__
#define PUTTYKEY_H_INCLUDED__

#define PPK_PASSPHRASE_MAX 63
#define      PPK_PUBLIC_TAG "Public-Lines"
#define     PPK_PRIVATE_TAG "Private-Lines"
#define PPK_PRIVATE_MAC_TAG "Private-MAC"

#define PPK_E_FILE_CANNOT_OPEN            1
#define PPK_E_AUTHTYPE_ENTRY_NOT_FOUND    2
#define PPK_E_ENCRYPTION_ENTRY_NOT_FOUND  3
#define PPK_E_COMMENT_ENTRY_NOT_FOUND     4
#define PPK_E_PUBLIC_ENTRY_NOT_FOUND      5
#define PPK_E_PRIVATE_ENTRY_NOT_FOUND     6
#define PPK_E_PRIVATE_MAC_ENTRY_NOT_FOUND 7
#define PPK_E_PASSPHRASE_TOO_LONG         8
#define PPK_E_UNEXPECTED_ENCRIPTION       9
#define PPK_E_MISMATCH_PRIVATE_MAC        10

extern int ppk_errno;

unsigned ppk_read_public_blob (
  const char *ppk_path
, void *dst
, unsigned size
);
unsigned ppk_read_private_blob (
  const char *ppk_path
, const char *passphrase
, void *dst, unsigned size
);

typedef struct {
	unsigned userexp; unsigned userexp_len;
	unsigned usermod; unsigned usermod_len;
} userkey_s;
_Bool ppk_parse_public_blob (
	  const void *blob, unsigned blob_len
	, userkey_s *retval
);

#endif // PUTTYKEY_H_INCLUDED__
