#include "fall_detect.h"

// Define label constants matching HAPT
#define LABEL_WALKING           1
#define LABEL_WALKING_UPSTAIRS  2
#define LABEL_WALKING_DOWNSTAIRS 3
#define LABEL_SITTING           4
#define LABEL_STANDING          5
#define LABEL_LAYING            6
#define LABEL_STAND_TO_SIT      7
#define LABEL_SIT_TO_STAND      8
#define LABEL_SIT_TO_LIE        9
#define LABEL_LIE_TO_SIT        10
#define LABEL_STAND_TO_LIE      11
#define LABEL_LIE_TO_STAND      12

static FallParams_t cfg_params = {
    .freefall_g = 0.60f,
    .impact_g = 2.00f,
    .confirm_windows = 4,
    .laying_confirm = 2,
    .voluntary_guard = 4,
    .recover_windows = 2
};

static FallState_t current_state = FALL_STATE_IDLE;
static uint8_t prev_label = 0;
static uint8_t guard_timer = 0;
static int8_t shock_timer = 0;
static uint8_t laying_count = 0;
static uint8_t recover_count = 0;

static void reset_soft(void) {
    current_state = FALL_STATE_IDLE;
    shock_timer = 0;
    laying_count = 0;
    recover_count = 0;
}

void Fall_Init(const FallParams_t *params) {
    if (params) {
        cfg_params = *params;
    }
    current_state = FALL_STATE_IDLE;
    prev_label = 0;
    guard_timer = 0;
    reset_soft();
}

static bool is_shock(float a_min, float a_max) {
    bool freefall = (a_min <= cfg_params.freefall_g);
    bool impact = (a_max >= cfg_params.impact_g);
    return (freefall && impact);
}

static bool is_active_upright(uint8_t label) {
    return (label == LABEL_WALKING || label == LABEL_WALKING_UPSTAIRS ||
            label == LABEL_WALKING_DOWNSTAIRS || label == LABEL_STANDING ||
            label == LABEL_SITTING);
}

static bool is_upright(uint8_t label) {
    return (label == LABEL_WALKING || label == LABEL_WALKING_UPSTAIRS ||
            label == LABEL_WALKING_DOWNSTAIRS || label == LABEL_STANDING);
}

static bool is_abrupt_laying(uint8_t label) {
    return (label == LABEL_LAYING && is_active_upright(prev_label) && guard_timer == 0);
}

FallEvent_t Fall_Update(uint8_t predicted_label, float a_min, float a_max) {
    FallEvent_t event = FALL_EVENT_NONE;

    if (guard_timer > 0) {
        guard_timer--;
    }

    if (predicted_label == LABEL_SIT_TO_LIE || predicted_label == LABEL_STAND_TO_LIE) {
        guard_timer = cfg_params.voluntary_guard;
    }

    bool shock = is_shock(a_min, a_max);

    switch (current_state) {
        case FALL_STATE_IDLE:
            if (shock || is_abrupt_laying(predicted_label)) {
                current_state = FALL_STATE_SHOCK;
                shock_timer = cfg_params.confirm_windows;
                laying_count = (predicted_label == LABEL_LAYING) ? 1 : 0;
                if (laying_count >= cfg_params.laying_confirm) {
                    current_state = FALL_STATE_FALLEN;
                    event = FALL_EVENT_DETECTED;
                }
            }
            break;

        case FALL_STATE_SHOCK:
            shock_timer--;
            if (predicted_label == LABEL_SIT_TO_LIE || predicted_label == LABEL_STAND_TO_LIE || guard_timer > 0) {
                reset_soft(); // Nằm xuống chủ động -> huỷ nghi ngờ
            } else if (predicted_label == LABEL_LAYING) {
                laying_count++;
                if (laying_count >= cfg_params.laying_confirm) {
                    current_state = FALL_STATE_FALLEN;
                    recover_count = 0;
                    event = FALL_EVENT_DETECTED;
                }
            } else if (is_upright(predicted_label)) {
                reset_soft(); // Tự đứng dậy / đi tiếp -> huỷ nghi ngờ
            } else if (shock_timer <= 0) {
                reset_soft();
            }
            break;

        case FALL_STATE_FALLEN:
            if (predicted_label == LABEL_LAYING) {
                recover_count = 0;
            } else {
                recover_count++;
                if (recover_count >= cfg_params.recover_windows) {
                    reset_soft();
                    event = FALL_EVENT_RECOVERED;
                }
            }
            break;
    }

    prev_label = predicted_label;
    return event;
}

FallState_t Fall_GetState(void) {
    return current_state;
}

const char* Fall_GetStateName(void) {
    switch (current_state) {
        case FALL_STATE_IDLE:   return "IDLE";
        case FALL_STATE_SHOCK:  return "SHOCK";
        case FALL_STATE_FALLEN: return "FALLEN (TE NGA!)";
        default:                return "UNKNOWN";
    }
}
