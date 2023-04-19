#ifndef INTV_HPP_INCLUDED__
#define INTV_HPP_INCLUDED__

struct intv {
	unsigned bitused;
	uint8_t priv[4];
	uint32_t v[1];
};
void intv_security_erase (struct intv *this_);
void intv_dtor (struct intv *this_);
struct intv *intv_ctor (unsigned bitmax);
_Bool intv_load_u8be (struct intv *this_, const void *src, unsigned cb);
unsigned intv_store_u8be (const struct intv *this_, void *dst);
int u8be_cmp (const void *lhs, unsigned lhs_cb, const void *rhs, unsigned rhs_cb);
_Bool vmodpow32 (struct intv *retval, uint32_t g, const struct intv *exp, const struct intv *mod, void (*bits_fn)(void *bits_arg), void *bits_arg, unsigned bits_span);
_Bool vmodpow (struct intv *retval, struct intv *g, const struct intv *exp, const struct intv *mod, void (*bits_fn)(void *bits_arg), void *bits_arg, unsigned bits_span);
_Bool intv_h_validate ();

#endif //def INTV_HPP_INCLUDED__
