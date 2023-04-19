#ifndef PARSER_EVENT_H_INCLUDED__
#define PARSER_EVENT_H_INCLUDED__

struct parser_event_i {
	void (* dirty_notify) (parser_event_s *this_);
};

#endif // PARSER_EVENT_H_INCLUDED__
