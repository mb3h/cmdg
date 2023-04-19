
#include <stdint.h>
#define vt100_event_s void
#include "vt100_parser.hpp"

#include "vt100.h" // TODO: removing
#include "assert.h"

#include <stdbool.h>
#include <stdlib.h> // exit
#include <string.h>
#include <malloc.h>
typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;
typedef     char s8;
typedef    short s16;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.
#define arraycountof(x) (sizeof(x)/sizeof((x)[0]))


static const u16 translate[] = {
  0x0002, 0x0202, 0x0202, 0x0227, 0x0202, 0x0202, 0x0202, 0x0202,
  0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0205, 0x0202, 0x0202,
  0x2829, 0x022a, 0x0202, 0x0202, 0x0202, 0x022b, 0x022c, 0x2d2e,
  0x1d1e, 0x1f20, 0x2122, 0x2324, 0x2526, 0x1c1b, 0x0208, 0x0918,
  0x0a0b, 0x060c, 0x0d32, 0x3334, 0x0e35, 0x0f10, 0x1107, 0x3637,
  0x1238, 0x393a, 0x3b3c, 0x3d3e, 0x3f40, 0x4103, 0x0204, 0x2f30,
  0x0242, 0x4317, 0x4445, 0x4647, 0x1948, 0x494a, 0x1a13, 0x144b,
  0x4c4d, 0x154e, 0x164f, 0x5051, 0x5253, 0x5402, 0x0202, 0x3102,
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
  0x0000, 0x5502, 0x5602, 0x5602, 0x5603, 0x5601, 0x5701, 0x5701,
  0x5701, 0x5802, 0x5802, 0x5802, 0x5802, 0x5802, 0x5802, 0x5802,
  0x5802, 0x5802, 0x5802, 0x5802, 0x5802, 0x5802, 0x5802, 0x5802,
  0x5803, 0x5803, 0x5803, 0x5900, 0x5901, 0x5a01, 0x5a03, 0x5a03,
  0x5b01, 0x5b01, 0x5b01, 0x5b02, 0x5b02, 0x5b02, 0x5c01, 0x5d01,
  0x5d01, 0x5e01, 0x5e01, 0x5e01, 0x5e01, 0x5e01, 0x5e01, 0x5e01,
  0x5f01, 0x5f01, 0x6004, 0x6005, 0x6005, 0x6101, 0x6201, 0x6202,
  0x6301, 0x6301, 0x6301, 0x6401, 0x6401, 0x6401, 0x6401, 0x6401,
  0x6401, 0x6401, 0x6401, 0x6401, 0x6401, 0x6401, 0x6401, 0x6401,
  0x6401, 0x6401, 0x6401, 0x6501, 0x6501, 0x6501, 0x6501, 0x6501,
  0x6501, 0x6501, 0x6501, 0x6501, 0x6501, 0x6501, 0x6501, 0x6501,
  0x6501, 0x6501, 0x6501, 0x6501, 0x6501, 0x6501, 0x6501, 0x6501,
  0x6501, 0x6501, 0x6501, 0x6501, 0x6501, 0x6601, 0x6601, 0x6601,
  0x6601, 0x6601, 0x6601, 0x6601, 0x6601, 0x6601, 0x6601, 0x6601,
  0x6601, 0x6601, 0x6601, 0x6601, 0x6601, 0x6601, 0x6601, 0x6601,
  0x6601, 0x6601, 0x6601, 0x6601, 0x6601, 0x6601, 0x6601
};

