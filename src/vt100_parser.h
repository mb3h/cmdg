#ifndef VT100_PARSER_H_INCLUDED__
#define VT100_PARSER_H_INCLUDED__

//void vt100_parser_release ();
void escseq_reset ();
unsigned escseq_get_paused_len ();
int escseq_write (const void *buf_, unsigned len);

void on_csi_ich (uint16_t n);                       // CSI @
void on_csi_cuu (uint16_t n);                       // CSI A
void on_csi_cuf (uint16_t n);                       // CSI C
void on_csi_cup (uint16_t y, uint16_t x);           // CSI H
void on_csi_ed (/*u2*/ uint8_t n);                  // CSI J
void on_csi_el (/*u2*/ uint8_t n);                  // CSI K
void on_csi_il (uint16_t n);                        // CSI L
void on_csi_dl (uint16_t n);                        // CSI M
void on_csi_dch (uint16_t n);                       // CSI P
void on_csi_sgr_none ();                            // CSI 0m
void on_csi_sgr_bold (_Bool is_enabled);            // CSI 1|22m
void on_csi_sgr_italic (_Bool is_enabled);          // CSI 3|23m
void on_csi_sgr_underline (_Bool is_enabled);       // CSI 4|24m
void on_csi_sgr_reverse (_Bool is_enabled);         // CSI 7|27m
void on_csi_sgr_strikethrough (_Bool is_enabled);   // CSI 9|29m
void on_csi_sgr_fgcl3 (/*u24*/ uint8_t cl);         // CSI 30-37m 38;5;[0-255]m
void on_csi_sgr_fgcl24 (/*u24*/ uint32_t rgb24);    // CSI 38;2[;:][0-255][;:][0-255][;:][0-255]m
void on_csi_sgr_bgcl3 (/*u24*/ uint8_t cl);         // CSI 40-47m 48;5;[0-255]m
void on_csi_sgr_bgcl24 (/*u24*/ uint32_t rgb24);    // CSI 48;2[;:][0-255][;:][0-255][;:][0-255]m
void on_csi_dsr (uint8_t n);                        // CSI 5|6n
void on_csi_dec_stbm (uint16_t tm, uint16_t bm);    // CSI r
void on_csi_dec_slpp (uint8_t op_id, uint8_t aux1, uint8_t aux2); // CSI [1-23];<Ps2>;<Ps3>t
void on_csi_da2 (uint16_t n);                       // CSI > c
void on_csi_xtmodkeys (uint8_t n1, uint8_t n2);     // CSI > m
void on_csi_dec_exm (uint16_t n, _Bool is_enabled); // CSI ? [1-8200][hl]
void on_vim_unknown_1_csi ();                       // CSI ? 4m

void on_osc_set_window_title (const char *title);   // OSC [0-2];<title>BEL
void on_osc_hyperlinks (const char *uri);           // OSC 8;[<id>];<uri>ST
void on_osc_fg_rgb_query ();                        // OSC 10;?BEL
void on_osc_bg_rgb_query ();                        // OSC 11;?BEL

void on_esc_ri ();                                  // ESC M
void on_esc_dcs (const char *s);                    // ESC P<s>ST
void on_esc_dec_kpam ();                            // ESC =
void on_esc_dec_kpnm ();                            // ESC >

#endif // VT100_PARSER_H_INCLUDED__
