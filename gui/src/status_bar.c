/*
 * QARMA - Status Bar Implementation (New Architecture)
 */

#include "status_bar.h"
#include "renderer.h"
#include "memory/heap.h"
#include "string.h"
#include "kernel.h"
#include "handle_manager.h"
#include "config.h"

#define STATUS_BAR_BG_COLOR    0xFF2D2D30
#define STATUS_BAR_BORDER_COLOR 0xFF3E3E42
#define STATUS_BAR_ITEM_SPACING 4

// Message handler for status bar frame
static int32_t status_bar_message_handler(qarma_handle_t recipient, qarma_message_t* msg) {
    if (!msg) return 0;
    
    qarma_frame_t* frame = (qarma_frame_t*)handle_get_object(recipient);
    if (!frame) return 0;
    
    status_bar_t* bar = (status_bar_t*)frame->user_data;
    if (!bar) return 0;
    
    switch (msg->type) {
        case MSG_PAINT:
            status_bar_render(bar);
            return 1;
            
        case MSG_LBUTTONDOWN: {
            // Check which item was clicked
            int x = (int)(msg->wparam & 0xFFFF);
            int y = (int)(msg->wparam >> 16);
            
            for (int i = 0; i < bar->item_count; i++) {
                if (!bar->items[i].visible) continue;
                
                status_bar_item_t* item = &bar->items[i];
                if (item->type == STATUS_ITEM_BUTTON) {
                    // Check if click is within button bounds
                    if (x >= item->x_offset && x < item->x_offset + item->width &&
                        y >= 0 && y < STATUS_BAR_HEIGHT) {
                        // Trigger button click
                        if (item->on_click) {
                            item->on_click(item->user_data);
                        }
                        return 1;
                    }
                }
            }
            return 0;
        }
        
        default:
            return 0;
    }
}

status_bar_t* status_bar_create(int x, int y, int width, int height) {
    SERIAL_LOG("[STATUS_BAR_NEW] Creating status bar\n");
    
    status_bar_t* bar = (status_bar_t*)heap_alloc(sizeof(status_bar_t));
    if (!bar) {
        SERIAL_LOG("[STATUS_BAR_NEW] ERROR: Failed to allocate status_bar_t\n");
        return NULL;
    }
    
    memset(bar, 0, sizeof(status_bar_t));
    bar->item_count = 0;
    bar->focused_item = -1;
    bar->bg_color = STATUS_BAR_BG_COLOR;
    bar->border_color = STATUS_BAR_BORDER_COLOR;
    
    // Create main frame
    bar->main_frame = frame_create(
        NULL,  // No parent
        x, y, width, height,
        FRAME_STYLE_BORDER,
        "Status Bar"
    );
    
    if (!bar->main_frame) {
        SERIAL_LOG("[STATUS_BAR_NEW] ERROR: Failed to create frame\n");
        heap_free(bar);
        return NULL;
    }
    
    // Set frame colors
    bar->main_frame->background.red = (STATUS_BAR_BG_COLOR >> 16) & 0xFF;
    bar->main_frame->background.green = (STATUS_BAR_BG_COLOR >> 8) & 0xFF;
    bar->main_frame->background.blue = STATUS_BAR_BG_COLOR & 0xFF;
    bar->main_frame->background.alpha = 255;
    
    bar->main_frame->border_color.red = (STATUS_BAR_BORDER_COLOR >> 16) & 0xFF;
    bar->main_frame->border_color.green = (STATUS_BAR_BORDER_COLOR >> 8) & 0xFF;
    bar->main_frame->border_color.blue = STATUS_BAR_BORDER_COLOR & 0xFF;
    bar->main_frame->border_color.alpha = 255;
    
    // Set message handler
    bar->main_frame->message_handler = status_bar_message_handler;
    bar->main_frame->user_data = bar;
    
    SERIAL_LOG("[STATUS_BAR_NEW] Status bar created successfully\n");
    return bar;
}

void status_bar_destroy(status_bar_t* bar) {
    if (!bar) return;
    
    SERIAL_LOG("[STATUS_BAR_NEW] Destroying status bar\n");
    
    // Controls are owned by frame and will be destroyed with it
    if (bar->main_frame) {
        frame_destroy(bar->main_frame);
    }
    
    heap_free(bar);
}

void status_bar_layout(status_bar_t* bar) {
    if (!bar || !bar->main_frame) return;
    
    int left_x = STATUS_BAR_ITEM_SPACING;
    int right_x = bar->main_frame->bounds.width - STATUS_BAR_ITEM_SPACING;
    int center_total_width = 0;
    
    // Calculate total width of center items
    for (int i = 0; i < bar->item_count; i++) {
        if (bar->items[i].alignment == STATUS_ALIGN_CENTER && bar->items[i].visible) {
            center_total_width += bar->items[i].width + STATUS_BAR_ITEM_SPACING;
        }
    }
    
    int center_x = (bar->main_frame->bounds.width - center_total_width) / 2;
    
    // Layout items
    for (int i = 0; i < bar->item_count; i++) {
        if (!bar->items[i].visible) continue;
        
        status_bar_item_t* item = &bar->items[i];
        
        switch (item->alignment) {
            case STATUS_ALIGN_LEFT:
                item->x_offset = left_x;
                left_x += item->width + STATUS_BAR_ITEM_SPACING;
                break;
                
            case STATUS_ALIGN_CENTER:
                item->x_offset = center_x;
                center_x += item->width + STATUS_BAR_ITEM_SPACING;
                break;
                
            case STATUS_ALIGN_RIGHT:
                right_x -= item->width;
                item->x_offset = right_x;
                right_x -= STATUS_BAR_ITEM_SPACING;
                break;
        }
        
        // Update control position if it exists
        if (item->control) {
            item->control->x = item->x_offset;
            item->control->y = 2;  // Small padding from top
        }
    }
}

