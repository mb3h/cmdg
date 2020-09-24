#include <string.h>
#include <malloc.h>
#include <stdint.h>
#include <unistd.h>
#include "defs.h"

#include "vt100.h"
typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;
typedef     char s8;
typedef    short s16;

#define arraycountof(x) (sizeof(x)/sizeof((x)[0]))


static const u16 translate[] = {
  0x0002, 0x0202, 0x0202, 0x021a, 0x0202, 0x0202, 0x0202, 0x0202,
  0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0205, 0x0202, 0x0202,
  0x1b1c, 0x021d, 0x0202, 0x0202, 0x0202, 0x021e, 0x021f, 0x2021,
  0x0e11, 0x1213, 0x1415, 0x1617, 0x1819, 0x100f, 0x0202, 0x0202,
  0x2208, 0x0609, 0x0a26, 0x2728, 0x292a, 0x2b0b, 0x2c07, 0x2d2e,
  0x0c2f, 0x3031, 0x3233, 0x3435, 0x3637, 0x3803, 0x0204, 0x2324,
  0x0239, 0x3a3b, 0x3c3d, 0x3e3f, 0x4041, 0x4243, 0x440d, 0x4546,
  0x4748, 0x494a, 0x4b4c, 0x4d4e, 0x4f50, 0x5102, 0x0202, 0x2502,
  0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,
  0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,
  0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,
  0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,
  0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,
  0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,
  0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,
  0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,
  0x0102
};

static const u16 r[] = {
  0x0000, 0x5202, 0x5302, 0x5302, 0x5303, 0x5402, 0x5402, 0x5402,
  0x5402, 0x5402, 0x5402, 0x5402, 0x5500, 0x5501, 0x5601, 0x5601,
  0x5602, 0x5603, 0x5603, 0x5701, 0x5701, 0x5801, 0x5802, 0x5901,
  0x5901, 0x5a01, 0x5b01, 0x5b01, 0x5b01, 0x5b01, 0x5b01, 0x5b01,
  0x5b01, 0x5b01, 0x5b01, 0x5c04, 0x5d01, 0x5d02, 0x5e01, 0x5e01,
  0x5e01, 0x5e01, 0x5e01, 0x5e01, 0x5e01, 0x5e01, 0x5e01, 0x5e01,
  0x5e01, 0x5e01, 0x5e01, 0x5e01, 0x5e01, 0x5e01, 0x5f01, 0x5f01,
  0x5f01, 0x5f01, 0x5f01, 0x5f01, 0x5f01, 0x5f01, 0x5f01, 0x5f01,
  0x5f01, 0x5f01, 0x5f01, 0x5f01, 0x5f01, 0x5f01, 0x5f01, 0x5f01,
  0x5f01, 0x5f01, 0x5f01, 0x5f01, 0x5f01, 0x5f01, 0x5f01, 0x5f01,
  0x6001, 0x6001, 0x6001, 0x6001, 0x6001, 0x6001, 0x6001, 0x6001,
  0x6001, 0x6001, 0x6001, 0x6001, 0x6001, 0x6001, 0x6001, 0x6001,
  0x6001, 0x6001, 0x6001, 0x6001, 0x6001, 0x6001, 0x6001, 0x6001,
  0x6001, 0x6001
};

static const u16 defact[] = {
  0x000c, 0x0000, 0x0013, 0x1a1b, 0x1c1d, 0x1e1f, 0x2021, 0x2202,
  0x0000, 0x0d14, 0x1519, 0x0017, 0x1803, 0x0001, 0x1006, 0x0507,
  0x0809, 0x0a0b, 0x0000, 0x1600, 0x0413, 0x1114, 0x1237, 0x4236,
  0x3839, 0x4045, 0x5c2f, 0x2e26, 0x2728, 0x292a, 0x2b2c, 0x3032,
  0x3335, 0x3a3b, 0x3c3d, 0x3e3f, 0x4143, 0x4446, 0x4748, 0x494a,
  0x4b4c, 0x4d4e, 0x4f50, 0x5152, 0x5354, 0x5556, 0x5758, 0x595a,
  0x5b5d, 0x5e5f, 0x6061, 0x6263, 0x6465, 0x6667, 0x6869, 0x2d00,
  0x2431, 0x3423, 0x2500
};

