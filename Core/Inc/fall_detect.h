#ifndef FALL_DETECT_H
#define FALL_DETECT_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    FALL_STATE_IDLE = 0,
    FALL_STATE_SHOCK = 1,
    FALL_STATE_FALLEN = 2
} FallState_t;

typedef enum {
    FALL_EVENT_NONE = 0,
    FALL_EVENT_DETECTED = 1,
    FALL_EVENT_RECOVERED = 2
} FallEvent_t;

typedef struct {
    float freefall_g;      // default 0.60g
    float impact_g;        // default 2.00g
    uint8_t confirm_windows; // default 4 (~5.1s)
    uint8_t laying_confirm;  // default 2 (~2.6s)
    uint8_t voluntary_guard; // default 4 (~5.1s)
    uint8_t recover_windows; // default 2 (~2.6s)
} FallParams_t;

void Fall_Init(const FallParams_t *params);
FallEvent_t Fall_Update(uint8_t predicted_label, float a_min, float a_max);
FallState_t Fall_GetState(void);
const char* Fall_GetStateName(void);

#endif /* FALL_DETECT_H */
