#include "fmo_display_policy.h"

void fmo_display_policy_touch(fmo_display_policy_t *policy, uint64_t now_ms)
{
    policy->activity_ms = now_ms;
}

fmo_display_level_t fmo_display_policy_step(fmo_display_policy_t *policy,
                                           uint64_t now_ms, bool keep_awake)
{
    if (keep_awake || now_ms < policy->activity_ms) policy->activity_ms = now_ms;
    uint64_t idle = now_ms - policy->activity_ms;
    policy->level = idle >= FMO_DISPLAY_OFF_MS ? FMO_DISPLAY_DARK :
                    idle >= FMO_DISPLAY_DIM_MS ? FMO_DISPLAY_DIM : FMO_DISPLAY_ACTIVE;
    return policy->level;
}
