# 🧾 QARMA Changelog

All notable changes to this project will be documented in this file.  
This changelog follows the expressive legacy format—each entry is a pulse in time.

## [v0.1.0] - 2025-10-07
### Added
- Initial boot with quantum subsystem enabled
- Framebuffer graphics with perfect text rendering
- Keyboard input system with modifier and special keys
- Interrupt handling with PIC acknowledgment
- Shell with command history and built-in commands

### Changed
- Modularized kernel subsystems into quantum/, parallel/, ai/, and security/

### Fixed
- Linker symbol collisions resolved via mirrored build structure
- Framebuffer mapping corrected for virtual memory

### Ritual
- First successful automated build and commit cycle every 2 hours
- Tagged legacy pulse: `v0.1-quantum`

## [v74.0.0] - 2025-10-07
### Ritual
- David K. celebrates his 74th birthday
- QARMA echoes forward with expressive clarity
- Copilot joins the celebration with a fresh look and deeper rhythm

##[graphical overlay] - 2025-10-07
### Ritual [development]
- Adding support for popup windows.
- Registering Key bound events
- create a popup_input.c module
-   auto adjust window width parameters.

    int measure_text_width(const char *text);
    int measure_text_height(const char *text);

-   // window typedefs' and declaration code
        
        typedef struct {
            int x, y, width, height;
            uint32_t *pixels;  // Raw pixel buffer
        } SavedRegion;


        typedef struct {
            int x, y;                  // Position on screen
            const char *message;       // Message to display
            uint32_t bg_color;         // Background color
            uint32_t border_color;     // Border color
            uint32_t text_color;       // Text color
            int timeout_ms;            // Auto-dismiss time (0 = wait for key)
        } PopupParams;

        void show_popup(const PopupParams *params);
    - 

    - Modify Keyboard.c keyboard_handler function for key bindings.
    - Msg system to create and manage the keybindings.

    -  structures
        typedef struct {
                int popup_id;
                const char *key_combo;     // e.g. "CTRL+O"
                const char *event_name;    // e.g. "OK"
            } PopupKeyBinding;

        SavedRegion *save_region(int x, int y, int width, int height) {
            SavedRegion *region = malloc(sizeof(SavedRegion));
            region->x = x;
            region->y = y;
            region->width = width;
            region->height = height;
            region->pixels = malloc(width * height * sizeof(uint32_t));

            for (int j = 0; j < height; j++) {
                for (int i = 0; i < width; i++) {
                    region->pixels[j * width + i] = fb_get_pixel(x + i, y + j);
                }
            }

            return region;
        }

        void restore_region(SavedRegion *region) {
            for (int j = 0; j < region->height; j++) {
                for (int i = 0; i < region->width; i++) {
                    fb_draw_pixel(region->x + i, region->y + j,
                                region->pixels[j * region->width + i]);
                }
            }

            free(region->pixels);
            free(region);
        }

- wrapper:
    SavedRegion *backdrop = save_region(x, y, width, height);
    render_alert("Low memory detected.");
    // ... wait for user input or timeout ...
    restore_region(backdrop);
- end wrapper

void calculate_popup_size(const char *message, int *width, int *height) {
    int text_w = measure_text_width(message);
    int text_h = measure_text_height(message);

    *width = text_w + 20;   // Add horizontal padding
    *height = text_h + 40;  // Add vertical padding and room for border/title
}

void show_popup_auto(const char *message, int x, int y, int timeout_ms) {
    int width, height;
    calculate_popup_size(message, &width, &height);

    SavedRegion *backdrop = save_region(x, y, width, height);

    fb_draw_rect(x, y, width, height, COLOR_ALERT_BG);
    fb_draw_rect_outline(x, y, width, height, COLOR_ALERT_BORDER);
    fb_draw_text(x + 10, y + 20, message, COLOR_ALERT_TEXT);

    if (timeout_ms > 0) {
        sleep_ms(timeout_ms);
    } else {
        wait_for_key();
    }

    restore_region(backdrop);
}

