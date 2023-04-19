#define _XOPEN_SOURCE 600 // SUSv3 (includes POSIX.1-2001) posix_openpt grantpt unlockpt ptsname
#include <features.h>

#include <stdint.h>
#include "ptmx.hpp"

#include "log.h"
#include "vt100.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> // posix_openpt
#include <errno.h>
#include <memory.h>
#include <alloca.h>
#include <unistd.h> // fork
#include <fcntl.h> // O_RDWR
#include <sys/ioctl.h> // winsize TIOCSWINSZ

typedef uint16_t u16;

#define unixerror strerror
#define errno_pts errno

///////////////////////////////////////////////////////////////////////////////

typedef struct ptmx_ ptmx_s;
struct ptmx_ {
	int ptm; // send/recv pipe
};

static int ptmx_write (void *this_, const void *buf, unsigned cb)
{
ptmx_s *m_;
	m_ = (ptmx_s *)this_;
	return write (m_->ptm, buf, cb);
}

static int ptmx_read (void *this_, void *buf, unsigned cb)
{
ptmx_s *m_;
	m_ = (ptmx_s *)this_;
	return read (m_->ptm, buf, cb);
}

static bool ptmx_connect (void *this_, u16 ws_col, u16 ws_row)
{
ptmx_s *m_;
	m_ = (ptmx_s *)this_;
	m_->ptm = posix_openpt (O_RDWR | O_NONBLOCK);
	grantpt (m_->ptm);
	unlockpt (m_->ptm);
char *pts_name;
	pts_name = ptsname (m_->ptm);

	// sub-process: bash thunk
	if (0 == fork ()) {
		setsid ();
int pts;
		pts = open (pts_name, O_RDWR | O_NONBLOCK);
		close (m_->ptm);

struct winsize ws;
		ws.ws_col    = ws_col;
		ws.ws_row    = ws_row;
		ws.ws_xpixel = 0;
		ws.ws_ypixel = 0;
		ioctl (pts, TIOCSWINSZ, &ws);

bool unexpected;
		unexpected = true;
		do {
			if (-1 == dup2 (pts, STDIN_FILENO))
				break;
			if (-1 == dup2 (pts, STDOUT_FILENO))
				break;
			if (-1 == dup2 (pts, STDERR_FILENO))
				break;
			unexpected = false;
		} while (0);
		close (pts);
		if (unexpected) {
log_ (VTRR "ERROR" VTO " pipe cannot connected stdin/stdout/stderr. @err=%d'%s'" "\n", errno_pts, unixerror (errno_pts));
			exit (-1);
		}
const char *shell_path;
		if (NULL == (shell_path = getenv ("SHELL")) || '\0' == *shell_path)
			shell_path = "/bin/sh";
char *safety; // const removing (*)for regacy main()
	safety = (char *)alloca (strlen (shell_path) +1), strcpy (safety, shell_path);
char *argv[]
		= { safety, NULL };
		execvp (shell_path, argv);
		exit (0);
	}
	return true;
}

static void ptmx_dtor (void *this_)
{
ptmx_s *m_;
	m_ = (ptmx_s *)this_;
	memset (m_, 0, sizeof(ptmx_s));
}

static struct conn_i ptmx;
struct conn_i *ptmx_ctor (void *this_, unsigned cb)
{
	if (cb < sizeof(ptmx_s))
		return NULL;
ptmx_s *m_;
	m_ = (ptmx_s *)this_;
	memset (m_, 0, sizeof(ptmx_s));
	return &ptmx;
}

static struct conn_i ptmx = {
	ptmx_dtor,
	ptmx_connect,
	ptmx_write,
	ptmx_read,
};
