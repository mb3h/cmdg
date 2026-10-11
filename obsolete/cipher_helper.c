///////////////////////////////////////////////////////////////////////////////
// utility (cipher)

#include "cipher_helper.h"

unsigned cipher_align_from_type (unsigned cipher_type)
{
unsigned retval;
	switch (cipher_type) {
	case CIPHER_TYPE_AES256_CBC: retval = 16; break;
	default: return 1; // unsupported type
	}
	return retval;
}
