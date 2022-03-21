#include "heap.h"
#include "custom_unistd.h"

static heap_t heap = {
        .head = NULL,
        .tail = NULL,
        .pagesTaken = 0,
        .isInit = FALSE
}, *the_Heap = &heap;

static heap_info_t heap_info = {
        .largest_used_block_size = 0,
        .largest_empty_block_size = 0,
        .used_space = 0,
        .empty_space = 0,
}, *the_Info = &heap_info;

static char fence[FENCE_SIZE];

#define FILL_FENCES(block){\
memcpy((void*)((intptr_t)block + MEMBLOCK_T_SIZE), fence, FENCE_SIZE);\
memcpy((void*)((intptr_t)block + metadata->size + FENCE_SIZE + MEMBLOCK_T_SIZE), fence, FENCE_SIZE);\
}

int heap_setup(void){
    if(the_Heap->isInit == TRUE){
        return -1;
    }
    the_Heap->head = custom_sbrk(0);
    if(custom_sbrk(PAGE_SIZE) == (void*)-1){
        return -1;
    }
    the_Heap->pagesTaken = 1;
    the_Heap->tail = (memblock_t*)((memblock_t*)custom_sbrk(0) - 1);
    memblock_t* firstBlock = (memblock_t*)(the_Heap->head + 1);

    size_t size = (size_t)((intptr_t)the_Heap->tail - (intptr_t)firstBlock - MEMBLOCK_T_SIZE);
    init_memblock(firstBlock, the_Heap->head, the_Heap->tail, size, TRUE);
    init_memblock(the_Heap->head, NULL, firstBlock, 0, FALSE);
    init_memblock(the_Heap->tail, firstBlock, NULL, 0, FALSE);
    the_Heap->isInit = TRUE;

    memcpy(fence, FENCE_STRING, FENCE_SIZE);

    update_heap_info();
    return 0;
}

void heap_clean(void){
    if(the_Heap->isInit == TRUE){
        custom_sbrk( - (the_Heap->pagesTaken * PAGE_SIZE));
        the_Heap->isInit = FALSE;
    }
}

void* heap_malloc(size_t size){
    if(heap_validate() || size == 0 || size >= ((PAGE_SIZE * PAGES_AVAILABLE) - (MEMBLOCK_T_SIZE * 2)))
        return NULL;
    memblock_t* metadata = the_Heap->head;
    boolean check = TRUE;
    while(check == TRUE && metadata){
        if(metadata->isEmpty == TRUE && metadata->size >= (size + 2*FENCE_SIZE))
            check = FALSE;
        else
            metadata = metadata->next;
    }
    if(check == TRUE && !metadata){
        size_t size_needed = size + 2*FENCE_SIZE;
        memblock_t* Prev;
        if(the_Heap->tail->prev->isEmpty == TRUE){
            size_needed -= (intptr_t)((intptr_t)the_Heap->tail - (intptr_t)the_Heap->tail->prev - MEMBLOCK_T_SIZE - 2*FENCE_SIZE);
            Prev = the_Heap->tail->prev;
        }
        else
            Prev = the_Heap->tail;
        int pages_needed = (int)((size_needed + (PAGE_SIZE - (size_needed%PAGE_SIZE)))/PAGE_SIZE);
        do{
            if(custom_sbrk(PAGE_SIZE) == (void*)-1)
                return NULL;
            the_Heap->pagesTaken++;
        }while(0 < --pages_needed);
        the_Heap->tail = (memblock_t*)((memblock_t*)custom_sbrk(0) - 1);
        init_memblock(the_Heap->tail, Prev, NULL, 0, FALSE);
        init_memblock(Prev, Prev->prev, the_Heap->tail, (size_t)((intptr_t)the_Heap->tail - (intptr_t)Prev - MEMBLOCK_T_SIZE), TRUE);
        metadata = the_Heap->tail->prev;
    }
    memblock_t* Next = metadata->next;
    if(metadata->size >= (size + MEMBLOCK_T_SIZE + 2*FENCE_SIZE)){
        Next = (memblock_t*)((intptr_t)metadata + MEMBLOCK_T_SIZE + 2*FENCE_SIZE + size);
        init_memblock(Next, metadata, metadata->next, (size_t)((intptr_t)metadata->next - (intptr_t)Next - MEMBLOCK_T_SIZE), TRUE);
        Next->next->prev = Next;
    }
    init_memblock(metadata, metadata->prev, Next, size, FALSE);
    FILL_FENCES(metadata);
    update_heap_info();
    return (void*)((intptr_t)metadata + FENCE_SIZE + MEMBLOCK_T_SIZE);
}

