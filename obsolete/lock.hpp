#ifndef LOCK_HPP_INCLUDED__
#define LOCK_HPP_INCLUDED__

# ifdef __cplusplus
extern "C" {
# endif // __cplusplus

struct lock {
#  ifdef __x86_64__
	uint8_t priv[40]; // gcc's value to x86_64
#  else // __i386__
	uint8_t priv[24]; // gcc's value to i386
#  endif
};
void lock_dtor (struct lock *this_);
void lock_ctor (struct lock *this_, unsigned cb);
void lock_lock (struct lock *this_);
void lock_unlock (struct lock *this_);

typedef struct lock Lock;

# ifdef __cplusplus
}
# endif // __cplusplus

#endif // LOCK_HPP_INCLUDED__
