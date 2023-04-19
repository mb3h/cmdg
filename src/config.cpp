#define _POSIX_C_SOURCE 200809 // strdup (>= 200809) strtoul (>= 200112)
#include <features.h>

#include <stdint.h>
struct config_;
#define config_reader_s struct config_
#include "config.hpp"

#include "lock.hpp"
typedef struct lock lock_s;
#include "log.h"
#include "vt100.h" // TODO: removing
#include "assert.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <alloca.h>
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h> // close write access getuid
#include <sys/stat.h>
typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;

#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.
#define ustrlen(x) (unsigned)strlen(x) // part of 64bit supports.

#include "appconst.h"
///////////////////////////////////////////////////////////////////////////////

static bool mkdir_p (const char *dirpath, mode_t mode)
{
bool retval;
	retval = true;
char *dup;
	dup = strdup (dirpath);
ASSERTE(dup)
unsigned pos;
	pos = ('/' == *dirpath) ? 1 : 0;
	while (pos < strlen (dirpath)) {
		pos += strcspn (dirpath +pos, "/");
		dup[pos] = '\0';
		if (! (0 == mkdir (dup, mode) || EEXIST == errno)) {
log_ ("%s: " VTRR "directory cannot create" VTO "." "\n", dup);
			retval = false;
			break;
		}
		dup[pos++] = '/';
	}
	free (dup);
	return retval;
}

///////////////////////////////////////////////////////////////////////////////
// non-state

