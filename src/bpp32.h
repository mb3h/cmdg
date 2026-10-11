#ifndef BPP32_H_INCLUDED__
#define BPP32_H_INCLUDED__

typedef struct {
#ifdef __i386__
//	uint8_t priv[];
#else //def __x86_64__
	uint8_t priv[18+6];
#endif
} bpp21;

void bpp21_resize (bpp21 *this_,
                   /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                   /*u21*/ uint32_t pad_val);
/*u21*/ uint32_t bpp21_get (bpp21 *this_,
                            /*u15*/ uint16_t x, /*u17*/ uint32_t y);
void bpp21_set (bpp21 *this_,
                /*u15*/ uint16_t x, /*u17*/ uint32_t y,
                /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                /*u21*/ uint32_t val);
void bpp21_copy (bpp21 *this_,
                 /*u15*/ uint16_t x, /*u17*/ uint32_t y,
                 /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                 /*u15*/ uint16_t src_x, /*u17*/ uint32_t src_y);
void bpp21_read (bpp21 *this_,
                 /*u15*/ uint16_t x, /*u17*/ uint32_t y,
                 /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                 void *dst_);
void bpp21_write (bpp21 *this_,
                  /*u15*/ uint16_t x, /*u17*/ uint32_t y,
                  /*u15*/ uint16_t width, /*u17*/ uint32_t height,
                  const void *src_);

#endif // BPP32_H_INCLUDED__
