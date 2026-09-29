/* ----------------------------------------------------------------------------
 * umm_malloc.h - a memory allocator for embedded systems (microcontrollers)
 *
 * See copyright notice in LICENSE.TXT
 * ----------------------------------------------------------------------------
 */

#ifndef UMM_MALLOC_H
#define UMM_MALLOC_H

#include <stddef.h>
#include <stdint.h>

typedef int int24_t;
typedef unsigned int uint24_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */

typedef struct umm_heap_config {
    void *pheap;
    uint24_t heap_size;
    uint16_t numblocks;
} umm_heap;

extern void  umm_multi_init_heap(umm_heap *heap, void *ptr, uint24_t size);

extern void *umm_multi_malloc(umm_heap *heap, uint24_t size);
extern void *umm_multi_calloc(umm_heap *heap, uint24_t num, uint24_t size);
extern void *umm_multi_realloc(umm_heap *heap, void *ptr, uint24_t size);
extern void  umm_multi_free(umm_heap *heap, void *ptr);

/* ------------------------------------------------------------------------ */

extern void  umm_init_heap(void *ptr, uint24_t size);

extern void *umm_malloc(uint24_t size);
extern void *umm_calloc(uint24_t num, uint24_t size);
extern void *umm_realloc(void *ptr, uint24_t size);
extern void  umm_free(void *ptr);
extern void umm_heap_stats(uint24_t *total, uint24_t *used, uint24_t *free, uint24_t *largest_free);

/* ------------------------------------------------------------------------ */

#ifdef __cplusplus
}
#endif

#endif /* UMM_MALLOC_H */
