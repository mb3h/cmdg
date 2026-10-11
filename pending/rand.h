#ifndef RAND_H_INCLUDED__
#define RAND_H_INCLUDED__

void random_init ();
void random_add_noise (const void *src, unsigned cb);
void store_random (void *dst, unsigned cb);

#endif //def RAND_H_INCLUDED__
