#ifndef SHA2_H_IS_INCLUDED__
#define SHA2_H_IS_INCLUDED__

struct sha256 {
	uint8_t priv[128];
};
bool sha256_init (struct sha256 *ctx, size_t len);
void sha256_update (struct sha256 *ctx, const void *input, size_t len);
void sha256_finish (struct sha256 *ctx, uint8_t *output);

#endif //ndef SHA2_H_IS_INCLUDED__
