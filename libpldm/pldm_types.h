#include <stdint.h>

typedef uint8_t bitfield8_t;

/** @struct pldm_version
 *
 *
 */
typedef struct pldm_version {
	uint8_t major;
	uint8_t minor;
	uint8_t update;
	uint8_t alpha;
} __attribute__((packed)) ver32_t;