void* heap_calloc(size_t number, size_t size){
    if(heap_validate() || number == 0 || size == 0)
        return NULL;
    size_t full_size = number * size;
    if(full_size >= ((PAGE_SIZE * PAGES_TOTAL) - (size_t)((the_Heap->pagesTaken + 1) * PAGE_SIZE)))
        return NULL;
    void* ptr = heap_malloc(number * size);
    if(!ptr)
        return NULL;
    memset(ptr, 0, full_size);
    return ptr;
}

void* heap_realloc(void* memblock, size_t size){
    if(heap_validate() || size >= ((PAGE_SIZE * PAGES_AVAILABLE) - (MEMBLOCK_T_SIZE * 2)))
        return NULL;
    if(memblock == NULL)
        return heap_malloc(size);
    if(get_pointer_type(memblock) != pointer_valid)
        return NULL;
    if(size == 0){
        heap_free(memblock);
        return NULL;
    }
    memblock_t* metadata = (void*)((intptr_t)memblock - MEMBLOCK_T_SIZE - FENCE_SIZE);
    if(size == metadata->size)
        return memblock;
    else if(size < metadata->size){
        metadata->size = size;
        FILL_FENCES(metadata);
        update_heap_info();
        return memblock;
    }
    else{
        size_t old_size = metadata->size;
        boolean freed = FALSE;
        if(metadata->next->isEmpty == TRUE){
            freed = TRUE;
            init_memblock(metadata, metadata->prev, metadata->next->next, (size_t)((intptr_t)metadata->next->next - (intptr_t)metadata - MEMBLOCK_T_SIZE), TRUE);
            metadata->next->prev = metadata;
            update_heap_info();
        }
        if(metadata->size >= (size + 2*FENCE_SIZE)){
            memblock_t* Next = metadata->next;
            if(metadata->size >= (size + MEMBLOCK_T_SIZE + 2*FENCE_SIZE)){
                Next = (memblock_t*)((intptr_t)metadata + MEMBLOCK_T_SIZE + 2*FENCE_SIZE + size);
                init_memblock(Next, metadata, metadata->next, (size_t)((intptr_t)metadata->next - (intptr_t)Next - MEMBLOCK_T_SIZE), TRUE);
                Next->next->prev = Next;
            }
            init_memblock(metadata, metadata->prev, Next, size, FALSE);
            FILL_FENCES(metadata);
            update_heap_info();
            return (void*)((intptr_t)metadata + FENCE_SIZE + MEMBLOCK_T_SIZE);
        }
        else if(metadata->next == the_Heap->tail){
            size_t size_needed = size + 2 * FENCE_SIZE;
            memblock_t* Prev;
            if(the_Heap->tail->prev->isEmpty == TRUE){
                size_needed -= (intptr_t)((intptr_t)the_Heap->tail - (intptr_t)the_Heap->tail->prev - MEMBLOCK_T_SIZE - 2*FENCE_SIZE);
                Prev = the_Heap->tail->prev;
            }
            else
                Prev = the_Heap->tail;
            int pages_needed = (int)((size_needed + (PAGE_SIZE - (size_needed%PAGE_SIZE)))/PAGE_SIZE);
            do{
                if(custom_sbrk(PAGE_SIZE) == (void*)-1)
                    return NULL;
                the_Heap->pagesTaken++;
            }while(0 < --pages_needed);
            the_Heap->tail = (memblock_t*)((memblock_t*)custom_sbrk(0) - 1);
            init_memblock(the_Heap->tail, Prev, NULL, 0, FALSE);
            init_memblock(Prev, Prev->prev, the_Heap->tail, (size_t)((intptr_t)the_Heap->tail - (intptr_t)Prev - MEMBLOCK_T_SIZE), TRUE);
            metadata = the_Heap->tail->prev;
            memblock_t* Next = metadata->next;
            if(metadata->size >= (size + MEMBLOCK_T_SIZE + 2*FENCE_SIZE)){
                Next = (memblock_t*)((intptr_t)metadata + MEMBLOCK_T_SIZE + 2*FENCE_SIZE + size);
                init_memblock(Next, metadata, metadata->next, (size_t)((intptr_t)metadata->next - (intptr_t)Next - MEMBLOCK_T_SIZE), TRUE);
                Next->next->prev = Next;
            }
            init_memblock(metadata, metadata->prev, Next, size, FALSE);
            FILL_FENCES(metadata);
            update_heap_info();
            return (void*)((intptr_t)metadata + FENCE_SIZE + MEMBLOCK_T_SIZE);
        }
        else{
            if(freed == TRUE){
                if(metadata->prev->isEmpty == TRUE){
                    init_memblock(metadata->prev, metadata->prev->prev, metadata->next, metadata->prev->size + metadata->size + MEMBLOCK_T_SIZE, metadata->prev->isEmpty);
                    metadata->next->prev = metadata->prev;
                    update_heap_info();
                }
            }else
                heap_free(memblock);
            memblock_t* search = heap_malloc(size);
            memcpy((void*)search, memblock, old_size);
            metadata = (void*)((intptr_t)search - MEMBLOCK_T_SIZE - FENCE_SIZE);
            FILL_FENCES(metadata);
            update_heap_info();
            return (void*)((intptr_t)metadata + FENCE_SIZE + MEMBLOCK_T_SIZE);
        }
    }
    return NULL;
}

