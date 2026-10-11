#ifndef LOG_H_INCLUDED__
# define LOG_H_INCLUDED__
# ifdef __cplusplus
extern "C" {
# endif // __cplusplus

void log_ (const char *format, ...);
void echo (const char *format, ...);

# ifdef __cplusplus
}
# endif // __cplusplus
#endif // LOG_H_INCLUDED__
