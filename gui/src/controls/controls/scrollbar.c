#include "controls/controls/scrollbar.h"
#include "renderer.h"
#include "qarma_input_events.h"

#define SCROLLBAR_WIDTH 16
#define SCROLLBAR_MIN_THUMB_SIZE 20
#define SCROLLBAR_TRACK_COLOR 0xFF3C3C3C
#define SCROLLBAR_THUMB_COLOR 0xFF606060
#define SCROLLBAR_THUMB_HOVER_COLOR 0xFF707070
#define SCROLLBAR_THUMB_PRESSED_COLOR 0xFF505050
#define SCROLLBAR_BORDER_COLOR 0xFF282828

static void scrollbar_calculate_thumb(Scrollbar* sb);
static bool scrollbar_handle_event_impl(Scrollbar* sb, QARMA_INPUT_EVENT* event);

void scrollbar_init(Scrollbar* sb, int x, int y, int length, ScrollbarOrientation orientation) {
    if (!sb) return;
    
    sb->base.x = x;
    sb->base.y = y;
    sb->orientation = orientation;
    
    if (orientation == SCROLLBAR_HORIZONTAL) {
        sb->base.width = length;
        sb->base.height = SCROLLBAR_WIDTH;
    } else {
        sb->base.width = SCROLLBAR_WIDTH;
        sb->base.height = length;
    }
    
    sb->base.visible = true;
    sb->base.enabled = true;
    sb->base.id = control_generate_id();
    sb->base.instance = sb;
    sb->base.render = (void (*)(void*, uint32_t*, int, int))scrollbar_render;
    sb->base.handle_event = (bool (*)(void*, QARMA_INPUT_EVENT*))scrollbar_handle_event_impl;
    
    sb->min_value = 0;
    sb->max_value = 100;
    sb->current_value = 0;
    sb->page_size = 10;
    
    sb->thumb_hovered = false;
    sb->thumb_pressed = false;
    sb->thumb_focused = false;
    sb->thumb_drag_offset = 0;
    
    sb->on_scroll = NULL;
    sb->userdata = NULL;
    
    scrollbar_calculate_thumb(sb);
}

static void scrollbar_calculate_thumb(Scrollbar* sb) {
    if (!sb) return;
    
    // Calculate track size (usable scrolling area)
    if (sb->orientation == SCROLLBAR_HORIZONTAL) {
        sb->track_size = sb->base.width - 4;  // 2px margins on each side
    } else {
        sb->track_size = sb->base.height - 4;
    }
    
    // Calculate thumb size based on page size vs total range
    int range = sb->max_value - sb->min_value;
    if (range <= 0) range = 1;
    
    float visible_ratio = (float)sb->page_size / (float)(range + sb->page_size);
    sb->thumb_size = (int)(sb->track_size * visible_ratio);
    
    // Enforce minimum thumb size
    if (sb->thumb_size < SCROLLBAR_MIN_THUMB_SIZE) {
        sb->thumb_size = SCROLLBAR_MIN_THUMB_SIZE;
    }
    if (sb->thumb_size > sb->track_size) {
        sb->thumb_size = sb->track_size;
    }
    
    // Calculate thumb position based on current value
    int scrollable_range = sb->track_size - sb->thumb_size;
    if (range > 0 && scrollable_range > 0) {
        float value_ratio = (float)(sb->current_value - sb->min_value) / (float)range;
        sb->thumb_pos = (int)(scrollable_range * value_ratio);
    } else {
        sb->thumb_pos = 0;
    }
}

