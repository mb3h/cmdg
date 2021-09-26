#define _GNU_SOURCE // strtoul
#include <features.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>
#include "defs.h"

#include "conf.h"
extern FILE *stderr_old;
#include "vt100.h"
typedef uint32_t u32;
typedef  uint8_t u8;
typedef uint16_t u16;

#define WS_COL_DEFAULT 80
#define WS_ROW_DEFAULT 24
#define BACKLOG_DEFAULT 1000
#define WS_COL_MIN 4
#define WS_ROW_MIN 1
#define FONT_DEFAULT "Ubuntu Mono 13" // fixed font
#define PANGO_UNDRAW_DEFAULT 1
///////////////////////////////////////////////////////////////////////////////

static bool mkdir_p (const char *dirpath, mode_t mode)
{
bool retval;
	retval = true;
char *dup;
	dup = strdup (dirpath);
ASSERTE(dup)
size_t pos;
	pos = ('/' == *dirpath) ? 1 : 0;
	while (pos < strlen (dirpath)) {
		pos += strcspn (dirpath +pos, "/");
		dup[pos] = '\0';
		if (! (0 == mkdir (dup, mode) || EEXIST == errno)) {
fprintf (stderr_old, "%s: " VTRR "directory cannot create" VTO "." "\n", dup);
			retval = false;
			break;
		}
		dup[pos++] = '/';
	}
	free (dup);
	return retval;
}

///////////////////////////////////////////////////////////////////////////////
// 

static const char *s_config_def =
     "#sgr_col.0 = 000000"
"\n" "#sgr_col.1 = ff0000"
"\n" "#sgr_col.2 = 00ff00"
"\n" "#sgr_col.3 = ffff00"
"\n" "#sgr_col.4 = 0000ff"
"\n" "#sgr_col.5 = ff00ff"
"\n" "#sgr_col.6 = 00ffff"
"\n" "#sgr_col.7 = d3d7cf"
"\n" "#ws_col = 80"
"\n" "#ws_row = 24"
"\n" "#n_max = 1000"
"\n" "#font_name = Ubuntu Mono 13"
"\n" "#pango_undraw = 1"
;

static bool config_file_default (const char *path, mode_t mode)
{
int fd;
	if (-1 == (fd = open (path, O_WRONLY | O_CREAT | O_EXCL, mode))) {
fprintf (stderr_old, "%s: " VTRR "file cannot create" VTO ". '%s'(%d)" "\n", path, strerror (errno), errno);
			return false;
	}
	// write default configuration
size_t total, unput;
	total = unput = strlen (s_config_def);
ssize_t put;
	while (0 < unput) {
		if (-1 == (put = write (fd, s_config_def +total -unput, unput))) {
fprintf (stderr_old, "%s: " VTRR "write abort" VTO ". '%s'(%d)" "\n", path, strerror (errno), errno);
			break;
		}
		unput -= put;
	}
	close (fd);
	if (0 < unput)
		return false;
	// safety
	if (!(0 == access (path, F_OK))) {
fprintf (stderr_old, "%s: " VTRR "file cannot read" VTO " (unexpected). '%s'(%d)" "\n", path, strerror (errno), errno);
		return false;
	}
	return true;
}

#define APPNAME "cmdg"
#define APPNAME_LEN 4

static char *s_confdir = NULL;
static const char *config_basedir_get ()
{
	if (s_confdir)
		return s_confdir;
	do {
char *env;
		if (env = getenv("XDG_CONFIG_HOME")) {
			s_confdir = (char *)malloc (strlen(env) +1 +APPNAME_LEN +1);
			sprintf (s_confdir, "%s" "/" APPNAME, env);
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
		s_confdir = (char *)malloc (strlen(home) +1 +7 +1 +APPNAME_LEN +1);
		sprintf (s_confdir, "%s/.config/" APPNAME, home);
	} while (0);
struct stat st;
	if (!(0 == stat (s_confdir, &st) && S_ISDIR(st.st_mode)) && !mkdir_p (s_confdir, 0700))
		exit (-1);
	return s_confdir;
}

static const char *config_file_cache ()
{
static char *retval = NULL;
	if (retval)
		return retval;
const char *confdir;
	confdir = config_basedir_get ();
char *path;
	path = (char *)malloc (strlen (confdir) +10 +1);
	sprintf (path, "%s/cmdg.conf", confdir);
struct stat st;
	if (!(0 == access (path, F_OK)) && !config_file_default (path, 0700))
		exit (-1);
FILE *fp;
	if (NULL == (fp = fopen (path, "rb"))) {
fprintf (stderr_old, "%s: " VTRR "file cannot open" VTO "." "\n", path);
		exit (-1);
	}
	fseek (fp, 0, SEEK_END);
size_t fsize;
	fsize = ftell (fp);
	fseek (fp, 0, SEEK_SET);
	retval = (char *)malloc (fsize +2);
ASSERTE(retval)
	fread (retval, 1, fsize, fp);
	fclose (fp);
size_t i;
	for (i = 0; i < fsize; ++i)
		if ('\n' == retval[i])
			retval[i] = '\0';
	(retval+ fsize)[0] = '\0';
	(retval+ fsize)[1] = '\0';
	return retval;
}

static const char *config_seek (const char *key)
{
	if (NULL == key || '\0' == *key)
		return NULL;
const char *text, *scan;
	text = config_file_cache ();
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

static bool config_read_s (const char *key, const char *defval, char *buf, size_t cb)
{
const char *val;
	if (NULL == (val = config_seek (key)) || '\0' == *val) {
		strncpy (buf, defval, cb);
		return false;
	}
	strncpy (buf, val, cb);
	return true;
}

static unsigned config_read_u (const char *key, unsigned defval)
{
const char *val;
	if (NULL == (val = config_seek (key)) || !isdigit(*val))
		return defval;
	return atoi(val);
}

static u32 config_read_bgr24 (const char *key, u32 defval)
{
const char *val;
	if (NULL == (val = config_seek (key)) || '\0' == *val)
		return defval;
u32 rgb24;
char *endptr;
	rgb24 = strtoul (val, &endptr, 16);
u32 bgr24;
	bgr24 = 0xFF & rgb24 >> 16 | 0xFF00 & rgb24 | 0xFF0000 & rgb24 << 16;
	return bgr24;
}

static struct {
	const u32 bgr24[8];
} s_ = {
	{ 0, 0xff, 0x00ff00, 0x00ffff, 0xff0000, 0xff00ff, 0xffff00, 0xcfd7d3 }
};
bool config_get (Config *conf)
{
	if (NULL == conf)
		return false;
	conf->ws_col = (u16)config_read_u ("ws_col", WS_COL_DEFAULT);
	conf->ws_row = (u16)config_read_u ("ws_row", WS_ROW_DEFAULT);
	conf->n_max = (u32)config_read_u ("n_max", BACKLOG_DEFAULT);
	conf->ws_col_min = WS_COL_MIN;
	conf->ws_row_min = WS_ROW_MIN;
	config_read_s ("font_name", FONT_DEFAULT, conf->font_name, sizeof(conf->font_name) -1);
	conf->pango_undraw = (u8)config_read_u ("pango_undraw", PANGO_UNDRAW_DEFAULT);
int n;
	for (n = 0; n < 8; ++n) {
char key[9 +1];
		sprintf (key, "sgr_col.%d", n);
		conf->bgr24[n] = config_read_bgr24 (key, s_.bgr24[n]);
	}
	return true;
}