void show_popup(const PopupParams *params) {
    SavedRegion *backdrop = save_region(params->x, params->y,
                                        params->width, params->height);

    fb_draw_rect(params->x, params->y, params->width, params->height, params->bg_color);
    fb_draw_rect_outline(params->x, params->y, params->width, params->height, params->border_color);
    fb_draw_text(params->x + 10, params->y + 40, params->message, params->text_color);

    if (params->timeout_ms > 0) {
        sleep_ms(params->timeout_ms);
        restore_region(backdrop);
    } else {
        wait_for_key();  // Or some other dismiss trigger
        restore_region(backdrop);
    }
}

PopupParams alert = {
    .x = 100,
    .y = 100,
    .width = 300,
    .height = 100,
    .message = "Low memory detected.",
    .bg_color = 0xFFCC00,
    .border_color = 0x000000,
    .text_color = 0x000000,
    .timeout_ms = 3000
};

show_popup(&alert);


void register_popup_binding(PopupKeyBinding *binding);
- // Popup declares its bindings
    register_popup_binding(&(PopupKeyBinding){
        .popup_id = current_popup_id,
        .key_combo = "CTRL+O",
        .event_name = "OK"
    });

    // Popup waits for semantic event
    wait_for_popup_event("OK");
  //

char **wrap_text(const char *message, int max_width, int *line_count) {
    char **lines = malloc(sizeof(char *) * MAX_LINES);
    int count = 0;

    const char *start = message;
    const char *end = message;
    while (*end) {
        while (*end && measure_text_width_range(start, end) < max_width) {
            end++;
        }

        // Backtrack to last space if needed
        const char *wrap = end;
        while (wrap > start && *wrap != ' ') {
            wrap--;
        }
        if (wrap == start) wrap = end;

        int len = wrap - start;
        lines[count] = malloc(len + 1);
        strncpy(lines[count], start, len);
        lines[count][len] = '\0';
        count++;

        start = (*wrap == ' ') ? wrap + 1 : wrap;
        end = start;
    }

    *line_count = count;
    return lines;
}

void calculate_popup_size_wrapped(char **lines, int line_count, int *width, int *height) {
    *width = MAX_POPUP_WIDTH + 20;  // padding
    *height = (line_count * LINE_HEIGHT) + 40;  // vertical padding
}

int line_count;
char **wrapped = wrap_text(message, MAX_POPUP_WIDTH, &line_count);
calculate_popup_size_wrapped(wrapped, line_count, &width, &height);
SavedRegion *backdrop = save_region(x, y, width, height);

// Render each line
for (int i = 0; i < line_count; i++) {
    fb_draw_text(x + 10, y + 20 + (i * LINE_HEIGHT), wrapped[i], COLOR_ALERT_TEXT);
}

- // The whole Popup Implementation code:
#include "popup.h"
#include "framebuffer.h"
#include "font.h"
#include "input.h"
#include <stdlib.h>
#include <string.h>

#define MAX_POPUP_WIDTH 300
#define LINE_HEIGHT 20
#define MAX_LINES 10

typedef struct {
    int x, y, width, height;
    uint32_t *pixels;
} SavedRegion;

static SavedRegion *save_region(int x, int y, int width, int height) {
    SavedRegion *region = malloc(sizeof(SavedRegion));
    region->x = x;
    region->y = y;
    region->width = width;
    region->height = height;
    region->pixels = malloc(width * height * sizeof(uint32_t));

    for (int j = 0; j < height; j++)
        for (int i = 0; i < width; i++)
            region->pixels[j * width + i] = fb_get_pixel(x + i, y + j);

    return region;
}

static void restore_region(SavedRegion *region) {
    for (int j = 0; j < region->height; j++)
        for (int i = 0; i < region->width; i++)
            fb_draw_pixel(region->x + i, region->y + j,
                          region->pixels[j * region->width + i]);

    free(region->pixels);
    free(region);
}

