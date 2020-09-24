#ifndef INTV_H_IS_INCLUDED__
#define INTV_H_IS_INCLUDED__

struct intv {
	size_t bitused;
	uint8_t priv[4];
	uint32_t v[1];
};
void intv_security_erase (struct intv *this_);
void intv_dtor (struct intv *this_);
struct intv *intv_ctor (size_t bitmax);
bool intv_load_u8be (struct intv *this_, const void *src, size_t cb);
size_t intv_store_u8be (const struct intv *this_, void *dst);
int u8be_cmp (const void *lhs, size_t lhs_cb, const void *rhs, size_t rhs_cb);
bool vmodpow32 (struct intv *retval, uint32_t g, const struct intv *exp, const struct intv *mod, void (*bits_fn)(void *bits_arg), void *bits_arg, size_t bits_span);
bool vmodpow (struct intv *retval, struct intv *g, const struct intv *exp, const struct intv *mod, void (*bits_fn)(void *bits_arg), void *bits_arg, size_t bits_span);
bool intv_h_validate ();

#endif //def INTV_H_IS_INCLUDED__
