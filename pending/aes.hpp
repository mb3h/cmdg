#ifndef AES_HPP_INCLUDED__
#define AES_HPP_INCLUDED__

struct aes {
	uint8_t priv[512];
};
void aes_init (struct aes *ctx, unsigned len);
_Bool aes_setkey (struct aes *ctx, const void *key, unsigned bitlen);
void aes_setiv (struct aes *ctx, const uint8_t iv128[16]);
_Bool aes_cbc_decrypt (struct aes *ctx, const void *input,
                      unsigned length, void *output);
#define aes_ctr_decrypt aes_ctr_crypt
#define aes_ctr_encrypt aes_ctr_crypt
_Bool aes_ctr_crypt (struct aes *ctx, const void *input,
                    unsigned length, void *output);

#endif // AES_HPP_INCLUDED__
