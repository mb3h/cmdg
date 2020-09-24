#ifndef SHA1_H_INCLUDED__
#define SHA1_H_INCLUDED__

void sha1 (const void *input, size_t len, uint8_t output[20]);
void sha1_core (uint32_t digest[5], const uint8_t input[64]);
struct sha1 {
	uint8_t priv[128];
};
bool sha1_init (struct sha1 *ctx, size_t len);
void sha1_update (struct sha1 *ctx, const void *input, size_t len);
void sha1_finish (struct sha1 *ctx, uint8_t output[20]);

struct sha1_hmac {
	uint8_t priv[128];
};
bool sha1_hmac_init (struct sha1_hmac *ctx, size_t len, const void *key, size_t keylen);
void sha1_hmac_update (struct sha1_hmac *ctx, const void *input, size_t len);
void sha1_hmac_finish (struct sha1_hmac *ctx, uint8_t output[20]);

#endif // SHA1_H_INCLUDED__