int status_bar_add_button(status_bar_t* bar, const char* text, status_alignment_t align,
                          void (*on_click)(void* user_data), void* user_data) {
    if (!bar || !text || bar->item_count >= STATUS_BAR_MAX_ITEMS) return -1;
    
    int index = bar->item_count++;
    status_bar_item_t* item = &bar->items[index];
    
    item->type = STATUS_ITEM_BUTTON;
    item->alignment = align;
    item->visible = true;
    item->width = strlen(text) * 8 + 20;
    item->on_click = on_click;
    item->user_data = user_data;
    
    // Create button control
    item->control = button_create(
        bar->main_frame,
        text,  // name
        0, 2,  // Position will be set during layout
        item->width,
        STATUS_BAR_HEIGHT - 4,
        text   // button text
    );
    
    if (item->control) {
        // Set button click handler via control's implementation data
        button_data_t* btn_data = (button_data_t*)item->control->implementation_data;
        if (btn_data) {
            btn_data->on_click = on_click;
            // Note: button_data_t doesn't have user_data field, would need to add it
        }
        frame_add_control(bar->main_frame, item->control);
    }
    
    status_bar_layout(bar);
    SERIAL_LOG("[STATUS_BAR_NEW] Added button: ");
    SERIAL_LOG(text);
    SERIAL_LOG("\n");
    
    return index;
}

int status_bar_add_label(status_bar_t* bar, const char* text, status_alignment_t align) {
    if (!bar || !text || bar->item_count >= STATUS_BAR_MAX_ITEMS) return -1;
    
    int index = bar->item_count++;
    status_bar_item_t* item = &bar->items[index];
    
    item->type = STATUS_ITEM_LABEL;
    item->alignment = align;
    item->visible = true;
    item->width = strlen(text) * 8 + 10;
    item->on_click = NULL;
    item->user_data = NULL;
    
    // Create label control
    item->control = label_create(
        bar->main_frame,
        text,  // name
        0, 2,  // Position will be set during layout
        item->width,
        STATUS_BAR_HEIGHT - 4,
        text   // label text
    );
    
    if (item->control) {
        frame_add_control(bar->main_frame, item->control);
    }
    
    status_bar_layout(bar);
    SERIAL_LOG("[STATUS_BAR_NEW] Added label: ");
    SERIAL_LOG(text);
    SERIAL_LOG("\n");
    
    return index;
}

int status_bar_add_spacer(status_bar_t* bar, int width, status_alignment_t align) {
    if (!bar || bar->item_count >= STATUS_BAR_MAX_ITEMS) return -1;
    
    int index = bar->item_count++;
    status_bar_item_t* item = &bar->items[index];
    
    item->type = STATUS_ITEM_SPACER;
    item->alignment = align;
    item->visible = true;
    item->width = width;
    item->control = NULL;
    item->on_click = NULL;
    item->user_data = NULL;
    
    status_bar_layout(bar);
    return index;
}

void status_bar_remove_item(status_bar_t* bar, int item_index) {
    if (!bar || item_index < 0 || item_index >= bar->item_count) return;
    
    // Remove control from frame
    if (bar->items[item_index].control) {
        frame_remove_control(bar->main_frame, bar->items[item_index].control);
    }
    
    // Shift items down
    for (int i = item_index; i < bar->item_count - 1; i++) {
        bar->items[i] = bar->items[i + 1];
    }
    bar->item_count--;
    
    status_bar_layout(bar);
}

void status_bar_update_label_text(status_bar_t* bar, int item_index, const char* new_text) {
    if (!bar || !new_text || item_index < 0 || item_index >= bar->item_count) return;
    
    status_bar_item_t* item = &bar->items[item_index];
    if (item->type != STATUS_ITEM_LABEL || !item->control) return;
    
    label_set_text(item->control, new_text);
    item->width = strlen(new_text) * 8 + 10;
    status_bar_layout(bar);
}

void status_bar_update_button_text(status_bar_t* bar, int item_index, const char* new_text) {
    if (!bar || !new_text || item_index < 0 || item_index >= bar->item_count) return;
    
    status_bar_item_t* item = &bar->items[item_index];
    if (item->type != STATUS_ITEM_BUTTON || !item->control) return;
    
    button_set_text(item->control, new_text);
    item->width = strlen(new_text) * 8 + 20;
    status_bar_layout(bar);
}

void status_bar_set_visible(status_bar_t* bar, bool visible) {
    if (!bar || !bar->main_frame) return;
    
    if (visible) {
        bar->main_frame->visible = true;
    } else {
        bar->main_frame->visible = false;
    }
}

void status_bar_handle_event(status_bar_t* bar, qarma_message_t* msg) {
    if (!bar || !msg) return;
    
    // Forward message to frame's message handler
    if (bar->main_frame && bar->main_frame->message_handler) {
        bar->main_frame->message_handler(bar->main_frame->handle, msg);
    }
}

void status_bar_render(status_bar_t* bar) {
    if (!bar || !bar->main_frame || !bar->main_frame->visible) return;
    
    // Render frame (which will render all child controls)
    frame_render(bar->main_frame);
}
