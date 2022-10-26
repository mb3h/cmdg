#ifdef WIN32
# define uint16_t WORD
 // TODO complete others.
#else
# define _XOPEN_SOURCE 600 // posix_openpt (>= 600)
# include <features.h>
# include <stdlib.h> // posix_openpt
# include <alloca.h>
# include <unistd.h> // fork
# include <fcntl.h> // O_RDWR
# include <termios.h> // termios ICANON ..
# include <sys/ioctl.h> // winsize TIOCSWINSZ
#endif
#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include "defs.h"
typedef uint16_t u16;

#include "ptmx.h"
#include "vt100.h"
extern FILE *stderr_old;

typedef struct ptmx_ ptmx_s;
struct ptmx_ {
	int ptm; // send/recv pipe
};

static ssize_t ptmx_write (void *this_, const void *buf, size_t cb)
{
ptmx_s *m_;
	m_ = (ptmx_s *)this_;
	return write (m_->ptm, buf, cb);
}

static ssize_t ptmx_read (void *this_, void *buf, size_t cb)
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
fprintf (stderr_old, VTRR "ERROR" VTO " pipe cannot connected stdin/stdout/stderr. @err=%d'%s'" "\n", errno, unixerror (errno));
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
struct conn_i *ptmx_ctor (void *this_, size_t cb)
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
