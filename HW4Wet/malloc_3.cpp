#include <unistd.h>
#include <iostream>
#include <string.h>
#include <sys/mman.h>

#define SMALLOC 0
#define SREALLOC 1
#define MMAP_SIZE 131072
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

static void* smalloc_naive(size_t size){
    if (size == 0 || size > 100000000){
        return NULL;
    }
    void* old_prog_brk = sbrk(size);
    if (old_prog_brk == (void*)(-1)) {
        return NULL;
    }
    return old_prog_brk;
}

struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* list_next;
    MallocMetadata* list_prev;
    MallocMetadata* histo_next;
    MallocMetadata* histo_prev;
};


static MallocMetadata* all_list_head = nullptr;
static MallocMetadata* all_list_end = nullptr;
static MallocMetadata* free_histogram[128] = {nullptr};
static size_t num_of_metadatas = 0;
static size_t mmaped_blocks_num = 0;
static size_t mmaped_bytes_num = 0;


void print_all_list(){
    std::cout << "printing all_list:" << std::endl;
    if (!all_list_head){
        std::cout << "list is empty!" << std::endl;
    }
    MallocMetadata* populate = all_list_head;
    int count = 0;
    while (populate) {
        std::cout << "[" << ++count << "]" << " address: " << populate + 1 <<  " size: "<< populate->size << " is_free: " << populate->is_free << " prev: " << populate->list_prev << " next: " << populate->list_next << std::endl;
        populate = populate->list_next;
    }
}

bool found_93(){
    for (int i = 0; i < 128; i++) {
        MallocMetadata* populate = free_histogram[i];
        while (populate){
            if(populate->size == 93) {
                return true;
            } 
            populate = populate->histo_next;
        }
    }
    return false;
}

void print_histogram(){
    std::cout << "printing histogram:" << std::endl;
    for (int i = 0; i < 128; i++)
    {
        MallocMetadata* populate = free_histogram[i];
        while (populate){
            std::cout << "i=" << i << " size: " <<  populate->size << " is_free: " << populate->is_free << std::endl;
            populate = populate->histo_next;
        }
         
    }
}
    

static MallocMetadata* create_metadata_sbrk(size_t size_in, bool is_free){
    void* ptr_void = smalloc_naive(sizeof(MallocMetadata));
    if (!ptr_void){
        return NULL;
    }
    MallocMetadata* new_node = (MallocMetadata*)ptr_void;
    new_node->size = size_in;
    new_node->is_free = is_free;
    new_node->histo_next = NULL;
    new_node->histo_prev = NULL;
    new_node->list_next = NULL;
    new_node->list_prev = NULL;
    
    num_of_metadatas++;
    
    return new_node;
}

static void remove_node_histo (MallocMetadata* ptr) {
    if (!ptr) {
        return;
    }
    if (!ptr->is_free){
        return;
    }
    int i = ptr->size / 1024;
    if (!free_histogram[i]) {
        return;
    }
    MallocMetadata* prev_ptr = ptr->histo_prev;
    MallocMetadata* next_ptr = ptr->histo_next;
    // first node in list
    if (!prev_ptr){
        free_histogram[i] = next_ptr;
        if (next_ptr){
            free_histogram[i]->histo_prev = NULL;
        }
        ptr->histo_prev = NULL;
        ptr->histo_next = NULL;
        return;
    }
    prev_ptr->histo_next = next_ptr;
    if(next_ptr)
        next_ptr->histo_prev = prev_ptr;
    ptr->histo_prev = NULL;
    ptr->histo_next = NULL;
}

// there is ia copy in add_node_histo - make sure to change both
static void add_node_list (MallocMetadata* node_to_add, MallocMetadata* ptr_prev) {
    // add to start of list
    if (!ptr_prev){
        node_to_add->list_next = all_list_head;
        node_to_add->list_prev = NULL;
        all_list_head = node_to_add;
        all_list_head->list_prev = NULL;
        // last node in list
        if (!node_to_add->list_next){
            all_list_end = node_to_add;
        }
        return;
    }
    MallocMetadata* ptr_next = ptr_prev->list_next;
    node_to_add->list_prev = ptr_prev; 
    ptr_prev->list_next = node_to_add;
    node_to_add->list_next = ptr_next;
    if (ptr_next)
        ptr_next->list_prev = node_to_add;
    // last node in list
    if (!node_to_add->list_next){
        all_list_end = node_to_add;
    }    
}


