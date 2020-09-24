#ifdef WIN32
#else
# define _XOPEN_SOURCE 500 // usleep (>= 500)
# include <features.h>
# include <gtk/gtk.h>
# include <fcntl.h> // O_RDWR
# include <termios.h> // termios ICANON ..
# include <sys/ioctl.h> // winsize TIOCSWINSZ
#endif
#if 1 // ndef READ_SHELL_THREAD
# include <sys/time.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <malloc.h>
#include <memory.h>
#include "defs.h"

#ifdef SSH_NOT_AVAILABLE
# include "ptmx.h"
typedef struct ptmx ptmx_s;
#else
# include "ssh2.h"
typedef struct ssh2 ssh2_s;
#endif
#include "gtkview.h"
#include "cmdg.h"
#define VT100_CALLBACK Cmdg
#include "vt100.h"
#undef VT100_CALLBACK
#include "conf.h"
#include "iid.h"

typedef uint32_t u32;
typedef uint8_t u8;
typedef uint16_t u16;
typedef char s8;
#define arraycountof(x) (sizeof(x)/sizeof(x[0]))
#define ALIGN32(x) (-32 & (x) +32 -1)
#define ALIGN4(x) (-4 & (x) +4 -1)
#define ALIGN(x,a) (((x) +(a) -1)/(a)*(a))

enum {
	SGR_FG_LSB    = ATTR_FG_LSB   ,
	SGR_FG_R      = ATTR_FG_R     ,
	SGR_FG_G      = ATTR_FG_G     ,
	SGR_FG_B      = ATTR_FG_B     ,
	SGR_BG_LSB    = ATTR_BG_LSB   ,
	SGR_BG_R      = ATTR_BG_R     ,
	SGR_BG_G      = ATTR_BG_G     ,
	SGR_BG_B      = ATTR_BG_B     ,
	SGR_REV_MASK  = ATTR_REV_MASK ,
	SGR_BOLD_MASK = ATTR_BOLD_MASK,
};
#define SGR_FG(cl) ((7 & (cl)) << SGR_FG_LSB)
#define SGR_BG(cl) ((7 & (cl)) << SGR_BG_LSB)
#define SGR_DEFAULT (SGR_FG(7)|SGR_BG(0))
#define SGR_DEC(a) (SGR_DEFAULT ^ (a))
#define SGR_ENC SGR_DEC
extern FILE *stderr_old;

// for debug trace
#define malloc_s malloc
#define realloc_s realloc
///////////////////////////////////////////////////////////////////////////////
// model

//#define N_NULL (int)-1
#define N_NULL 0
enum { // flags
	LINKED = 1,
};
typedef struct EXUTF8__ EXUTF8;
struct EXUTF8__ {
	u32 n_prev;
	u32 n_next;
	u8 flags;
	u8 pad_8;
	u16 cb;
//	size_t cbAttr;
	char *text;
	u16 *sgr;
};

#define CSR_BG_BGR8 2 // green
#define CSR_FG_BGR8 0 // black
enum CsrState__ {
	SHOWED = (1 << 0),
	BG_ORIG_LSB = 1,
	FG_ORIG_LSB = 4,
};
struct Cmdg__ {
	View *view;
	u32 n_max;
	EXUTF8 *EXUTF8s;
	u32 n_blank_top;

	u32 n_cur;
	u16 i_cur;
	u8 nl_aux_cur;
	u16 x_cur;
	u8 y_cur;
	u8 sgr_cur;
	u8 csr_state;

	u32 n_top;
	u8 nl_trim_top;
	u8 lf_height;
	s8 px_trim_top;

	char *read_shell_buf;
	Vt100 *vt100;
	u8 keyshift;
#ifdef SSH_NOT_AVAILABLE
	ptmx_s remote_;
#else
	ssh2_s remote_;
#endif
	struct conn_i *remote;

	IImage image_iface;
	u8 decoder_state[16];
	struct {
		u16 x, y, n_cols, n_rows;
	} loading_dirty;
};

///////////////////////////////////////////////////////////////////////////////
// utility

static u32 load_le32 (const void *src)
{
#if __BYTE_ORDER == __BIG_ENDIAN
const u8 *p;
	p = (const u8 *)src;
	return p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24;
#else
	return *(const u32 *)src;
#endif
}

static u16 load_le16 (const void *src)
{
#if __BYTE_ORDER == __BIG_ENDIAN
const u8 *p;
	p = (const u8 *)src;
	return p[0] | p[1] << 8;
#else
	return *(const u16 *)src;
#endif
}

///////////////////////////////////////////////////////////////////////////////
// bmp

static u32 bmp_get_width (const void *this_)
{
	const u8 *bmphdr;

	bmphdr = (const u8 *)this_;
	return load_le32 (bmphdr +0x12);
}

static u32 bmp_get_height (const void *this_)
{
	const u8 *bmphdr;

	bmphdr = (const u8 *)this_;
	return load_le32 (bmphdr +0x16);
}

static int bmp_get_n_channels (const void *this_)
{
	const u8 *bmphdr;

	bmphdr = (const u8 *)this_;
u16 bpp;
	if (! (24 == (bpp = load_le16 (bmphdr +0x1c)) || 32 == bpp))
		return 0;
	return bpp / 8;
}

static void bmp_get_div_pixels (const void *this_, int div_x, int div_y, int get_index, void *rgba)
{
	const u8 *bmphdr;

	bmphdr = (const u8 *)this_;
u16 bpp;
	if (! (24 == (bpp = load_le16 (bmphdr +0x1c)) || 32 == bpp))
		return;
int n_channels, width, height;
	n_channels = bpp / 8;
	width = load_le32 (bmphdr +0x12);
	height = load_le32 (bmphdr +0x16);
u8 *p;
	p = (u8 *)rgba;
u16 img_col;
	img_col = ALIGN(width, div_x)/div_x;
int y0, x0;
	y0 = get_index / img_col * div_y;
	x0 = get_index % img_col * div_x;
int y, x;
	for (y = 0; y < div_y; ++y) {
		if (! (y0 + y < height)) {
			memset (p, 0, ALIGN4(div_x * n_channels));
			p += ALIGN4(div_x * n_channels);
			continue;
		}
const u8 *src;
		src = bmphdr +0x36 + (height - (y0 + y) -1) * ALIGN4(width * n_channels) + x0 * n_channels;
		for (x = 0; x < div_x; ++x) {
			if (! (x0 + x < width)) {
				*p++ = 0;
				*p++ = 0;
				*p++ = 0;
				if (4 == n_channels)
					*p++ = 0;
				continue;
			}
			*p++ = src[2]; // red
			*p++ = src[1]; // green
			*p++ = src[0]; // blue
			src += 3;
			if (4 == n_channels)
				*p++ = *src++;
		}
		for (x = div_x * n_channels; x < ALIGN4(div_x * n_channels); ++x)
			*p++ = 0;
	}
}

