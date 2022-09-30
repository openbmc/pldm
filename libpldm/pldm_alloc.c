#include <assert.h>

#include "pldm_alloc.h"

struct {
	void *(*m_alloc)(size_t);
	void (*m_free)(void *);
	void *(*m_realloc)(void *, size_t);
} alloc_ops = {
#ifdef PLDM_DEFAULT_ALLOC
    malloc,
    free,
    realloc,
#else
    0
#endif
};

/* internal-only allocation functions */
void *__pldm_alloc(size_t size)
{
	if (alloc_ops.m_alloc)
		return alloc_ops.m_alloc(size);
	if (alloc_ops.m_realloc)
		return alloc_ops.m_realloc(NULL, size);
	assert(0);
	return NULL;
}

void __pldm_free(void *ptr)
{
	if (alloc_ops.m_free)
		alloc_ops.m_free(ptr);
	else if (alloc_ops.m_realloc)
		alloc_ops.m_realloc(ptr, 0);
	else
		assert(0);
}

void *__pldm_realloc(void *ptr, size_t size)
{
	if (alloc_ops.m_realloc)
		return alloc_ops.m_realloc(ptr, size);
	assert(0);
	return NULL;
}

void pldm_set_alloc_ops(void *(*m_alloc)(size_t), void (*m_free)(void *),
			void *(m_realloc)(void *, size_t))
{
	alloc_ops.m_alloc = m_alloc;
	alloc_ops.m_free = m_free;
	alloc_ops.m_realloc = m_realloc;
}
