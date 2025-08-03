// SPDX-License-Identifier: BSD-3-Clause

#include "memlist.h"

#include <unistd.h>
#include <stdlib.h>

static struct block_meta *memhead;
static struct block_meta *memtail;
static size_t memlist_size;

static struct block_meta *__memlist_insert(struct block_meta *block,
										   const size_t index)
{
	if (!memlist_size) {
		memhead = block;
		memtail = block;
		memlist_size++;
		return block;
	}

	if (index >= memlist_size) {
		memtail->next = block;

		block->prev = memtail;
		block->next = NULL;
		memtail = block;

		memlist_size++;
		return block;
	}

	if (index == 0) {
		memhead->prev = block;

		block->next = memhead;
		block->prev = NULL;
		memhead = block;

		memlist_size++;
		return block;
	}

	struct block_meta *it = memhead;

	for (size_t i = 0; i < index; i++)
		it = it->next;

	block->prev = it->prev;
	block->next = it;

	it->prev->next = block;
	it->prev = block;

	memlist_size++;
	return block;
}

static size_t __memlist_pool(struct block_meta *block)
{
	struct block_meta *it = memhead;
	size_t index = 0;

	while (it != block) {
		it = it->next;
		index++;
	}
	return index;
}

static struct block_meta *__memlist_pool_free(size_t size)
{
	struct block_meta *it = memhead;
	struct block_meta *free_block = NULL;
	size_t min_size = 1 << 31;

	for (size_t i = 0; i < memlist_size; i++) {
		if (it->status == STATUS_FREE && ALIGN(it->size) >= ALIGN(size) &&
			ALIGN(it->size) < min_size) {
			min_size = ALIGN(it->size);
			free_block = it;
		}
		it = it->next;
	}
	return free_block;
}

static struct block_meta *__memlist_fit(size_t size)
{
	if (!memlist_size)
		return NULL;
	struct block_meta *block = __memlist_pool_free(size);

	if (!block)
		return NULL;
	// Try split block and join split with other
	struct block_meta *split = memlist_split(block, size);

	if (split)
		memlist_join(split, 0);
	block->status = STATUS_ALLOC;
	return block;
}

static void *__memlist_expand(size_t size)
{
	if (memtail->status != STATUS_FREE || ALIGN(size) <= ALIGN(memtail->size))
		return NULL;
	void *block = sbrk(ALIGN(size) - ALIGN(memtail->size));

	DIE(block == NULL, "Allocation failed after trying to expand.");

	memtail->status = STATUS_ALLOC;
	memtail->size = ALIGN(size);
	return memtail;
}

struct block_meta *memlist_split(struct block_meta *block, size_t size)
{
	if (ALIGN(block->size) < ALIGNMENT + ALIGN(size) + META_SIZE)
		return NULL;
	struct block_meta *split = (void *)block + META_SIZE + ALIGN(size);

	split->size = ALIGN(block->size) - ALIGN(size) - META_SIZE;
	split->status = STATUS_FREE;

	block->size = ALIGN(size);
	block->status = STATUS_ALLOC;

	size_t index = __memlist_pool(block);

	__memlist_insert(split, index + 1);
	split->status = STATUS_FREE;
	return split;
}

struct block_meta *memlist_populate_metadata(void *allocated,
											 size_t size, size_t threshold)
{
	struct block_meta *block = (struct block_meta *)allocated;

	block->size = ALIGN(size);
	block->status = ALIGN(size) + META_SIZE >= threshold
					? STATUS_MAPPED
					: STATUS_ALLOC;
	return block;
}

struct block_meta *memlist_insert(struct block_meta *block)
{
	if (block->status == STATUS_MAPPED)
		return __memlist_insert(block, 0);
	return  __memlist_insert(block, memlist_size);
}

void *memlist_remove(struct block_meta *block)
{
	if (memlist_size == 1) {
		memhead = NULL;
		memtail = NULL;
		memlist_size--;
		return block;
	}

	if (block == memhead) {
		memhead->next->prev = NULL;
		memhead = memhead->next;
		memlist_size--;
		return block;
	}

	if (block == memtail) {
		memtail->prev->next = NULL;
		memtail = memtail->prev;
		memlist_size--;
		return block;
	}

	block->prev->next = block->next;
	block->next->prev = block->prev;
	memlist_size--;
	return block;
}

struct block_meta *memlist_join(struct block_meta *block, int check_prev)
{
	struct block_meta *it = block;

	if (check_prev)
		while (it && it->prev && it->prev->status == STATUS_FREE)
			it = it->prev;

	while (it != memtail && it->next->status == STATUS_FREE) {
		it->size = ALIGN(it->size) + ALIGN(it->next->size) + META_SIZE;
		memlist_remove((void *)it->next);
	}
	return it;
}

void *memlist_rfit(struct block_meta *block, size_t size)
{
	if (block != memtail)
		return NULL;
	block->status = STATUS_FREE;
	block = __memlist_expand(size);
	if (!block)
		return NULL;

	block->status = STATUS_ALLOC;
	return block;
}

void *memlist_get_free(size_t size)
{
	void *block = __memlist_fit(size);

	if (block)
		return block;
	block = __memlist_expand(size);
	if (block)
		return block;
	return NULL;
}
