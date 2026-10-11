#ifndef BPP16_H_INCLUDED__
#define BPP16_H_INCLUDED__

typedef struct {
#ifdef __i386__
//	uint8_t priv[];
#else //def __x86_64__
	uint8_t priv[18+6];
#endif
} bpp16;

void bpp16_resize (bpp16 *this_,
                   /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                   uint16_t pad_val);
uint16_t bpp16_get (bpp16 *this_,
                    /*u15*/ uint16_t x, /*u17*/ uint32_t y);
void bpp16_set (bpp16 *this_,
                /*u15*/ uint16_t x, /*u17*/ uint32_t y,
                /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                uint16_t val);
void bpp16_copy (bpp16 *this_,
                 /*u15*/ uint16_t x, /*u17*/ uint32_t y,
                 /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                 /*u15*/ uint16_t src_x, /*u17*/ uint32_t src_y);
void bpp16_read (bpp16 *this_,
                 /*u15*/ uint16_t x, /*u17*/ uint32_t y,
                 /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                 void *dst_);
void bpp16_write (bpp16 *this_,
                  /*u15*/ uint16_t x, /*u17*/ uint32_t y,
                  /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                  const void *src_);

#endif // BPP16_H_INCLUDED__
