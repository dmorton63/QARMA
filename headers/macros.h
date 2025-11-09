#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include "dispatch_logf.h"

#define WINDOW_USE_TEXT_EDITOR(self) \
    self->input = &text_input_subsystem; \
    self->on_keydown = text_editor_keydown; \
    self->on_paint = text_editor_paint; \
    self->user_data = create_text_buffer(); \
    register_glyph(self, "KEY_OK", text_editor_confirm); \
    register_glyph(self, "KEY_CANCEL", text_editor_cancel);

#define WINDOW_USE_VIDEO_RENDERING(self) \
    self->video = &video_subsystem; \
    self->on_video_frame = video_render_frame;

#define WINDOW_USE_DIALOG_HANDLERS(self, handlers) \
    self->on_focus = handlers->on_focus; \
    self->on_blur = handlers->on_blur; \
    register_glyph(self, "KEY_SAVE", handlers->on_save);

#define REGISTER_CORE_SUBSYSTEM(subsystem_ptr, core_id)         \
    do {                                                        \
        (subsystem_ptr)->core_affinity = (core_id);             \
        register_with_message_manager(subsystem_ptr);           \
        register_with_cpu_core_manager(subsystem_ptr);          \
        dispatch_logf("Registered core subsystem '%s' on core %d", \
                      (subsystem_ptr)->name, (core_id));        \
    } while (0)

#define REGISTER_MESSAGE_ONLY_SUBSYSTEM(subsystem_ptr)          \
    do {                                                        \
        (subsystem_ptr)->core_affinity = -1;                    \
        register_with_message_manager(subsystem_ptr);           \
        dispatch_logf("Registered message-only subsystem '%s'", \
                      (subsystem_ptr)->name);                   \
    } while (0)

#define DISPATCH_TO_CORE(subsystem_ptr)                         \
    do {                                                        \
        if ((subsystem_ptr)->core_affinity >= 0) {              \
            dispatch_to_core((subsystem_ptr)->core_affinity);    \
            dispatch_logf("Dispatched subsystem '%s' to core %d", \
                          (subsystem_ptr)->name,                \
                          (subsystem_ptr)->core_affinity);      \
        } else {                                                \
            dispatch_logf("Subsystem '%s' has no core affinity", \
                          (subsystem_ptr)->name);               \
        }                                                       \
    } while (0)

#define LOG_SUBSYSTEM_EVENT(subsystem_ptr, event_msg)          \
    do {                                                        \
        dispatch_logf("Subsystem '%s': %s",                     \
                      (subsystem_ptr)->name, (event_msg));      \
    } while (0)

#define INCREMENT_SUBSYSTEM_METRIC(subsystem_ptr, value)       \
    do {                                                        \
        dispatch_metrics_increment((subsystem_ptr)->name, (value)); \
    } while (0)

#ifndef PUSH_SUBSYSTEM_HISTORY
#define PUSH_SUBSYSTEM_HISTORY(subsystem_ptr, data)            \
    do {                                                        \
        subsystem_history_push((subsystem_ptr)->name, (data));   \
    } while (0)
#endif

#define SUBSYSTEM_HEARTBEAT(subsystem_ptr) \
    LOG_SUBSYSTEM_EVENT(subsystem_ptr, "Heartbeat received"); \
    INCREMENT_SUBSYSTEM_METRIC(subsystem_ptr, 1);

#ifndef SUBSYSTEM_INIT
#define SUBSYSTEM_INIT(subsystem_ptr) \
    do { \
        REGISTER_CORE_SUBSYSTEM(subsystem_ptr, subsystem_ptr->core_affinity); \
        LOG_SUBSYSTEM_EVENT(subsystem_ptr, "Initialized"); \
    } while (0)
#endif

#define SUBSYSTEM_INIT_AND_REGISTER(subsystem_ptr, name_str, core_id) \
    do {                                                               \
        SUBSYSTEM_INIT(subsystem_ptr, name_str);                       \
        REGISTER_CORE_SUBSYSTEM(subsystem_ptr, core_id);               \
    } while (0)

#define SUBSYTEM_SHUTDOWN(subsystem_ptr) \
    do { \
        LOG_SUBSYSTEM_EVENT(subsystem_ptr, "Shutting down"); \
        unregister_from_message_manager(subsystem_ptr); \
        unregister_from_cpu_core_manager(subsystem_ptr); \
    } while (0)

    #define SUBSYSTEM_INIT_MESSAGE_ONLY(subsystem_ptr, name_str) \
    do {                                                      \
        SUBSYSTEM_INIT(subsystem_ptr, name_str);              \
        REGISTER_MESSAGE_ONLY_SUBSYSTEM(subsystem_ptr);       \
    } while (0)


#define WINDOW_INIT(win_ptr, title_str, width_val, height_val)             \
    do {                                                                   \
        (win_ptr)->header.title = (title_str);                             \
        (win_ptr)->geometry.width = (width_val);                           \
        (win_ptr)->geometry.height = (height_val);                         \
        (win_ptr)->position.x = 100;                                       \
        (win_ptr)->position.y = 100;                                       \
        (win_ptr)->style.resizable = true;                                 \
        (win_ptr)->style.borderless = false;                               \
        (win_ptr)->style.fullscreen = false;                               \
        (win_ptr)->style.always_on_top = false;                            \
        (win_ptr)->style.minimized = false;                                \
        (win_ptr)->style.maximized = false;                                \
        (win_ptr)->display_settings.brightness = 1.0f;                     \
        (win_ptr)->display_settings.contrast = 1.0f;                       \
        (win_ptr)->display_settings.saturation = 1.0f;                     \
        (win_ptr)->display_settings.hue = 0.0f;                            \
        (win_ptr)->display_settings.gamma = 1.0f;                          \
        (win_ptr)->display_settings.refresh_rate = 60;                     \
        (win_ptr)->display_settings.aspect_ratio =                        \
            (float)(width_val) / (float)(height_val);                      \
        (win_ptr)->context.profile = 0;                                    \
        (win_ptr)->context.flags = 0;                                      \
        (win_ptr)->context.context_major = 3;                              \
        (win_ptr)->context.context_minor = 3;                              \
        (win_ptr)->context.samples = 4;                                    \
        (win_ptr)->context.accelerated = 1;                                \
        (win_ptr)->vsync_enabled = true;                                   \
        (win_ptr)->event_callback = NULL;                                  \
        (win_ptr)->user_data = NULL;                                       \
        dispatch_logf("Initialized window '%s' [%dx%d]",                   \
                      (title_str), (width_val), (height_val));             \
    } while (0)