void  heap_free(void* memblock){
    if(!memblock || heap_validate() || get_pointer_type(memblock) != pointer_valid)
        return;
    memblock_t* metadata = (void*)((intptr_t)memblock - FENCE_SIZE - MEMBLOCK_T_SIZE);
    metadata->size = (intptr_t)metadata->next - (intptr_t)metadata - MEMBLOCK_T_SIZE;
    metadata->isEmpty = TRUE;
    if(metadata->next->isEmpty == TRUE){
        init_memblock(metadata, metadata->prev, metadata->next->next, metadata->size + metadata->next->size + MEMBLOCK_T_SIZE, metadata->isEmpty);
        metadata->next->prev = metadata;
    }
    if(metadata->prev->isEmpty == TRUE){
        init_memblock(metadata->prev, metadata->prev->prev, metadata->next, metadata->prev->size + metadata->size + MEMBLOCK_T_SIZE, metadata->prev->isEmpty);
        metadata->next->prev = metadata->prev;
    }
    update_heap_info();
}

enum pointer_type_t get_pointer_type(const void* const pointer){
    if(!pointer)
        return pointer_null;
    if(heap_validate())
        return pointer_heap_corrupted;
    if((intptr_t)pointer < (intptr_t)the_Heap->head || (intptr_t)pointer > (intptr_t)((intptr_t)the_Heap->tail + MEMBLOCK_T_SIZE))
        return pointer_unallocated;
    memblock_t* temp = the_Heap->head;
    while(temp){
        if(((intptr_t)pointer >= ((intptr_t)temp)) && ((intptr_t)pointer < (intptr_t)((intptr_t)temp + MEMBLOCK_T_SIZE)))
            return pointer_control_block;
        if(temp->isEmpty == FALSE && temp->size > 0){
            if(((intptr_t)pointer >= (intptr_t)(((intptr_t)temp) + MEMBLOCK_T_SIZE)) && ((intptr_t)pointer < ((intptr_t)(((intptr_t)temp) + MEMBLOCK_T_SIZE + FENCE_SIZE))))
                return pointer_inside_fences;
            else if(((intptr_t)pointer >= ((intptr_t)((intptr_t)temp + MEMBLOCK_T_SIZE + temp->size + FENCE_SIZE))) && ((intptr_t)pointer < (intptr_t)((intptr_t)temp + MEMBLOCK_T_SIZE + temp->size + 2*FENCE_SIZE)))
                return pointer_inside_fences;
            else if((intptr_t)pointer == (intptr_t)((intptr_t)temp + MEMBLOCK_T_SIZE + FENCE_SIZE))
                return pointer_valid;
            else if(((intptr_t)pointer > (intptr_t)((intptr_t)temp + MEMBLOCK_T_SIZE + FENCE_SIZE)) && ((intptr_t)pointer < (intptr_t)((intptr_t)temp + temp->size + MEMBLOCK_T_SIZE +FENCE_SIZE)))
                return pointer_inside_data_block;
        }
        temp = temp->next;
    }
    return pointer_unallocated;
}