static void add_node_histo (MallocMetadata* node_to_add) {
    int i = node_to_add->size / 1024;
    MallocMetadata* ptr = free_histogram[i];
    MallocMetadata* ptr_prev = NULL;
    while(ptr) {
        if(ptr->size > node_to_add->size) {
            break;
        }
        ptr_prev = ptr;
        ptr = ptr->histo_next;
        if (!ptr){
            break;
        }
    }
    // copied from add_node_list - make sure to change both
    // add to list head
    if (!ptr_prev){
        node_to_add->histo_next = free_histogram[i];
        node_to_add->histo_prev = NULL;
        if(free_histogram[i])
            free_histogram[i]->histo_prev = node_to_add;
        free_histogram[i] = node_to_add;
        return;
    }
    MallocMetadata* ptr_next = ptr_prev->histo_next;
    node_to_add->histo_prev = ptr_prev; 
    ptr_prev->histo_next = node_to_add;
    node_to_add->histo_next = ptr_next;
    if (ptr_next)
        ptr_next->histo_prev = node_to_add;
}

static void place_metadata(MallocMetadata* ptr, bool is_free, size_t size){    
    ptr->is_free = is_free;
    ptr->size = size;
}

//  splits the current block like this: 
//  [metadata][........block........] -> [metadata][..block..][metadata][_______]
// func = 0 ->smalloc , func = 1->srealloc
static MallocMetadata* split_block(MallocMetadata* current, size_t size, bool func) {
    size_t remaining_size = current->size - size - sizeof(MallocMetadata);
    MallocMetadata* new_meta_free = (MallocMetadata*)((char*)current + sizeof(MallocMetadata) + size);
    place_metadata(new_meta_free, true, remaining_size);
    num_of_metadatas++;
    // update histogram (remove old one and add new one)
    add_node_histo(new_meta_free);
    remove_node_histo(current);
    // update current metadata
    place_metadata(current, false, size);
    // update list (only in case of split)
    add_node_list(new_meta_free, current);
    //returns the pointer to the beggining of the allocated block
    return current;
}

// returns a pointer to METADATA of allocated block
//currently we don't take into consideration!!! a situation in which size is smaller than the current block size
static void* sbrk_wrapper (size_t size) {
    // in case the wilderness block is free - sbrk the difference & remove him from the histogram of free nodes
    if (all_list_end) {
        size_t wilderness_size = all_list_end->size; 
        if(all_list_end->is_free) {
            void* newptr = sbrk(size - wilderness_size);
            if (newptr == (void*)(-1)) {
                return NULL;
            }
            remove_node_histo(all_list_end);
            all_list_end->size = size;
            all_list_end->is_free = false;
            return all_list_end+1;
        }
    }
    // otherwise: normal sbrk
    MallocMetadata* new_metadata = create_metadata_sbrk(size, false);
    if(!new_metadata)
        return NULL;
    void* newptr = sbrk(size);
    if (newptr == (void*)(-1)) {
        return NULL;
    }
    add_node_list(new_metadata, all_list_end);
    return newptr;
}

