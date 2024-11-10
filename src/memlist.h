/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include "block_meta.h"

#define ALIGNMENT 8
#define MMAP_THRESHOLD 1 << 17
#define PREALLOC_SIZE 128000
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define META_SIZE (ALIGN(sizeof(struct block_meta)))
#define SET_BLOCK_META(block, _size, _status, _next, _prev)	\
	do {	\
		block->size = _size;	\
		block->status = _status;	\
		block->next = _next;	\
		block->prev = _prev;	\
	} while(0)


void list_add(struct block_meta *block);