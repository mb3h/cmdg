#define _POSIX_C_SOURCE 200809 // strdup (>= 200809) strtoul (>= 200112)
#include <features.h>
#include <stdint.h>

#include "conf.h"
#include "helper/pow2_ge.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> // exit
#include <string.h>
#include <errno.h>
#include <fcntl.h> // mode_t
#include <pwd.h> // getpwuid
#include <unistd.h> // access getuid
#include <sys/stat.h> // mkdir
typedef  uint8_t u8, u1;

#define ustrlen(x) (unsigned)strlen(x) // part of 64bit supports.

#define FALSE 0
#define TRUE  1
///////////////////////////////////////////////////////////////
// configuration

#include "appconst.h"
#define CONF_FILENAME "cmdg.conf"

#define DEFAULT_FONT_NAME "Ubuntu Mono 12"

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
printf (VTRR "ASSERT" VTO ": " #expr " [%s]" "\n", __func__); \
			exit (-1); \
		} \
	} while (0);

#define caller_bug assert
#define   flow_bug assert

///////////////////////////////////////////////////////////////

typedef struct {
	unsigned lookup;
	unsigned offset;
	char origin;
} MODIFIED;

typedef struct {
	MODIFIED *modified;
	unsigned modified_used;
	char *file_buf;
	unsigned file_size;
	char *confdir;
	u8 read_failed :1;
	u8 padding     :7;
} conf_s;
static conf_s s_singlton = {0};

///////////////////////////////////////////////////////////////

static bool _mkdir_p (const char *dirpath, mode_t mode)
{
u1 is_fullpath
	= ('/' == *dirpath) ? TRUE : FALSE;
char *copied_path
	= strdup (dirpath);

unsigned sep
	= (is_fullpath) ? 1 : 0;
	while (sep < strlen (dirpath)) {
		sep += strcspn (dirpath +sep, "/");
		copied_path[sep] = '\0';
u1 mkdir_failed
		= !(0 == mkdir (copied_path, mode)) ? TRUE : FALSE;
u1 still_existed
		= (mkdir_failed && EEXIST == errno) ? TRUE : FALSE;
		if (mkdir_failed && !still_existed) {
			free (copied_path);
			return false;
		}
		copied_path[sep++] = '/';
	}
	free (copied_path);
	return true;
}

static bool _create_file (const char *path, mode_t mode)
{
FILE *fp;
	if (! (fp = fopen (path, "wb")))
		return false;

	fprintf (fp, "#" CONF_FONT_NAME " = " DEFAULT_FONT_NAME "\n");
	fprintf (fp, "#" CONF_WS_WIDTH  " = " "\n");
	fprintf (fp, "#" CONF_WS_HEIGHT " = " "\n");
	fprintf (fp, "#" CONF_WS_X      " = " "\n");
	fprintf (fp, "#" CONF_WS_Y      " = " "\n");

	fclose (fp);
	return true;
}

static void *_read_file (const char *path, unsigned *got_)
{
FILE *fp
	= fopen (path, "rb");
	if (NULL == fp)
		return NULL;
	fseek (fp, 0, SEEK_END);
unsigned fsize
	= (unsigned)ftell (fp);
	fseek (fp, 0, SEEK_SET);
char *buf
	= (char *)malloc (fsize +1);
unsigned got
	= (unsigned)fread (buf, 1, fsize, fp);
	fclose (fp);
	buf[got] = '\0';

	if (got_)
		*got_ = got;
	return buf;
}

static char *_make_confdir ()
{
char *confdir, *env; const char *homedir;
	// case #1: $XDG_CONFIG_HOME available
	if (env = getenv("XDG_CONFIG_HOME")) {
		confdir = (char *)malloc (strlen(env) +1 +APPNAME_LEN +1);
		sprintf (confdir, "%s" "/" APPNAME, env);
	}
	else {
		// case #2-1: $HOME available
		if (homedir = getenv("HOME"))
			;
		// case #2-2: password file available
		else {
struct passwd *pwd;
			if (! (pwd = getpwuid (getuid ())))
				return NULL;
			if (!pwd->pw_dir)
				return NULL;
			homedir = pwd->pw_dir;
		}
		// case #2-shared: homedir -> confdir
unsigned needed
		= strlen(homedir) +1 +7 +1 +APPNAME_LEN +1;
		confdir = (char *)malloc (needed);
		sprintf (confdir, "%s/.config/" APPNAME, homedir);
	}
	// directory still existed or generating success?
struct stat st;
	if (0 == stat (confdir, &st) && S_ISDIR(st.st_mode)
	 || _mkdir_p (confdir, 0700))
		return confdir;

	// directory cannot prepared
	free (confdir);
	return NULL;
}

static char *_seek (const char *key)
{
conf_s *m_ = &s_singlton;

caller_bug(key && '\0' != *key)

u1 read_succeeded
	= (m_->file_buf) ? TRUE : FALSE;
	if (!read_succeeded && !m_->read_failed) {
		// make path
		if (NULL == m_->confdir)
			m_->confdir = _make_confdir ();
unsigned needed
		= strlen (m_->confdir) +1 +strlen(CONF_FILENAME) +1;
char *path
		= (char *)malloc (needed);
		sprintf (path, "%s/" CONF_FILENAME, m_->confdir);
		// try to read
		do {
struct stat st;
			if (! (0 == access (path, F_OK)))
				if (!_create_file (path, 0700))
					break;

			m_->file_buf = (char *)_read_file (path, &m_->file_size);
		} while (0);
		free (path);
		// result
		read_succeeded = (m_->file_buf) ? TRUE : FALSE;
	}
	if (!read_succeeded) {
		m_->read_failed = TRUE;
		return NULL;
	}

unsigned key_len
	= ustrlen (key);
char *p = m_->file_buf, *end = p +m_->file_size;
	while (p && p < end) {
		do {
			p += strspn (p, " \t");
			if (! (0 == memcmp (p, key, key_len)))
				break;
			p += key_len;
			p += strspn (p, " \t");
			if (! ('=' == *p))
				break;
			++p;
			p += strspn (p, " \t");
			return p;
		} while (0);
		p += strcspn (p, "\n");
		if (p < end)
			++p;
	}
	return NULL;
}

