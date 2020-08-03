#ifndef PLDM_TYPES_H
#define PLDM_TYPES_H

#include <stdint.h>

typedef union {
	uint8_t byte;
	struct {
		uint8_t bit0 : 1;
		uint8_t bit1 : 1;
		uint8_t bit2 : 1;
		uint8_t bit3 : 1;
		uint8_t bit4 : 1;
		uint8_t bit5 : 1;
		uint8_t bit6 : 1;
		uint8_t bit7 : 1;
	} __attribute__((packed)) bits;
} bitfield8_t;

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

typedef uint8_t bool8_t;

typedef union {
	uint32_t value;
	struct {
		uint8_t bit0 : 1;
		uint8_t bit1 : 1;
		uint8_t bit2 : 1;
		uint8_t bit3 : 1;
		uint8_t bit4 : 1;
		uint8_t bit5 : 1;
		uint8_t bit6 : 1;
		uint8_t bit7 : 1;
		uint8_t bit8 : 1;
		uint8_t bit9 : 1;
		uint8_t bit10 : 1;
		uint8_t bit11 : 1;
		uint8_t bit12 : 1;
		uint8_t bit13 : 1;
		uint8_t bit14 : 1;
		uint8_t bit15 : 1;
		uint8_t bit16 : 1;
		uint8_t bit17 : 1;
		uint8_t bit18 : 1;
		uint8_t bit19 : 1;
		uint8_t bit20 : 1;
		uint8_t bit21 : 1;
		uint8_t bit22 : 1;
		uint8_t bit23 : 1;
		uint8_t bit24 : 1;
		uint8_t bit25 : 1;
		uint8_t bit26 : 1;
		uint8_t bit27 : 1;
		uint8_t bit28 : 1;
		uint8_t bit29 : 1;
		uint8_t bit30 : 1;
		uint8_t bit31 : 1;
	} __attribute__((packed)) bits;
} bitfield32_t;

typedef float real32_t;

typedef enum {
	PLDM_TS104__UTCR_UTC_UNSPECIFIED,
	PLDM_TS104_UTCR_MINUTE,
	PLDM_TS104_UTCR_TEN_MINUTE,
	PLDM_TS104_UTCR_HOUR
} utc_resolution;

typedef enum {
	PLDM_TS104_TR_MICROSECOND,
	PLDM_TS104_TR_TEN_MICROSECOND,
	PLDM_TS104_TR_HUNDRED_MICROSECOND,
	PLDM_TS104_TR_MILLISECOND,
	PLDM_TS104_TR_TEN_MILLISECOND,
	PLDM_TS104_TR_HUNDRED_MILLISECOND,
	PLDM_TS104_TR_SECOND,
	PLDM_TS104_TR_TEN_SECOND,
	PLDM_TS104_TR_MINUTE,
	PLDM_TS104_TR_TEN_MINUTE_TR,
	PLDM_TS104_TR_HOUR,
	PLDM_TS104_TR_DAY,
	PLDM_TS104_TR_MONTH,
	PLDM_TS104_TR_YEAR
} time_resolution;

/* Timestamp104 datatype format */
typedef struct timestamp104 {
	int16_t utc_offset; // UTC offset in minutes
	uint8_t microsecond[3];
	uint8_t seconds;
	uint8_t minute;
	uint8_t hour;
	uint8_t day;
	uint8_t month;
	uint16_t year;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	utc_resolution ur : 4;
	time_resolution tr : 4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	time_resolution tr : 4;
	utc_resolution ur : 4;
#endif

} __attribute__((packed)) timestamp104_t;

#endif /* PLDM_TYPES_H */
