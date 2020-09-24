#ifndef AES_H_INCLUDED__
#define AES_H_INCLUDED__

struct aes {
	uint8_t priv[128];
};
bool aes_init (struct aes *ctx, size_t len);
bool aes_setkey (struct aes *ctx, const void *key, size_t bitlen);
void aes_setiv (struct aes *ctx, const uint8_t iv128[16]);
bool aes_cbc_decrypt (struct aes *ctx, const void *input,
                      size_t length, void *output);
#define aes_ctr_decrypt aes_ctr_crypt
#define aes_ctr_encrypt aes_ctr_crypt
bool aes_ctr_crypt (struct aes *ctx, const void *input,
                    size_t length, void *output);

#endif // AES_H_INCLUDED__
