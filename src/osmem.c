// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"

#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

#include "block_meta.h"
#include "memlist.h"

static void *__prealloc()
{
	void *allocated = sbrk(MMAP_THRESHOLD);
	DIE(allocated == (void*)-1, "Heap could not be preallocated.");

	struct block_meta *block = (struct block_meta*)allocated;
	SET_BLOCK_META(block, MMAP_THRESHOLD - META_SIZE, STATUS_FREE, NULL, NULL);
	return allocated;
}

void *os_malloc(size_t size)
{
	if (!size) {
		return NULL;
	}
	size_t block_size = META_SIZE + ALIGN(size);
	void *allocated = NULL;
	
	// Check if allocation exceeds threshold
	if (block_size >= MMAP_THRESHOLD) {
		allocated = mmap(0, block_size, PROT_READ | PROT_WRITE,
						 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		DIE(allocated == (void*)-1,
			"Allocation failed after exceeding MMAP_THRESHOLD");
	} else {
		if (memlist_should_prealloc()) {
			allocated = __prealloc();
			memlist_insert(allocated, 0);
		}
		allocated  = memlist_get_free(size);
		if (allocated == NULL) {
			allocated = sbrk(size);
		} else {
			return allocated + META_SIZE;
		}
		DIE(allocated == NULL, "Failed allocation block on heap.");
	}
	struct block_meta *block = memlist_wrap(allocated, size);
	memlist_insert(block, memlist_get_size() + 1);
	return allocated + META_SIZE;

}

void os_free(void *ptr)
{
	/* TODO: Implement os_free */
}

void *os_calloc(size_t nmemb, size_t size)
{
	/* TODO: Implement os_calloc */
	return NULL;
}

void *os_realloc(void *ptr, size_t size)
{
	/* TODO: Implement os_realloc */
	return NULL;
}
