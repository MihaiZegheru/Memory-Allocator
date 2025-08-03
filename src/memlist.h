// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "block_meta.h"

#define ALIGNMENT 8
#define MMAP_THRESHOLD		(128 * 1024)
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define META_SIZE (ALIGN(sizeof(struct block_meta)))
#define MEMSTART(block) ((void *)block + META_SIZE)

struct block_meta* memlist_insert(struct block_meta* block);

void *memlist_remove(struct block_meta *block);

struct block_meta *memlist_populate_metadata(void *allocated,
											 size_t size, size_t threshold);

void *memlist_get_free(size_t size);

struct block_meta *memlist_join(struct block_meta *block, int check_prev);

void *memlist_rfit(struct block_meta *block, size_t size);

struct block_meta *memlist_split(struct block_meta* block, size_t size);