static const u16 defact[] = {
  0x001b, 0x0000, 0x0607, 0x0800, 0x051b, 0x0026, 0x2728, 0x292a,
  0x2b2c, 0x2d2e, 0x2f02, 0x001c, 0x1d20, 0x2122, 0x2730, 0x3100,
  0x0300, 0x0100, 0x1c00, 0x0b12, 0x090a, 0x0c0d, 0x0e0f, 0x1011,
  0x1314, 0x1516, 0x1700, 0x0023, 0x2425, 0x0000, 0x0004, 0x1819,
  0x1a1e, 0x1f00, 0x004c, 0x5745, 0x4b4d, 0x4e52, 0x5455, 0x565a,
  0x7172, 0x7678, 0x676c, 0x7044, 0x433b, 0x3c3d, 0x3e3f, 0x4041,
  0x4748, 0x4a4f, 0x5051, 0x5358, 0x595b, 0x5c5d, 0x5e5f, 0x6061,
  0x6263, 0x6465, 0x6668, 0x696a, 0x6b6d, 0x6e6f, 0x7374, 0x7577,
  0x797a, 0x7b7c, 0x7d7e, 0x3839, 0x3a00, 0x4236, 0x4649, 0x3500,
  0x0032, 0x3733, 0x3400
};

static const u16 defgoto[] = {
  0x0008, 0x0916, 0x1718, 0x191a, 0x1b1c, 0x2021, 0x908a, 0x8b8c,
  0x8d8e
};

static const u16 pact[] = {
  0x0750, 0x3102, 0x9046, 0x0000, 0x0000, 0x004a, 0x0000, 0x4404,
  0x4000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0003, 0x2057, 0x0440, 0x0000, 0x0000, 0x03b0, 0x0000, 0x0020,
  0x0000, 0x6700, 0x0059, 0x0640, 0x4200, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x4404, 0x4000, 0x0000, 0x0006, 0x006a, 0x1080, 0x0000, 0x0000,
  0x0000, 0x4404, 0x406e, 0x06e0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xb900,
  0x0000, 0x0000, 0x0000, 0x0061, 0x0620, 0x0000, 0x0000, 0x0000
};

static const u16 pgoto[] = {
  0x0000, 0x0000, 0x0000, 0x07e0, 0x7804, 0xe033, 0x0350, 0x1e00,
  0x0000, 0x0470, 0x0000, 0x0001, 0x0000, 0x0000
};

static const u16 tabchk[] = {
  0x1d03, 0x3919, 0x1e03, 0x2607, 0x2708, 0x090a, 0x3c1c, 0x280b,
  0x290c, 0x2a0d, 0x2b0e, 0x2c0f, 0x2d10, 0x2e11, 0x2f12, 0x3013,
  0x3114, 0x0b1e, 0x1c1f, 0x0d20, 0x0a19, 0x2201, 0x3719, 0x2107,
  0x3819, 0x0b1e, 0x0c1f, 0x0d20, 0x0e21, 0x0f22, 0x1023, 0x1124,
  0x1225, 0x1326, 0x1427, 0x3a1e, 0x3b1f, 0x883d, 0x3f1a, 0x401b,
  0x351c, 0x361d, 0x3942, 0x3943, 0x0b1e, 0x0c1f, 0x0d20, 0x0e21,
  0x0f22, 0x1023, 0x1124, 0x1225, 0x1326, 0x1427, 0x3215, 0x3316,
  0x3417, 0x3d08, 0x863d, 0x3e18, 0x873d, 0x351c, 0x361d, 0x3742,
  0x3743, 0x3842, 0x3843, 0x0104, 0x0205, 0x0306, 0x431c, 0x0408,
  0x0509, 0x060a, 0x351c, 0x361d, 0x240a, 0x250b, 0x4136, 0x4237,
  0x441c, 0x8e19, 0x230a, 0x9328, 0x9428, 0x928a, 0x9045, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x888a, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x868a,
  0x0000, 0x878a, 0x4507, 0x4608, 0x0000, 0x0000, 0x470b, 0x480c,
  0x490d, 0x4a0e, 0x4b0f, 0x4c10, 0x4d11, 0x4e12, 0x4f13, 0x5014,
  0x5115, 0x5216, 0x5317, 0x5418, 0x0000, 0x551a, 0x561b, 0x571c,
  0x581d, 0x0b1e, 0x0c1f, 0x0d20, 0x0e21, 0x0f22, 0x1023, 0x1124,
  0x1225, 0x1326, 0x1427, 0x9128, 0x5929, 0x5a2a, 0x5b2b, 0x5c2c,
  0x5d2d, 0x5e2e, 0x5f2f, 0x6030, 0x6131, 0x6232, 0x6333, 0x6434,
  0x6535, 0x6636, 0x6737, 0x6838, 0x6939, 0x6a3a, 0x6b3b, 0x6c3c,
  0x6d3d, 0x6e3e, 0x6f3f, 0x7040, 0x7141, 0x7242, 0x7343, 0x7444,
  0x7545, 0x7646, 0x7747, 0x7848, 0x7949, 0x7a4a, 0x7b4b, 0x7c4c,
  0x7d4d, 0x7e4e, 0x7f4f, 0x8050, 0x8151, 0x8252, 0x8353, 0x8454,
  0x8555, 0x4507, 0x4608, 0x0000, 0x0000, 0x470b, 0x480c, 0x490d,
  0x4a0e, 0x4b0f, 0x4c10, 0x4d11, 0x4e12, 0x4f13, 0x5014, 0x5115,
  0x5216, 0x5317, 0x5418, 0x0000, 0x551a, 0x561b, 0x571c, 0x581d,
  0x0b1e, 0x0c1f, 0x0d20, 0x0e21, 0x0f22, 0x1023, 0x1124, 0x1225,
  0x1326, 0x1427, 0x0000, 0x5929, 0x5a2a, 0x5b2b, 0x5c2c, 0x5d2d,
  0x5e2e, 0x5f2f, 0x6030, 0x6131, 0x6232, 0x6333, 0x6434, 0x6535,
  0x6636, 0x6737, 0x6838, 0x6939, 0x6a3a, 0x6b3b, 0x6c3c, 0x6d3d,
  0x6e3e, 0x6f3f, 0x7040, 0x7141, 0x7242, 0x7343, 0x7444, 0x7545,
  0x7646, 0x7747, 0x7848, 0x7949, 0x7a4a, 0x7b4b, 0x7c4c, 0x7d4d,
  0x7e4e, 0x7f4f, 0x8050, 0x8151, 0x8252, 0x8353, 0x8454, 0x8555
};
#define OSC_RGB_QUERY 10000