static bool config_create_file (const char *path, mode_t mode)
{
int fd;
	if (-1 == (fd = open (path, O_WRONLY | O_CREAT | O_EXCL, mode))) {
log_ ("%s: " VTRR "file cannot create" VTO ". '%s'(%d)" "\n", path, strerror (errno), errno);
			return false;
	}
	// write default configuration
char *DEFAULT_CONFIG; size_t cb;
	DEFAULT_CONFIG = (char *)alloca (cb = 1024);
	snprintf (DEFAULT_CONFIG, cb, 
		     "#" CFG_SGRCL_PREFIX "0 = " "0x%06x"
		"\n" "#" CFG_SGRCL_PREFIX "1 = " "0x%06x"
		"\n" "#" CFG_SGRCL_PREFIX "2 = " "0x%06x"
		"\n" "#" CFG_SGRCL_PREFIX "3 = " "0x%06x"
		"\n" "#" CFG_SGRCL_PREFIX "4 = " "0x%06x"
		"\n" "#" CFG_SGRCL_PREFIX "5 = " "0x%06x"
		"\n" "#" CFG_SGRCL_PREFIX "6 = " "0x%06x"
		"\n" "#" CFG_SGRCL_PREFIX "7 = " "0x%06x"
		"\n" "#" CFG_WS_COL " = %d"
		"\n" "#" CFG_WS_ROW " = %d"
		"\n" "#" CFG_MAX_BACKLOG " = %d"
		"\n" "#" CFG_FONT " = %s"
		"\n" "#" CFG_PANGO_UNDRAW " = %d"
		, 0xffffff & (DEFAULT_COLOR_0) , 0xffffff & (DEFAULT_COLOR_1)
		, 0xffffff & (DEFAULT_COLOR_2) , 0xffffff & (DEFAULT_COLOR_3)
		, 0xffffff & (DEFAULT_COLOR_4) , 0xffffff & (DEFAULT_COLOR_5)
		, 0xffffff & (DEFAULT_COLOR_6) , 0xffffff & (DEFAULT_COLOR_7)
		, DEFAULT_WS_COL, DEFAULT_WS_ROW, DEFAULT_BACKLOG
		, DEFAULT_FONT, DEFAULT_PANGO_UNDRAW
		);
unsigned total, unput;
	total = unput = ustrlen(DEFAULT_CONFIG);
int put;
	while (0 < unput) {
		if (-1 == (put = (int)write (fd, DEFAULT_CONFIG +total -unput, unput))) {
log_ ("%s: " VTRR "write abort" VTO ". '%s'(%d)" "\n", path, strerror (errno), errno);
			break;
		}
		unput -= put;
	}
	close (fd);
	if (0 < unput)
		return false;
	// safety
	if (!(0 == access (path, F_OK))) {
log_ ("%s: " VTRR "file cannot read" VTO " (unexpected). '%s'(%d)" "\n", path, strerror (errno), errno);
		return false;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// private

typedef struct config_ {
	char *basedir;
	char *rawdata;
} config_s;

static const char *config_get_basedir (config_s *m_)
{
	if (m_->basedir)
		return m_->basedir;
	do {
char *env;
		if (env = getenv("XDG_CONFIG_HOME")) {
			m_->basedir = (char *)malloc (strlen(env) +1 +APPNAME_LEN +1);
			sprintf (m_->basedir, "%s" "/" APPNAME, env);
			break;
		}
const char *home;
		if (NULL == (home = getenv("HOME"))) {
struct passwd *pwd;
			pwd = getpwuid (getuid ());
			if (! (pwd && pwd->pw_dir))
				return NULL;
			home = pwd->pw_dir;
		}
		m_->basedir = (char *)malloc (strlen(home) +1 +7 +1 +APPNAME_LEN +1);
		sprintf (m_->basedir, "%s/.config/" APPNAME, home);
	} while (0);
struct stat st;
	if (!(0 == stat (m_->basedir, &st) && S_ISDIR(st.st_mode)) && !mkdir_p (m_->basedir, 0700))
		exit (-1);
	return m_->basedir;
}

static const char *config_get_rawdata (config_s *m_)
{
	// second later read
	if (m_->rawdata)
		return m_->rawdata;
	// first read
const char *confdir;
	confdir = config_get_basedir (m_);
char *path;
	path = (char *)malloc (strlen (confdir) +10 +1);
	sprintf (path, "%s/cmdg.conf", confdir);
struct stat st;
	if (!(0 == access (path, F_OK)) && !config_create_file (path, 0700))
		exit (-1);
FILE *fp;
	if (NULL == (fp = fopen (path, "rb"))) {
log_ ("%s: " VTRR "file cannot open" VTO "." "\n", path);
		exit (-1);
	}
	fseek (fp, 0, SEEK_END);
unsigned fsize;
	fsize = (unsigned)ftell (fp);
	fseek (fp, 0, SEEK_SET);
	m_->rawdata = (char *)malloc (fsize +2);
ASSERTE(m_->rawdata)
unsigned got;
	got = (unsigned)fread (m_->rawdata, 1, fsize, fp);
	fclose (fp);
	if (! (fsize == got)) {
log_ ("%s: " VTRR "reading not enough" VTO ". (req %d but res %d bytes)." "\n", path, fsize, got);
		exit (-1);
	}
unsigned i;
	for (i = 0; i < fsize; ++i)
		if ('\n' == m_->rawdata[i])
			m_->rawdata[i] = '\0';
	(m_->rawdata+ fsize)[0] = '\0';
	(m_->rawdata+ fsize)[1] = '\0';
	return m_->rawdata;
}

static const char *config_seek (config_s *m_, const char *key)
{
	if (NULL == key || '\0' == *key)
		return NULL;
const char *text, *scan;
	text = config_get_rawdata (m_);
	while (! ('\0' == *text)) {
		scan = text + strspn (text, " \t");
		text = strchr (text, '\0') +1;
		if (! (0 == memcmp (scan, key, strlen(key))))
			continue;
		scan += strlen(key);
		scan += strspn (scan, " \t");
		if (! ('=' == *scan))
			continue;
		++scan;
		scan += strspn (scan, " \t");
		return scan;
	}
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// interface

static bool config_read_s (config_s *m_, const char *key, const char *defval, char *buf, unsigned cb)
{
const char *val;
	if (NULL == (val = config_seek (m_, key)) || '\0' == *val) {
		strncpy (buf, defval, cb);
		return false;
	}
	strncpy (buf, val, cb);
	return true;
}

static unsigned config_read_u (config_s *m_, const char *key, unsigned defval)
{
const char *val;
	if (NULL == (val = config_seek (m_, key)) || !isdigit(*val))
		return defval;
	return atoi(val);
}

static u32 config_read_rgb24 (config_s *m_, const char *key, u32 defval)
{
const char *val;
	if (NULL == (val = config_seek (m_, key)) || '\0' == *val)
		return defval;
u32 rgb24; char *endptr;
	if (0 == memcmp ("0x", val, 2))
		rgb24 = strtoul (val +2, &endptr, 16);
	else
		rgb24 = atoi (val);
	return rgb24;
}

///////////////////////////////////////////////////////////////////////////////
// dtor/ctor

static void config_reset (config_s *m_)
{
#ifndef __cplusplus
	memset (m_, 0, sizeof(config_s));
#else //def __cplusplus
	m_->basedir = NULL;
	m_->rawdata = NULL;
#endif // __cplusplus
}

static void config_dtor (struct config *this_)
{
config_s *m_;
	m_ = (config_s *)this_;
	if (m_->rawdata)
		free (m_->rawdata);
	if (m_->basedir)
		free (m_->basedir);
	config_reset (m_);
}

static void config_ctor (struct config *this_, unsigned cb)
{
#ifndef __cplusplus
BUGc(sizeof(config_s) <= cb, " %d <= " VTRR "%d" VTO, usizeof(config_s), cb)
#endif // __cplusplus
config_s *m_;
	m_ = (config_s *)this_;
	config_reset (m_);
}

///////////////////////////////////////////////////////////////////////////////
// singlton

static lock_s s_thread_lock;
#define _lock_cleanup() lock_dtor(&s_thread_lock);
#define _lock_warmup() lock_ctor(&s_thread_lock, sizeof(s_thread_lock));
#define _lock() lock_lock(&s_thread_lock);
#define _unlock() lock_unlock(&s_thread_lock);

static struct config s_obj, *s_ptr = NULL;
struct config *config_get_singlton ()
{
_lock()
	if (NULL == s_ptr) {
		config_ctor (&s_obj, sizeof(s_obj));
		s_ptr = &s_obj;
	}
_unlock()
	return s_ptr;
}

// PENDING: call get_singlton() between _lock() .. _unlock() (*)define as invalid client code.
void config_cleanup()
{
_lock()
	if (s_ptr) {
		s_ptr = NULL;
		config_dtor (&s_obj);
	}
_unlock()
_lock_cleanup()
}

void config_warmup() { _lock_warmup() }

///////////////////////////////////////////////////////////////////////////////

static struct config_reader_i o_config_reader_i = {
	.read_s     = config_read_s,
	.read_u     = config_read_u,
	.read_rgb24 = config_read_rgb24,
};
struct config_reader_i *config_query_config_reader_i (struct config *this_) { return &o_config_reader_i; }
