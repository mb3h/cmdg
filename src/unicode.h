#ifndef UNICODE_H_INCLUDED__
#define UNICODE_H_INCLUDED__

//#include <wchar.h> // wchar_t

int wcwidth_bugfix (wchar_t c);
/*u21*/ uint32_t u21from (uint32_t utf8);

#endif // UNICODE_H_INCLUDED__
