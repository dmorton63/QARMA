#include "control_base.h"

// ============================================================================
// Control Base Implementation
// ============================================================================

bool control_point_in_bounds(ControlBase* ctrl, int x, int y) {
    if (!ctrl) return false;
    return x >= ctrl->x && x < ctrl->x + ctrl->width &&
           y >= ctrl->y && y < ctrl->y + ctrl->height;
}

static uint32_t next_control_id = 1;

uint32_t control_generate_id(void) {
    return next_control_id++;
}