void* heap_malloc_aligned(size_t count){
    if(heap_validate() || count == 0 || count >= ((PAGE_SIZE * PAGES_AVAILABLE) - (MEMBLOCK_T_SIZE * 2)))
        return NULL;
    memblock_t* metadata = the_Heap->head;
    boolean check = TRUE; size_t offset; boolean page_pointer_check = TRUE;
    while(check == TRUE && metadata){
        if(metadata->isEmpty == TRUE){
            offset = OFFSET(metadata);
            if(metadata->size >= (offset + count + 2*FENCE_SIZE + MEMBLOCK_T_SIZE)){
                if(((intptr_t)((intptr_t)metadata + MEMBLOCK_T_SIZE + FENCE_SIZE) & (intptr_t)(PAGE_SIZE - 1)) == 0)
                    check = FALSE;
                else if(OFFSET(metadata) >= MEMBLOCK_T_SIZE + FENCE_SIZE){
                    memblock_t* newBlock = (memblock_t*)((intptr_t)metadata + offset - MEMBLOCK_T_SIZE - FENCE_SIZE);
                    metadata->prev->next = newBlock;
                    metadata->next->prev = newBlock;
                    init_memblock(newBlock, metadata->prev, metadata->next, (size_t)((intptr_t)metadata->next - (intptr_t)newBlock - MEMBLOCK_T_SIZE), TRUE);
                    newBlock->next->prev = newBlock;
                    metadata = newBlock;
                    check = FALSE;
                }
                else
                    metadata = metadata->next;
            }
            else
                metadata = metadata->next;
        }
        else
            metadata = metadata->next;
    }
    if(check == TRUE && !metadata){
        offset = 0;
        size_t size_needed = count + 2*FENCE_SIZE;
        memblock_t* Prev = the_Heap->tail;
        if(the_Heap->tail->prev->isEmpty == TRUE && (((intptr_t)((intptr_t)the_Heap->tail->prev + MEMBLOCK_T_SIZE + FENCE_SIZE) & (intptr_t)(PAGE_SIZE - 1)) == 0)){
            size_needed -= (intptr_t)((intptr_t)the_Heap->tail - (intptr_t)the_Heap->tail->prev - MEMBLOCK_T_SIZE);
            Prev = the_Heap->tail->prev;
        }
        else if(get_pointer_type((void*)((intptr_t)the_Heap->tail - FENCE_SIZE)) == pointer_unallocated){
            Prev = (memblock_t*)((intptr_t)the_Heap->tail - FENCE_SIZE);
            init_memblock(Prev, the_Heap->tail->prev, the_Heap->tail->next, FENCE_SIZE, TRUE);
            Prev->prev->next = Prev;
            Prev->prev->next = Prev;
            if(Prev->prev->isEmpty == TRUE)
                Prev->prev->size -= FENCE_SIZE;
        }
        else{
            page_pointer_check = FALSE;
            offset = OFFSET(metadata);
            size_needed += offset;
        }
        int pages_needed = (int)((size_needed + (PAGE_SIZE - (size_needed%PAGE_SIZE)))/PAGE_SIZE);
        do{
            if(custom_sbrk(PAGE_SIZE) == (void*)-1)
                return NULL;
            the_Heap->pagesTaken++;
        }while(0 < --pages_needed);
        the_Heap->tail = (memblock_t*)((memblock_t*)custom_sbrk(0) - 1);
        if(page_pointer_check == FALSE){
            memblock_t* newBlock = (memblock_t*)((intptr_t)Prev + offset - FENCE_SIZE);
            init_memblock(newBlock, Prev, the_Heap->tail, (size_t)((intptr_t)the_Heap->tail - (intptr_t)newBlock - MEMBLOCK_T_SIZE), TRUE);
            init_memblock(Prev, Prev->prev, newBlock, offset, TRUE);
            Prev = newBlock;
            offset = 0;
        }
        init_memblock(the_Heap->tail, Prev, NULL, 0, FALSE);
        init_memblock(Prev, Prev->prev, the_Heap->tail, (size_t)((intptr_t)the_Heap->tail - (intptr_t)Prev - MEMBLOCK_T_SIZE), TRUE);
        metadata = the_Heap->tail->prev;
    }
    memblock_t* Next = metadata->next;
    if(metadata->size >= (count + offset + 2*MEMBLOCK_T_SIZE + 2*FENCE_SIZE)){
        Next = (memblock_t*)((intptr_t)metadata + MEMBLOCK_T_SIZE + 2*FENCE_SIZE + count + offset);
        init_memblock(Next, metadata, metadata->next, (size_t)((intptr_t)metadata->next - (intptr_t)Next - MEMBLOCK_T_SIZE), TRUE);
        Next->next->prev = Next;
    }
    init_memblock(metadata, metadata->prev, Next, count, FALSE);
    FILL_FENCES(metadata);
    update_heap_info();
    return (void*)((intptr_t)metadata + FENCE_SIZE + MEMBLOCK_T_SIZE);
}

