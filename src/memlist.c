#include "memlist.h"

static struct block_meta *memhead;
static struct block_meta *memtail;
static int memlist_size = 0;

struct block_meta *memlist_wrap(void *allocated, size_t size)
{
    struct block_meta *block = (struct block_meta*)allocated;
    block->size = ALIGN(size);
    block->status = ALIGN(size) + META_SIZE >= MMAP_THRESHOLD
                    ? STATUS_MAPPED
                    : STATUS_ALLOC;
    return block;
}

static void *__memlist_insert(struct block_meta *block, const size_t index) {
    if (!memlist_size) {
        memhead = block;
        memtail = block;
        memlist_size++;
        return block;
    }

    // insert last
    if (index >= memlist_size) {
        memtail->next = block;

        block->prev = memtail;
        block->next = NULL;
        memtail = block;

        memlist_size++;
        return block;
    }

    // insert first
    if (index == 0) {
        memhead->prev = block;

        block->next = memhead;
        block->prev = NULL;
        memhead = block;

        memlist_size++;
        return block;
    }

    // insert middle
    struct block_meta *it = memhead;
    for (size_t i = 0; i < index; i++) {
        it = it->next;
    }

    it->prev->next = block;
    it->prev = block;

    block->prev = it->prev;
    block->next = it;

    memlist_size++;
    return block; 
}

static size_t __memlist_pool(struct block_meta *block)
{
    if (!memlist_size) {
        return 0;
    }
    size_t index = 0;
    struct block_meta *it = memhead;
    while (it != NULL && it != block) {
        it = it->next;
        index++;
    }
    DIE(it != block, "Memory pooling failed.");
    return index;
}

// Searches for the best fitting free block i.e. smallest possible block that
// would fit the provided size.
//
// Can return NULL if no block found.
static void *__memlist_pool_free(size_t size)
{
    struct block_meta *it = memhead;
    struct block_meta *free_block = NULL;
    size_t min_size = 1 << 31;
    for (size_t i = 0; i < memlist_size; i++) {
        if (it->status == STATUS_FREE && ALIGN(it->size) >= ALIGN(size)
            && ALIGN(it->size) < min_size) {
                min_size = ALIGN(it->size);
                free_block = it;
        }
        it = it->next;
    }
    return free_block;
}

// Splits the given STATUS_FREE block such that
//
// Can return NULL if cannot insert metadata for 2nd block.
static void *__memlist_split(struct block_meta *block, size_t size)
{
    if (ALIGN(block->size) < ALIGNMENT + ALIGN(size) + META_SIZE) {
        return NULL;
    }
    struct block_meta *split = (void *)block + META_SIZE + ALIGN(size);
    split->size = ALIGN(block->size) - ALIGN(size) - META_SIZE;
    split->status = STATUS_FREE;

    block->size = ALIGN(size);
    block->status = STATUS_ALLOC;

    size_t index = __memlist_pool(block);
    DIE(__memlist_insert(split, index + 1) != NULL,
        "Failed inserting splitted block");
    return split;
}

static void *__memlist_fit(size_t size)
{
    if (!memlist_size) {
        return NULL;
    }

    void *block = __memlist_pool_free(size);
    if (!block) {
        return NULL;
    }

    // Try split block and join split with other
    void *split = __memlist_split(block, size);
    if (split) {
        // TODO: Add coalesce
    }
    return block;
}

static void *__memlist_expand(size_t size)
{
    if (memtail->status != STATUS_FREE || ALIGN(memtail->size) <= ALIGN(size)) {
        return NULL;
    }
    void *block = sbrk(ALIGN(size) - ALIGN(memtail->size));
    DIE(block == NULL, "Allocation failed after trying to expand.");
    memtail->status = STATUS_ALLOC;
    memtail->size = ALIGN(size);
    return memtail;
}

void memlist_insert(struct block_meta *block, const size_t index)
{
    if (block->status == STATUS_MAPPED) {
        return __memlist_insert(block, 0);
    }
    return __memlist_insert(block, memlist_size);
}

void *memlist_get_free(size_t size)
{
    void *block = __memlist_fit(size);
    if (block) {
        return block;
    }
    block = __memlist_expand(size);
    if (block) {
        return block;
    }
    return NULL;
}

size_t memlist_get_size()
{
    return memlist_size;
}

int memlist_should_prealloc()
{
    return !memlist_size;
}