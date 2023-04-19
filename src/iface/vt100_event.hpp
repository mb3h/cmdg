#ifndef VT100_EVENT_H_INCLUDED__
#define VT100_EVENT_H_INCLUDED__

struct vt100_event_i {
	void (*csi_ich) (vt100_event_s *this_, uint16_t n);                       // CSI @
	void (*csi_cuu) (vt100_event_s *this_, uint16_t n);                       // CSI A
	void (*csi_cuf) (vt100_event_s *this_, uint16_t n);                       // CSI C
	void (*csi_cup) (vt100_event_s *this_, uint16_t y, uint16_t x);           // CSI H
	void (*csi_ed) (vt100_event_s *this_, uint16_t n);                        // CSI J
	void (*csi_el) (vt100_event_s *this_, uint16_t n);                        // CSI K
	void (*csi_il) (vt100_event_s *this_, uint16_t n);                        // CSI L
	void (*csi_dl) (vt100_event_s *this_, uint16_t n);                        // CSI M
	void (*csi_dch) (vt100_event_s *this_, uint16_t n);                       // CSI P
	void (*csi_sgr_none) (vt100_event_s *this_);                              // CSI 0m
	void (*csi_sgr_bold) (vt100_event_s *this_, _Bool is_enabled);            // CSI 1|22m
	void (*csi_sgr_italic) (vt100_event_s *this_, _Bool is_enabled);          // CSI 3|23m
	void (*csi_sgr_reverse) (vt100_event_s *this_, _Bool is_enabled);         // CSI 7|27m
	void (*csi_sgr_strikethrough) (vt100_event_s *this_, _Bool is_enabled);   // CSI 9|29m
	void (*csi_sgr_fgcl) (vt100_event_s *this_, uint8_t cl);                  // CSI 30-37m 38;5;[0-255]m
	void (*csi_sgr_fgcl_raw) (vt100_event_s *this_, uint32_t rgb24);          // CSI 38;2[;:][0-255][;:][0-255][;:][0-255]m
	void (*csi_sgr_bgcl) (vt100_event_s *this_, uint8_t cl);                  // CSI 40-47m 48;5;[0-255]m
	void (*csi_sgr_bgcl_raw) (vt100_event_s *this_, uint32_t rgb24);          // CSI 48;2[;:][0-255][;:][0-255][;:][0-255]m
	void (*csi_dsr) (vt100_event_s *this_, uint8_t n);                        // CSI 5|6n
	void (*csi_dec_stbm) (vt100_event_s *this_, uint16_t tm, uint16_t bm);    // CSI r
	void (*csi_dec_slpp) (vt100_event_s *this_, uint8_t op_id, uint8_t aux1, uint8_t aux2); // CSI [1-23];<Ps2>;<Ps3>t
	void (*csi_da2) (vt100_event_s *this_, uint16_t n);                       // CSI > c
	void (*csi_dec_exm) (vt100_event_s *this_, uint16_t n, _Bool is_enabled); // CSI ? [1-8200][hl]
	void (*osc_title_chg) (vt100_event_s *this_, const char *title);          // OSC [0-2];<title>BEL
	void (*osc_vtwin_fgcl_query) (vt100_event_s *this_);                      // OSC 10;?BEL
	void (*osc_vtwin_bgcl_query) (vt100_event_s *this_);                      // OSC 11;?BEL
	void (*esc_ri) (vt100_event_s *this_);                                    // ESC M
	void (*esc_dec_kpam) (vt100_event_s *this_);                              // ESC =
	void (*esc_dec_kpnm) (vt100_event_s *this_);                              // ESC >
	unsigned (*esc_bmp) (vt100_event_s *this_, const void *buf, unsigned len);
};

#endif // VT100_EVENT_H_INCLUDED__
