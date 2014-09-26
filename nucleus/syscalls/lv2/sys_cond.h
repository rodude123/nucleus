/**
 * (c) 2014 Nucleus project. All rights reserved.
 * Released under GPL v2 license. Read LICENSE for more details.
 */

#pragma once

#include "nucleus/common.h"

struct sys_cond_attribute_t
{
    be_t<u32> pshared;
    be_t<u64> ipc_key;
    be_t<s32> flags;
    s8 name[8];
};

// SysCalls
s32 sys_cond_create(be_t<u32>* cond_id, u32 mutex_id, sys_cond_attribute_t* attr);
s32 sys_cond_destroy(u32 cond_id);
s32 sys_cond_wait(u32 cond_id, u64 timeout);
s32 sys_cond_signal(u32 cond_id);
s32 sys_cond_signal_all(u32 cond_id);
s32 sys_cond_signal_to(u32 cond_id, u32 thread_id);
