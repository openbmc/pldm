#ifndef STATES_H
#define STATES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pldm_types.h"

/** @brief PLDM state set ids
 */
enum pldm_state_set_ids {
	PLDM_BOOT_PROGRESS_STATE = 196,
};

/** @brief PLDM enums for the boot progress state set
 */
enum pldm_boot_progress_states {
	PLDM_BOOT_NOT_ACTIVE = 1,
	PLDM_BOOT_COMPLETED = 2,
};

#ifdef __cplusplus
}
#endif

#endif /* STATES_H */
