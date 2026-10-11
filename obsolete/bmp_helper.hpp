#ifndef BMP_HELPER_H_INCLUDED__
#define BMP_HELPER_H_INCLUDED__

#include "iface/img_helper.hpp"

struct bmp_helper { // now all non-state.
#ifdef __x86_64__
	uint8_t dummy[8]; // gcc's value to x86_64
#else //def __i386__
	uint8_t dummy[4]; // gcc's value to i386
#endif
};
void bmp_helper_ctor (struct bmp_helper *this_, unsigned cb);
struct img_helper_i *bmp_helper_query_img_helper_i (struct bmp_helper *this_);

#define sizeofBITMAPFILEHEADER 0x0E
#define sizeofBITMAPINFOHEADER 0x28
#define sizeofBMPHDR (sizeofBITMAPFILEHEADER + sizeofBITMAPINFOHEADER)

#endif // BMP_HELPER_H_INCLUDED__
