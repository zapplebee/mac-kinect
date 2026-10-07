#include "portable.h"
#include <stdarg.h>
#include <time.h>
void *openk4a_alloc(size_t n) { return malloc(n); }
void *openk4a_alloc_zero(size_t n) { return calloc(1, n); }
void openk4a_free(void *p) { free(p); }
uint32_t openk4a_le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
uint64_t openk4a_le64(const uint8_t *p) { return openk4a_le32(p) | (uint64_t)openk4a_le32(p+4)<<32; }
float openk4a_le_float(const uint8_t *p) { uint32_t u=openk4a_le32(p); float f; memcpy(&f,&u,4); return f; }
uint64_t openk4a_now_fine_nsec(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000000000+(uint64_t)t.tv_nsec; }
bool openk4a_exe_dir(char *out, size_t size) { (void)out; (void)size; return false; }
void openk4a_log(int level, const char *fmt, ...) { (void)level; va_list ap; va_start(ap,fmt); vfprintf(stderr,fmt,ap); va_end(ap); fputc('\n',stderr); }