struct csi_param {
	u16 val     :15;
	u16 prezero : 1;
};
struct statval {
	s16 state;
	u16 value   :15;
	u16 prezero : 1;
};
typedef struct vt100_parser_ {
	struct vt100_event_i *handler;
	vt100_event_s *handler_this_;
	char *cstr;
	u32 cb_cstr;
	u32 giveup_bytes;
	s16 state;
	s8 result;
	u8 padding_23[1];
#ifndef __cplusplus
	u16 cb;
#endif // __cplusplus
	u8 n_list_used;
	u8 sv_stack_used;
#ifndef __cplusplus
	struct csi_param n_list[1];
#else //def __cplusplus
	struct csi_param n_list[10];
	u8 padding_50[2];
	struct statval sv_stack[19]; // 19 = (128 -(4(dtor only vtbl) +offsetof(n_list) +sizeof(n_list))) / sizeof(struct statval)
#endif // __cplusplus
} vt100_parser_s;

#define UNDEF_VALUE 0

#ifndef __cplusplus
#define stack_is_nospc(m_, n, sv) (! ((void *)(m_->n_list + m_->n_list_used +(n)) <= (void *)(stack -(sv))))
#define   n_list_is_nospc(m_, n)  stack_is_nospc(m_, n, 0)
#define sv_stack_is_nospc(m_, sv) stack_is_nospc(m_, 0, sv)
#define sv_stack_get_bottom(m_) ((struct statval *)((u8 *)(m_) + (m_)->cb) -1)

#else //def __cplusplus
#define stack_is_nospc(m_, n, sv) (n_list_is_nospc (m_, n) || sv_stack_is_nospc (m_, sv))
#define   n_list_is_nospc(m_, n)  (! (m_->n_list_used +(n) <= arraycountof(m_->n_list)))
#define sv_stack_is_nospc(m_, sv) (! ((sv) +m_->sv_stack_used +1 <= arraycountof(m_->sv_stack)))
#define sv_stack_get_bottom(m_) (m_->sv_stack +arraycountof(m_->sv_stack) -1)

