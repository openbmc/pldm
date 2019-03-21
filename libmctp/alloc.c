/* SPDX-License-Identifier: Apache-2.0 */

#include <assert.h>

#include "libmctp.h"
#include "libmctp-alloc.h"

struct {
	void	*(*m_alloc)(size_t);
	void	(*m_free)(void *);
	void	*(*m_realloc)(void *, size_t);
} alloc_ops = {
#ifndef MCTP_NO_DEFAULT_ALLOC
	malloc,
	free,
	realloc,
#endif
};

/* internal-only allocation functions */
void *__mctp_alloc(size_t size)
{
	if (alloc_ops.m_alloc)
		return alloc_ops.m_alloc(size);
	if (alloc_ops.m_realloc)
		return alloc_ops.m_realloc(NULL, size);
	assert(0);
}

void __mctp_free(void *ptr)
{
	if (alloc_ops.m_free)
		alloc_ops.m_free(ptr);
	else if (alloc_ops.m_realloc)
		alloc_ops.m_realloc(ptr, 0);
	else
		assert(0);
}

void *__mctp_realloc(void *ptr, size_t size)
{
	if (alloc_ops.m_realloc)
		return alloc_ops.m_realloc(ptr, size);
	assert(0);
}

void mctp_set_alloc_ops(void *(*m_alloc)(size_t),
		void (*m_free)(void *),
		void *(m_realloc)(void *, size_t))
{
	alloc_ops.m_alloc = m_alloc;
	alloc_ops.m_free = m_free;
	alloc_ops.m_realloc = m_realloc;
}
