#ifndef IDLE_EVENT_H_INCLUDED__
#define IDLE_EVENT_H_INCLUDED__

struct idle_event_i {
	_Bool (* on_idle) (idle_event_s *this_);
};

#endif // IDLE_EVENT_H_INCLUDED__