#endif // __cplusplus

#define n_list_last(m_) (&m_->n_list[m_->n_list_used -1])

static u32 bcd20_from_u16 (u16 d)
{
u32 retval;
	retval = 0;
u8 i;
	for (i = 0; i < 5; ++i) {
		retval >>= 4;
		retval |= d % 10 << 16;
		d /= 10;
	}
	return retval;
}
static u8 dispatch_sgr (vt100_parser_s *m_, u32 bcd20, u8 i_bcd, struct csi_param *aux, unsigned len)
{
u8 lead;
	lead = 15 & bcd20 >> i_bcd * 4;
	switch (lead) {
	case 0:
		if (m_->handler && m_->handler->csi_sgr_none)
			m_->handler->csi_sgr_none (m_->handler_this_);
		return 1;
	case 1:
		if (0 == i_bcd || 0 < (15 & bcd20 >> --i_bcd * 4)) {
			if (m_->handler && m_->handler->csi_sgr_bold)
				m_->handler->csi_sgr_bold (m_->handler_this_, true);
			return 1; // CSI '1[m;1-9]'
		}
		lead = 10;
//	case 10:
		if (0 == i_bcd || 107 < (lead = (15 & bcd20 >> (i_bcd -1) * 4) + 10 * lead))
			return 2 +i_bcd; // ERROR: CSI '10[;m]' or '10[89][;m]'
		if (m_->handler && m_->handler->csi_sgr_bgcl)
			m_->handler->csi_sgr_bgcl (m_->handler_this_, lead -100 +8);
		return 3; // CSI '10[0-7][;m]'
	case 2:
		if (i_bcd < 1)
			return 1 +i_bcd; // ERROR: CSI '2[;m]'
		--i_bcd;
		switch (lead = (15 & bcd20 >> i_bcd * 4) + 10 * lead) {
		case 22:
			if (m_->handler && m_->handler->csi_sgr_bold)
				m_->handler->csi_sgr_bold (m_->handler_this_, false);
			break; // CSI '22...[;m]'
		case 23:
			if (m_->handler && m_->handler->csi_sgr_italic)
				m_->handler->csi_sgr_italic (m_->handler_this_, false);
			break; // CSI '23...[;m]'
		case 27:
			if (m_->handler && m_->handler->csi_sgr_reverse)
				m_->handler->csi_sgr_reverse (m_->handler_this_, false);
			break; // CSI '27...[;m]'
		case 29:
			if (m_->handler && m_->handler->csi_sgr_strikethrough)
				m_->handler->csi_sgr_strikethrough (m_->handler_this_, false);
			break; // CSI '29...[;m]'
		case 24:
		case 25:
			break; // CSI '2[245]...[;m]'
		default:
			break; // ERROR: CSI '2[0168]...[;m]'
		}
		return 2;
	case 3:
		if (0 == i_bcd) {
			if (m_->handler && m_->handler->csi_sgr_italic)
				m_->handler->csi_sgr_italic (m_->handler_this_, true);
			return 1; // CSI '3[;m]'
		}
		--i_bcd;
		if (38 < (lead = (15 & bcd20 >> i_bcd * 4) + 10 * lead))
			return 2; // ERROR: CSI '39[;m]'
		if (lead < 38) {
			if (m_->handler && m_->handler->csi_sgr_fgcl)
				m_->handler->csi_sgr_fgcl (m_->handler_this_, lead -30);
			return 2; // CSI '3[0-7][;m]'
		}
		if (0 == i_bcd && 1 < len && 5 == aux[0].val) {
			if (m_->handler && m_->handler->csi_sgr_fgcl)
				m_->handler->csi_sgr_fgcl (m_->handler_this_, aux[1].val);
			return 2 +2; // CSI '38;5;<N>[;m]'
		}
		else if (0 == i_bcd && 3 < len && 2 == aux[0].val) {
			if (m_->handler && m_->handler->csi_sgr_fgcl_raw)
				m_->handler->csi_sgr_fgcl_raw (m_->handler_this_, aux[1].val << 16 | aux[2].val << 8 | aux[3].val);
			return 2 +4; // CSI '38;2[;:]<R>[;:]<G>[;:]<B>[;m]'
		}
ASSERTE(i_bcd <= 3)
		return 1 +i_bcd +min(2, len); // ERROR: CSI '38...m' but not '38;5;<N>[;m]'
	case 4:
		if (i_bcd < 1)
			return 1 +i_bcd; // ERROR: CSI '4[;m]'
		--i_bcd;
		if (48 < (lead = (15 & bcd20 >> i_bcd * 4) + 10 * lead))
			return 2; // ERROR: CSI '49[;m]'
		if (lead < 48) {
			if (m_->handler && m_->handler->csi_sgr_bgcl)
				m_->handler->csi_sgr_bgcl (m_->handler_this_, lead -40);
			return 2; // CSI '4[0-7][;m]'
		}
		if (0 == i_bcd && 1 < len && 5 == aux[0].val) {
			if (m_->handler && m_->handler->csi_sgr_bgcl)
				m_->handler->csi_sgr_bgcl (m_->handler_this_, aux[1].val);
			return 2 +2; // CSI '48;5;<N>[;m]'
		}
		else if (0 == i_bcd && 3 < len && 2 == aux[0].val) {
			if (m_->handler && m_->handler->csi_sgr_bgcl_raw)
				m_->handler->csi_sgr_bgcl_raw (m_->handler_this_, aux[1].val << 16 | aux[2].val << 8 | aux[3].val);
			return 2 +4; // CSI '48;2[;:]<R>[;:]<G>[;:]<B>[;m]'
		}
ASSERTE(i_bcd <= 3)
		return 1 +i_bcd +min(2, len); // ERROR: CSI '48...m' but not '48;5;<N>[;m]'
	case 5:
		return 1; // CSI '[57]...[;m]'
	case 7:
		if (m_->handler && m_->handler->csi_sgr_reverse)
			m_->handler->csi_sgr_reverse (m_->handler_this_, true);
		return 1; // CSI '7[m;]'
	case 9:
		if (0 == i_bcd) {
			if (m_->handler && m_->handler->csi_sgr_strikethrough)
				m_->handler->csi_sgr_strikethrough (m_->handler_this_, true);
			return 1; // CSI '9[;m]'
		}
		--i_bcd;
		if (97 < (lead = (15 & bcd20 >> i_bcd * 4) + 10 * lead))
			return 2; // ERROR: CSI '9[89][;m]'
		if (m_->handler && m_->handler->csi_sgr_fgcl)
			m_->handler->csi_sgr_fgcl (m_->handler_this_, lead -90 +8);
		return 2; // CSI '9[0-7][;m]'
	//case 6:
	//case 8:
	default:
		return 1 +i_bcd; // ERROR: CSI '[68]...[;m]'
	}
}

