#ifndef FFWRAPPERS_DEFINED
#define FFWRAPPERS_DEFINED
#include "ff.h"
#ifdef __cplusplus
extern "C" {
#endif

int normalize_path(const char *input, char *output, size_t output_size);
FRESULT my_chdir(const TCHAR *path);
FRESULT my_getcwd(TCHAR *buffer, UINT len);

#ifdef __cplusplus
}
#endif

#define f_chdir my_chdir
#define f_getcwd my_getcwd
#endif
