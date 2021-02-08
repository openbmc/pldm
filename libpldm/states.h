#ifndef STATES_H
#define STATES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pldm_types.h"

/** @brief PLDM enums for the boot progress state set
 */
enum pldm_boot_progress_states {
	PLDM_BOOT_NOT_ACTIVE = 1,
	PLDM_BOOT_COMPLETED = 2,
};

/** @brief PLDM enums for system power states
 */
enum pldm_system_power_states {
	PLDM_OFF_SOFT_GRACEFUL = 9,
};

/** @brief PLDM enums for link state set
 */
enum pldm_link_states {
	PLDM_CONNECTED = 1,
	PLDM_DISCONNECTED = 2,
};

/** @brief PLDM enums for configuration state set
 */
enum pldm_configuration_states {
	PLDM_VALID_CONFIG = 1,
	PLDM_INVALID_CONFIG = 2,
	PLDM_NOT_CONFIGURED = 3,
	PLDM_MISSING_CONFIG = 4,
};

#ifdef __cplusplus
}
#endif

#endif /* STATES_H */
