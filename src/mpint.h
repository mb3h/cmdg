#ifndef MPINT_H_INCLUDED__
#define MPINT_H_INCLUDED__

// r = g ^ a mod p
unsigned mpint_modpow (
	  const void *g, unsigned g_len
	, const void *a, unsigned a_len
	, const void *p, unsigned p_len
	,       void *r, unsigned r_len
	, void (*bits_fn)(void *bits_arg)
	, void *bits_arg
	, unsigned bits_span
);

#endif //ndef MPINT_H_INCLUDED__
