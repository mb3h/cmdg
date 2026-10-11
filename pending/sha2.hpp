#ifndef SHA2_HPP_INCLUDED__
#define SHA2_HPP_INCLUDED__

struct sha256 {
	uint8_t priv[256];
};
void sha256_init (struct sha256 *ctx, unsigned len);
void sha256_update (struct sha256 *ctx, const void *input, unsigned len);
void sha256_finish (struct sha256 *ctx, uint8_t *output);

#endif //ndef SHA2_HPP_INCLUDED__
