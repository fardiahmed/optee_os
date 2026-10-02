// SPDX-License-Identifier: BSD-2-Clause
/* Verified boot root of trust, kept in secure memory for the current boot only */

#include <kernel/pseudo_ta.h>
#include <kernel/spinlock.h>
#include <kernel/tee_ta_manager.h>
#include <kernel/ts_manager.h>
#include <pta_boot_rot.h>
#include <string.h>
#include <tee_api_defines.h>

#define PTA_NAME "boot_rot.pta"

static struct pta_boot_rot boot_rot;
static bool boot_rot_set;
/* Set once the bootloader set it or a TA read it: the normal world cannot change it later */
static bool boot_rot_closed;
static unsigned int boot_rot_lock = SPINLOCK_UNLOCK;

static bool called_by_ta(void)
{
	struct tee_ta_session *s = to_ta_session(ts_get_current_session());

	return s->clnt_id.login == TEE_LOGIN_TRUSTED_APP;
}

static TEE_Result set_rot(uint32_t ptypes, TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
					  TEE_PARAM_TYPE_NONE,
					  TEE_PARAM_TYPE_NONE,
					  TEE_PARAM_TYPE_NONE);
	TEE_Result res = TEE_ERROR_ACCESS_DENIED;
	struct pta_boot_rot rot = { };
	uint32_t exceptions = 0;

	if (ptypes != exp_pt || !params[0].memref.buffer ||
	    params[0].memref.size != sizeof(rot))
		return TEE_ERROR_BAD_PARAMETERS;
	if (called_by_ta())
		return TEE_ERROR_ACCESS_DENIED;

	/* Normal world buffer: copy it once */
	memcpy(&rot, params[0].memref.buffer, sizeof(rot));
	if (rot.device_locked > 1 ||
	    rot.verified_boot_state > PTA_BOOT_ROT_STATE_RED)
		return TEE_ERROR_BAD_PARAMETERS;

	exceptions = cpu_spin_lock_xsave(&boot_rot_lock);
	if (!boot_rot_closed) {
		boot_rot = rot;
		boot_rot_set = true;
		boot_rot_closed = true;
		res = TEE_SUCCESS;
	}
	cpu_spin_unlock_xrestore(&boot_rot_lock, exceptions);

	if (!res)
		IMSG("Boot root of trust: state %"PRIu32", %slocked",
		     rot.verified_boot_state, rot.device_locked ? "" : "un");
	return res;
}

static TEE_Result get_rot(uint32_t ptypes, TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,
					  TEE_PARAM_TYPE_NONE,
					  TEE_PARAM_TYPE_NONE,
					  TEE_PARAM_TYPE_NONE);
	struct pta_boot_rot rot = { };
	uint32_t exceptions = 0;
	bool set = false;

	if (ptypes != exp_pt)
		return TEE_ERROR_BAD_PARAMETERS;
	if (!called_by_ta())
		return TEE_ERROR_ACCESS_DENIED;

	exceptions = cpu_spin_lock_xsave(&boot_rot_lock);
	boot_rot_closed = true;
	set = boot_rot_set;
	rot = boot_rot;
	cpu_spin_unlock_xrestore(&boot_rot_lock, exceptions);

	if (!set)
		return TEE_ERROR_ITEM_NOT_FOUND;
	if (params[0].memref.size < sizeof(rot) || !params[0].memref.buffer) {
		params[0].memref.size = sizeof(rot);
		return TEE_ERROR_SHORT_BUFFER;
	}
	memcpy(params[0].memref.buffer, &rot, sizeof(rot));
	params[0].memref.size = sizeof(rot);
	return TEE_SUCCESS;
}

static TEE_Result invoke_command(void *session __unused, uint32_t cmd,
				 uint32_t ptypes,
				 TEE_Param params[TEE_NUM_PARAMS])
{
	switch (cmd) {
	case PTA_BOOT_ROT_CMD_SET:
		return set_rot(ptypes, params);
	case PTA_BOOT_ROT_CMD_GET:
		return get_rot(ptypes, params);
	default:
		return TEE_ERROR_NOT_IMPLEMENTED;
	}
}

pseudo_ta_register(.uuid = PTA_BOOT_ROT_UUID, .name = PTA_NAME,
		   .flags = PTA_DEFAULT_FLAGS | TA_FLAG_CONCURRENT,
		   .invoke_command_entry_point = invoke_command);
