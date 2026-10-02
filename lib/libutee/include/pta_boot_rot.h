/* SPDX-License-Identifier: BSD-2-Clause */
/* Verified boot root of trust: set once per boot by the bootloader, read by TAs (KeyMint) */
#ifndef __PTA_BOOT_ROT_H
#define __PTA_BOOT_ROT_H

#include <stdint.h>

#define PTA_BOOT_ROT_UUID { 0x368a3590, 0x0d0c, 0x400d, \
		{ 0x8b, 0xd5, 0x35, 0xcb, 0x37, 0x85, 0xe4, 0x35 } }

/* Android verified boot states, same values as KeyMint VerifiedBoot */
#define PTA_BOOT_ROT_STATE_GREEN	0
#define PTA_BOOT_ROT_STATE_YELLOW	1
#define PTA_BOOT_ROT_STATE_ORANGE	2
#define PTA_BOOT_ROT_STATE_RED		3

struct pta_boot_rot {
	uint8_t verified_boot_key[32];	/* SHA-256 of the vbmeta public key, zeros if none */
	uint8_t verified_boot_hash[32];	/* SHA-256 digest of the vbmeta images */
	uint32_t device_locked;
	uint32_t verified_boot_state;
};

/* [in] memref[0]: struct pta_boot_rot. Normal world only, once per boot and before any read */
#define PTA_BOOT_ROT_CMD_SET		0

/* [out] memref[0]: struct pta_boot_rot. TAs only, TEE_ERROR_ITEM_NOT_FOUND if never set */
#define PTA_BOOT_ROT_CMD_GET		1

#endif /* __PTA_BOOT_ROT_H */
