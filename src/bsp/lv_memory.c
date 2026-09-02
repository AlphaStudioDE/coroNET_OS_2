#include "lv_memory.h"

#include "esp_heap_caps.h"

static const uint32_t kPsramCaps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
static const uint32_t kInternalCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;

void* coronet_lv_malloc(size_t size) {
    if (size == 0) return NULL;
    void* pointer = heap_caps_malloc(size, kPsramCaps);
    return pointer ? pointer : heap_caps_malloc(size, kInternalCaps);
}

void* coronet_lv_realloc(void* pointer, size_t size) {
    if (!pointer) return coronet_lv_malloc(size);
    if (size == 0) {
        heap_caps_free(pointer);
        return NULL;
    }

    void* resized = heap_caps_realloc(pointer, size, kPsramCaps);
    return resized ? resized : heap_caps_realloc(pointer, size, kInternalCaps);
}

void coronet_lv_free(void* pointer) {
    heap_caps_free(pointer);
}
