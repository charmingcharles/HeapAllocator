#if !defined(_HEAP_H_)
#define _HEAP_H_

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <math.h>

#define PAGE_SIZE       4096    // Długość strony w bajtach
#define PAGE_FENCE      1       // Liczba stron na jeden płotek
#define PAGES_AVAILABLE 16384   // Liczba stron dostępnych dla sterty
#define PAGES_TOTAL     (PAGES_AVAILABLE + 2 * PAGE_FENCE)

typedef struct memblock_t memblock_t;

typedef enum {TRUE = 1, FALSE}boolean;

struct __attribute__((packed)) memblock_t{
    memblock_t *next;
    memblock_t *prev;
    size_t size;
    boolean isEmpty;
};

#define MEMBLOCK_T_SIZE sizeof(memblock_t)

#define FENCE_STRING "KTONIESKACZETENZPOLICJIHOPHOPHOP"

#define OFFSET(memblock) (size_t)(PAGE_SIZE - (((intptr_t)memblock) % PAGE_SIZE))

typedef struct{ //control block
    memblock_t* head;
    memblock_t* tail;
    size_t pagesTaken;
    boolean isInit;
}heap_t;

typedef struct {
    size_t largest_used_block_size;
    size_t largest_empty_block_size;
    size_t used_space;
    size_t empty_space;
}heap_info_t;

#define FENCE_SIZE (size_t)32

enum pointer_type_t
{
    pointer_null,
    pointer_heap_corrupted,
    pointer_control_block,
    pointer_inside_fences,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
};

int heap_setup(void);
void heap_clean(void);

void* heap_malloc(size_t size);
void* heap_calloc(size_t number, size_t size);
void* heap_realloc(void* memblock, size_t count);
void  heap_free(void* memblock);

enum pointer_type_t get_pointer_type(const void* const pointer);

void* heap_malloc_aligned(size_t count);
void* heap_calloc_aligned(size_t number, size_t size);
void* heap_realloc_aligned(void* memblock, size_t size);

int heap_validate(void);
size_t heap_get_largest_used_block_size(void);

void update_heap_info();
void init_memblock(memblock_t* This, memblock_t* Prev, memblock_t* Next, int Size, boolean IsEmpty);
int pointer_bytes_sum(void* ptr, void* check);

#endif