void vt100_parser_reset (struct vt100_parser *this_)
{
vt100_parser_s *m_;
	m_ = (vt100_parser_s *)this_;
	m_->result        = 0;
	m_->giveup_bytes  = 0;
	m_->state         = 0;
	m_->sv_stack_used = 0;
	m_->n_list_used   = 0;
#ifndef __cplusplus
struct statval *stack;
	stack = sv_stack_get_bottom(m_);
#endif // __cplusplus
ASSERTE(!stack_is_nospc (m_, 0, 0))
	sv_stack_get_bottom(m_)->value = UNDEF_VALUE;
}
void vt100_parser_release (struct vt100_parser *this_)
{
vt100_parser_s *m_;
	m_ = (vt100_parser_s *)this_;
	if (m_->cstr) {
		free (m_->cstr);
		m_->cstr = NULL, m_->cb_cstr = 0;
	}
}
#ifdef __cplusplus
void vt100_parser_dtor (struct vt100_parser *this_)
{
}
#endif // __cplusplus
void vt100_parser_ctor (struct vt100_parser *this_, unsigned cb, struct vt100_event_i *handler, vt100_event_s *handler_this_)
{
vt100_parser_s *m_;
	m_ = (vt100_parser_s *)this_;
#ifndef __cplusplus
BUGc(sizeof(vt100_parser_s) <= cb, " %d <= " VTRR "%d" VTO, usizeof(vt100_parser_s), cb)
BUG(cb <= 0xffff)
	m_->cb = (u16)cb;
#endif // __cplusplus
	m_->cstr = NULL, m_->cb_cstr = 0;
	m_->handler = handler;
	m_->handler_this_ = handler_this_;
	vt100_parser_reset (this_);
}
unsigned vt100_parser_get_resume (struct vt100_parser *this_)
{
vt100_parser_s *m_;
	m_ = (vt100_parser_s *)this_;
	return m_->giveup_bytes;
}

