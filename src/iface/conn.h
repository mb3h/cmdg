#ifndef IFACE_CONN_H_INCLUDED__
#define IFACE_CONN_H_INCLUDED__

struct conn_i {
	// IUnknown
	void (*release) (void *this_);
	// IFileDescriptor
	bool (*connect) (void *this_, uint16_t ws_col, uint16_t ws_row);
	ssize_t (*write) (void *this_, const void *buf, size_t cb);
	ssize_t (*read) (void *this_, void *buf, size_t cb);
};

#endif // IFACE_CONN_H_INCLUDED__
