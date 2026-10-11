#include <stdint.h>
#include "lock.hpp"

#include "vt100.h" // TODO: removing
#include "assert.h"

#include <stdbool.h>
#include <stdlib.h> // exit
#include <pthread.h>

#define usizeof(x) (unsigned)sizeof(x) // part of 64bit supports.

///////////////////////////////////////////////////////////////////////////////

typedef struct lock_ {
	pthread_mutex_t mtx;
} lock_s;

void lock_dtor (struct lock *this_)
{
lock_s *m_;
	m_ = (lock_s *)this_;
	pthread_mutex_destroy (&m_->mtx);
}
void lock_ctor (struct lock *this_, unsigned cb)
{
BUGc(sizeof(lock_s) <= cb, " %d <= " VTRR "%d" VTO, usizeof(lock_s), cb)
lock_s *m_;
	m_ = (lock_s *)this_;
	pthread_mutex_init (&m_->mtx, NULL);
}
void lock_lock (struct lock *this_)
{
lock_s *m_;
	m_ = (lock_s *)this_;
	pthread_mutex_lock (&m_->mtx);
}
void lock_unlock (struct lock *this_)
{
lock_s *m_;
	m_ = (lock_s *)this_;
	pthread_mutex_unlock (&m_->mtx);
}
