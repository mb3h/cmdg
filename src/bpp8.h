#ifndef BPP8_H_INCLUDED__
#define BPP8_H_INCLUDED__

typedef struct {
#ifdef __i386__
//	uint8_t priv[];
#else //def __x86_64__
	uint8_t priv[18+6];
#endif
} bpp8;

void bpp8_resize (bpp8 *this_,
                  /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                  uint8_t pad_val);
uint8_t bpp8_get (bpp8 *this_,
                  /*u15*/ uint16_t x, /*u17*/ uint32_t y);
void bpp8_set (bpp8 *this_,
               /*u15*/ uint16_t x, /*u17*/ uint32_t y,
               /*u15*/ uint16_t width, /*u17*/ uint32_t height,
               uint8_t val);
void bpp8_copy (bpp8 *this_,
                /*u15*/ uint16_t x, /*u17*/ uint32_t y,
                /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                /*u15*/ uint16_t src_x, /*u17*/ uint32_t src_y);
void bpp8_read (bpp8 *this_,
                /*u15*/ uint16_t x, /*u17*/ uint32_t y,
                /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                void *dst_);
void bpp8_write (bpp8 *this_,
                 /*u15*/ uint16_t x, /*u17*/ uint32_t y,
                 /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                 const void *src_);

#endif // BPP8_H_INCLUDED__