void* heap_calloc_aligned(size_t number, size_t size){
    if(heap_validate() || number == 0 || size == 0)
        return NULL;
    size_t full_size = number * size;
    if(full_size >= ((PAGE_SIZE * PAGES_TOTAL) - (size_t)((the_Heap->pagesTaken + 1) * PAGE_SIZE)))
        return NULL;
    void* ptr = heap_malloc_aligned(number * size);
    if(!ptr)
        return NULL;
    memset(ptr, 0, full_size);
    return ptr;
}

void* heap_realloc_aligned(void* memblock, size_t size){
    if(heap_validate() || size >= ((PAGE_SIZE * PAGES_AVAILABLE) - (MEMBLOCK_T_SIZE * 2))) //OK
        return NULL;
    if(memblock == NULL) //OK
        return heap_malloc_aligned(size);
    if(get_pointer_type(memblock) != pointer_valid) //OK
        return NULL;
    if(size == 0){ //OK
        heap_free(memblock);
        return NULL;
    }
    memblock_t* metadata = (void*)((intptr_t)memblock - MEMBLOCK_T_SIZE - FENCE_SIZE);
    if(size == metadata->size) //OK
        return memblock;
    else if(size < metadata->size){ //OK
        metadata->size = size;
        FILL_FENCES(metadata);
        update_heap_info();
        return memblock;
    }
    else { //OK
        size_t offset = 0; boolean page_pointer_check = TRUE;
        size_t old_size = metadata->size;
        boolean freed = FALSE;
        if (metadata->next->isEmpty == TRUE) { //OK
            freed = TRUE;
            init_memblock(metadata, metadata->prev, metadata->next->next,
                          (size_t) ((intptr_t) metadata->next->next - (intptr_t) metadata - MEMBLOCK_T_SIZE), TRUE);
            metadata->next->prev = metadata;
            update_heap_info();
        }
        if (metadata->size >= (size + 2 * FENCE_SIZE)) { //OK
            memblock_t *Next = metadata->next;
            if (metadata->size >= (size + MEMBLOCK_T_SIZE + 2 * FENCE_SIZE)) {
                Next = (memblock_t *) ((intptr_t) metadata + MEMBLOCK_T_SIZE + 2 * FENCE_SIZE + size);
                init_memblock(Next, metadata, metadata->next,
                              (size_t) ((intptr_t) metadata->next - (intptr_t) Next - MEMBLOCK_T_SIZE), TRUE);
                Next->next->prev = Next;
            }
            init_memblock(metadata, metadata->prev, Next, size, FALSE);
            FILL_FENCES(metadata);
            update_heap_info();
            return (void *) ((intptr_t) metadata + FENCE_SIZE + MEMBLOCK_T_SIZE);
        } else if (metadata->next == the_Heap->tail) { //OK
            size_t size_needed = size + 2*FENCE_SIZE;
            memblock_t* Prev = the_Heap->tail;
            if(the_Heap->tail->prev->isEmpty == TRUE && (((intptr_t)((intptr_t)the_Heap->tail->prev + MEMBLOCK_T_SIZE + FENCE_SIZE) & (intptr_t)(PAGE_SIZE - 1)) == 0)){
                size_needed -= (intptr_t)((intptr_t)the_Heap->tail - (intptr_t)the_Heap->tail->prev - MEMBLOCK_T_SIZE);
                Prev = the_Heap->tail->prev;
            }
            else if(get_pointer_type((void*)((intptr_t)the_Heap->tail - FENCE_SIZE)) == pointer_unallocated){
                Prev = (memblock_t*)((intptr_t)the_Heap->tail - FENCE_SIZE);
                init_memblock(Prev, the_Heap->tail->prev, the_Heap->tail->next, FENCE_SIZE, TRUE);
                Prev->prev->next = Prev;
            }
            else{
                page_pointer_check = FALSE;
                offset = OFFSET(metadata);
                size_needed += offset;
            }
            int pages_needed = (int)((size_needed + (PAGE_SIZE - (size_needed%PAGE_SIZE)))/PAGE_SIZE);
            do{
                if(custom_sbrk(PAGE_SIZE) == (void*)-1)
                    return NULL;
                the_Heap->pagesTaken++;
            }while(0 < --pages_needed);
            the_Heap->tail = (memblock_t*)((memblock_t*)custom_sbrk(0) - 1);
            if(page_pointer_check == FALSE){
                memblock_t* newBlock = (memblock_t*)((intptr_t)Prev + offset - FENCE_SIZE);
                init_memblock(newBlock, Prev, the_Heap->tail, (size_t)((intptr_t)the_Heap->tail - (intptr_t)newBlock - MEMBLOCK_T_SIZE), TRUE);
                init_memblock(Prev, Prev->prev, newBlock, offset, TRUE);
                Prev = newBlock;
                offset = 0;
            }
            init_memblock(the_Heap->tail, Prev, NULL, 0, FALSE);
            init_memblock(Prev, Prev->prev, the_Heap->tail, (size_t)((intptr_t)the_Heap->tail - (intptr_t)Prev - MEMBLOCK_T_SIZE), TRUE);
            metadata = the_Heap->tail->prev;
            memblock_t* Next = metadata->next;
            if(metadata->size >= (size + offset + 2*MEMBLOCK_T_SIZE + 2*FENCE_SIZE)){
                Next = (memblock_t*)((intptr_t)metadata + MEMBLOCK_T_SIZE + 2*FENCE_SIZE + size + offset);
                init_memblock(Next, metadata, metadata->next, (size_t)((intptr_t)metadata->next - (intptr_t)Next - MEMBLOCK_T_SIZE), TRUE);
                Next->next->prev = Next;
            }
            init_memblock(metadata, metadata->prev, Next, size, FALSE);
            FILL_FENCES(metadata);
            update_heap_info();
            return (void*)((intptr_t)metadata + FENCE_SIZE + MEMBLOCK_T_SIZE);
        } else { //OK
            if (freed == TRUE) {
                if (metadata->prev->isEmpty == TRUE) {
                    init_memblock(metadata->prev, metadata->prev->prev, metadata->next,
                                  metadata->prev->size + metadata->size + MEMBLOCK_T_SIZE, metadata->prev->isEmpty);
                    metadata->next->prev = metadata->prev;
                    update_heap_info();
                }
            } else
                heap_free(memblock);
            memblock_t *search = heap_malloc_aligned(size);
            memcpy((void *) search, memblock, old_size);
            metadata = (void *) ((intptr_t) search - MEMBLOCK_T_SIZE - FENCE_SIZE);
            FILL_FENCES(metadata);
            update_heap_info();
            return (void *) ((intptr_t) metadata + FENCE_SIZE + MEMBLOCK_T_SIZE);
        }
    }
    return NULL;
}

