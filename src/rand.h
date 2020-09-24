#ifndef RAND_H_IS_INCLUDED__
#define RAND_H_IS_INCLUDED__

void random_init ();
void random_add_noise (const void *src, size_t cb);
void store_random (void *dst, size_t cb);

#endif //def RAND_H_IS_INCLUDED__
