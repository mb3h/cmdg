#ifndef UNICODE_H_INCLUDED__
#define UNICODE_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

int wcwidth_bugfix (wchar_t c);
uint16_t utf8to16 (uint32_t u);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // UNICODE_H_INCLUDED__