static const u16 defgoto[] = {
  0x0005, 0x1011, 0x1213, 0x2c27, 0x1819, 0x1a70, 0x7172, 0x7300
};

static const u16 pact[] = {
  0x7ec5, 0xd121, 0x7567, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0xe6f5, 0x000b, 0x0000, 0xc300, 0x0000, 0xcd00, 0xd100, 0x0000,
  0x0000, 0x0000, 0xe5e5, 0x0022, 0x0000, 0x00d1, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x007f,
  0x0000, 0x0000, 0x0000
};

static const u16 pgoto[] = {
  0x0000, 0x0000, 0x0001, 0xfe15, 0x00d0, 0x0000, 0x7100, 0x0000
};

static const u16 tabchk[] = {
  0x2503, 0x000e, 0x240f, 0x0010, 0x0011, 0x1512, 0x1613, 0x1714,
  0x1815, 0x1916, 0x1a17, 0x1b18, 0x1c19, 0x1d1a, 0x3925, 0x3b26,
  0x2907, 0x3c07, 0x3d08, 0x3e09, 0x3f0a, 0x400b, 0x410c, 0x420d,
  0x430e, 0x240f, 0x4410, 0x4511, 0x1512, 0x1613, 0x1714, 0x1815,
  0x1916, 0x1a17, 0x1b18, 0x1c19, 0x1d1a, 0x7d28, 0x461c, 0x471d,
  0x481e, 0x491f, 0x4a20, 0x4b21, 0x4c22, 0x4d23, 0x4e24, 0x4f25,
  0x5026, 0x5127, 0x5228, 0x5329, 0x542a, 0x552b, 0x562c, 0x572d,
  0x582e, 0x592f, 0x5a30, 0x5b31, 0x5c32, 0x5d33, 0x5e34, 0x5f35,
  0x6036, 0x6137, 0x6238, 0x6339, 0x643a, 0x653b, 0x663c, 0x673d,
  0x683e, 0x693f, 0x6a40, 0x6b41, 0x6c42, 0x6d43, 0x6e44, 0x6f45,
  0x7046, 0x7147, 0x7248, 0x7349, 0x744a, 0x754b, 0x764c, 0x774d,
  0x784e, 0x794f, 0x7a50, 0x7b51, 0x7c52, 0x010e, 0x2a01, 0x0110,
  0x0111, 0x1512, 0x1613, 0x1714, 0x1815, 0x1916, 0x1a17, 0x1b18,
  0x1c19, 0x1d1a, 0x1004, 0x1105, 0x1206, 0x7d70, 0x3c07, 0x3d08,
  0x3e09, 0x3f0a, 0x400b, 0x410c, 0x420d, 0x430e, 0x240f, 0x4410,
  0x4511, 0x1512, 0x1613, 0x1714, 0x1815, 0x1916, 0x1a17, 0x1b18,
  0x1c19, 0x1d1a, 0x821b, 0x461c, 0x471d, 0x481e, 0x491f, 0x4a20,
  0x4b21, 0x4c22, 0x4d23, 0x4e24, 0x4f25, 0x5026, 0x5127, 0x5228,
  0x5329, 0x542a, 0x552b, 0x562c, 0x572d, 0x582e, 0x592f, 0x5a30,
  0x5b31, 0x5c32, 0x5d33, 0x5e34, 0x5f35, 0x6036, 0x6137, 0x6238,
  0x6339, 0x643a, 0x653b, 0x663c, 0x673d, 0x683e, 0x693f, 0x6a40,
  0x6b41, 0x6c42, 0x6d43, 0x6e44, 0x6f45, 0x7046, 0x7147, 0x7248,
  0x7349, 0x744a, 0x754b, 0x764c, 0x774d, 0x784e, 0x794f, 0x7a50,
  0x7b51, 0x7c52, 0x2302, 0x3610, 0x140f, 0x3708, 0x2306, 0x1512,
  0x1613, 0x1714, 0x1815, 0x1916, 0x1a17, 0x1b18, 0x1c19, 0x1d1a,
  0x240f, 0x8370, 0x0f00, 0x1512, 0x1613, 0x1714, 0x1815, 0x1916,
  0x1a17, 0x1b18, 0x1c19, 0x1d1a, 0x0f00, 0x2c07, 0x0f00, 0x2d09,
  0x2e0a, 0x2f0b, 0x300c, 0x310d, 0x380f, 0x2325, 0x2326, 0x1512,
  0x1613, 0x1714, 0x1815, 0x1916, 0x1a17, 0x1b18, 0x1c19, 0x1d1a,
  0x2202, 0x0f00, 0x0f00, 0x320e, 0x2b06, 0x3310, 0x3411
};

