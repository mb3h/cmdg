#ifndef CONN_HPP_INCLUDED__
#define CONN_HPP_INCLUDED__

struct conn_i {
	// IUnknown
	void (*release) (void *this_);
	// IFileDescriptor
	_Bool (*connect) (void *this_, uint16_t ws_col, uint16_t ws_row);
	int (*write) (void *this_, const void *buf, unsigned cb);
	int (*read) (void *this_, void *buf, unsigned cb);
};

#endif // CONN_HPP_INCLUDED__