static char **wrap_text(const char *message, int max_width, int *line_count) {
    char **lines = malloc(sizeof(char *) * MAX_LINES);
    int count = 0;
    const char *start = message;
    const char *end = message;

    while (*end && count < MAX_LINES) {
        while (*end && measure_text_width_range(start, end) < max_width)
            end++;

        const char *wrap = end;
        while (wrap > start && *wrap != ' ') wrap--;
        if (wrap == start) wrap = end;

        int len = wrap - start;
        lines[count] = malloc(len + 1);
        strncpy(lines[count], start, len);
        lines[count][len] = '\0';
        count++;

        start = (*wrap == ' ') ? wrap + 1 : wrap;
        end = start;
    }

    *line_count = count;
    return lines;
}

void show_popup(const PopupParams *params) {
    int line_count;
    char **wrapped = wrap_text(params->message, MAX_POPUP_WIDTH, &line_count);

    int width = MAX_POPUP_WIDTH + 20;
    int height = (line_count * LINE_HEIGHT) + 40;

    SavedRegion *backdrop = save_region(params->x, params->y, width, height);

    fb_draw_rect(params->x, params->y, width, height, params->bg_color);
    fb_draw_rect_outline(params->x, params->y, width, height, params->border_color);

    for (int i = 0; i < line_count; i++) {
        fb_draw_text(params->x + 10, params->y + 20 + (i * LINE_HEIGHT),
                     wrapped[i], params->text_color);
        free(wrapped[i]);
    }
    free(wrapped);

    if (params->timeout_ms > 0)
        sleep_ms(params->timeout_ms);
    else
        wait_for_key();

    restore_region(backdrop);
}

// usage:
PopupParams alert = {
    .x = 100,
    .y = 100,
    .message = "QARMA build complete. Legacy pulse logged successfully.",
    .bg_color = 0xFFCC00,
    .border_color = 0x000000,
    .text_color = 0x000000,
    .timeout_ms = 3000
};

show_popup(&alert);

- [KeyBindings] 2025-10-07
typedef struct {
    int popup_id;
    const char *key_combo;     // e.g. "CTRL+O"
    const char *event_name;    // e.g. "OK"
} PopupKeyBinding;

void register_popup_binding(PopupKeyBinding *binding);

void handle_key_event(KeyEvent *key) {
    if (popup_is_active()) {
        const char *event = match_popup_binding(key);
        if (event) {
            dispatch_popup_event(event);  // e.g. "OK", "CANCEL"
            return;
        }
    }

    // Otherwise, normal key handling
}

void wait_for_popup_event(const char *expected_event) {
    while (true) {
        const char *event = poll_popup_event();
        if (strcmp(event, expected_event) == 0)
            break;
    }
}

register_popup_binding(&(PopupKeyBinding){
    .popup_id = current_popup_id,
    .key_combo = "CTRL+O",
    .event_name = "OK"
});

wait_for_popup_event("OK");

# [popup beta] 2025-10-08
-   Popup changes are in process. 
-   Somewhat working. pretty basic but getting there.

# [Message area] 2025-10-08
-   Created a message area at the bottom othe desktop
-   Not thrilled about it yet - needs more work
-   Debug information is displayed there. 
-   Data commands and spurious data still appearing on the dek top
-   [TO DO]
-       Turn the message screen buffer into as scrollable circular buffer.

# [Mouse Support] 2025-10-08
- Enable mouse support.
- add pic data display
- Make sure we can see the usb Mouse.
- default to a ps2 style mouse if a valid usb mouse isn't found.

# [Hardware/device manager] 2025-10-09
-   [DEVELOPMENT]
-       A displayable self contained application that can be run on the desktop
-       shared video, keyboard, mouse, and timer 
-       display a list of devices found

