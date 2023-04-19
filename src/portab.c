#include <endian.h>

#include <stdint.h>
#include "portab.h"

typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;

#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.

// .text
u32 load_be32 (const void *src)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
u32 val;
	val = *(const u32 *)src;
	return (u32)(0xff & val >> 24 | 0xff00 & val >> 8 | 0xff0000 & val << 8 | 0xff000000UL & val << 24);
#else
	return *(const u32 *)src;
#endif
}
u32 load_le32 (const void *src)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return *(const u32 *)src;
#else
u32 val;
	val = *(const u32 *)src;
	return (u32)(0xff & val >> 24 | 0xff00 & val >> 8 | 0xff0000 & val << 8 | 0xff000000UL & val << 24);
#endif
}
u16 load_le16 (const void *src)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
const u8 *p;
	return *(const u16 *)src;
#else
	p = (const u8 *)src;
	return p[0]<<8|p[1];
#endif
}

u32 load_le24 (const void *src)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return 0xffffff & *(const u32 *)src;
#else
const u8 *p;
	p = (const u8 *)src;
	return p[0]<<16|p[1]<<8|p[2];
#endif
}

unsigned store_be32 (void *dst, u32 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(u32 *)dst = (u32)(0xff & val >> 24 | 0xff00 & val >> 8 | 0xff0000 & val << 8 | 0xff000000UL & val << 24);
#else
	*(u32 *)dst = val;
#endif
	return usizeof(u32);
}
unsigned store_le32 (void *dst, u32 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(u32 *)dst = val;
#else
	*(u32 *)dst = (u32)(0xff & val >> 24 | 0xff00 & val >> 8 | 0xff0000 & val << 8 | 0xff000000UL & val << 24);
#endif
	return usizeof(u32);
}

unsigned store_be16 (void *dst, u16 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(u16 *)dst = (u16)(0xff & val >> 8 | 0xff00 & val << 8);
#else
	*(u16 *)dst = val;
#endif
	return usizeof(u16);
}
unsigned store_le16 (void *dst, u16 val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	*(u16 *)dst = val;
#else
	*(u16 *)dst = (u16)(0xff & val >> 8 | 0xff00 & val << 8);
#endif
	return usizeof(u16);
}

unsigned store_be24 (void *dst, u32 val)
{
u8 *p;
	p = (u8 *)dst;
	*p = (u8)(0xff & val >> 16);
	store_be16 (p +1, (u16)val);
	return 3;
}
unsigned store_le24 (void *dst, u32 val)
{
u8 *p;
	p = (u8 *)dst;
	store_le16 (p +0, (u16)val);
	*(p +2) = (u8)(0xff & val >> 16);
	return 3;
}
