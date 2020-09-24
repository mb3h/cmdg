#ifndef LOCK_H_INCLUDED__
#define LOCK_H_INCLUDED__

#ifdef WIN32
// TODO implements by CriticalSection.

#else // gnu-linux
struct Lock__ {
	void *priv;
};
typedef struct Lock__ Lock;

void lock_dtor (Lock *m_);
bool lock_ctor (Lock *m_);
void lock_lock (Lock *m_);
void lock_unlock (Lock *m_);

#endif

#endif // LOCK_H_INCLUDED__
