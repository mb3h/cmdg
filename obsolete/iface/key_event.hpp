#ifndef KEY_EVENT_H_INCLUDED__
#define KEY_EVENT_H_INCLUDED__

struct key_event_i {
	int (* on_key) (key_event_s *this_, const char *buf, unsigned cb);
};

#endif // KEY_EVENT_H_INCLUDED__