static const MODIFIED *_lookup_modification (unsigned lookup)
{
conf_s *m_ = &s_singlton;

const MODIFIED *p, *end;
	if (NULL == (p = m_->modified))
		return NULL;

	end = p +m_->modified_used;
	for (; p < end; ++p) // TRADEOFF: (*1)
		if (lookup == p->lookup)
			return p;
	return NULL;
}

static void _save_modification (unsigned lookup,
                                unsigned offset, char origin)
{
conf_s *m_ = &s_singlton;

u1 nothing_used
	= !(0 < m_->modified_used && m_->modified) ? TRUE : FALSE;

	if (nothing_used) {
flow_bug(NULL == m_->modified)
		m_->modified_used = 0;
unsigned needed
		= pow2_ge((m_->modified_used +1));
		m_->modified = (MODIFIED *)malloc (sizeof(MODIFIED) * needed);
	}
	else if (pow2_ge(m_->modified_used) < pow2_ge(m_->modified_used +1)) {
unsigned needed
		= pow2_ge((m_->modified_used +1));
		m_->modified = (MODIFIED *)realloc (m_->modified,
		                                    sizeof(MODIFIED) * needed);
	}
MODIFIED *p
	= &m_->modified[m_->modified_used];
	p->lookup = lookup;
	p->offset = offset;
	p->origin = origin;
	++m_->modified_used;
}

static unsigned _get_end_of_line_or_token (const char *val)
{
conf_s *m_ = &s_singlton;

caller_bug(m_->file_buf <= val)
caller_bug(val < m_->file_buf +m_->file_size)
unsigned cstr_begin
	= (unsigned)(val -m_->file_buf);

const MODIFIED *modified
	= _lookup_modification (cstr_begin);
	if (modified)
		return modified->offset;

flow_bug('\0' == m_->file_buf[m_->file_size])
unsigned found_end
	= (unsigned)strcspn (m_->file_buf +cstr_begin, "\n" "\r");
	return found_end;
}

///////////////////////////////////////////////////////////////

static const char *_read_cstr (const char *key,
                               const char *err_cstr)
{
conf_s *m_ = &s_singlton;

	do {
		if (m_->read_failed)
			break;
char *val
		= _seek (key);
u1 key_not_found
		= (NULL == val) ? TRUE : FALSE;
		if (key_not_found)
			break;

u1 quart_opened
		= ('"' == *val) ? TRUE : FALSE;
		if (quart_opened)
			++val;

unsigned cstr_len
		= _get_end_of_line_or_token (val);
		if (quart_opened && 0 < cstr_len) {
char *p, *end;
			p = val +strcspn (val, "\"" "\n");
			end = m_->file_buf +m_->file_size;
u1 quart_not_closed
			= (!(p < end) || '\n' == *p) ? TRUE : FALSE;
			if (quart_not_closed)
				break;
			else /*if ('\0' == *p || '\"' == *p)*/
				cstr_len = (unsigned)(p -val); 
		}
			
u1 empty_value
		= (0 == cstr_len) ? TRUE : FALSE;
		if (empty_value)
			break;

u1 available_as_cstr
		= ('\0' == val[cstr_len]) ? TRUE : FALSE;
		if (!available_as_cstr) {
			_save_modification ((unsigned)(val -m_->file_buf),
			                    cstr_len, val[cstr_len]);
			val[cstr_len] = '\0';
		}
		return val;
	} while (0);

	return err_cstr;
}

static unsigned _read_uint (const char *key,
                            unsigned err_uint)
{
conf_s *m_ = &s_singlton;

	do {
		if (m_->read_failed)
			break;
char *val
		= _seek (key);
u1 key_not_found
		= (NULL == val) ? TRUE : FALSE;
		if (key_not_found)
			break;

unsigned uint_len
		= (unsigned)strspn (val, "0123456789");
u1 empty_value
		= (0 == uint_len) ? TRUE : FALSE;
		if (empty_value)
			break;

u1 available_as_uint
		= ('\0' == val[uint_len]) ? TRUE : FALSE;
		if (!available_as_uint) {
			_save_modification ((unsigned)(val -m_->file_buf),
			                    uint_len, val[uint_len]);
			val[uint_len] = '\0';
		}
unsigned val_uint
		= (unsigned)atoi (val);
		return val_uint;
	} while (0);

	return err_uint;
}

static void _dtor ()
{
conf_s *m_ = &s_singlton;

	if (m_->modified)
		free (m_->modified), m_->modified = NULL;
	if (m_->file_buf)
		free (m_->file_buf), m_->file_buf = NULL;
	if (m_->confdir)
		free (m_->confdir), m_->confdir = NULL;
}

///////////////////////////////////////////////////////////////
// readability sugar

static const READABILITY_SUGAR_CONF _conf = {
	.dtor = _dtor,
	.read_uint = _read_uint,
	.read_cstr = _read_cstr,
};
const READABILITY_SUGAR_CONF *conf = &_conf;

///////////////////////////////////////////////////////////////
// footnote

/* TRADEOFF: (*1)
   don't pay binary seek testing cost, until the necessary will
   get obvious. */
