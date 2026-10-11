/* TODO: right 1-bit coding and testing
   These are fake codes copied from "bpp8".
  (any 1-bit coding of memory layout, bit operation and stuff
   not written yet.) */
#ifndef BPP1_H_INCLUDED__
#define BPP1_H_INCLUDED__

typedef struct {
#ifdef __i386__
//	uint8_t priv[];
#else //def __x86_64__
	uint8_t priv[18+6];
#endif
} bpp1;

void bpp1_resize (bpp1 *this_,
                  /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                  /*u1*/ uint8_t pad_val);
/*u1*/ uint8_t bpp1_get (bpp1 *this_,
                         /*u15*/ uint16_t x, /*u17*/ uint32_t y);
void bpp1_set (bpp1 *this_,
               /*u15*/ uint16_t x, /*u17*/ uint32_t y,
               /*u15*/ uint16_t width, /*u17*/ uint32_t height,
               /*u1*/ uint8_t val);
void bpp1_copy (bpp1 *this_,
                /*u15*/ uint16_t x, /*u17*/ uint32_t y,
                /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                /*u15*/ uint16_t src_x, /*u17*/ uint32_t src_y);
void bpp1_read (bpp1 *this_,
                /*u15*/ uint16_t x, /*u17*/ uint32_t y,
                /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                void *dst_);
void bpp1_write (bpp1 *this_,
                 /*u15*/ uint16_t x, /*u17*/ uint32_t y,
                 /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                 const void *src_);

#endif // BPP1_H_INCLUDED__