#define sizeofBMPHDR 0x36
typedef struct bmp_load_state__ bmp_load_state;
struct bmp_load_state__ {
	u8 *bmphdr;
	size_t got;
};
static bool bmp_decode_start (void *state_, size_t state_len)
{
bmp_load_state *state;

	if (state_len < sizeof(bmp_load_state))
		return false;
	state = (bmp_load_state *)state_;
	state->bmphdr = NULL;
	state->got = 0;
	return true;
}
/* NOTE (*a) adjust when file-length (+02,4) is zero. but not adjust when is over-length,
             and ignore waste bytes silently (trust file-length).
 */
static size_t bmp_decode (void *state_, const void *src_, size_t len, decoder_callback *i, void *i_param)
{
bmp_load_state *state;
	state = (bmp_load_state *)state_;
const u8 *src;
	src = (const u8 *)src_;

	if (! (0 < len))
		return 0;
	if (NULL == state->bmphdr) {
		// first event
		if (! (0 == state->got && i->malloc_s && (state->bmphdr = (u8 *)i->malloc_s (sizeofBMPHDR))))
			return 0;
		memcpy (state->bmphdr, "BM", 2);
		state->got = 2;
	}
size_t used;
	used = 0;
	if (state->got < sizeofBMPHDR) {
		// header loading
		used = (sizeofBMPHDR < state->got + len) ? sizeofBMPHDR - state->got : len;
		memcpy (state->bmphdr + state->got, src, used);
		state->got += used;
		src += used;
		len -= used;
		if (state->got < sizeofBMPHDR)
			return used;
	}
	// header parse
u16 bpp;
	bpp = load_le16 (state->bmphdr +0x1c);
	if (! (0 == bpp % 8 && 0 < bpp && bpp <= 32 || 1 == bpp))
		return used;
u32 width, height, bytes, max_bytes;
	width = load_le32 (state->bmphdr +0x12);
	height = load_le32 (state->bmphdr +0x16);
	bytes = load_le32 (state->bmphdr +0x02);
	max_bytes = ALIGN32(bpp * width)/8 * height + sizeofBMPHDR;
	if (0 == bytes && 0 < width && 0 < height) // (*a)
		bytes = max_bytes;
	// header event
	if (state->got - used < sizeofBMPHDR) {
		if (! (i->realloc_s && (state->bmphdr = (u8 *)i->realloc_s (state->bmphdr, bytes))))
			return used;
		if (i->notify_size)
			i->notify_size (i_param, width, height, state->bmphdr);
	}
	// body loading
	if (0 < len && state->got < bytes) {
size_t remind;
		remind = 0;
		if (bytes < state->got + len)
			remind = state->got + len - bytes;
		if (! (XPIXELS_MAX < width || YPIXELS_MAX < height)) {
			memcpy (state->bmphdr + state->got, src, len - remind);
		}
		else {
			// clip loading : TODO
		}
		state->got += len - remind;
		used += len - remind;
	}
	if (! (0 == used || state->got < bytes))
		if (i->notify_finish)
			i->notify_finish (i_param, state->bmphdr);
	return used;
}

