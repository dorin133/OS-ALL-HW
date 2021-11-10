#include <unistd.h>

void* smalloc(size_t size){
    // a. If ‘size’ is 0 returns NULL.
    // b. If ‘size’ is more than 10^8 , return NULL.
    if (size == 0 || size > 100000000){
        return NULL;
    }
    // Tries to allocate ‘size’ bytes.
    void* old_prog_brk = sbrk(size);
    // c. If sbrk fails, return NULL.
    if (old_prog_brk == (void*)(-1)) {
        return NULL;
    }
    // i. Success –a pointer to the first allocated byte within the allocated block.
    return old_prog_brk;
}