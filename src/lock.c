#ifdef WIN32
#else
# include <pthread.h>
#endif
#include <malloc.h>
#include "defs.h"

#include "lock.h"
///////////////////////////////////////////////////////////////////////////////
#ifdef WIN32 // win32

// TODO implements by CriticalSection.

///////////////////////////////////////////////////////////////////////////////
#else // linux

void lock_dtor (Lock *m_)
{
pthread_mutex_t *mtx;
	if (NULL == (mtx = (pthread_mutex_t *)m_->priv))
		return;
	pthread_mutex_destroy (mtx);
	free (mtx);
	m_->priv = NULL;
}
bool lock_ctor (Lock *m_)
{
pthread_mutex_t *mtx;
	if (NULL == (mtx = (pthread_mutex_t *)malloc (sizeof(pthread_mutex_t))))
		return false;
	pthread_mutex_init (mtx, NULL);
	m_->priv = mtx;
	return true;
}
void lock_lock (Lock *m_)
{
pthread_mutex_t *mtx;
	if (NULL == (mtx = (pthread_mutex_t *)m_->priv))
		return;
	pthread_mutex_lock (mtx);
}
void lock_unlock (Lock *m_)
{
pthread_mutex_t *mtx;
	if (NULL == (mtx = (pthread_mutex_t *)m_->priv))
		return;
	pthread_mutex_unlock (mtx);
}

///////////////////////////////////////////////////////////////////////////////
#endif
