#define _POSIX_C_SOURCE 200809L /* POSIX.1-2008 dprintf
#define _POSIX_C_SOURCE 200112L    POSIX.1-2001 strtoul
#define _POSIX_C_SOURCE 199309L    POSIX.1b-1993 nanosleep */
//#define _FILE_OFFSET_BITS 64
#include <features.h>
//#include <endian.h>
#include <stdint.h>

#include "vt100_parser.h"

#include <stdbool.h>
#include <stdlib.h> // exit
#include <string.h>
#include <malloc.h>
#include <unistd.h> // STDERR_FILENO
typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;
typedef     char s8;
typedef    short s16;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.
#define arraycountof(x) (sizeof(x)/sizeof((x)[0]))
///////////////////////////////////////////////////////////////
// VT100

#define VTO  "\033[0m"
#define VTRR "\033[1;31m"
#define VTGG "\033[1;32m"
#define VTYY "\033[1;33m"
#define VTBB "\033[1;34m"
#define VTMM "\033[1;35m"
#define VTCC "\033[1;36m"

///////////////////////////////////////////////////////////////
// assertion

#define assert(expr) \
	do { \
		if (! (expr)) { \
dprintf (STDOUT_FILENO, VTRR "ASSERT" VTO ": " #expr " [%s]" "\n", __func__); \
			exit (-1); \
		} \
	} while (0);
#define assert_compare(expr, fmt, a, b) \
	if (! (expr)) { \
dprintf (STDOUT_FILENO, VTRR "ASSERT" VTO "! " #expr fmt "  [" VTCC "%s" VTO "]" "\n", a, b, __func__); \
		exit (-1); \
	}

#define no_memory   assert
#define input_error assert
#define unexpected  assert
#define caller_bug  assert
#define unexpected_compare assert_compare

///////////////////////////////////////////////////////////////

static const u16 translate[] = {
  0x0002, 0x0202, 0x0202, 0x0229, 0x0202, 0x0202, 0x0202, 0x0202,
  0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0205, 0x0202, 0x0202,
  0x2a2b, 0x022c, 0x021d, 0x0202, 0x0202, 0x022d, 0x022e, 0x2f30,
  0x2021, 0x2223, 0x1c24, 0x2526, 0x2728, 0x1f1e, 0x020a, 0x0b19,
  0x0c0d, 0x060e, 0x0f34, 0x3536, 0x1037, 0x1112, 0x1307, 0x3839,
  0x083a, 0x3b3c, 0x3d3e, 0x3f40, 0x4142, 0x4303, 0x0904, 0x3132,
  0x0244, 0x4518, 0x4647, 0x4849, 0x1a4a, 0x4b4c, 0x1b14, 0x154d,
  0x4e4f, 0x1650, 0x1751, 0x5253, 0x5455, 0x5602, 0x0202, 0x3302,
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
  0x0000, 0x5702, 0x5802, 0x5802, 0x5803, 0x5801, 0x5901, 0x5904,
  0x5901, 0x5901, 0x5a02, 0x5a02, 0x5a02, 0x5a02, 0x5a02, 0x5a02,
  0x5a02, 0x5a02, 0x5a02, 0x5a02, 0x5a02, 0x5a02, 0x5a02, 0x5a02,
  0x5a02, 0x5a03, 0x5a03, 0x5a03, 0x5a03, 0x5a03, 0x5b00, 0x5b01,
  0x5c00, 0x5c01, 0x5d02, 0x5d01, 0x5d04, 0x5d03, 0x5d04, 0x5d03,
  0x5d02, 0x5d02, 0x5e01, 0x5e01, 0x5e01, 0x5e02, 0x5e02, 0x5e02,
  0x5f01, 0x6001, 0x6001, 0x6101, 0x6101, 0x6101, 0x6101, 0x6101,
  0x6101, 0x6101, 0x6201, 0x6201, 0x6304, 0x6305, 0x6305, 0x6305,
  0x6401, 0x6502, 0x6501, 0x6600, 0x6601, 0x6701, 0x6702, 0x6801,
  0x6801, 0x6801, 0x6901, 0x6901, 0x6901, 0x6901, 0x6901, 0x6901,
  0x6901, 0x6901, 0x6901, 0x6901, 0x6901, 0x6901, 0x6901, 0x6901,
  0x6901, 0x6901, 0x6901, 0x6a01, 0x6a01, 0x6a01, 0x6a01, 0x6a01,
  0x6a01, 0x6a01, 0x6a01, 0x6a01, 0x6a01, 0x6a01, 0x6a01, 0x6a01,
  0x6a01, 0x6a01, 0x6a01, 0x6a01, 0x6a01, 0x6a01, 0x6a01, 0x6a01,
  0x6a01, 0x6a01, 0x6a01, 0x6a01, 0x6a01, 0x6b01, 0x6b01, 0x6b01,
  0x6b01, 0x6b01, 0x6b01, 0x6b01, 0x6b01, 0x6b01, 0x6b01, 0x6b01,
  0x6b01, 0x6b01, 0x6b01, 0x6b01, 0x6b01, 0x6b01, 0x6b01, 0x6b01,
  0x6b01, 0x6b01, 0x6b01, 0x6b01, 0x6b01, 0x6b01, 0x6b01
};

static const u16 defact[] = {
  0x001e, 0x0000, 0x0600, 0x0809, 0x0005, 0x1e00, 0x3430, 0x3132,
  0x3335, 0x3637, 0x3839, 0x0200, 0x001f, 0x232a, 0x2b2c, 0x3100,
  0x3a3b, 0x0003, 0x005c, 0x676a, 0x5455, 0x5b5d, 0x5e62, 0x6465,
  0x6681, 0x8286, 0x8877, 0x7c80, 0x5352, 0x4a4b, 0x4c4d, 0x4e4f,
  0x5057, 0x585a, 0x5f60, 0x6163, 0x6869, 0x6b6c, 0x6d6e, 0x6f70,
  0x7172, 0x7374, 0x7576, 0x7879, 0x7a7b, 0x7d7e, 0x7f83, 0x8485,
  0x8789, 0x8a8b, 0x8c8d, 0x8e47, 0x4849, 0x0051, 0x4556, 0x5901,
  0x001f, 0x3400, 0x0c13, 0x140a, 0x0b0d, 0x0e0f, 0x1011, 0x1215,
  0x1617, 0x1828, 0x2922, 0x2d2e, 0x2f00, 0x0000, 0x0004, 0x0046,
  0x1a19, 0x1d1b, 0x1c25, 0x2700, 0x0043, 0x0007, 0x2426, 0x4000,
  0x0000, 0x443c, 0x3e3f, 0x0042, 0x3d41
};

static const u16 defgoto[] = {
  0x0009, 0x0a17, 0x1819, 0x1a1b, 0x6869, 0x6a23, 0x24a0, 0xa9a2,
  0x6b6c, 0x6d6e, 0x6f00
};

static const u16 pact[] = {
  0x10c1, 0xb807, 0x6096, 0x0001, 0x6c00, 0x0000, 0x08c0, 0x001f,
  0xa207, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x01db, 0x08d0, 0xe61d, 0x3000, 0x0000, 0x0006, 0xe080,
  0x0000, 0x0008, 0x5000, 0x0a20, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x000a,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0870, 0xd909, 0x70e4, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x01fa, 0x1fa0, 0x0000, 0x0000, 0x0000, 0x9b0e,
  0x20ea, 0x16c0, 0x0010, 0x9000, 0x0000, 0x0000, 0x0000, 0x0001,
  0xe01e, 0xd101, 0x1011, 0x6c11, 0xb000, 0x0000, 0x0000, 0x00ec,
  0x0fb0, 0x9416, 0xc000, 0x0000, 0x0011, 0xd000, 0x0000, 0x0000
};

static const u16 pgoto[] = {
  0x0000, 0x0000, 0x0000, 0x12a0, 0x000f, 0xc095, 0x0860, 0x8808,
  0xa000, 0x0000, 0xdd00, 0x0000, 0x0010, 0x0005, 0xf000, 0x0000
};

static const u16 tabchk[] = {
  0x3c02, 0x4103, 0x3d02, 0x4203, 0x3e02, 0x9001, 0xbb8d, 0xaa21,
  0xab22, 0x3c0b, 0x3c0c, 0x3d0b, 0x3d0c, 0x3e0b, 0x3e0c, 0x2e21,
  0x3f22, 0x3023, 0xc706, 0xc39a, 0xb115, 0x4507, 0x4028, 0xac1f,
  0xb219, 0xa71b, 0xa015, 0xa81b, 0xad1f, 0xa91b, 0xaf06, 0x4607,
  0x4708, 0x4809, 0xae08, 0x490b, 0xb315, 0x4a0d, 0x4b0e, 0x4c0f,
  0x4d10, 0x4e11, 0x4f12, 0x5013, 0x5114, 0x5215, 0x5316, 0x5417,
  0x5518, 0x5619, 0xb81f, 0x571b, 0x581c, 0x2d1d, 0xc82a, 0x591f,
  0x5a20, 0x2e21, 0x2f22, 0x3023, 0x3124, 0x3225, 0x3326, 0x3427,
  0x3528, 0x3629, 0xb06b, 0x5b2b, 0x5c2c, 0x5d2d, 0x5e2e, 0x5f2f,
  0x6030, 0x6131, 0x6232, 0x6333, 0x6434, 0x6535, 0x6636, 0x6737,
  0x6838, 0x6939, 0x6a3a, 0x6b3b, 0x6c3c, 0x6d3d, 0x6e3e, 0x6f3f,
  0x7040, 0x7141, 0x7242, 0x7343, 0x7444, 0x7545, 0x7646, 0x7747,
  0x7848, 0x7949, 0x7a4a, 0x7b4b, 0x7c4c, 0x7d4d, 0x7e4e, 0x7f4f,
  0x8050, 0x8151, 0x8252, 0x8353, 0x8454, 0x8555, 0x8656, 0x8757,
  0xa41f, 0xa520, 0xb09b, 0x0015, 0xa116, 0xa217, 0xa318, 0xb41b,
  0xb51c, 0xb91f, 0xb0a3, 0xa41f, 0xa520, 0xa41f, 0xa520, 0x920b,
  0x940c, 0xba1f, 0x3c84, 0x3c85, 0x3d84, 0x3d85, 0x3e84, 0x3e85,
  0x2204, 0x2305, 0x2406, 0xbc0a, 0x2508, 0x2609, 0xc52a, 0x270b,
  0x280c, 0xb684, 0xb785, 0xbf1a, 0xa796, 0xa797, 0xa896, 0xa897,
  0xa996, 0xa997, 0x4607, 0x4708, 0x4809, 0xc62a, 0x490b, 0xca0a,
  0x4a0d, 0x4b0e, 0x4c0f, 0x4d10, 0x4e11, 0x4f12, 0x5013, 0x5114,
  0x5215, 0x5316, 0x5417, 0x5518, 0x5619, 0x910b, 0x571b, 0x581c,
  0x2d1d, 0x2100, 0x591f, 0x5a20, 0x2e21, 0x2f22, 0x3023, 0x3124,
  0x3225, 0x3326, 0x3427, 0x3528, 0x3629, 0xc42a, 0x5b2b, 0x5c2c,
  0x5d2d, 0x5e2e, 0x5f2f, 0x6030, 0x6131, 0x6232, 0x6333, 0x6434,
  0x6535, 0x6636, 0x6737, 0x6838, 0x6939, 0x6a3a, 0x6b3b, 0x6c3c,
  0x6d3d, 0x6e3e, 0x6f3f, 0x7040, 0x7141, 0x7242, 0x7343, 0x7444,
  0x7545, 0x7646, 0x7747, 0x7848, 0x7949, 0x7a4a, 0x7b4b, 0x7c4c,
  0x7d4d, 0x7e4e, 0x7f4f, 0x8050, 0x8151, 0x8252, 0x8353, 0x8454,
  0x8555, 0x8656, 0x8757, 0x4607, 0x4708, 0x4809, 0xc199, 0x490b,
  0x2100, 0x4a0d, 0x4b0e, 0x4c0f, 0x4d10, 0x4e11, 0x4f12, 0x5013,
  0x5114, 0x5215, 0x5316, 0x5417, 0x5518, 0x5619, 0x2100, 0x571b,
  0x581c, 0x2d1d, 0x2100, 0x591f, 0x5a20, 0x2e21, 0x2f22, 0x3023,
  0x3124, 0x3225, 0x3326, 0x3427, 0x3528, 0x3629, 0x2100, 0x5b2b,
  0x5c2c, 0x5d2d, 0x5e2e, 0x5f2f, 0x6030, 0x6131, 0x6232, 0x6333,
  0x6434, 0x6535, 0x6636, 0x6737, 0x6838, 0x6939, 0x6a3a, 0x6b3b,
  0x6c3c, 0x6d3d, 0x6e3e, 0x6f3f, 0x7040, 0x7141, 0x7242, 0x7343,
  0x7444, 0x7545, 0x7646, 0x7747, 0x7848, 0x7949, 0x7a4a, 0x7b4b,
  0x7c4c, 0x7d4d, 0x7e4e, 0x7f4f, 0x8050, 0x8151, 0x8252, 0x8353,
  0x8454, 0x8555, 0x8656, 0x8757, 0x2b0c, 0x2100, 0x2100, 0x2100,
  0x2100, 0x2100, 0x2100, 0x2100, 0x2100, 0x0115, 0x2100, 0x2100,
  0x2100, 0x2100, 0x2c1a, 0x2100, 0x2100, 0x2d1d, 0x2100, 0x2100,
  0x2100, 0x2e21, 0x2f22, 0x3023, 0x3124, 0x3225, 0x3326, 0x3427,
  0x3528, 0x3629, 0x9507, 0x9608, 0x9709, 0x2100, 0x2100, 0x2100,
  0x980d, 0x990e, 0x9a0f, 0x9b10, 0x9c11, 0x9d12, 0x9e13, 0x9f14,
  0x2d1d, 0xa61e, 0x2100, 0x2100, 0x2e21, 0x2f22, 0x3023, 0x3124,
  0x3225, 0x3326, 0x3427, 0x3528, 0x3629, 0x2d1d, 0xbd1e, 0x2100,
  0x2100, 0x2e21, 0x2f22, 0x3023, 0x3124, 0x3225, 0x3326, 0x3427,
  0x3528, 0x3629, 0x2d1d, 0xbe1e, 0x2100, 0x2100, 0x2e21, 0x2f22,
  0x3023, 0x3124, 0x3225, 0x3326, 0x3427, 0x3528, 0x3629, 0x2d1d,
  0x2100, 0x2100, 0x2100, 0x2e21, 0x2f22, 0x3023, 0x3124, 0x3225,
  0x3326, 0x3427, 0x3528, 0x3629, 0x931d, 0x2100, 0x2100, 0x2100,
  0x2e21, 0x2f22, 0x3023, 0x3124, 0x3225, 0x3326, 0x3427, 0x3528,
  0x3629
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
typedef struct {
	char *cstr;
	u32 cb_cstr;
	u32 giveup_bytes;
	s16 state;
	s8 result;
	u8 padding_23[1];
//#define EXPANDABLE
#ifdef EXPANDABLE
	u16 cb;
#endif
	u8 n_list_used;
	u8 sv_stack_used;
#ifdef EXPANDABLE
	struct csi_param n_list[1];
#else // fixed
	struct csi_param n_list[10];
	u8 padding_50[2];
	struct statval sv_stack[19]; // 19 = (128 -(4(dtor only vtbl) +offsetof(n_list) +sizeof(n_list))) / sizeof(struct statval)
#endif
} vt100_parser_s;
static vt100_parser_s s_singlton = {0};

#define UNDEF_VALUE 0

#ifdef EXPANDABLE
#define stack_is_nospc(m_, n, sv) (! ((void *)(m_->n_list + m_->n_list_used +(n)) <= (void *)(stack -(sv))))
#define   n_list_is_nospc(m_, n)  stack_is_nospc(m_, n, 0)
#define sv_stack_is_nospc(m_, sv) stack_is_nospc(m_, 0, sv)
#define sv_stack_get_bottom(m_) ((struct statval *)((u8 *)(m_) + (m_)->cb) -1)

#else // fixed
#define stack_is_nospc(m_, n, sv) (n_list_is_nospc (m_, n) || sv_stack_is_nospc (m_, sv))
#define   n_list_is_nospc(m_, n)  (! (m_->n_list_used +(n) <= arraycountof(m_->n_list)))
#define sv_stack_is_nospc(m_, sv) (! ((sv) +m_->sv_stack_used +1 <= arraycountof(m_->sv_stack)))
#define sv_stack_get_bottom(m_) (m_->sv_stack +arraycountof(m_->sv_stack) -1)

#endif

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
		on_csi_sgr_none ();
		return 1;
	case 1:
		if (0 == i_bcd || 0 < (15 & bcd20 >> --i_bcd * 4)) {
			on_csi_sgr_bold (true);
			return 1; // CSI '1[m;1-9]'
		}
		lead = 10;
//	case 10:
		if (0 == i_bcd || 107 < (lead = (15 & bcd20 >> (i_bcd -1) * 4) + 10 * lead))
			return 2 +i_bcd; // ERROR: CSI '10[;m]' or '10[89][;m]'
		on_csi_sgr_bgcl3 (lead -100 +8);
		return 3; // CSI '10[0-7][;m]'
	case 2:
		if (i_bcd < 1)
			return 1 +i_bcd; // ERROR: CSI '2[;m]'
		--i_bcd;
		switch (lead = (15 & bcd20 >> i_bcd * 4) + 10 * lead) {
		case 22:
			on_csi_sgr_bold (false);
			break; // CSI '22...[;m]'
		case 23:
			on_csi_sgr_italic (false);
			break; // CSI '23...[;m]'
		case 24:
			on_csi_sgr_underline (false);
			break; // CSI '24...[;m]'
		case 27:
			on_csi_sgr_reverse (false);
			break; // CSI '27...[;m]'
		case 29:
			on_csi_sgr_strikethrough (false);
			break; // CSI '29...[;m]'
		case 25:
			break; // CSI '25...[;m]'
		default:
			break; // ERROR: CSI '2[0168]...[;m]'
		}
		return 2;
	case 3:
		if (0 == i_bcd) {
			on_csi_sgr_italic (true);
			return 1; // CSI '3[;m]'
		}
		--i_bcd;
		if (38 < (lead = (15 & bcd20 >> i_bcd * 4) + 10 * lead))
			return 2; // ERROR: CSI '39[;m]'
		if (lead < 38) {
			on_csi_sgr_fgcl3 (lead -30);
			return 2; // CSI '3[0-7][;m]'
		}
		if (0 == i_bcd && 1 < len && 5 == aux[0].val) {
			on_csi_sgr_fgcl3 (aux[1].val);
			return 2 +2; // CSI '38;5;<N>[;m]'
		}
		else if (0 == i_bcd && 3 < len && 2 == aux[0].val) {
u8 r, g, b;
			r = min(aux[1].val, 255);
			g = min(aux[2].val, 255);
			b = min(aux[3].val, 255);
			on_csi_sgr_fgcl24 (r << 16 | g << 8 | b);
			return 2 +4; // CSI '38;2[;:]<R>[;:]<G>[;:]<B>[;m]'
		}
input_error(i_bcd <= 3)
		return 1 +i_bcd +min(2, len); // ERROR: CSI '38...m' but not '38;5;<N>[;m]'
	case 4:
		if (i_bcd < 1) {
			on_csi_sgr_underline (true);
			return 1; // CSI '4[;m]'
		}
		--i_bcd;
		if (48 < (lead = (15 & bcd20 >> i_bcd * 4) + 10 * lead))
			return 2; // ERROR: CSI '49[;m]'
		if (lead < 48) {
			on_csi_sgr_bgcl3 (lead -40);
			return 2; // CSI '4[0-7][;m]'
		}
		if (0 == i_bcd && 1 < len && 5 == aux[0].val) {
			on_csi_sgr_bgcl3 (aux[1].val);
			return 2 +2; // CSI '48;5;<N>[;m]'
		}
		else if (0 == i_bcd && 3 < len && 2 == aux[0].val) {
u8 r, g, b;
			r = min(aux[1].val, 255);
			g = min(aux[2].val, 255);
			b = min(aux[3].val, 255);
			on_csi_sgr_bgcl24 (r << 16 | g << 8 | b);
			return 2 +4; // CSI '48;2[;:]<R>[;:]<G>[;:]<B>[;m]'
		}
input_error(i_bcd <= 3)
		return 1 +i_bcd +min(2, len); // ERROR: CSI '48...m' but not '48;5;<N>[;m]'
	case 5:
		return 1; // CSI '[57]...[;m]'
	case 7:
		on_csi_sgr_reverse (true);
		return 1; // CSI '7[m;]'
	case 9:
		if (0 == i_bcd) {
			on_csi_sgr_strikethrough (true);
			return 1; // CSI '9[;m]'
		}
		--i_bcd;
		if (97 < (lead = (15 & bcd20 >> i_bcd * 4) + 10 * lead))
			return 2; // ERROR: CSI '9[89][;m]'
		on_csi_sgr_fgcl3 (lead -90 +8);
		return 2; // CSI '9[0-7][;m]'
	//case 6:
	//case 8:
	default:
		return 1 +i_bcd; // ERROR: CSI '[68]...[;m]'
	}
}

void escseq_reset ()
{
vt100_parser_s *m_ = &s_singlton;
#ifdef EXPANDABLE
	m_->cb = (u16)sizeof(s_singlton);
#endif

	m_->result        = 0;
	m_->giveup_bytes  = 0;
	m_->state         = 0;
	m_->sv_stack_used = 0;
	m_->n_list_used   = 0;
#ifdef EXPANDABLE
struct statval *stack
	= sv_stack_get_bottom(m_);
#endif
no_memory(!stack_is_nospc (m_, 0, 0))
	sv_stack_get_bottom(m_)->value = UNDEF_VALUE;
}

//void vt100_parser_release ()
//{
//vt100_parser_s *m_ = &s_singlton;
//
//	if (m_->cstr) {
//		free (m_->cstr);
//		m_->cstr = NULL, m_->cb_cstr = 0;
//	}
//}

unsigned escseq_get_paused_len ()
{
vt100_parser_s *m_ = &s_singlton;

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
int escseq_write (const void *buf_, unsigned len)
{
vt100_parser_s *m_ = &s_singlton;

const char *buf;
	buf = (const char *)buf_;
	m_->giveup_bytes = 0;
unsigned used;
	if (127 == m_->result) {
assert(1 == 0)
//		if ((used = on_esc_bmp (buf, len)) < len)
//			m_->result = 1;
//		return used;
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
		if (! (-135 == (n = U16AS12(pact, state) -135))) {
			if (-2 == c) {
				if (!(buf < end) || '\0' == *buf) { // null cannot parse now, always treat end of input.
					result = -1; // uncompleted (input ended before parsing complete)
					break;
				}
				c = (u8)*buf++;
			}
			n += U16AS8(translate, c);
			if (0 <= n && n < arraycountof(tabchk) && LO8(tabchk[n]) -1 == U16AS8(translate, c)) {
				if (0 == (n = HI8(tabchk[n]) -33) || -33 == n +1) {
					result = -2; // syntax error
					continue;
				}
				else if (111 == n) {
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
assert(1 == 0)
//			result = 127;
//			on_esc_bmp (NULL, 0);
//			used = 0;
//			if (buf < end && (used = on_esc_bmp (buf, end - buf)) < end - buf)
//				result = 1;
//			buf += used;
			continue;
		case 6:
			on_esc_ri ();
			break;
		case 7:
			on_esc_dcs (m_->cstr);
			free (m_->cstr);
			m_->cstr = NULL;
			break;
		case 8:
			on_esc_dec_kpam ();
			break;
		case 9:
			on_esc_dec_kpnm ();
			break;
		case 2: case 3: case 5:
			result = 1;
			continue;
		case 12: case 14:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 1;
			//if (iface && iface->csi_cuX)
			//	iface->csi_cuX (param, num);
			break;
		case 10:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 1;
			on_csi_ich (num);
			break;
		case 11:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 1;
			on_csi_cuu (num);
			break;
		case 13:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 1;
			on_csi_cuf (num);
			break;
		case 15:
			y = (0 < m_->n_list_used) ? m_->n_list[0].val : 1;
			x = (1 < m_->n_list_used) ? m_->n_list[1].val : 1;
			on_csi_cup (y, x);
			break;
		case 16:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 0;
			on_csi_ed (num);
			break;
		case 17:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 0;
			on_csi_el (num);
			break;
		case 18:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 1;
			on_csi_il (num);
			break;
		case 19:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 1;
			on_csi_dl (num);
			break;
		case 20:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 1;
			on_csi_dch (num);
			break;
		case 21:
			if (0 == m_->n_list_used)
				m_->n_list[m_->n_list_used++].val = 0; // CSI 'm' = '0m'
			sgr_end = m_->n_list +m_->n_list_used;
u32 bcd20; int bcd_len;
			for (sgr_p = m_->n_list, bcd_len = -1; sgr_p < sgr_end;) {
				if (bcd_len < 0) {
					bcd20 = bcd20_from_u16 (sgr_p->val);
					for (bcd_len = 4; 0 < bcd_len && 0 == (15 & bcd20 >> bcd_len * 4);)
						--bcd_len;
					if (sgr_p->prezero && 0 < (15 & bcd20 >> bcd_len * 4))
						++bcd_len;
				}
u8 dispatched_len;
				if (! (bcd_len < (dispatched_len = dispatch_sgr (m_, bcd20, (u8)bcd_len, sgr_p +1, sgr_end - (sgr_p +1)))))
					bcd_len -= dispatched_len;
				else {
unexpected(sgr_p +(dispatched_len -bcd_len) <= sgr_end)
					sgr_p += dispatched_len -bcd_len, bcd_len = -1;
				}
			}
			break;
		case 22:
			on_csi_dsr (m_->n_list[0].val);
			break;
		case 23:
			tm = m_->n_list[0].val;
			bm = (1 < m_->n_list_used) ? m_->n_list[1].val : 0;
			on_csi_dec_stbm (tm, bm);
			break;
		case 24: // TODO:
			if (m_->n_list_used < 3 || 255 < m_->n_list[0].val || 255 < m_->n_list[1].val || 255 < m_->n_list[2].val) {
			}
			on_csi_dec_slpp (m_->n_list[0].val, m_->n_list[1].val, m_->n_list[2].val);
			break;
		case 25:
			num = (0 < m_->n_list_used) ? m_->n_list[0].val : 0;
			on_csi_da2 (num);
			break;
		case 26:
			if (! (2 == m_->n_list_used)) {
				result = -2; // syntax error
				continue;
			}
			on_csi_xtmodkeys (m_->n_list[0].val, m_->n_list[1].val);
			break;
		case 27:
			for (i = 0; i < m_->n_list_used; ++i)
				on_csi_dec_exm (m_->n_list[i].val, true);
			break;
		case 28:
			for (i = 0; i < m_->n_list_used; ++i)
				on_csi_dec_exm (m_->n_list[i].val, false);
			break;
		case 29:
				on_vim_unknown_1_csi ();
			break;
		case 35: case 37: case 39:
			if (n_list_is_nospc (m_, 1)) {
				result = -128; // not enough memory (<n>[;<n>...] store overflow)
				continue;
			}
			++m_->n_list_used;
			n_list_last(m_)->prezero = stack->prezero;
			n_list_last(m_)->val = stack->value;
			break;
		case 40: case 41:
			if (n_list_is_nospc (m_, 1)) {
				result = -128; // not enough memory (<n>[;<n>...] store overflow)
				continue;
			}
			++m_->n_list_used;
			n_list_last(m_)->prezero = false;
			n_list_last(m_)->val = 0;
			break;
		case 42:
		case 43: case 44:
			prezero = false;
			value = stack->value;
			break;
		case 46: case 47:
			if (0 == stack[1].value) {
				prezero = true;
				value = stack->value;
				break;
			}
		case 45:
			prezero = (stack[1].prezero) ? true : false;
			value = stack[1].value * 10 + stack->value;
			break;
		case 71: case 72: case 73:
			value = stack->value;
			break;
		case 58: case 59:
			value = stack->value;
			break;
		case 48: case 49: case 50: case 51:
		case 52: case 53: case 54: case 55:
		case 56: case 57:
			prezero = false;
			value = buf[-1] - '0';
			break;
		case 69: case 70:
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
		case 60:
			on_osc_set_window_title (m_->cstr);
			free (m_->cstr);
			m_->cstr = NULL;
			break;
		case 61: {
			on_osc_hyperlinks (m_->cstr);
			if (NULL == m_->cstr) // <uri> omitted
				break;
			free (m_->cstr);
			m_->cstr = NULL;
			break;
		}
		case 64:
			value = OSC_RGB_QUERY;
			break;
		case 62:
			if (! (OSC_RGB_QUERY == stack[1].value))
				break;
			on_osc_fg_rgb_query ();
			break;
		case 63:
			if (! (OSC_RGB_QUERY == stack[1].value))
				break;
			on_osc_bg_rgb_query ();
			break;
		default:
			break;
		}
		stack += reduce_len -1;
		stack->value = value;
		stack->prezero = (prezero) ? 1 : 0;
		n = HI8(r[n]);
		state = U16AS12(pgoto, n -87) -135 + stack[1].state;
		if (! (0 <= state && state < arraycountof(tabchk) && LO8(tabchk[state]) -1 == stack[1].state))
			state = U16AS8(defgoto, n -87) -1;
		else
			state = HI8(tabchk[state]) -33;
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


