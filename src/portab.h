#ifndef PORTAB_H_INCLUDED__
#define PORTAB_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

uint32_t load_be32 (const void *src);
uint32_t load_le32 (const void *src);
uint16_t load_le16 (const void *src);
uint32_t load_le24 (const void *src);
unsigned store_be32 (void *dst, uint32_t val);
unsigned store_le32 (void *dst, uint32_t val);
unsigned store_be16 (void *dst, uint16_t val);
unsigned store_le16 (void *dst, uint16_t val);
unsigned store_be24 (void *dst, uint32_t val);
unsigned store_le24 (void *dst, uint32_t val);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // PORTAB_H_INCLUDED__