static void *mmap_wrapper(size_t size) {
    void *ptr = nullptr;
    ptr = mmap(NULL, (size + sizeof(MallocMetadata)), PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (ptr == (void *)-1)
    { 
        return nullptr;
    }
    MallocMetadata *ptr_metadata = (MallocMetadata *)ptr;
    ptr_metadata->is_free = false;
    ptr_metadata->size = size;
    //update the stats
    num_of_metadatas++;
    mmaped_blocks_num++;
    mmaped_bytes_num += size;
    return ptr_metadata + 1;
}

static void deallocate_mmap(MallocMetadata* ptr) {
    //update stats
    mmaped_blocks_num--;
    mmaped_bytes_num -= ptr->size;
    num_of_metadatas--;
    //unmap please
    munmap(ptr, ptr->size + sizeof(MallocMetadata));
    return;
}

void* smalloc(size_t size){
    void *status = nullptr;
    if (size == 0 || size > 100000000)
    {
        return NULL;
    }
    if (size >= MMAP_SIZE) { // check if size is more than 128kb
        return mmap_wrapper(size);
    }
    // go through histo to find empty block
    int initial_index = size / 1024;
    bool found_block = 0;
    MallocMetadata* current = free_histogram[initial_index];
    for(int i = initial_index; i < 128; i++) {
        current = free_histogram[i];
        while(current) {
            if((current->size >= size) && current->is_free) {
                found_block = 1;
                break;
            }
            current = current->histo_next;
        }
        if(found_block) {
            break;
        }
    }
    // need to allocate new memory
    if(!found_block) {
        //wilderness or normal sbrk
        return sbrk_wrapper(size);
    }
    // found a free block large enough
    else {
        size_t remaining_size = current->size - size - sizeof(MallocMetadata);
        // no need to split
        if (current->size - size < 128 + sizeof(MallocMetadata)) { 
            remove_node_histo(current);
            current->is_free = false; 
            return ++current;
        }
        // split block:
        // create new metadata for split (the remaining free block)        
        return split_block(current, size, SMALLOC) + 1;       
    }
}

// checks if right side is free and unites if needed
void unite_right(MallocMetadata* p_metadata){
    if (!p_metadata)
        return;
    if (!p_metadata->is_free)
        return;
    MallocMetadata* node_to_remove = p_metadata->list_next;
    // no block to unite with :(
    if (!node_to_remove) {
        return;
    }
    if (!node_to_remove->is_free) {
        return;
    }
    // unite with right side:
    // remove both blocks from the free histogram
    remove_node_histo(node_to_remove);
    remove_node_histo(p_metadata);
    //remove node from the list: a lazy way to implement remove_node_list
    MallocMetadata* prev_ptr = node_to_remove->list_prev;
    MallocMetadata* next_ptr = node_to_remove->list_next;
    prev_ptr->list_next = next_ptr;
    if(next_ptr)
        next_ptr->list_prev = prev_ptr;
    // last node in list
    else {
        all_list_end = prev_ptr;
    }
    node_to_remove->list_prev = NULL;
    node_to_remove->list_next = NULL;
    // update p_metadata
    p_metadata->size += node_to_remove->size + sizeof(MallocMetadata);
    p_metadata->is_free = true;
    //add the new p_metadata to histogram
    add_node_histo(p_metadata);
    num_of_metadatas--;
}


void sfree(void* p) {
    // If ‘p’ is NULL or already released, simply returns.
    if (!p)
        return;
    MallocMetadata* p_metadata = (MallocMetadata*)((char*)p - sizeof(MallocMetadata));
    if (p_metadata->is_free)
        return;
    if (p_metadata->size >= MMAP_SIZE) {
        deallocate_mmap(p_metadata);
        return;
    }
    // add to free histogram
    p_metadata->is_free = true;
    add_node_histo(p_metadata);
    unite_right(p_metadata);
    unite_right(p_metadata->list_prev);    
}

void unite_srealloc(MallocMetadata* oldp_metadata, MallocMetadata* unite_with, bool second_time){
    oldp_metadata->is_free = true;
    if (!second_time)
        add_node_histo(oldp_metadata);
    unite_right(unite_with);
}

// 2. After the process described in the previous section, if one of the options ‘a’ to ‘d’ worked,
// and the unused section of the block is large enough, split the block (according to the
// instructions in challenge 1)!
// 3. If srealloc()is called on the “wilderness” chunk and the chunk can be enlarged (with
// sbrk()) to accommodate the new request, go ahead and enlarge it.
void* srealloc(void* oldp, size_t size) {
    if (size == 0 || size > 100000000) {
        return NULL;
    }
    // If ‘oldp’ is NULL, allocates space for ‘size’ bytes and returns a pointer to it.
    if (!oldp){
        return smalloc(size);
    }
    MallocMetadata* oldp_metadata = (MallocMetadata*)((char*)oldp - sizeof(MallocMetadata));
    //mmap treatment
    if (size >= MMAP_SIZE) {
        void *ptr = smalloc(size); 
        if (!ptr) {
            return NULL;
        }
        memmove(ptr, oldp, MIN(oldp_metadata->size, size));
        sfree(oldp); 
        return ptr;
    }
    // a. Try to reuse the current block without any merging.
    if (oldp_metadata->size >= size) {
        if (oldp_metadata->size - size >= 128 + sizeof(MallocMetadata))
            // remember that split doesn't do ++
            return split_block(oldp_metadata, size, SREALLOC) + 1;
        return oldp;
    }
    // b. Try to merge with the adjacent block with the lower address.
    MallocMetadata* oldp_prev = oldp_metadata->list_prev;
    if (oldp_prev) {
        if(oldp_prev->size + oldp_metadata->size + sizeof(MallocMetadata) >=size && oldp_prev->is_free) {
            unite_srealloc(oldp_metadata, oldp_prev, 0);
            memmove(oldp_prev + 1, oldp, MIN(oldp_metadata->size, oldp_prev->size));
            remove_node_histo(oldp_prev);
            oldp_prev->is_free = false;
            // check if need to split
            if (oldp_prev->size - size >= 128 + sizeof(MallocMetadata)){
                oldp_prev = split_block(oldp_prev, size, SREALLOC);
            }
            //rejoin free blocks (free from split and right block if he's free)
            return oldp_prev + 1;
        }
    }
    // c. Try to merge with the adjacent block with the higher address.
    MallocMetadata* oldp_next = oldp_metadata->list_next;
    if (oldp_next) {
        if (oldp_next->size + oldp_metadata->size + sizeof(MallocMetadata) >= size && oldp_next->is_free){
            unite_srealloc(oldp_metadata, oldp_metadata, 0);
            // no need to memmove because pointer to memory stays at the same place
            remove_node_histo(oldp_metadata);
            oldp_metadata->is_free = false;
            // check if need to split
            if (oldp_metadata->size - size >= 128 + sizeof(MallocMetadata)){
                oldp_metadata = split_block(oldp_metadata, size, SREALLOC);
            }
            return oldp_metadata + 1;
        }
    }
    // d. Try to merge all those three adjacent blocks together.
    if (oldp_prev && oldp_next){
        if (oldp_prev->size + oldp_next->size + oldp_metadata->size + 2 * sizeof(MallocMetadata) >= size && oldp_next->is_free && oldp_prev->is_free){
            unite_srealloc(oldp_metadata, oldp_metadata, 0);
            unite_srealloc(oldp_metadata, oldp_prev, 1);
            oldp_prev->is_free = false;
            memmove(oldp_prev + 1, oldp, MIN(oldp_metadata->size, oldp_prev->size));
            if (oldp_prev->size - size >= 128 + sizeof(MallocMetadata)){
                oldp_prev = split_block(oldp_prev, size, SREALLOC);
            }
            return oldp_prev + 1;
        }
    }
    // wilderness case: if the wilderness is the block to realloc, use it!
    if(!oldp_metadata->list_next && all_list_end == oldp_metadata) {
        size_t wilderness_size = oldp_metadata->size;
        void* newptr = sbrk(size - wilderness_size);
        if (newptr == (void*)(-1)) {
            return NULL;
        }
        all_list_end->size = size;
        return oldp_metadata + 1;
    }
    // e. Try to find a different block that’s large enough to contain the request (don’t forget
    // that you need to free the current block, therefore you should, if possible, merge it
    // with neighboring blocks before proceeding).
    // f. Allocate a new block with sbrk().           
    // otherwise, finds/allocates ‘size’ bytes for a new space, copies content of oldp into the new allocated space and frees the oldp.
    void* newp = smalloc(size);
    if (!newp)
        return NULL;
    // copy the contents from oldp to newp
    memmove(newp, oldp, MIN(oldp_metadata->size, size));
    sfree(oldp);
    // Returns pointer to the first byte in the (newly) allocated space.
    return newp;
}

void* scalloc(size_t num, size_t size) {
    void* p = smalloc(num*size);
    if(!p) {
        return NULL;
    }
    memset(p, 0, num*size);
    return p;
}

size_t _num_meta_data_bytes() {
// Returns the overall number of meta-data bytes currently in the heap.
    return num_of_metadatas * sizeof(MallocMetadata);
}

size_t _size_meta_data() {
// Returns the number of bytes of a single meta-data structure in your system.
    return sizeof(MallocMetadata);
}

// Returns the number of allocated blocks in the heap that are currently free.
size_t _num_free_blocks(){
    MallocMetadata* current = all_list_head;
    size_t count = 0;
    while (current){
        if (current->is_free){ 
            count++;
        }
        current = current->list_next;
    }
    return count;
}

// Returns the overall (free and used) number of allocated blocks in the heap.
size_t _num_allocated_blocks(){
    MallocMetadata* current = all_list_head;
    size_t count = 0;
    while (current){
        count++;
        current = current->list_next;
    }
    return count + mmaped_blocks_num;
}

size_t _num_free_bytes(){
    MallocMetadata* current = all_list_head;
    size_t res = 0;
    while (current) {
        if (current->is_free){
            res += current->size;
        }
        current = current->list_next;
    }
    return res; 
}

size_t _num_allocated_bytes(){
    MallocMetadata* current = all_list_head;
    size_t res = 0;
    while (current){
        res += current->size;
        current = current->list_next;
    }
    return res + mmaped_bytes_num;
}
