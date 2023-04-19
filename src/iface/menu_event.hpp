#ifndef MENU_EVENT_H_INCLUDED__
#define MENU_EVENT_H_INCLUDED__

struct menu_event_i {
	void (* on_test) (menu_event_s *this_, GtkWidget *da);
};

#endif // MENU_EVENT_H_INCLUDED__
