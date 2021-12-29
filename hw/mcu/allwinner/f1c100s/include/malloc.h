#ifndef __MALLOC_H__
#define __MALLOC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

void * mm_create(void * mem, size_t bytes);
void mm_destroy(void * mm);
void * mm_get_pool(void * mm);
void * mm_add_pool(void * mm, void * mem, size_t bytes);
void mm_remove_pool(void * mm, void * pool);
void * mm_malloc(void * mm, size_t size);
void * mm_memalign(void * mm, size_t align, size_t size);
void * mm_realloc(void * mm, void * ptr, size_t size);
void mm_free(void * mm, void * ptr);

void * malloc(size_t size);
void * memalign(size_t align, size_t size);
void * realloc(void * ptr, size_t size);
void * calloc(size_t nmemb, size_t size);
void free(void * ptr);

void do_init_mem_pool(void);

#ifdef __cplusplus
}
#endif

#endif /* __MALLOC_H__ */
