#include <stddef.h>
#include <lib/alloc.hpp>
#include <lib/builtins.h>
#include <lib/math.hpp>
#include <mm/pmm.hpp>
#include <mm/vmm.hpp>

struct alloc_metadata {
    size_t pages;
    size_t size;
};

void *alloc(size_t size) {
    size_t page_count = div_roundup(size, PAGE_SIZE);

    char *ptr = (char *)pmm_allocz(page_count + 1);

    if (!ptr)
        return nullptr;

    ptr += MEM_PHYS_OFFSET;

    alloc_metadata *metadata = (alloc_metadata *)ptr;
    ptr += PAGE_SIZE;

    metadata->pages = page_count;
    metadata->size = size;

    return ptr;
}

void free(void *ptr) {
    alloc_metadata *metadata = (alloc_metadata *)((char *)ptr - PAGE_SIZE);

    pmm_free((void *)((size_t)metadata - MEM_PHYS_OFFSET), metadata->pages + 1);
}

void *realloc(void *ptr, size_t new_size) {
    /* check if 0 */
    if (!ptr)
        return alloc(new_size);

    /* Reference metadata page */
    alloc_metadata *metadata = (alloc_metadata *)((char *)ptr - PAGE_SIZE);

    if (div_roundup(metadata->size, PAGE_SIZE) == div_roundup(new_size, PAGE_SIZE)) {
        metadata->size = new_size;
        return ptr;
    }

    void *new_ptr = alloc(new_size);
    if (new_ptr == nullptr)
        return nullptr;

    if (metadata->size > new_size)
        /* Copy all the data from the old pointer to the new pointer,
         * within the range specified by `size`. */
        memcpy(new_ptr, ptr, new_size);
    else
        memcpy(new_ptr, ptr, metadata->size);

    free(ptr);

    return new_ptr;
}
