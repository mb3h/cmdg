#ifndef LSB0_BIT_HELPER_H_INCLUDED__
#define LSB0_BIT_HELPER_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

unsigned lsb0_bitrchr (
	  const void *s
	, int b
	, unsigned n
);
void lsb0_bitset (
	  void *s, unsigned ofs
	, int b
	, unsigned n
);
void lsb0_bitmove (
	  void *dst_, unsigned n_dst
	, const void *src_, unsigned n_src
	, unsigned n
);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // LSB0_BIT_HELPER_H_INCLUDED__
