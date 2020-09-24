#ifndef CMDG_H_INCLUDED__
#define CMDG_H_INCLUDED__

typedef struct Cmdg__ Cmdg;

void cmdg_dtor (Cmdg *m_);
Cmdg *cmdg_ctor (View *view, uint16_t ws_col, uint32_t n_max);
bool cmdg_connect (Cmdg *m_, const char *hostname, uint16_t port, const char *ppk_path);
uint32_t cmdg_reput (Cmdg *m_, uint32_t itr_base, int offset);
#ifndef READ_SHELL_THREAD
ssize_t cmdg_read_to_view (Cmdg *m_);
#endif

#define ITR_CUR (uint32_t)-1
uint32_t itr_seek (Cmdg *m_, uint32_t itr_base, int offset);
int itr_find (Cmdg *m_, uint32_t itr_base, int offset, uint32_t itr_target);

#endif // CMDG_H_INCLUDED__
