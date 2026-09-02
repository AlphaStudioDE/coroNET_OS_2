#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* coronet_lv_malloc(size_t size);
void* coronet_lv_realloc(void* pointer, size_t size);
void coronet_lv_free(void* pointer);

#ifdef __cplusplus
}
#endif
