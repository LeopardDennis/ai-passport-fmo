#include "fmo_display_policy.h"
#include <assert.h>

int main(void)
{
    fmo_display_policy_t policy = {0};
    fmo_display_policy_touch(&policy, 1000);
    assert(fmo_display_policy_step(&policy, 30999, false) == FMO_DISPLAY_ACTIVE);
    assert(fmo_display_policy_step(&policy, 31000, false) == FMO_DISPLAY_DIM);
    assert(fmo_display_policy_step(&policy, 90999, false) == FMO_DISPLAY_DIM);
    assert(fmo_display_policy_step(&policy, 91000, false) == FMO_DISPLAY_DARK);
    fmo_display_policy_touch(&policy, 200000);
    assert(fmo_display_policy_step(&policy, 200000, false) == FMO_DISPLAY_ACTIVE);
    for (unsigned i = 1; i <= 100; ++i)
        assert(fmo_display_policy_step(&policy, 200000+i*10000, true) == FMO_DISPLAY_ACTIVE);
    assert(fmo_display_policy_step(&policy, 1229999, false) == FMO_DISPLAY_ACTIVE);
    assert(fmo_display_policy_step(&policy, 1230000, false) == FMO_DISPLAY_DIM);
    assert(fmo_display_policy_step(&policy, 1290000, false) == FMO_DISPLAY_DARK);
    // A setup session uses the same keep-awake rule; monotonic rollback is safe.
    assert(fmo_display_policy_step(&policy, 1500000, true) == FMO_DISPLAY_ACTIVE);
    assert(fmo_display_policy_step(&policy, 100, false) == FMO_DISPLAY_ACTIVE);
    return 0;
}
