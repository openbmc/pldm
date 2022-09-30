#ifndef __PLDM_ALLOC_H
#define __PLDM_ALLOC_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

void *__pldm_alloc(size_t size);
void __pldm_free(void *ptr);
void *__pldm_realloc(void *ptr, size_t size);
void mctp_set_alloc_ops(void *(*m_alloc)(size_t), void (*m_free)(void *),
			void *(m_realloc)(void *, size_t));

#ifdef __cplusplus
}
#endif

#endif /* __PLDM_ALLOC_H */