#define U16AS8(a,i) (0xff & (a)[(i) / 2] >> ((i) % 2 ? 0 : 8))
#define U16AS12(a,i) ((0 == (i) % 4) ?                                           (a)[((i) -0) * 3 / 4 +0] >> 4 : \
                      (1 == (i) % 4) ?   0xf00 & (a)[((i) -1) * 3 / 4 +0] << 8 | (a)[((i) -1) * 3 / 4 +1] >> 8 : \
                      (2 == (i) % 4) ?   0xff0 & (a)[((i) -2) * 3 / 4 +1] << 4 | (a)[((i) -2) * 3 / 4 +2] >> 12 : \
                    /*(3 == (i) % 4) ?*/ 0xfff & (a)[((i) -3) * 3 / 4 +2])
#define HI8(u16) (0xff & (u16) >> 8)
#define LO8(u16) (0xff & (u16))
// TODO: remove 'm_->result'
int vt100_parser_request (struct vt100_parser *this_, const void *buf_, unsigned len)
{
vt100_parser_s *m_;
	m_ = (vt100_parser_s *)this_;
const char *buf;
	buf = (const char *)buf_;
	m_->giveup_bytes = 0;
unsigned used;
	if (127 == m_->result) {
		if ((used = m_->handler->esc_bmp (m_->handler_this_, buf, len)) < len)
			m_->result = 1;
		return used;
	}
const char *begin, *end;
	begin = buf, end = buf +len;
int c;
	c = -2;
struct statval *stack;
	stack = sv_stack_get_bottom(m_) - m_->sv_stack_used;
s16 state;
s8 result;
	state = m_->state;
	result = m_->result;
	while (0 == result) {
		stack->state = state;
		if (sv_stack_is_nospc (m_, 1)) {
			result = -128; // not enough memory (stack overflow)
			continue;
		}
bool do_defact;
		do_defact = true;
int n;
		if (! (-53 == (n = U16AS12(pact, state) -53))) {
			if (-2 == c) {
				if (!(buf < end) || '\0' == *buf) { // null cannot parse now, always treat end of input.
					result = -1; // uncompleted (input ended before parsing complete)
					break;
				}
				c = (u8)*buf++;
			}
			n += U16AS8(translate, c);
			if (0 <= n && n < arraycountof(tabchk) && LO8(tabchk[n]) -1 == U16AS8(translate, c)) {
				if (0 == (n = HI8(tabchk[n]) +0) || +0 == n +1) {
					result = -2; // syntax error
					continue;
				}
				else if (34 == n) {
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
			result = -2; // syntax error
			continue;
		}
int reduce_len;
		reduce_len = LO8(r[n]);
bool prezero;
		prezero = (stack + reduce_len -1)->prezero ? true : false;
u16 value, num, x, y, tm, bm;
		value = (stack + reduce_len -1)->value;
u32 pos;
u32 u;
int i;
struct csi_param *sgr_p, *sgr_end;
		switch (n) {
		case 4:
			result = 127;
			m_->handler->esc_bmp (m_->handler_this_, NULL, 0);
			used = 0;
			if (buf < end && (used = m_->handler->esc_bmp (m_->handler_this_, buf, end - buf)) < end - buf)
				result = 1;
			buf += used;
			continue;
		case 6:
			if (m_->handler && m_->handler->esc_ri)
				m_->handler->esc_ri (m_->handler_this_);
			break;
		case 7:
			if (m_->handler && m_->handler->esc_dec_kpam)
				m_->handler->esc_dec_kpam (m_->handler_this_);
			break;
		case 8:
			if (m_->handler && m_->handler->esc_dec_kpnm)
				m_->handler->esc_dec_kpnm (m_->handler_this_);
			break;
		case 2: case 3: case 5:
			result = 1;
			continue;
		case 11: case 13:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 1;
			//if (iface && iface->csi_cuX)
			//	iface->csi_cuX (param, num);
			break;
		case 9:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 1;
			if (m_->handler && m_->handler->csi_ich)
				m_->handler->csi_ich (m_->handler_this_, num);
			break;
		case 10:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 1;
			if (m_->handler && m_->handler->csi_cuu)
				m_->handler->csi_cuu (m_->handler_this_, num);
			break;
		case 12:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 1;
			if (m_->handler && m_->handler->csi_cuf)
				m_->handler->csi_cuf (m_->handler_this_, num);
			break;
		case 14:
			y = (0 < m_->n_list_used) ? m_->n_list[0].val : 1;
			x = (1 < m_->n_list_used) ? m_->n_list[1].val : 1;
			if (m_->handler && m_->handler->csi_cup)
				m_->handler->csi_cup (m_->handler_this_, y, x);
			break;
		case 15:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 0;
			if (m_->handler && m_->handler->csi_ed)
				m_->handler->csi_ed (m_->handler_this_, num);
			break;
		case 16:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 0;
			if (m_->handler && m_->handler->csi_el)
				m_->handler->csi_el (m_->handler_this_, num);
			break;
		case 17:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 1;
			if (m_->handler && m_->handler->csi_il)
				m_->handler->csi_il (m_->handler_this_, num);
			break;
		case 18:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 1;
			if (m_->handler && m_->handler->csi_dl)
				m_->handler->csi_dl (m_->handler_this_, num);
			break;
		case 19:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 1;
			if (m_->handler && m_->handler->csi_dch)
				m_->handler->csi_dch (m_->handler_this_, num);
			break;
		case 20:
			if (0 == m_->n_list_used)
				m_->n_list[m_->n_list_used++].val = 0; // CSI 'm' = '0m'
			sgr_end = m_->n_list +m_->n_list_used;
u32 bcd20;
			for (sgr_p = m_->n_list, i = -1; sgr_p < sgr_end;) {
				if (i < 0) {
					bcd20 = bcd20_from_u16 (sgr_p->val);
					for (i = 4; 0 < i && 0 == (15 & bcd20 >> i * 4);)
						--i;
					if (sgr_p->prezero && 0 < (15 & bcd20 >> i * 4))
						++i;
				}
u8 done;
				if (! (i < (done = dispatch_sgr (m_, bcd20, (u8)i, sgr_p +1, sgr_end - (sgr_p +1)))))
					i -= done;
				else {
ASSERTE(sgr_p +(done -i) <= sgr_end)
					sgr_p += done -i, i = -1;
				}
			}
			break;
		case 21:
			if (m_->handler && m_->handler->csi_dsr)
				m_->handler->csi_dsr (m_->handler_this_, m_->n_list[0].val);
			break;
		case 22:
			tm = m_->n_list[0].val;
			bm = (1 < m_->n_list_used) ? m_->n_list[1].val : 1;
			if (m_->handler && m_->handler->csi_dec_stbm)
				m_->handler->csi_dec_stbm (m_->handler_this_, tm, bm);
			break;
		case 23: // TODO:
			if (m_->n_list_used < 3 || 255 < m_->n_list[0].val || 255 < m_->n_list[1].val || 255 < m_->n_list[2].val) {
			}
			if (m_->handler && m_->handler->csi_dec_slpp)
				m_->handler->csi_dec_slpp (m_->handler_this_, m_->n_list[0].val, m_->n_list[1].val, m_->n_list[2].val);
			break;
		case 24:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 0;
			if (m_->handler && m_->handler->csi_da2)
				m_->handler->csi_da2 (m_->handler_this_, num);
			break;
		case 25:
			for (i = 0; i < m_->n_list_used; ++i)
				if (m_->handler && m_->handler->csi_dec_exm)
					m_->handler->csi_dec_exm (m_->handler_this_, m_->n_list[i].val, true);
			break;
		case 26:
			for (i = 0; i < m_->n_list_used; ++i)
				if (m_->handler && m_->handler->csi_dec_exm)
					m_->handler->csi_dec_exm (m_->handler_this_, m_->n_list[i].val, false);
			break;
		case 29: case 30: case 31:
			if (n_list_is_nospc (m_, 1)) {
				result = -128; // not enough memory (<n>[;<n>...] store overflow)
				continue;
			}
			++m_->n_list_used;
			n_list_last(m_)->prezero = stack->prezero;
			n_list_last(m_)->val = stack->value;
			break;
		case 32:
		case 33: case 34:
			prezero = false;
			value = stack->value;
			break;
		case 36: case 37:
			if (0 == stack[1].value) {
				prezero = true;
				value = stack->value;
				break;
			}
		case 35:
			prezero = (stack[1].prezero) ? true : false;
			value = stack[1].value * 10 + stack->value;
			break;
		case 56: case 57: case 58:
			value = stack->value;
			break;
		case 48: case 49:
			value = stack->value;
			break;
		case 38: case 39: case 40: case 41:
		case 42: case 43: case 44: case 45:
		case 46: case 47:
			prezero = false;
			value = buf[-1] - '0';
			break;
		case 54: case 55:
			if (NULL == m_->cstr) {
				if (NULL == (m_->cstr = (char *)malloc (m_->cb_cstr = 128))) {
					result = -128; // not enough memory (Pt malloc)
					continue;
				}
				*m_->cstr = '\0';
			}
size_t len;
			if (m_->cb_cstr < (len = strlen (m_->cstr)) +1 +1)
				if (NULL == (m_->cstr = (char *)realloc (m_->cstr, m_->cb_cstr = 2 * m_->cb_cstr))) {
					result = -128; // not enough memory (Pt malloc)
					continue;
				}
			m_->cstr[len] = buf[-1];
			m_->cstr[++len] = '\0';
			break;
		case 50:
			if (m_->handler && m_->handler->osc_title_chg)
				m_->handler->osc_title_chg (m_->handler_this_, m_->cstr);
			free (m_->cstr);
			m_->cstr = NULL;
			break;
		case 53:
			value = OSC_RGB_QUERY;
			break;
		case 51:
			if (! (OSC_RGB_QUERY == stack[1].value))
				break;
			if (m_->handler && m_->handler->osc_vtwin_fgcl_query)
				m_->handler->osc_vtwin_fgcl_query (m_->handler_this_);
			break;
		case 52:
			if (! (OSC_RGB_QUERY == stack[1].value))
				break;
			if (m_->handler && m_->handler->osc_vtwin_bgcl_query)
				m_->handler->osc_vtwin_bgcl_query (m_->handler_this_);
			break;
		default:
			break;
		}
		stack += reduce_len -1;
		stack->value = value;
		stack->prezero = (prezero) ? 1 : 0;
		n = HI8(r[n]);
		state = U16AS12(pgoto, n -85) -53 + stack[1].state;
		if (! (0 <= state && state < arraycountof(tabchk) && LO8(tabchk[state]) -1 == stack[1].state))
			state = U16AS8(defgoto, n -85) -1;
		else
			state = HI8(tabchk[state]) +0;
	}
	m_->sv_stack_used = sv_stack_get_bottom(m_) - stack;
	m_->state = state;
	m_->result = result;
	if (result < 0) {
		m_->giveup_bytes = (u32)(buf - begin);
		m_->result = 0; // for resume
		return result;
	}
	return buf - begin;
}