int heap_validate(void){
    //RETURNS 2
    if(!the_Heap->head || !the_Heap->tail || the_Heap->isInit == FALSE || the_Heap->pagesTaken == 0)
        return 2;
    //RETURNS 3
    memblock_t* ptr = the_Heap->head;
    while(ptr){
        if(ptr->isEmpty != FALSE && ptr->isEmpty != TRUE)
            return 3;
        if((intptr_t)ptr < (intptr_t)the_Heap->head || (intptr_t)ptr >= (intptr_t)((intptr_t)the_Heap->tail + MEMBLOCK_T_SIZE))
            return 3;
        if(ptr->prev && ((intptr_t)ptr->prev < (intptr_t)the_Heap->head || (intptr_t)ptr->prev >= (intptr_t)((intptr_t)the_Heap->tail + MEMBLOCK_T_SIZE)))
            return 3;
        if(ptr->next && ((intptr_t)ptr->next < (intptr_t)the_Heap->head || (intptr_t)ptr->next >= (intptr_t)((intptr_t)the_Heap->tail + MEMBLOCK_T_SIZE)))
            return 3;
        ptr = ptr->next;
    }
    ptr = the_Heap->tail;
    while(ptr){
        if(ptr->isEmpty != FALSE && ptr->isEmpty != TRUE)
            return 3;
        if((intptr_t)ptr < (intptr_t)the_Heap->head || (intptr_t)ptr >= (intptr_t)((intptr_t)the_Heap->tail + MEMBLOCK_T_SIZE))
            return 3;
        if(ptr->prev && ((intptr_t)ptr->prev < (intptr_t)the_Heap->head || (intptr_t)ptr->prev >= (intptr_t)((intptr_t)the_Heap->tail + MEMBLOCK_T_SIZE)))
            return 3;
        if(ptr->next && ((intptr_t)ptr->next < (intptr_t)the_Heap->head || (intptr_t)ptr->next >= (intptr_t)((intptr_t)the_Heap->tail + MEMBLOCK_T_SIZE)))
            return 3;
        ptr = ptr->prev;
    }
    int head_count = 0, head_sum = 0; size_t used_space = 0, empty_space = 0;
    ptr = the_Heap->head; void* check = (void*)0;
    while(ptr){
        head_sum += pointer_bytes_sum(ptr, check);
        if(check == (void*)1 && head_sum == 0)
            return 3;
        if(ptr->isEmpty == TRUE)
            empty_space += ptr->size;
        else
            used_space += ptr->size;
        ptr = ptr->next;
        head_count++;
    }
    if(empty_space != the_Info->empty_space || used_space != the_Info->used_space)
        return 3;
    int tail_count = 0, tail_sum = 0; used_space = 0, empty_space = 0;
    ptr = the_Heap->tail;
    while(ptr){
        tail_sum += pointer_bytes_sum(ptr, check);
        if(check == (void*)1 && tail_sum == 0)
            return 3;
        if(ptr->isEmpty == TRUE)
            empty_space += ptr->size;
        else
            used_space += ptr->size;

        ptr = ptr->prev;
        tail_count++;
    }
    if(empty_space != the_Info->empty_space || used_space != the_Info->used_space)
        return 3;
    if(head_count != tail_count || tail_sum != head_sum)
        return 3;
    ptr = the_Heap->head;
    char *c1, *c2;
    while(ptr){
        if(ptr->isEmpty == FALSE && ptr->size > 0) {
            c1 = (char *) ((intptr_t) ptr + MEMBLOCK_T_SIZE);
            c2 = c1 + FENCE_SIZE + ptr->size;
            for (int i = 0; i < (int) FENCE_SIZE; i++) {
                if ((intptr_t) (c1 + i) < (intptr_t) the_Heap->head || (intptr_t) (c1 + i) >= (intptr_t) ((intptr_t) the_Heap->tail + MEMBLOCK_T_SIZE))
                    return 3;
                if ((intptr_t) (c2 + i) < (intptr_t) the_Heap->head || (intptr_t) (c2 + i) >= (intptr_t) ((intptr_t) the_Heap->tail + MEMBLOCK_T_SIZE))
                    return 3;
            }
        }
        ptr = ptr->next;
    }
    ptr = the_Heap->tail;
    while(ptr){
        if(ptr->isEmpty == FALSE && ptr->size > 0) {
            c1 = (char *) ((intptr_t) ptr + MEMBLOCK_T_SIZE);
            c2 = c1 + FENCE_SIZE + ptr->size;
            for (int i = 0; i < (int) FENCE_SIZE; i++) {
                if ((intptr_t) (c1 + i) < (intptr_t) the_Heap->head || (intptr_t) (c1 + i) >= (intptr_t) ((intptr_t) the_Heap->tail + MEMBLOCK_T_SIZE))
                    return 3;
                if ((intptr_t) (c2 + i) < (intptr_t) the_Heap->head || (intptr_t) (c2 + i) >= (intptr_t) ((intptr_t) the_Heap->tail + MEMBLOCK_T_SIZE))
                    return 3;
            }
        }
        ptr = ptr->prev;
    }
    //RETURNS 1
    ptr = the_Heap->head;
    while(ptr){
        if(ptr->isEmpty == FALSE && ptr->size > 0) {
            c1 = (char *) ((intptr_t) ptr + MEMBLOCK_T_SIZE);
            c2 = c1 + FENCE_SIZE + ptr->size;
            if((int)memcmp(c1, fence, FENCE_SIZE) + (int)memcmp(c2, fence, FENCE_SIZE) != 0)
                return 1;
        }
        ptr = ptr->next;
    }
    return 0;
}

