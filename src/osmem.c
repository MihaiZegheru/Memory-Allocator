// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"

#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memlist.h"

static int memallocated;

static void *__os_prealloc(void)
{
	void *allocated = sbrk(MMAP_THRESHOLD);

	DIE(allocated == (void *)-1, "Heap could not be preallocated.");
	struct block_meta *block = (struct block_meta *)allocated;

	block->size = MMAP_THRESHOLD - META_SIZE;
	block->status = STATUS_FREE;
	memallocated = 1;
	return allocated;
}

static void *__os_malloc(size_t size, size_t threshold)
{
	if (!size)
		return NULL;
	size_t block_size = META_SIZE + ALIGN(size);
	void *allocated = NULL;

	// Check if allocation exceeds threshold
	if (block_size >= threshold) {
		allocated = mmap(0, block_size, PROT_READ | PROT_WRITE,
						 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		DIE(allocated == (void *)-1,
			"Allocation failed after exceeding threshold.");
	} else {
		if (!memallocated) {
			allocated = __os_prealloc();
			memlist_insert((struct block_meta *)allocated);
		}
		allocated = memlist_get_free(size);
		if (allocated == NULL)
			allocated = sbrk(block_size);
		else
			return allocated + META_SIZE;
		DIE(allocated == NULL, "Failed allocation block on heap.");
	}

	struct block_meta *block =
			memlist_populate_metadata((struct block_meta *)allocated, size,
									  threshold);

	memlist_insert(block);
	return MEMSTART(allocated);
}

void *os_malloc(size_t size)
{
	return __os_malloc(size, MMAP_THRESHOLD);
}

void os_free(void *ptr)
{
	if (ptr == NULL)
		return;

	struct block_meta *block = (struct block_meta *)(ptr - META_SIZE);

	if (block->status == STATUS_MAPPED) {
		void *removed = memlist_remove(block);

		DIE(removed != block, "Failed removing memory block.");
		int result = munmap((void *)block, META_SIZE + ALIGN(block->size));

		DIE(result < 0, "Failed freeing memory block.");
	} else {
		block->status = STATUS_FREE;
		memlist_join(block, 1);
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (!nmemb || !size)
		return NULL;
	void *allocated = __os_malloc(nmemb * size, getpagesize());

	memset(allocated, 0, size * nmemb);
	return allocated;
}

void *os_realloc(void *ptr, size_t size)
{
	if (ptr && !size) {
		os_free(ptr);
		return NULL;
	}
	if (!ptr)
		return os_malloc(size);

	struct block_meta *block = ptr - META_SIZE;

	if (block->status == STATUS_FREE)
		return NULL;
	if (block->status == STATUS_MAPPED
		|| (META_SIZE + ALIGN(size) >= MMAP_THRESHOLD)) {
		void *allocated = os_malloc(size);
		size_t cpy_size = ALIGN(size) > ALIGN(block->size)
						  ? ALIGN(block->size)
						  : ALIGN(size);

		memcpy(allocated, ptr, cpy_size);
		os_free(MEMSTART(block));
		return allocated;
	}

	// Try refit block
	struct block_meta *rfit = memlist_rfit(block, ALIGN(size));

	if (rfit) {
		rfit->status = STATUS_ALLOC;
		return MEMSTART(rfit);
	}

	block = memlist_join(block, 0);
	block->status = STATUS_ALLOC;
	if (ALIGN(size) <= ALIGN(block->size)) {
		struct block_meta *split = memlist_split(block, ALIGN(size));

		if (split) {
			split->status = STATUS_FREE;
			memlist_join(split, 0);
		}
		return MEMSTART(block);
	}

	void *allocated = os_malloc(size);
	size_t cpy_size = ALIGN(size) > ALIGN(block->size)
							? ALIGN(block->size)
							: ALIGN(size);

	memcpy(allocated, ptr, cpy_size);
	os_free(MEMSTART(block));
	return allocated;
}