void scrollbar_render(Scrollbar* sb, uint32_t* buffer, int buf_width, int buf_height) {
    if (!sb || !buffer || !sb->base.visible) return;
    
    int x = sb->base.x;
    int y = sb->base.y;
    int w = sb->base.width;
    int h = sb->base.height;
    
    // Draw track background
    draw_filled_rect(buffer, buf_width, x, y, w, h, SCROLLBAR_TRACK_COLOR);
    draw_rect_border(buffer, buf_width, x, y, w, h, SCROLLBAR_BORDER_COLOR, 1);
    
    // Calculate thumb rectangle
    int thumb_x, thumb_y, thumb_w, thumb_h;
    
    if (sb->orientation == SCROLLBAR_HORIZONTAL) {
        thumb_x = x + 2 + sb->thumb_pos;
        thumb_y = y + 2;
        thumb_w = sb->thumb_size;
        thumb_h = h - 4;
    } else {
        thumb_x = x + 2;
        thumb_y = y + 2 + sb->thumb_pos;
        thumb_w = w - 4;
        thumb_h = sb->thumb_size;
    }
    
    // Draw thumb with appropriate color
    uint32_t thumb_color;
    if (sb->thumb_pressed) {
        thumb_color = SCROLLBAR_THUMB_PRESSED_COLOR;
    } else if (sb->thumb_hovered) {
        thumb_color = SCROLLBAR_THUMB_HOVER_COLOR;
    } else {
        thumb_color = SCROLLBAR_THUMB_COLOR;
    }
    
    draw_filled_rect(buffer, buf_width, thumb_x, thumb_y, thumb_w, thumb_h, thumb_color);
    draw_rect_border(buffer, buf_width, thumb_x, thumb_y, thumb_w, thumb_h, SCROLLBAR_BORDER_COLOR, 1);
    
    // Draw focus indicator if focused
    if (sb->thumb_focused) {
        draw_rect_border(buffer, buf_width, thumb_x + 2, thumb_y + 2, thumb_w - 4, thumb_h - 4, 0xFFFFFFFF, 1);
    }
}