bool bmp_query_interface (IID easy_impl, void *unk)
{
	switch (easy_impl) {
	case IID_IImage:
	{
IImage *i = (IImage *)unk;
		i->get_width = bmp_get_width;
		i->get_height = bmp_get_height;
		i->get_n_channels = bmp_get_n_channels;
		i->get_div_pixels = bmp_get_div_pixels;
		i->decode_start = bmp_decode_start;
		i->decode = bmp_decode;
		break;
	}
	default:
		return false;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// output-cache manipurator (Level 0)

#define i_node_read sgr_read
static u16 sgr_read (EXUTF8 *row, u16 i)
{
	if (NULL == row->sgr)
		return 0;
	return row->sgr[i];
}
#define i_node_write sgr_write
static bool sgr_write (EXUTF8 *row, u16 i, u16 sgr_or_i_node)
{
	if (0 == sgr_or_i_node && NULL == row->sgr)
		return true;
	if (NULL == row->sgr) {
		if (NULL == (row->sgr = (u16 *)malloc_s (sizeof(row->sgr[0]) * row->cb)))
			return false;
		memset (row->sgr, 0, sizeof(row->sgr[0]) * row->cb);
	}
	row->sgr[i] = sgr_or_i_node;
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// output-cache manipurator (Level 1)

enum {
	SGR_NOT_MODIFY = 0,
	SGR_MODIFY = 1,
};
static bool text_init (EXUTF8 *row, u16 min_cb, u8 flags)
{
u16 new_cb;
	new_cb = TEXT_CB_MIN;
	while (new_cb < min_cb)
		new_cb = 2 * (new_cb +1) -1;

	row->sgr = NULL;
	if (NULL == (row->text = (char *)malloc_s (new_cb +1)))
		return false;
	memset (row->text, '\0', new_cb +1);

	if (SGR_MODIFY & flags) {
		if (NULL == (row->sgr = (u16 *)malloc_s (sizeof(row->sgr[0]) * new_cb)))
			return false;
		memset (row->sgr, 0, sizeof(row->sgr[0]) * new_cb);
	}
	row->cb = new_cb;
	return true;
}

static bool text_expand (EXUTF8 *row, u16 min_cb, u8 flags)
{
u16 new_cb;
	if ((new_cb = row->cb) < min_cb) {
		//if (new_cb < 1)
		//	new_cb = 1;
		do {
			new_cb *= 2;
		} while (new_cb < min_cb);
		if (NULL == (row->text = (char *)realloc_s (row->text, new_cb +1)))
			return false;
		memset (row->text +row->cb, '\0', new_cb +1 - row->cb);
		if (row->sgr) {
			if (NULL == (row->sgr = (u16 *)realloc_s (row->sgr, sizeof(row->sgr[0]) * new_cb)))
				return false;
			memset (row->sgr +row->cb, 0, sizeof(row->sgr[0]) * (new_cb - row->cb));
		}
	}
	if (NULL == row->sgr && SGR_MODIFY & flags) {
		if (NULL == (row->sgr = (u16 *)malloc_s (sizeof(row->sgr[0]) * new_cb)))
			return false;
		memset (row->sgr +row->cb, 0, sizeof(row->sgr[0]) * (new_cb - row->cb));
	}
	row->cb = (u16)(new_cb < 65536 ? new_cb : 65535);
	return true;
}

static u8 text_get_nl_aux (EXUTF8 *row, View *view)
{
u8 nl_aux;
	nl_aux = 0;
u16 i;
u8 c;
	for (i = 0; !('\0' == (c = *(u8 *)&row->text[i])); ++i) {
		if (! ('\033' == c)) {
			if (0x7f < c) {
				// TODO UTF-8 support
			}
			continue;
		}
u16 i_node;
		i_node = i_node_read (row, i);
u8 img_row;
		img_row = image_get_rows (view, i_node);
		if (1 +nl_aux < img_row)
			nl_aux = img_row -1;
		++i;
	}
	return nl_aux;
}

static void text_recycle (Cmdg *m_, u32 n_free)
{
EXUTF8 *dst;
	(dst = &m_->EXUTF8s[n_free])->flags = 0;
	dst->n_prev = N_NULL;
	if (n_free +1 == m_->n_blank_top)
		dst->n_next = N_NULL;
	else {
		dst->n_next = m_->n_blank_top;
		m_->EXUTF8s[m_->n_blank_top].n_next = n_free;
	}
size_t dst_cb;
	memset (dst->text, '\0', dst_cb = strlen (dst->text));
	if (dst->sgr)
		memset (dst->sgr, 0, sizeof(dst->sgr[0]) * dst_cb);
	m_->n_blank_top = n_free;
}

// (*1) TODO back-log limitation
static u32 n_blank_create (Cmdg *m_)
{
	if (! (m_->n_blank_top < m_->n_max)) { // (*1)
u32 n_new_max;
		n_new_max = 2 * m_->n_max;
		if (NULL == (m_->EXUTF8s = (EXUTF8 *)realloc_s (m_->EXUTF8s, 2 * n_new_max))) {
fprintf (stderr_old, "not enough memory." "\n");
			exit (1); // (*1)
		}
		memset (&m_->EXUTF8s[m_->n_max], 0, sizeof(m_->EXUTF8s[0]) * (n_new_max - m_->n_max));
		m_->n_max = n_new_max;
	}
EXUTF8 *blank;
	if (N_NULL == (blank = &m_->EXUTF8s[m_->n_blank_top])->n_next)
		++m_->n_blank_top;
	else {
		m_->EXUTF8s[blank->n_next].n_prev = N_NULL;
		m_->n_blank_top = blank->n_next;
		blank->n_next = N_NULL;
	}
	return blank - m_->EXUTF8s;
}

///////////////////////////////////////////////////////////////////////////////
// output-cache manipurator (Level 2)

static bool clen_resize (EXUTF8 *row, u16 i, u8 c2)
{
u8 c1;
u16 postfix;
	if (!(i < row->cb) || '\0' == row->text[i]) {
		text_expand (row, i +c2, SGR_NOT_MODIFY);
u16 text_cb;
		if ((text_cb = strlen (row->text)) < i) {
			memset (row->text +text_cb, ' ', i - text_cb);
			if (row->sgr)
				memset (row->sgr +text_cb, 0, sizeof(row->sgr[0]) * (i - text_cb));
		}
		c1 = 1;
		postfix = 0;
	}
	else {
		c1 = ('\033' == row->text[i]) ? 2 : 1; // TODO UTF-8 support
		postfix = strlen (row->text +i +c1);
		if (c1 < c2)
			text_expand (row, i +c2 +postfix, SGR_NOT_MODIFY);
	}

char *text_i;
	text_i = row->text +i;
u16 *sgr_i;
	sgr_i = (row->sgr) ? row->sgr +i : NULL;
	if (i +c1 < postfix) {
		memmove (text_i +c2, text_i +c1, postfix);
		if (sgr_i)
			memmove (sgr_i +c2, sgr_i +c1, sizeof(row->sgr[0]) * postfix);
	}
	if (c2 < c1) {
		memset (text_i +c2 +postfix, '\0', c1 - c2);
		if (sgr_i)
			memset (sgr_i +c2 +postfix, 0, c1 - c2);
	}
	return true;
}

static u32 rows_insert (Cmdg *m_, u32 n_dst, u16 min_col, u8 flags)
{
u32 n_insert;
	n_insert = n_blank_create (m_);
u16 ws_col;
	if ((ws_col = view_get_cols (m_->view)) < min_col)
		ws_col = min_col;
	text_init (&m_->EXUTF8s[n_insert], ws_col, flags);
u32 n_old_next;
	n_old_next = m_->EXUTF8s[n_dst].n_next;
	m_->EXUTF8s[n_old_next].n_prev = n_insert;
	m_->EXUTF8s[n_insert].n_next = n_old_next;
	m_->EXUTF8s[n_insert].n_prev = n_dst;
	m_->EXUTF8s[n_dst].n_next = n_insert;
	return n_insert;
}

///////////////////////////////////////////////////////////////////////////////
// output-cache manipurator (Level 3)

static bool text_putc (EXUTF8 *row, u16 i, int c, u8 sgr)
{
	text_expand (row, i +1, (SGR_DEFAULT == sgr) ? SGR_NOT_MODIFY : SGR_MODIFY);
	row->text[i] = (char)(u8)c;
	if (! (true == sgr_write (row, i, SGR_ENC(sgr))))
		return false;
	return true;
}

static bool text_putg (EXUTF8 *row, u16 i, u16 i_node)
{
	if (! (true == clen_resize (row, i, 2)))
		return false;
	memcpy (&row->text[i], "\033\033", 2);
	if (! (true == i_node_write (row, i, i_node)))
		return false;
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// output-cache iterator

static u32 itr_enum (Cmdg *m_, u32 itr_base, int offset, int (*lpfn)(u32 n_enum, u8 nl_begin, u8 nl_end, void *param), void *param)
{
u32 n_got;
	n_got       = (ITR_CUR == itr_base) ? m_->n_cur : itr_base >> 8;
u8 nl_trim_got;
	nl_trim_got = (ITR_CUR == itr_base) ?         0 : (u8)(0xff & itr_base);

	while (! (0 == offset)) {
int n;
		if (offset < 0) {
			// minus offset complete
			if (! (nl_trim_got < -offset)) {
				nl_trim_got -= -offset;
				if (lpfn && (n = lpfn (n_got, nl_trim_got, nl_trim_got + -offset, param)) == (u8)n)
					nl_trim_got = (u8)n;
				offset = 0;
				continue;
			}
			// callback abort
			if (lpfn && (n = lpfn (n_got, 0, nl_trim_got, param)) == (u8)n) {
				nl_trim_got = (u8)n;
				offset = 0;
				continue;
			}
			offset += nl_trim_got;
			nl_trim_got = 0;
			// minus offset abort
			if (N_NULL == m_->EXUTF8s[n_got].n_prev)
				break;
			n_got = m_->EXUTF8s[n_got].n_prev;
		}
u8 nl_aux;
		nl_aux = text_get_nl_aux (&m_->EXUTF8s[n_got], m_->view);
//LOG2(0, "(n=%d,i=..) " VTYY "get_nl_aux" VTO "()=%d", n_got -1, nl_aux)
		// previous row
		if (offset < 0) {
			nl_trim_got = 1 +nl_aux;
			continue;
		}
		// plus offset complete
		if (nl_trim_got +offset < 1 +nl_aux) {
			nl_trim_got += offset;
			if (lpfn && (n = lpfn (n_got, nl_trim_got - offset, nl_trim_got +1, param)) == (u8)n)
				nl_trim_got = (u8)n;
			offset = 0;
			continue;
		}
		// callback abort
		if (lpfn && (n = lpfn (n_got, nl_trim_got, 1 +nl_aux, param)) == (u8)n) {
			nl_trim_got = (u8)n;
			offset = 0;
			continue;
		}
		// plus offset abort
		if (N_NULL == m_->EXUTF8s[n_got].n_next) {
			offset -= nl_aux - nl_trim_got;
			nl_trim_got = nl_aux;
			break;
		}
		n_got = m_->EXUTF8s[n_got].n_next;
		// next row
		offset -= 1 +nl_aux - nl_trim_got;
		nl_trim_got = 0;
	}
	return n_got << 8 | nl_trim_got;
}
u32 itr_seek (Cmdg *m_, u32 itr_base, int offset)
{
	return itr_enum (m_, itr_base, offset, NULL, NULL);
}

struct itr_find {
	u32 itr_find;
	int offset;
	bool is_forward;
};
static int itr_find_cb (u32 n_enum, u8 nl_begin, u8 nl_end, void *param)
{
struct itr_find *p;
	p = (struct itr_find *)param;
	// unmatched
	if (p->is_forward)
		p->offset += nl_end - nl_begin;
	else
		p->offset -= nl_end - nl_begin;
	if (! (n_enum == p->itr_find >> 8))
		return -1;
u8 nl_find;
	if (! (nl_begin < (nl_find = (0xff & p->itr_find)) +1 && nl_find < nl_end))
		return -1;
	// matched
	if (p->is_forward)
		p->offset -= nl_end - nl_find;
	else
		p->offset += nl_find - nl_begin;
	return (int)nl_find;
}
int itr_find (Cmdg *m_, u32 itr_base, int offset, u32 itr_target)
{
	if (ITR_CUR == itr_base)
		itr_base = m_->n_cur << 8 | 0;
	if (ITR_CUR == itr_target)
		itr_target = m_->n_cur << 8 | 0;
	if (itr_target == itr_base)
		return 0;
	if (0 == offset)
		return offset;
struct itr_find cb;
	cb.itr_find = itr_target;
	cb.is_forward = (offset < 0) ? false : true;
	cb.offset = 0;
	itr_enum (m_, itr_base, offset, itr_find_cb, &cb);
	return cb.offset;
}

///////////////////////////////////////////////////////////////////////////////
// cursor management

static void csr_unlock (Cmdg *m_)
{
u8 bgfg_attr;
	bgfg_attr = (7 & m_->csr_state >> BG_ORIG_LSB) << ATTR_BG_LSB
	          | (7 & m_->csr_state >> FG_ORIG_LSB) << ATTR_FG_LSB;
	view_attr (m_->view, m_->x_cur, m_->y_cur, ATTR_BG_MASK | ATTR_FG_MASK, bgfg_attr);
	m_->csr_state &= ~(SHOWED | 7 << BG_ORIG_LSB
	                          | 7 << FG_ORIG_LSB);
}

static void csr_lock (Cmdg *m_, u16 x, u8 y)
{
unsigned a;
	a = view_attr (m_->view, m_->x_cur, m_->y_cur, ATTR_BG_MASK | ATTR_FG_MASK, CSR_BG_BGR8 << ATTR_BG_LSB
	                                                                          | CSR_FG_BGR8 << ATTR_FG_LSB);
	m_->csr_state &= ~(7 << BG_ORIG_LSB | 7 << FG_ORIG_LSB); // safety (not needed when call flow is on design)
	m_->csr_state |= SHOWED | (7 & a >> ATTR_BG_LSB) << BG_ORIG_LSB
	                        | (7 & a >> ATTR_FG_LSB) << FG_ORIG_LSB;
}

///////////////////////////////////////////////////////////////////////////////
// view communication

u32 cmdg_reput (Cmdg *m_, u32 itr_base, int offset)
{
u8 lf_height;
	if (! (m_->lf_height == (lf_height = font_get_height (m_->view)))) {
u16 height_trim_top;
		height_trim_top = m_->nl_trim_top * m_->lf_height + m_->px_trim_top;
		m_->nl_trim_top = (u8)(height_trim_top / lf_height);
		m_->px_trim_top = (u8)(height_trim_top % lf_height);
		m_->lf_height = lf_height;
	}
int n_top;
	n_top       = (ITR_CUR == itr_base) ? m_->n_top       : itr_base >> 8;
u8 nl_trim_top;
	nl_trim_top = (ITR_CUR == itr_base) ? m_->nl_trim_top : (u8)(0xff & itr_base);

u32 top_moved;
	top_moved = itr_seek (m_, n_top << 8 | nl_trim_top, offset);
int n_reput;
	n_reput = top_moved >> 8;
u8 nl_trim_reput;
	nl_trim_reput = (u8)(0xff & top_moved);

u8 ws_row;
	ws_row = view_get_rows (m_->view);
u16 ws_col;
	ws_col = view_get_cols (m_->view);
	if (SHOWED & m_->csr_state)
		if (m_->x_cur < ws_col && m_->y_cur < ws_row)
			csr_unlock (m_);
		else
			m_->csr_state &= ~SHOWED;
u8 y;
	for (y = 0; !(N_NULL == n_reput || ws_row -1 < y); n_reput = m_->EXUTF8s[n_reput].n_next, ++y) {
		if (N_NULL == m_->EXUTF8s[n_reput].n_next)
			m_->y_cur = y;
EXUTF8 *row;
		row = &m_->EXUTF8s[n_reput];
u8 nl_aux_reput;
		nl_aux_reput = text_get_nl_aux (row, m_->view);
u16 x;
u16 i;
u8 c;
		for (x = 0, i = 0; ((c = *(u8 *)&row->text[i]) || LINKED & row->flags) && x < ws_col; ++i, ++x) {
			if ('\0' == c) {
EXUTF8 *to, *from;
				to = row;
u16 from_cb;
				from_cb = strlen ((from = &m_->EXUTF8s[to->n_next])->text);
				text_expand (to, i + from_cb, (from->sgr) ? SGR_MODIFY : SGR_NOT_MODIFY);
				memmove (&to->text[i], from->text, from_cb +1);
				if (from->sgr)
					memmove (&to->sgr[i], from->sgr, sizeof(from->sgr[0]) * from_cb);
				to->flags &= ~LINKED;
				to->flags |= LINKED & from->flags;
				to->n_next = from->n_next;
				m_->EXUTF8s[from->n_next].n_prev = from->n_prev;
				// cursor
				if (&m_->EXUTF8s[m_->n_cur] == from) {
					m_->i_cur += i;
					m_->n_cur = n_reput;
					m_->x_cur += x;
				}
				// recycle
				text_recycle (m_, from - m_->EXUTF8s);
				//
				if ('\0' == (c = *(u8 *)&row->text[i]))
					break;
			}
			if ('\033' == c) {
u16 i_node;
				i_node = i_node_read (row, i);
				++i;
u16 img_col;
				img_col = image_get_cols (m_->view, i_node);
u16 i_matrix;
				i_matrix = (0 < nl_trim_reput) ? nl_trim_reput * img_col : 0;
				view_putg (m_->view, x, y, i_node, i_matrix, 0, 0);
u8 img_row;
				img_row = image_get_rows (m_->view, i_node);
u8 y_none;
				for (y_none = y +img_row; y_none < min(ws_row, y +1 +nl_aux_reput -nl_trim_reput); ++y_none) {
u16 x_none;
					for (x_none = x; x_none < min(ws_col, x +img_col); ++x_none) {
						view_attr (m_->view, x_none, y_none, (unsigned)-1, SGR_DEFAULT);
						view_putc (m_->view, x_none, y_none, ' ');
					}
				}
				x += img_col -1;
				continue;
			}
			if (0 == nl_trim_reput) {
				if (0x7f < c) {
					// TODO UTF-8 support
				}
u16 sgr;
				sgr = SGR_DEC(sgr_read (row, i));
				view_attr (m_->view, x, y, (unsigned)-1, sgr);
				view_putc (m_->view, x, y, c);
			}
u8 y_none;
			for (y_none = (0 < nl_trim_reput) ? y : y +1; y_none < min(ws_row, y +1 +nl_aux_reput -nl_trim_reput); ++y_none) {
				view_attr (m_->view, x, y_none, (unsigned)-1, SGR_DEFAULT);
				view_putc (m_->view, x, y_none, ' ');
			}
		}
		if (! ('\0' == c || x < ws_col)) {
u16 *from_sgr;
			from_sgr = (row->sgr) ? &row->sgr[i] : NULL;
char *from;
u16 from_cb;
			from_cb = strlen (from = &row->text[i]);
EXUTF8 *to;
			if (LINKED & row->flags) {
				to = &m_->EXUTF8s[row->n_next];
u16 to_cb;
				text_expand (to, from_cb + (to_cb = strlen (to->text)), (from_sgr) ? SGR_MODIFY : SGR_NOT_MODIFY);
				memmove (&to->text[from_cb], to->text, to_cb +1);
				if (to->sgr)
					memmove (&to->sgr[from_cb], to->sgr, sizeof(to->sgr[0]) * to_cb);
			}
			else {
				rows_insert (m_, n_reput, from_cb, (from_sgr) ? SGR_MODIFY : SGR_NOT_MODIFY);
				row = &m_->EXUTF8s[n_reput];
				row->flags |= LINKED;
				to = &m_->EXUTF8s[row->n_next];
			}
			memcpy (to->text, from, from_cb);
			if (from_sgr)
				memcpy (to->sgr, from_sgr, sizeof(row->sgr[0]) * from_cb);
			else if (to->sgr)
				memset (to->sgr, 0, sizeof(row->sgr[0]) * from_cb);
			//if (LINKED & row->flags)
			//	to->text[from_cb] = '\0';
			c = *from = '\0';
			// cursor
			if (n_reput == m_->n_cur && !(m_->i_cur < i)) {
				m_->i_cur -= i;
				m_->n_cur = row->n_next;
				m_->x_cur -= ws_col;
			}
			nl_aux_reput = text_get_nl_aux (row, m_->view);
		}
u16 x_none;
		for (x_none = x; x_none < ws_col; ++x_none) {
u8 y_none;
			for (y_none = y; y_none < ws_row; ++y_none) {
				view_attr (m_->view, x_none, y_none, (unsigned)-1, SGR_DEFAULT);
				view_putc (m_->view, x_none, y_none, ' ');
			}
		}
		if (n_reput == m_->n_cur && 0 == nl_trim_reput)
			csr_lock (m_, m_->x_cur, y);
		y += (nl_trim_reput < nl_aux_reput) ? nl_aux_reput - nl_trim_reput : 0;
//LOG3(0, "(n=%d,i=..) " VTYY "get_nl_aux" VTO "()=%d nl_trim=%d", n_reput, nl_aux_reput, nl_trim_reput)
		nl_trim_reput = 0;
	}
u8 y_none;
	for (y_none = y; y_none < ws_row; ++y_none) {
u16 x_none;
		for (x_none = 0; x_none < ws_col; ++x_none) {
			view_attr (m_->view, x_none, y_none, (unsigned)-1, SGR_DEFAULT);
			view_putc (m_->view, x_none, y_none, ' ');
		}
	}
	m_->n_top = top_moved >> 8;
	m_->nl_trim_top = (u8)(0xff & top_moved);
	if (! (0 == offset && ITR_CUR == itr_base))
		m_->px_trim_top = 0;
	return top_moved;
}

static u16 new_line (Cmdg *m_, bool bottom_cursor, u8 nl_aux)
{
u16 ws_row;
	ws_row = view_get_rows (m_->view);
	if (m_->y_cur +nl_aux +1 < ws_row) {
		if (bottom_cursor)
			m_->y_cur += nl_aux +1;
		nl_aux = 0;
	}
	else if (m_->y_cur +1 < ws_row) {
		nl_aux -= ws_row -1 -(m_->y_cur +1);
		if (bottom_cursor)
			m_->y_cur = ws_row -1;
	}
	else
		++nl_aux;
u8 nl_aux_top;
	nl_aux_top = text_get_nl_aux (&m_->EXUTF8s[m_->n_top], m_->view);
u8 rolled;
	for (rolled = 0; 0 < nl_aux; --nl_aux, ++rolled) {
		view_rolldown (m_->view);
		if (!bottom_cursor && 0 < m_->y_cur)
			--m_->y_cur;
		m_->px_trim_top = 0;
		if (! (++m_->nl_trim_top < 1 +nl_aux_top)) {
			m_->nl_trim_top = 0;
			m_->n_top = m_->EXUTF8s[m_->n_top].n_next;
			nl_aux_top = text_get_nl_aux (&m_->EXUTF8s[m_->n_top], m_->view);
		}
	}
	return rolled;
}

///////////////////////////////////////////////////////////////////////////////
// VT100 extent

static void on_ex_vt100_notify_size (void *this_, u32 width, u32 height, void *noset_image)
{
Cmdg *m_;

	m_ = (Cmdg *)this_;
u16 i_node;
	i_node = image_entry (m_->view, 0, noset_image, &m_->image_iface);
u16 lf_width, lf_height;
	lf_width = font_get_width (m_->view);
	lf_height = font_get_height (m_->view);
u16 img_cols, img_rows;
	img_cols = ALIGN(width, lf_width)/lf_width;
	img_rows = ALIGN(height, lf_height)/lf_height;
u16 ws_col, ws_row;
	ws_col = view_get_cols (m_->view);
	ws_row = view_get_rows (m_->view);
	m_->loading_dirty.x = m_->x_cur;
	m_->loading_dirty.y = m_->y_cur;
	m_->loading_dirty.n_cols = (m_->x_cur + img_cols < ws_col) ? img_cols : ws_col - m_->x_cur;
	m_->loading_dirty.n_rows = img_rows;
u16 rolled;
	if (m_->x_cur + img_cols < ws_col) {
		m_->x_cur += img_cols;
		if (m_->nl_aux_cur < img_rows -1)
			m_->nl_aux_cur = img_rows -1;
u16 i_matrix;
		i_matrix = 0;
		if (! (m_->y_cur +m_->nl_aux_cur < ws_row)) {
			rolled = new_line (m_, false, m_->nl_aux_cur -1);
			if (! (m_->nl_aux_cur < ws_row)) {
				m_->nl_aux_cur -= rolled;
				i_matrix += rolled * img_cols;
			}
			m_->loading_dirty.y = m_->y_cur;
		}
		text_putg (&m_->EXUTF8s[m_->n_cur], m_->i_cur, i_node);
		view_putg (m_->view, m_->x_cur - img_cols, m_->y_cur, i_node, i_matrix, 0, 0);
		m_->i_cur += 2;
		return;
	}
	m_->x_cur = 0;
#if 1
	rolled = new_line (m_, true, m_->nl_aux_cur);
	m_->nl_aux_cur = 0;
	if (! (m_->loading_dirty.y < rolled))
		m_->loading_dirty.y -= rolled;
	else if (rolled < m_->loading_dirty.y + img_rows) {
		m_->loading_dirty.n_rows -= m_->loading_dirty.y + img_rows - rolled;
		m_->loading_dirty.y = 0;
	}
	else
		m_->loading_dirty.n_rows = 0;
#else
	if (m_->y_cur +m_->nl_aux_cur +1 < ws_row) {
		m_->y_cur += 1 + m_->nl_aux_cur;
		m_->nl_aux_cur = 0;
	}
	else if (m_->y_cur +1 < ws_row) {
		m_->nl_aux_cur -= ws_row -1 -(m_->y_cur +1);
		m_->y_cur = ws_row -1;
	}
	else
		++m_->nl_aux_cur;
	while (0 < m_->nl_aux_cur--)
		view_rolldown (m_->view);
#endif
	// TODO
	//view_putg (m_->view, m_->x_cur, m_->y_cur, i_node, 0, 0, 0);
}
static void on_ex_vt100_notify_finish (void *this_, void *image)
{
Cmdg *m_;

	m_ = (Cmdg *)this_;
	// TODO draw not last one but per receive bytes block.
	if (0 < m_->loading_dirty.n_rows && 0 < 0 < m_->loading_dirty.n_cols)
		view_set_dirty (m_->view, m_->loading_dirty.x, m_->loading_dirty.y, m_->loading_dirty.n_cols, m_->loading_dirty.n_rows);
}
static size_t on_bmp_vt100_ex (Cmdg *m_, const void *buf_, size_t len)
{
const u8 *buf;
	buf = (const u8 *)buf_;

	// TODO
	if (0 == len) {
		// task reset
		bmp_query_interface (IID_IImage, &m_->image_iface);
		m_->image_iface.decode_start (&m_->decoder_state, sizeof(m_->decoder_state));
		return 0;
	}
decoder_callback i = {
		.malloc = malloc,
		.realloc = realloc,
		.notify_size = on_ex_vt100_notify_size,
		.notify_finish = on_ex_vt100_notify_finish,
	};
size_t written;
	written = m_->image_iface.decode (&m_->decoder_state, buf, len, &i, m_);
	return written;
}

///////////////////////////////////////////////////////////////////////////////
// VT100 translate

// TODO UTF-8 (and image) support
static void on_bs_vt100 (Cmdg *m_)
{
	if (0 < m_->i_cur) {
		--m_->i_cur;
		--m_->x_cur;
		return;
	}
EXUTF8 *cur;
	if (N_NULL == (cur = &m_->EXUTF8s[m_->n_cur])->n_prev)
		return;
EXUTF8 *prev;
	if (! (LINKED & (prev = &m_->EXUTF8s[cur->n_prev])->flags))
		return;
	m_->i_cur = strlen (prev->text) -1;
	m_->n_cur = cur->n_prev;
u16 ws_col;
	m_->x_cur = (ws_col = view_get_cols (m_->view)) -1;
	--m_->y_cur;
}

static void on_lf_vt100 (Cmdg *m_)
{
u32 n_got;
u8 nl_aux_got;
	if (! (N_NULL == (n_got = m_->EXUTF8s[m_->n_cur].n_next)))
		nl_aux_got = text_get_nl_aux (&m_->EXUTF8s[n_got], m_->view);
	else {
		n_got = rows_insert (m_, m_->n_cur, 0, SGR_NOT_MODIFY);
		nl_aux_got = 0;
	}
	m_->n_cur = n_got;
	new_line (m_, true, m_->nl_aux_cur);
	m_->nl_aux_cur = nl_aux_got;
}

static void on_cr_vt100 (Cmdg *m_)
{
	m_->i_cur = 0;
	m_->x_cur = 0;
}

static void on_cuu_vt100 (Cmdg *m_, int n)
{
#if 1
EXUTF8 *cur;
	cur = &m_->EXUTF8s[m_->n_cur];
int i;
	for (i = 0; i < n; ++i) {
		if (N_NULL == cur->n_prev)
			break;
EXUTF8 *cuu;
		cuu = &m_->EXUTF8s[cur->n_prev];
		if (N_NULL == cur->n_next && '\0' == *cur->text) {
			cuu->flags &= ~LINKED;
			cuu->n_next = N_NULL;
			text_recycle (m_, cur - m_->EXUTF8s);
		}
u8 nl_aux_cur;
		if (m_->y_cur < 1 +(nl_aux_cur = text_get_nl_aux (cuu, m_->view))) {
			// TODO
		}
		m_->y_cur -= 1 +nl_aux_cur;
		cur = cuu;
	}
	m_->n_cur = cur - m_->EXUTF8s;
#endif
}

static void on_cuf_vt100 (Cmdg *m_, int n)
{
int i;
	for (i = 0; i < n; ++i) {
		// TODO check line-tail and ws_col
		++m_->i_cur;
		++m_->x_cur;
	}
}

static void on_el_vt100 (Cmdg *m_, int n)
{
	if (! (0 == n))
		return; // TODO 1,2
u16 i, x;
	i = m_->i_cur;
	x = m_->x_cur;
u32 n_el;
	n_el = m_->n_cur;
EXUTF8 *el;
#if 0
	do {
#endif
		el = &m_->EXUTF8s[n_el];
size_t text_cb;
char *p;
		for (p = &el->text[i]; !('\0' == *p); ++p, ++x) {
			if ('\033' == *p) {
				// TODO view image clear
				*p++ = '\0';
			}
			else {
				// TODO support UTF-8
				view_attr (m_->view, x, m_->y_cur, (unsigned)-1, SGR_DEFAULT);
				view_putc (m_->view, x, m_->y_cur, ' ');
			}
			*p = '\0';
		}
#if 0
		x = i = 0;
		n_el = el->n_next;
	} while (LINKED & el->flags);
	while (! (&m_->EXUTF8s[m_->n_cur] == el)) {
		n_el = el->n_prev;
		text_recycle (m_, el - m_->EXUTF8s);
		(el = &m_->EXUTF8s[n_el])->flags &= ~LINKED;
	}
#endif
}

static void on_none_sgr_csi (Cmdg *m_)
{
	m_->sgr_cur = SGR_DEFAULT;
}

static void on_bold_sgr_csi (Cmdg *m_)
{
	m_->sgr_cur |= SGR_BOLD_MASK;
}

static void on_fgcl_sgr_csi (Cmdg *m_, int cl)
{
	m_->sgr_cur &= ~SGR_FG(7);
	m_->sgr_cur |= SGR_FG(cl);
}

static void on_title_chg_osc (Cmdg *m_, const char *title)
{
	view_set_title (m_->view, title);
}

static void on_ansi_vt100 (Cmdg *m_, int c)
{
u16 ws_col;
	ws_col = view_get_cols (m_->view);
	if (! (m_->x_cur < ws_col)) {
		m_->x_cur = 0;
		m_->i_cur = 0;
u32 n_cur_old;
		m_->n_cur = rows_insert (m_, n_cur_old = m_->n_cur, 0, SGR_NOT_MODIFY);
		m_->EXUTF8s[n_cur_old].flags |= LINKED;
		new_line (m_, true, m_->nl_aux_cur);
		m_->nl_aux_cur = 0;
	}
	if (NULL == m_->EXUTF8s[m_->n_cur].text)
		text_init (&m_->EXUTF8s[m_->n_cur], ws_col, SGR_NOT_MODIFY);
	text_putc (&m_->EXUTF8s[m_->n_cur], m_->i_cur, c, m_->sgr_cur);
	view_attr (m_->view, m_->x_cur, m_->y_cur, ~ATTR_REV_MASK, m_->sgr_cur);
	view_putc (m_->view, m_->x_cur, m_->y_cur, c);
	++m_->x_cur;
	++m_->i_cur;
}

static bool query_interface (IID easy_impl, void *unk)
{
	switch (easy_impl) {
	case IID_IVt100Action:
	{
IVt100Action *i = (IVt100Action *)unk;
		i->on_bmp = on_bmp_vt100_ex;
		i->on_bs = on_bs_vt100;
		i->on_lf = on_lf_vt100;
		i->on_cr = on_cr_vt100;
		i->on_cuu = on_cuu_vt100;
		i->on_cuf = on_cuf_vt100;
		i->on_ansi = on_ansi_vt100;
		i->on_none_sgr = on_none_sgr_csi;
		i->on_bold_sgr = on_bold_sgr_csi;
		i->on_fgcl_sgr = on_fgcl_sgr_csi;
		i->on_title_chg = on_title_chg_osc;
		i->on_el = on_el_vt100;
		break;
	}
	default:
		return false;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// framework

enum {
	LCTRL = 1 << 0,
	RCTRL = 1 << 1,
};
static bool write_gtkkey (guint keyval, bool pressed, void *param)
{
Cmdg *m_;
	m_ = (Cmdg *)param;
static const struct {
	int c;
	const char *cstr;
} map[] = {
	  { 0x51, "\033[D" }
	, { 0x52, "\033[A" }
	, { 0x53, "\033[C" }
	, { 0x54, "\033[B" }
	, { 0xff, "\033[3\176" }
	, {    0, NULL } // safety
};
unsigned hi;
	if (! (0x00 == (hi = /*0xff &*/ keyval >> 8) || 0xff == hi))
		return false;
u8 c;
	c = (u8)(0xff & keyval);
const char *cstr;
size_t len;
	cstr = (char *)&c;
	len = 1;
	if (hi) {
u8 mask;
int n;
		switch (c) {
		case 0x08:
		case 0x09:
		case 0x0d:
		case 0x1b:
			break;
		case 0x51: case 0x52: case 0x53: case 0x54:
		case 0xff:
			n = 0;
			while (map[n].cstr && !(c == map[n].c))
				++n;
			if (!map[n].cstr)
				return false;
			cstr = map[n].cstr;
			len = strlen (map[n].cstr);
			break;
		case 0xe3:
		case 0xe4:
			mask = (0xe3 == c) ? LCTRL : RCTRL;
			m_->keyshift &= ~mask;
			if (pressed)
				m_->keyshift |= mask;
			return false;
		default:
			return false;
		}
	}
	else if ((LCTRL|RCTRL) & m_->keyshift && 'a' <= c && c <= 'z')
		c -= 'a' -1;
	if (!pressed)
		return false;
	// ui-thread: gtkin -> shell // TODO sub-thread impl (should not prevent ui-thread)
const char *buf;
size_t nread;
ssize_t written;
	buf = cstr;
	nread = len;
	while (! (nread == (written = m_->remote->write (&m_->remote_, buf, nread)))) {
		if (! (0 < written)) {
fprintf (stderr_old, VTRR "WARN" VTO " write(shell) return %d, dispose %d bytes." "\n", written, nread);
			break;
		}
		nread -= written;
		buf += written;
	}
	return true;
}

# ifndef READ_SHELL_THREAD
ssize_t cmdg_read_to_view (Cmdg *m_)
# else
static ssize_t read_to_view (Cmdg *m_)
# endif
{
ssize_t nread;

	// gtkout <- shell
	if (0 < (nread = m_->remote->read (&m_->remote_, m_->read_shell_buf, READ_SHELL_BUFMAX))) {
		csr_unlock (m_);
		vt100_parse (m_->vt100, m_->read_shell_buf, nread);
		csr_lock (m_, m_->x_cur, m_->y_cur);
	}
	return nread;
}

#ifdef READ_SHELL_THREAD
static void *read_shell_thread (void *param)
{
	Cmdg *m_;
	m_ = (Cmdg *)param;

ssize_t nread;
	while (! (-1 == (nread = read_to_view (m_)) && EIO == errno)) {
		if (0 < nread)
			continue;
		usleep (FRAME_MS * 1000);
	}

	view_on_shell_disconnect (m_->view);
	return NULL;
}
#endif

bool cmdg_connect (Cmdg *m_, const char *hostname, u16 port, const char *ppk_path)
{
#ifndef SSH_NOT_AVAILABLE
	if (! (true == ssh2_preconnect (&m_->remote_, port, hostname, ppk_path)))
		return false;
#endif
	m_->lf_height = font_get_height (m_->view);
u16 ws_col, ws_row;
	ws_col = view_get_cols (m_->view);
	ws_row = view_get_rows (m_->view);
	m_->remote->connect (&m_->remote_, ws_col, ws_row);

#ifdef READ_SHELL_THREAD
pthread_t term_id;
	if (! (0 == pthread_create (&term_id, NULL, read_shell_thread, (void *)m_)))
		return false;
#endif
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// dtor/ctor

void cmdg_dtor (Cmdg *m_)
{
	int n;

	if (m_->vt100)
		vt100_dtor (m_->vt100);

	if (m_->EXUTF8s) {
		for (n = 0; n < m_->n_max; ++n)
			if (m_->EXUTF8s[n].text)
				free (m_->EXUTF8s[n].text);
		free (m_->EXUTF8s);
	}

	if (m_->read_shell_buf)
		free (m_->read_shell_buf);

	free (m_);
}

Cmdg *cmdg_ctor (View *view, u16 ws_col, u32 n_max)
{
	Cmdg *m_;

bool unexpected;
	unexpected = true;
	do {
		if (NULL == (m_ = (Cmdg *)malloc_s (sizeof(Cmdg))))
			break;
		memset (m_, 0, sizeof(Cmdg));
# ifdef SSH_NOT_AVAILABLE
		if (NULL == (m_->remote = ptmx_ctor (&m_->remote_, sizeof(m_->remote_))))
# else
		if (NULL == (m_->remote = ssh2_ctor (&m_->remote_, sizeof(m_->remote_))))
# endif
			break;
		if (NULL == (m_->read_shell_buf = (char *)malloc_s (READ_SHELL_BUFMAX)))
			break;

		m_->n_max = n_max;
		m_->view = view;
		m_->sgr_cur = SGR_DEFAULT;
		if (NULL == (m_->EXUTF8s = (EXUTF8 *)malloc_s (sizeof(EXUTF8) * m_->n_max)))
			break;
		memset (m_->EXUTF8s, 0, sizeof(EXUTF8) * m_->n_max);
		text_init (&m_->EXUTF8s[m_->n_cur = 1], ws_col, SGR_NOT_MODIFY);
		//m_->EXUTF8s[m_->n_cur].n_prev = m_->EXUTF8s[m_->n_cur].n_next = N_NULL;
		m_->n_top = m_->n_cur;
		m_->n_blank_top = m_->n_cur +1;
IVt100Action iface;
		query_interface (IID_IVt100Action, &iface);
		if (NULL == (m_->vt100 = vt100_ctor (&iface, m_)))
			break;
		unexpected = false;
	} while (0);
	if (unexpected) {
		cmdg_dtor (m_);
		return NULL;
	}

	view_connect_gtkkey_receiver (m_->view, write_gtkkey, (void *)m_);
	csr_lock (m_, 0, 0);
	return m_;
}
