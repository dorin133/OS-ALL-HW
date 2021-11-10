#include <unistd.h>
#include <iostream>
#include <string.h>

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

static void* smalloc_naive(size_t size){
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

struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

static MallocMetadata* free_list = nullptr;
static size_t free_blocks_num = 0;
static size_t free_bytes_num = 0;
static size_t num_of_metadatas = 0;
static size_t num_allocated_blocks = 0;
static size_t num_allocated_bytes = 0;

static bool list_is_empty(){
    if (!free_list)
        return true; 
    else 
        return false;
}

static MallocMetadata* create_metadata_sbrk(size_t size_in){
    void* ptr_void = smalloc_naive(sizeof(MallocMetadata));
    if (!ptr_void){
        return NULL;
    }
    MallocMetadata* new_node = (MallocMetadata*)ptr_void;
    new_node->size = size_in;
    new_node->is_free = false;
    new_node->next = NULL;
    new_node->prev = NULL;

    num_of_metadatas++;
    num_allocated_blocks++;
    num_allocated_bytes += size_in;

    return new_node;
}


void print_free_list(){
    std::cout << "printing free list" << std::endl;
    if (list_is_empty()){
        std::cout << "list is empty!" << std::endl;
    }
    MallocMetadata* populate = free_list;
    int count = 0;
    while (populate) {
        std::cout << "[" << ++count << "]" << " address: " << populate <<  " size: "<< populate->size << " is_free: " << populate->is_free << " prev: " << populate->prev << " next: " << populate->next << std::endl;
        populate = populate->next;
    }
}

static void remove_node (MallocMetadata* ptr) {
    // print_free_list();
    if (list_is_empty() || !ptr)
        return;  
    MallocMetadata* prev_ptr = ptr->prev;
    MallocMetadata* next_ptr = ptr->next;
    // first node
    if (!prev_ptr) {
        free_list = next_ptr;
        if (next_ptr){
            free_list->prev = NULL;
        }
        ptr->prev = NULL;
        ptr->next = NULL;
        return;
    }
    prev_ptr->next = next_ptr;
    if(next_ptr)
        next_ptr->prev = prev_ptr;
    ptr->prev = NULL;
    ptr->next = NULL;
    // print_free_list();
}

static void add_node (MallocMetadata* node_to_add, MallocMetadata* ptr_prev) {
    // add to free_list head
    // if(!node_to_add) {
    //     return;
    // }
    if (!ptr_prev){
        node_to_add->next = free_list;
        if (free_list)
            free_list->prev = node_to_add;
        node_to_add->prev = NULL;
        free_list = node_to_add;
        return;
    }
    MallocMetadata* ptr_next = ptr_prev->next;
    node_to_add->prev = ptr_prev;
    ptr_prev->next = node_to_add;
    node_to_add->next = ptr_next;
    if (ptr_next)
        ptr_next->prev = node_to_add;
}


//Searches for a free block with up to ‘size’ bytes or allocates (sbrk()) one if none are found
void* smalloc(size_t size){
    // print_free_list();
    if (size == 0 || size > 100000000) {
        return NULL;
    }
    // go through list to find empty block
    MallocMetadata* current = free_list;
    while (current) {
        if(current->size >= size) {
            break;
        }
        current = current->next;
    }  
    if(list_is_empty() || !current) { //no space found in list
        MallocMetadata* new_metadata = create_metadata_sbrk(size);
        if(!new_metadata){
            return NULL;
        }
        // Tries to allocate ‘size’ bytes.
        void* old_prog_brk = sbrk(size);
        // c. If sbrk fails, return NULL.
        if (old_prog_brk == (void*)(-1)) {
            return NULL;
        }
        return old_prog_brk;
    }
    // else
    current->is_free = false;
    remove_node(current);
    // print_free_list();
    free_bytes_num = free_bytes_num - current->size;
    free_blocks_num--;
    // std::cout << "num of free blocks at the end of smalloc:" << free_blocks_num << std::endl;
    return ++current;
}

void sfree(void* p) {
    // If ‘p’ is NULL or already released, simply returns.
    if (!p)
        return;
    MallocMetadata* p_metadata = (MallocMetadata*)((char*)p - sizeof(MallocMetadata));
    if (p_metadata->is_free)
        return;
    // go over free list to find where to insert
    MallocMetadata* populate = free_list;
    MallocMetadata* populate_prev = NULL;
    while (populate) {
        if(populate > p_metadata) {
            break;
        }
        populate_prev = populate;
        populate = populate->next;
    }
    free_blocks_num++;
    free_bytes_num  += p_metadata->size;
    p_metadata->is_free = true;
    // Releases the usage of the block that starts with the pointer ‘p’.
    add_node(p_metadata, populate_prev);
}

void* scalloc(size_t num, size_t size) {
    void* p = smalloc(num*size);
    if(!p) {
        return NULL;
    }
    memset(p, 0, num*size);
    return p;
}

void* srealloc(void* oldp, size_t size) {
    if (size == 0 || size > 100000000) {
        return NULL;
    }
    // b. If ‘oldp’ is NULL, allocates space for ‘size’ bytes and returns a pointer to it.
    if (!oldp){
        return smalloc(size);
    }
    MallocMetadata* oldp_metadata = (MallocMetadata*)((char*)oldp - sizeof(MallocMetadata));
    // If ‘size’ is smaller than the current block’s size, reuses the same block. 
    if (oldp_metadata->size >= size) {
        return oldp;
    }
    // otherwise, finds/allocates ‘size’ bytes for a new space, copies content of oldp into the new allocated space and frees the oldp.
    void* newp = smalloc(size);
    if (!newp)
        return NULL;
    // copy the contents from oldp to newp
    size_t size_to_move = MIN(size, oldp_metadata->size);
    memmove(newp, oldp, size_to_move);
    sfree(oldp);
    // Returns pointer to the first byte in the (newly) allocated space.
    return newp;
}

// Returns the number of allocated blocks in the heap that are currently free.
size_t _num_free_blocks(){
    return free_blocks_num;
}

// Returns the number of bytes in all allocated blocks in the heap that are currently free,
// excluding the bytes used by the meta-data structs.
size_t _num_free_bytes(){
    return free_bytes_num;
}

// Returns the overall (free and used) number of allocated blocks in the heap.
size_t _num_allocated_blocks(){
    return num_allocated_blocks;
}

// Returns the overall number (free and used) of allocated bytes in the heap, excluding
// the bytes used by the meta-data structs.
size_t _num_allocated_bytes() {
    return num_allocated_bytes;
}

size_t _num_meta_data_bytes() {
// Returns the overall number of meta-data bytes currently in the heap.
    return num_of_metadatas * sizeof(MallocMetadata);
}

size_t _size_meta_data() {
// Returns the number of bytes of a single meta-data structure in your system.
    return sizeof(MallocMetadata);
}
