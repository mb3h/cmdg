#ifndef CONFIG_READER_H_INCLUDED__
#define CONFIG_READER_H_INCLUDED__

#ifndef config_reader_s
# define config_reader_s void
#endif

struct config_reader_i {
	_Bool (*read_s) (config_reader_s *this_, const char *key, const char *defval, char *buf, unsigned cb);
	unsigned (*read_u) (config_reader_s *this_, const char *key, unsigned defval);
	uint32_t (*read_rgb24) (config_reader_s *this_, const char *key, uint32_t defval);
};

#endif // CONFIG_READER_H_INCLUDED__
