/* SPDX-License-Identifier: Apache-2.0 */

#include <assert.h>

#include "libmctp-alloc.h"

struct {
	void *(*alloc)(size_t);
	void (*free)(void *);
	void *(*realloc)(void *, size_t);
} alloc_ops = {
    malloc,
    free,
    realloc,
};

/* internal-only allocation functions */
void *__mctp_alloc(size_t size)
{
	if (alloc_ops.alloc)
		return alloc_ops.alloc(size);
	if (alloc_ops.realloc)
		return alloc_ops.realloc(NULL, size);
	assert(0);
}

void __mctp_free(void *ptr)
{
	if (alloc_ops.free)
		alloc_ops.free(ptr);
	else if (alloc_ops.realloc)
		alloc_ops.realloc(ptr, 0);
	else
		assert(0);
}

void *__mctp_realloc(void *ptr, size_t size)
{
	if (alloc_ops.realloc)
		return alloc_ops.realloc(ptr, size);
	assert(0);
}

void mctp_set_alloc_ops(void *(*alloc)(size_t), void (*free)(void *),
			void *(realloc)(void *, size_t))
{
	alloc_ops.alloc = alloc;
	alloc_ops.free = free;
	alloc_ops.realloc = realloc;
}