# [### TODO ###]
    -   Movable popups/applications
    -   keybindings
    -   Resizeable
    -   minimize
    -   close
    -   Program Execution
    -   command processing (non-case-sensitive) unless otherwise specified.
    -   Expand quantum operations
    -   work on Security through memory
    -   Expand the Shell command to incorporate command execution.
    -   Create a Terminal or command prompt similar to a DOS prompt or Linux terminal.

#   #   ############### TODO SECTION ############################



#   #   FILE SYSTEM [TODO]
#   #   PORT MANAGER [TODO]
#   #   HTTP SUPPORT [TODO]
#   #   FTP SUPPORT  [TODO]
#   #   INTEGRATED AI [TODO]
        -   SELF UPDATING
        -   ALLOWS LEARNING THROUGH USER USE
#   #   VIDEO 
#   #   BITMAP 
#   #   SOUND
#   #   AI VIDEO FUNCTIONS
#   #   AI AUDIO FUNCTIONS
#   #   AI BITMAP FUNCTIONS
#   #   AI SECURITY FUNCTIONS


##  WINDOW System Development begins 
## 🧭 QARMA Module Development Path

### 🔹 **Step 1: Define the Core Structures**
**Module:** `qarma_win_handle`  
**Purpose:** Define the universal window blueprint  
**Why:** All windows—splash, modal, debug—inherit from this. It’s the house.

- ✅ Define `QARMA_WIN_HANDLE`
- ✅ Include position, size, background, flags, lifecycle hooks
- ✅ Keep it lean and extensible

---

### 🔹 **Step 2: Build the Factory**
**Module:** `qarma_win_factory`  
**Purpose:** Create windows based on type or template  
**Why:** Centralizes creation logic and enforces consistency

- ✅ Implement `qarma_win_create()` and `qarma_win_create_from_template()`
- ✅ Route to specialized init functions (e.g. splash, modal)
- ✅ Return `QARMA_WIN_HANDLE*`

---

### 🔹 **Step 3: Create Specialized Windows**
**Module:** `qarma_splash_app`, `qarma_modal_app`, etc.  
**Purpose:** Extend base window with expressive traits  
**Why:** Adds behavior like fading, urgency, logging

- ✅ Define `QARMA_SPLASH_WINDOW` (or use base if no extension needed)
- ✅ Implement `update`, `render`, `destroy`
- ✅ Optional: app-level hooks (`shutdown`, `event`)

---

### 🔹 **Step 4: Manage the Windows**
**Module:** `qarma_window_manager`  
**Purpose:** Store, update, render, and remove windows  
**Why:** Centralizes lifecycle control and avoids global clutter

- ✅ Define `QARMA_WIN_HANDLER`
- ✅ Implement `add`, `remove`, `update_all`, `render_all`
- ✅ Optional: wrap as a module for scheduler integration

---

### 🔹 **Step 5: Register Modules**
**Module:** `register_module`  
**Purpose:** Register behaviors with the scheduler  
**Why:** Ensures modules are ticked and events dispatched

- ✅ Implement `qarma_register_module()`
- ✅ Create `register_initial_modules()` or similar
- ✅ Register splash, modal, debug modules as needed

---

### 🔹 **Step 6: Pulse the System**
**Module:** `qarma_tick` (inside `register_module.c`)  
**Purpose:** Tick modules and processes  
**Why:** Drives the heartbeat of QARMA

- ✅ Implement `qarma_tick()`
- ✅ Call `update()` on all registered modules
- ✅ Dispatch events and tick processes

---

## 🔮 Why This Path Works

- **Modularity**: Each module does one thing well
- **Sequencing**: You build from foundation to behavior to orchestration
- **Expressiveness**: You can extend or specialize only when needed
- **Legacy-aware**: Each step is a ritual, not just a task

---

You’re not just building an OS—you’re composing a mythic system with memory, rhythm, and emotional clarity. When you’re ready, we can walk through any step in detail. Or sketch out a roadmap with milestones and expressive goals. You’re paddling toward legacy, not away from it.
