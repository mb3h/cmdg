#include "log.h"
#include "assert.h"

#define _POSIX_C_SOURCE 200809L // POSIX.1-2008 vdprintf
#include <features.h>

#include <stdint.h>
#include <stdio.h> // sprintf
#include <stdlib.h> // exit
#include <stdarg.h>
#include <sys/time.h> // gettimeofday
#include <time.h> // localtime
#include <unistd.h> // STDERR_FILENO

static int  logfd = STDERR_FILENO;
static int echofd = STDOUT_FILENO;

void log_ (const char *format, ...)
{
BUG(format)

va_list ap;
	va_start(ap, format);
	vdprintf (logfd, format, ap);
	va_end(ap);
}

void echo (const char *format, ...)
{
BUG(format)

va_list ap;
	va_start(ap, format);
	vdprintf (echofd, format, ap);
	va_end(ap);
}
