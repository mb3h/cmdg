#ifndef PSEUDO_VT100_H_INCLUDED__
#define PSEUDO_VT100_H_INCLUDED__

/* Readability sugar.
   Never use 'READABILITY_SUGAR_PSEUDO_VT100' directly,
   only use 'pseudo_vt100'. */
typedef struct {
	_Bool (*connect) (uint16_t ws_col, uint16_t ws_row,
	                  _Bool allowLFtoCRLF);
	int (*write_input) (const void *buf, unsigned cb);
	int (*read_output) (void *buf, unsigned cb);
	void (*modifyws) (uint16_t ws_col, uint16_t ws_row);
} READABILITY_SUGAR_PSEUDO_VT100;
extern const READABILITY_SUGAR_PSEUDO_VT100 *pseudo_vt100;

#endif // PSEUDO_VT100_H_INCLUDED__
