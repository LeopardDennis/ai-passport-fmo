#pragma once
#include <stdbool.h>
#include <stdint.h>

#define FMO_DISPLAY_DIM_MS 30000ULL
#define FMO_DISPLAY_OFF_MS 90000ULL

typedef enum { FMO_DISPLAY_ACTIVE, FMO_DISPLAY_DIM, FMO_DISPLAY_DARK } fmo_display_level_t;
typedef struct {
    uint64_t activity_ms;
    fmo_display_level_t level;
} fmo_display_policy_t;

void fmo_display_policy_touch(fmo_display_policy_t *policy, uint64_t now_ms);
fmo_display_level_t fmo_display_policy_step(fmo_display_policy_t *policy,
                                           uint64_t now_ms, bool keep_awake);