static bool scrollbar_handle_event_impl(Scrollbar* sb, QARMA_INPUT_EVENT* event) {
    if (!sb || !event || !sb->base.enabled) return false;
    
    scrollbar_calculate_thumb(sb);
    
    // Calculate thumb bounds
    int thumb_x, thumb_y, thumb_w, thumb_h;
    if (sb->orientation == SCROLLBAR_HORIZONTAL) {
        thumb_x = sb->base.x + 2 + sb->thumb_pos;
        thumb_y = sb->base.y + 2;
        thumb_w = sb->thumb_size;
        thumb_h = sb->base.height - 4;
    } else {
        thumb_x = sb->base.x + 2;
        thumb_y = sb->base.y + 2 + sb->thumb_pos;
        thumb_w = sb->base.width - 4;
        thumb_h = sb->thumb_size;
    }
    
    if (event->type == QARMA_INPUT_EVENT_MOUSE_MOVE) {
        int mx = event->data.mouse.x;
        int my = event->data.mouse.y;
        
        // Check if mouse is over thumb
        bool was_hovered = sb->thumb_hovered;
        sb->thumb_hovered = (mx >= thumb_x && mx < thumb_x + thumb_w &&
                             my >= thumb_y && my < thumb_y + thumb_h);
        
        // Handle dragging
        if (sb->thumb_pressed) {
            int drag_pos;
            if (sb->orientation == SCROLLBAR_HORIZONTAL) {
                drag_pos = mx - sb->base.x - 2 - sb->thumb_drag_offset;
            } else {
                drag_pos = my - sb->base.y - 2 - sb->thumb_drag_offset;
            }
            
            // Clamp to valid range
            int max_pos = sb->track_size - sb->thumb_size;
            if (drag_pos < 0) drag_pos = 0;
            if (drag_pos > max_pos) drag_pos = max_pos;
            
            sb->thumb_pos = drag_pos;
            
            // Calculate new value from thumb position
            int range = sb->max_value - sb->min_value;
            if (max_pos > 0) {
                float pos_ratio = (float)sb->thumb_pos / (float)max_pos;
                int new_value = sb->min_value + (int)(range * pos_ratio);
                
                if (new_value != sb->current_value) {
                    sb->current_value = new_value;
                    if (sb->on_scroll) {
                        sb->on_scroll(sb->userdata, sb->current_value);
                    }
                }
            }
            
            return true;
        }
        
        return was_hovered != sb->thumb_hovered;
    }
    else if (event->type == QARMA_INPUT_EVENT_MOUSE_DOWN) {
        int mx = event->data.mouse.x;
        int my = event->data.mouse.y;
        
        // Check if clicking on thumb
        if (mx >= thumb_x && mx < thumb_x + thumb_w &&
            my >= thumb_y && my < thumb_y + thumb_h) {
            sb->thumb_pressed = true;
            sb->thumb_focused = true;
            
            // Store offset from thumb start for smooth dragging
            if (sb->orientation == SCROLLBAR_HORIZONTAL) {
                sb->thumb_drag_offset = mx - thumb_x;
            } else {
                sb->thumb_drag_offset = my - thumb_y;
            }
            
            return true;
        }
        // Check if clicking on track (jump to position)
        else if (control_point_in_bounds(&sb->base, mx, my)) {
            int click_pos;
            if (sb->orientation == SCROLLBAR_HORIZONTAL) {
                click_pos = mx - sb->base.x - 2 - sb->thumb_size / 2;
            } else {
                click_pos = my - sb->base.y - 2 - sb->thumb_size / 2;
            }
            
            // Clamp and set new position
            int max_pos = sb->track_size - sb->thumb_size;
            if (click_pos < 0) click_pos = 0;
            if (click_pos > max_pos) click_pos = max_pos;
            
            sb->thumb_pos = click_pos;
            
            // Calculate value
            int range = sb->max_value - sb->min_value;
            if (max_pos > 0) {
                float pos_ratio = (float)sb->thumb_pos / (float)max_pos;
                sb->current_value = sb->min_value + (int)(range * pos_ratio);
                
                if (sb->on_scroll) {
                    sb->on_scroll(sb->userdata, sb->current_value);
                }
            }
            
            return true;
        }
    }
    else if (event->type == QARMA_INPUT_EVENT_MOUSE_UP) {
        if (sb->thumb_pressed) {
            sb->thumb_pressed = false;
            return true;
        }
    }
    else if (event->type == QARMA_INPUT_EVENT_KEY_DOWN && sb->thumb_focused) {
        // Arrow key scrolling
        int scancode = event->data.key.scancode;
        int delta = 0;
        
        if (sb->orientation == SCROLLBAR_HORIZONTAL) {
            if (scancode == 0x4B) delta = -1;  // Left arrow
            if (scancode == 0x4D) delta = 1;   // Right arrow
        } else {
            if (scancode == 0x48) delta = -1;  // Up arrow
            if (scancode == 0x50) delta = 1;   // Down arrow
        }
        
        if (delta != 0) {
            scrollbar_scroll_by(sb, delta);
            return true;
        }
    }
    
    return false;
}

void scrollbar_set_range(Scrollbar* sb, int min_val, int max_val, int page_size) {
    if (!sb) return;
    
    sb->min_value = min_val;
    sb->max_value = max_val;
    sb->page_size = page_size;
    
    // Clamp current value to new range
    if (sb->current_value < min_val) sb->current_value = min_val;
    if (sb->current_value > max_val) sb->current_value = max_val;
    
    scrollbar_calculate_thumb(sb);
}

void scrollbar_set_value(Scrollbar* sb, int value) {
    if (!sb) return;
    
    // Clamp to range
    if (value < sb->min_value) value = sb->min_value;
    if (value > sb->max_value) value = sb->max_value;
    
    if (value != sb->current_value) {
        sb->current_value = value;
        scrollbar_calculate_thumb(sb);
        
        if (sb->on_scroll) {
            sb->on_scroll(sb->userdata, sb->current_value);
        }
    }
}

int scrollbar_get_value(Scrollbar* sb) {
    return sb ? sb->current_value : 0;
}

void scrollbar_scroll_by(Scrollbar* sb, int delta) {
    if (!sb) return;
    scrollbar_set_value(sb, sb->current_value + delta);
}
