#ifndef NATIVE_DEPTH_PORTABLE_H
#define NATIVE_DEPTH_PORTABLE_H
#define OPENK4A_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <k4a/k4a.h>
#define OPENK4A_DISTANT 3.0e38f
#define OPENK4A_LOG_WARNING 2
#define OPENK4A_LOG_INFO 3
void *openk4a_alloc(size_t n);
void *openk4a_alloc_zero(size_t n);
void openk4a_free(void *p);
uint32_t openk4a_le32(const uint8_t *p);
uint64_t openk4a_le64(const uint8_t *p);
float openk4a_le_float(const uint8_t *p);
uint64_t openk4a_now_fine_nsec(void);
bool openk4a_exe_dir(char *out, size_t size);
void openk4a_log(int level, const char *fmt, ...);
#include "portable_types.h"
#endif