struct esc_seq_stack {
	int state;
	u16 value;
};
struct esc_seq_state {
	u16 cb;
	int result;
	int state;
	char *cstr;
	size_t cb_cstr;
	size_t i_stack;
	size_t nums_count;
	u16 nums[1];
};
void esc_seq_parse_free (void *state_)
{
struct esc_seq_state *m_;
	m_ = (struct esc_seq_state *)state_;
	if (m_->cstr) {
		free (m_->cstr);
		m_->cstr = NULL;
		m_->cb_cstr = 0;
	}
}
#define stack_bottom(m_) ((struct esc_seq_stack *)((u8 *)(m_) + (m_)->cb) -1)
#define UNDEF_VALUE 0
bool esc_seq_parse_init (void *state_, size_t size)
{
	if (size < sizeof(struct esc_seq_state) || 0xffff < size)
		return false;
	memset (state_, 0, sizeof(struct esc_seq_state));
struct esc_seq_state *m_;
	m_ = (struct esc_seq_state *)state_;
	m_->cb = (u16)size;
	if ((void *)stack_bottom(m_) < (void *)m_->nums)
		return false;
	stack_bottom(m_)->value = UNDEF_VALUE;
	return true;
}

#define U16AS8(a,i) (0xff & (a)[(i) / 2] >> ((i) % 2 ? 0 : 8))
#define HI8(u16) (0xff & (u16) >> 8)
#define LO8(u16) (0xff & (u16))
size_t esc_seq_parse (void *state_, const void *buf_, size_t len, IVt100Action *iface, void *param)
{
const char *buf;
	buf = (const char *)buf_;
struct esc_seq_state *m_;
	m_ = (struct esc_seq_state *)state_;
size_t used;
	if (4 == m_->result) {
		if ((used = iface->on_bmp (param, buf, len)) < len)
			m_->result = 1;
		return used;
	}
const char *begin, *end;
	begin = buf, end = buf +len;
int c;
	c = -2;
struct esc_seq_stack *stack;
	stack = stack_bottom(m_) - m_->i_stack;
int state, result;
	state = m_->state;
	result = m_->result;
	while (0 == result) {
		stack->state = state;
		if ((void *)(stack -1) < (void *)(m_->nums + m_->nums_count)) {
			result = 3; // not enough memory (stack overflow)
			continue;
		}
bool do_defact;
		do_defact = true;
int n;
		if (! (-23 == (n = U16AS8(pact, state) -23))) {
			if (-2 == c) {
				if (!(buf < end) || '\0' == *buf) // null cannot parse now, always treat end of input.
					break;
				c = *buf++;
			}
			n += U16AS8(translate, c);
			if (0 <= n && n < arraycountof(tabchk) && LO8(tabchk[n]) -1 == U16AS8(translate, c)) {
				if (0 == (n = HI8(tabchk[n]) -15) || -15 == n +1) {
					result = 2; // syntax error
					continue;
				}
				else if (27 == n) {
					result = 1; // accept.
					continue;
				}
				else if (0 < n) {
					c = -2;
					(--stack)->value = UNDEF_VALUE;
					state = n;
					continue;
				}
				n = -n;
				do_defact = false;
			}
		}
		if (do_defact && 0 == (n = U16AS8(defact, state))) {
			result = 2; // syntax error
			continue;
		}
int reduce_len;
		reduce_len = LO8(r[n]);
u16 value, num;
		value = (stack + reduce_len -1)->value;
u32 u;
int i;
		switch (n) {
		case 4:
			result = 4;
			iface->on_bmp (param, NULL, 0);
			used = 0;
			if (buf < end && (used = iface->on_bmp (param, buf, end - buf)) < end - buf)
				result = 1;
			buf += used;
			continue;
		case 2: case 3:
			result = 1;
			continue;
		case 6: case 8:
			num = (0 < m_->nums_count) ? m_->nums[0] : 1;
			//iface->on_cuX (param, num);
			break;
		case 5:
			num = (0 < m_->nums_count) ? m_->nums[0] : 1;
			iface->on_cuu (param, num);
			break;
		case 7:
			num = (0 < m_->nums_count) ? m_->nums[0] : 1;
			iface->on_cuf (param, num);
			break;
		case 9:
			num = (0 < m_->nums_count) ? m_->nums[0] : 0;
			iface->on_el (param, num);
			break;
		case 10:
			num = (0 < m_->nums_count) ? m_->nums[0] : 1;
			//iface->on_dch (param, num);
			break;
		case 11:
			//foreach(NN-list) SGR(n)
			for (i = 0; i < m_->nums_count; ++i) {
				switch (num = m_->nums[i]) {
				case 0:
					iface->on_none_sgr (param);
					break;
				case 1:
					iface->on_bold_sgr (param);
					break;
				case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
					iface->on_fgcl_sgr (param, num - 30);
					break;
				}
			}
			break;
		case 12:
			break;
		case 13:
			if ((void *)stack < (void *)(m_->nums + m_->nums_count +1)) {
				result = 3; // not enough memory ([<n>] store overflow)
				continue;
			}
			m_->nums[m_->nums_count++] = stack->value;
			break;
		case 16:
			if ((void *)stack < (void *)(m_->nums + m_->nums_count +2)) {
				result = 3; // not enough memory (0<n>[[;:]<n>...] store overflow)
				continue;
			}
			m_->nums[m_->nums_count++] = 0;
			m_->nums[m_->nums_count++] = stack->value;
			break;
		case 14: case 15: case 17: case 18:
			if ((void *)stack < (void *)(m_->nums + m_->nums_count +1)) {
				result = 3; // not enough memory (<n>[[;:]<n>...] store overflow)
				continue;
			}
			m_->nums[m_->nums_count++] = stack->value;
			break;
		case 21:
			value = stack->value;
			break;
		case 22:
			if (0xffff < (u = (stack->value + 10 * stack[1].value))) {
				result = 2;
				break;
			}
			value = (u16)u;
			break;
		case 23: case 24:
			value = stack->value;
			break;
		case 25: case 26: case 27: case 28:
		case 29: case 30: case 31: case 32:
		case 33: case 34: case 19:
			value = buf[-1] - '0';
			break;
		case 36: case 37:
			if (NULL == m_->cstr) {
				if (NULL == (m_->cstr = (char *)malloc (m_->cb_cstr = 128))) {
					result = 3; // not enough memory (Pt malloc)
					continue;
				}
				*m_->cstr = '\0';
			}
size_t len;
			if (m_->cb_cstr < (len = strlen (m_->cstr)) +1 +1)
				if (NULL == (m_->cstr = (char *)realloc (m_->cstr, m_->cb_cstr = 2 * m_->cb_cstr))) {
					result = 3; // not enough memory (Pt malloc)
					continue;
				}
			m_->cstr[len] = buf[-1];
			m_->cstr[++len] = '\0';
			break;
		case 35:
			iface->on_title_chg (param, m_->cstr);
			free (m_->cstr);
			m_->cstr = NULL;
			break;
		default:
			break;
		}
		stack += reduce_len;
		(--stack)->value = value;
		n = HI8(r[n]);
		state = U16AS8(pgoto, n -82) -23 + stack[1].state;
		if (! (0 <= state && state < arraycountof(tabchk) && LO8(tabchk[state]) -1 == stack[1].state))
			state = U16AS8(defgoto, n -82) -1;
		else
			state = HI8(tabchk[state]) -15;
	}
	m_->i_stack = stack_bottom(m_) - stack;
	m_->state = state;
	m_->result = result;
	return buf - begin;
}