void update_heap_info(){
    size_t used = 0, empty = 0, largest_used = 0, largest_empty = 0;
    memblock_t* ptr = the_Heap->head;
    while(ptr){
        if(ptr->isEmpty == TRUE){
            empty += ptr->size;
            if(ptr->size >= largest_empty){
                largest_empty = ptr->size;
            }
        }
        else{
            used += ptr->size;
            if(ptr->size >= largest_used){
                largest_used = ptr->size;
            }
        }
        ptr = ptr->next;
    }
    the_Info->used_space = used;
    the_Info->empty_space = empty;
    the_Info->largest_used_block_size = largest_used;
    the_Info->largest_empty_block_size = largest_empty;
}

size_t heap_get_largest_used_block_size(void){
    if(heap_validate())
        return 0;
    update_heap_info();
    return the_Info->largest_used_block_size;
}

void init_memblock(memblock_t* This, memblock_t* Prev, memblock_t* Next, int Size, boolean IsEmpty){
    This->prev = Prev;
    This->next = Next;
    This->size = Size;
    This->isEmpty = IsEmpty;
}

int pointer_bytes_sum(void* ptr, void* check){
    int sum = 0; char* arr = (char*)ptr;
    for(int i = 0; i < 8; i++){
        if ((intptr_t) (arr + i) < (intptr_t) the_Heap->head || (intptr_t) (arr + i) >= (intptr_t) ((intptr_t) the_Heap->tail + MEMBLOCK_T_SIZE)){
            check = (void*)1;
            return 0;
        }
        sum += *(arr + i);
    }
    if(check != (void*)0)
        return 0;
    return sum;
}