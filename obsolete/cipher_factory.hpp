#ifndef CIPHER_FACTORY_HPP_INCLUDED__
#define CIPHER_FACTORY_HPP_INCLUDED__

#include "iface/cipher.hpp"

struct cipher_i *cipher_factory (struct cipher *this_, const char *name);

#endif // CIPHER_FACTORY_HPP_INCLUDED__
