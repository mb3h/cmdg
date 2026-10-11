#define _XOPEN_SOURCE 600 // SUSv3 (includes POSIX.1-2001) posix_openpt grantpt unlockpt ptsname
#include <features.h>
//#include <endian.h>
#include <stdint.h>

#include "pseudo_vt100.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> // posix_openpt
#include <errno.h>
#include <memory.h>
#include <alloca.h>
#include <unistd.h> // fork
#include <fcntl.h> // O_RDWR
#include <sys/ioctl.h> // winsize TIOCSWINSZ
#include <termios.h> // tcgetattr tcsetattr

typedef  uint8_t u8, u1;
typedef uint16_t u16;

#define FALSE 0
#define TRUE  1
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

#define flow_bug   assert
#define caller_bug assert

///////////////////////////////////////////////////////////////
// singlton memory

typedef struct {
	int ptm; // send/recv pipe
#ifdef __i386__
//	u8 align4_padding[];
#else //def __x86_64__
	u8 align8_padding[4];
#endif
} ptmx_s;

static ptmx_s s_singlton = {0};

///////////////////////////////////////////////////////////////

static int _write_input (const void *buf, unsigned cb)
{
ptmx_s *m_ = &s_singlton;

	return write (m_->ptm, buf, cb);
}

static int _read_output (void *buf, unsigned cb)
{
ptmx_s *m_ = &s_singlton;

	return read (m_->ptm, buf, cb);
}

static void _modifyws (u16 ws_col, u16 ws_row)
{
ptmx_s *m_ = &s_singlton;

	// terminal window size
struct winsize ws;
	ws.ws_col    = ws_col;
	ws.ws_row    = ws_row;
	ws.ws_xpixel = 0;
	ws.ws_ypixel = 0;
	ioctl (m_->ptm, TIOCSWINSZ, &ws);
}

static void _modifytio (bool allowLFtoCRLF)
{
ptmx_s *m_ = &s_singlton;

	// prevent/allow LF to CRLF conversion
struct termios tio;
	if (! (0 == tcgetattr (m_->ptm, &tio)))
		return; // tcgetattr() failed
	if (!allowLFtoCRLF && (ONLCR & tio.c_oflag)) {
		tio.c_oflag &= ~ONLCR;
		tcsetattr (m_->ptm, TCSANOW, &tio);
	}
	else if (allowLFtoCRLF && !(ONLCR & tio.c_oflag)) {
		tio.c_oflag |= ONLCR;
		tcsetattr (m_->ptm, TCSANOW, &tio);
	}
}

static bool _connect (u16 ws_col, u16 ws_row,
                      bool allowLFtoCRLF)
{
ptmx_s *m_ = &s_singlton;

flow_bug(0 == m_->ptm)
	m_->ptm = posix_openpt (O_RDWR | O_NONBLOCK);
	grantpt (m_->ptm);
	unlockpt (m_->ptm);
	_modifyws (ws_col, ws_row);
	_modifytio (allowLFtoCRLF);
char *pts_name;
	pts_name = ptsname (m_->ptm);

	// sub-process: bash thunk
	if (0 == fork ()) {
		setsid ();
int pts
		= open (pts_name, O_RDWR | O_NONBLOCK);
		close (m_->ptm);
		m_->ptm = -1;

//		_modifyws (ws_col, ws_row);
//		_modifytio (allowLFtoCRLF);

u1 forked_process_redirecting_available
		= FALSE;
		do {
			if (-1 == dup2 (pts, STDIN_FILENO))
				break;
			if (-1 == dup2 (pts, STDOUT_FILENO))
				break;
			if (-1 == dup2 (pts, STDERR_FILENO))
				break;
			forked_process_redirecting_available = TRUE;
		} while (0);
		close (pts);
		if (!forked_process_redirecting_available)
			exit (-1);

const char *shell_path
		= getenv ("SHELL");
u1 default_shell_available
		= (shell_path && shell_path[0]) ? TRUE : FALSE;
		if (!default_shell_available)
			shell_path = "/bin/sh";

char *shell_path_safety // regacy main() safety (not needed?)
	= (char *)alloca (strlen (shell_path) +1);
	strcpy (shell_path_safety, shell_path);
char *argv[]
		= { shell_path_safety, NULL };
		execvp (shell_path, argv);
		exit (0);
	}
	return true;
}

///////////////////////////////////////////////////////////////
// readability sugar

static const READABILITY_SUGAR_PSEUDO_VT100 _pseudo_terminal = {
	.connect = _connect,
	.write_input = _write_input,
	.read_output = _read_output,
	.modifyws = _modifyws,
};
const READABILITY_SUGAR_PSEUDO_VT100 *pseudo_vt100 = &_pseudo_terminal;
