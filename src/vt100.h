#ifndef VT100_H_INCLUDED__
# define VT100_H_INCLUDED__
# ifdef __cplusplus
extern "C" {
# endif // __cplusplus

#define VTO  "\033[0m"
#define VTRR "\033[1;31m"
#define VTGG "\033[1;32m"
#define VTYY "\033[1;33m"
#define VTMM "\033[1;35m"
#define VTCC "\033[1;36m"
//#define BS "\010" // = "\b"
#define EL "\033[K"

# ifdef __cplusplus
}
# endif // __cplusplus
#endif // VT100_H_INCLUDED__
