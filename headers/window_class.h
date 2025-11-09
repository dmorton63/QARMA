#pragma once

#include "subsystem.h"
#include "macros.h"
#include <stdbool.h>
#include "log.h"

//typedef struct Subsystem Subsystem;

#define MAX_WINDOWS 10

typedef struct WindowEventCallback {
	const char* event_type;
	void (*callback)(const char* event);
	struct WindowEventCallback* next;
} WindowEventCallback;

typedef struct {
	float brightness, contrast, saturation, hue, gamma;
	int refresh_rate;
	float aspect_ratio;
} DisplaySettings;

typedef struct {
	int x;
	int y;
} WindowPosition;

typedef struct {

	int width;
	int height;
} WindowGeometry;

typedef struct {
	const char* name;
	int id;	
	const char* title;
} WindowHeader;

typedef struct {
	bool resizable;
	bool borderless;
	bool fullscreen;
	bool minimized;
	bool maximized;
	bool always_on_top;
}WindowStyleFlags;

typedef struct {
    int profile;
    int flags;
    int context_major;
    int context_minor;
    int accelerated;
	int samples;
} GLContextConfig;


typedef struct {
	WindowHeader header;
	WindowGeometry geometry;
	WindowPosition position;
	WindowStyleFlags style;
	const char* cursor_type;
	const char* tooltip;
	const char* icon_path;
	void* user_data;
	int interval;
	bool vsync_enabled;
	DisplaySettings display_settings;
	GLContextConfig context;
	int multisampling;
	int stereo;
	int depth_bits;
	int stencil_bits;
	int debug;
	int shared;
	int multithreaded;
	int major;
	int minor;
	Subsystem_ptr* video;
	Subsystem_ptr* input;
	WindowEventCallback* event_callback;
} QarmaWindow;  // Renamed to avoid X11 Window conflict





typedef struct {
	QarmaWindow* children[MAX_WINDOWS];
	int child_count;
	QarmaWindow* focused;
}ParentWindow;

int create_window(QarmaWindow* window);
int destroy_window(QarmaWindow* window);
int focus_window(QarmaWindow* window);
int add_child_window(ParentWindow* parent, QarmaWindow* child);
int remove_child_window(ParentWindow* parent, QarmaWindow* child);
int render_window(QarmaWindow* window);
int render_parent_window(ParentWindow* parent);
int initialize_subsystem(Subsystem_ptr* subsystem);
int shutdown_subsystem(Subsystem_ptr* subsystem);
int set_window_title(QarmaWindow* window, const char* title);
int move_window(QarmaWindow* window, int x, int y);
int resize_window(QarmaWindow* window, int width, int height);
int list_child_windows(ParentWindow* parent, QarmaWindow** out_children, int max_children);
int is_window_focused(ParentWindow* parent, QarmaWindow* window);
int bring_window_to_front(ParentWindow* parent, QarmaWindow* window);
int send_input_to_focused_window(ParentWindow* parent, const char* input_event);
int get_window_geometry(QarmaWindow* window, int* x, int* y, int* width, int* height);
int set_window_geometry(QarmaWindow* window, int x, int y, int width, int height);
int toggle_window_fullscreen(QarmaWindow* window);
int minimize_window(QarmaWindow* window);
int maximize_window(QarmaWindow* window);
int restore_window(QarmaWindow* window);
bool is_window_minimized(QarmaWindow* window);
bool is_window_maximized(QarmaWindow* window);
bool is_window_fullscreen(QarmaWindow* window);
int set_window_opacity(QarmaWindow* window, float opacity);
int get_window_opacity(QarmaWindow* window, float* opacity);
int set_window_background_color(QarmaWindow* window, int r, int g, int b);
int get_window_background_color(QarmaWindow* window, int* r, int* g, int* b);
int capture_window_screenshot(QarmaWindow* window, const char* filepath);
int set_window_always_on_top(QarmaWindow* window, int always_on_top);
bool is_window_always_on_top(QarmaWindow* window);
int set_window_resizable(QarmaWindow* window, bool resizable);
bool is_window_resizable(QarmaWindow* window);
int set_window_borderless(QarmaWindow* window, bool borderless);
bool is_window_borderless(QarmaWindow* window);
int set_window_icon(QarmaWindow* window, const char* icon_path);
int get_window_icon(QarmaWindow* window, char* out_icon_path, int max_length);
int set_window_cursor(QarmaWindow* window, const char* cursor_type);
int get_window_cursor(QarmaWindow* window, char* out_cursor_type, int max_length);
int set_window_tooltip(QarmaWindow* window, const char* tooltip);
int get_window_tooltip(QarmaWindow* window, char* out_tooltip, int max_length);
int enable_window_vsync(QarmaWindow* window, int enable);
int is_window_vsync_enabled(QarmaWindow* window, int* enabled);
int set_window_brightness(QarmaWindow* window, float brightness);
int get_window_brightness(QarmaWindow* window, float* brightness);
int set_window_contrast(QarmaWindow* window, float contrast);
int get_window_contrast(QarmaWindow* window, float* contrast);
int set_window_saturation(QarmaWindow* window, float saturation);
int get_window_saturation(QarmaWindow* window, float* saturation);
int set_window_hue(QarmaWindow* window, float hue);
int get_window_hue(QarmaWindow* window, float* hue);
int set_window_gamma(QarmaWindow* window, float gamma);
int get_window_gamma(QarmaWindow* window, float* gamma);
int set_window_refresh_rate(QarmaWindow* window, int refresh_rate);
int get_window_refresh_rate(QarmaWindow* window, int* refresh_rate);
int set_window_aspect_ratio(QarmaWindow* window, float aspect_ratio);
int get_window_aspect_ratio(QarmaWindow* window, float* aspect_ratio);
int set_window_multisampling(QarmaWindow* window, int samples);
int get_window_multisampling(QarmaWindow* window, int* samples);
int set_window_stereo(QarmaWindow* window, int stereo);
int is_window_stereo(QarmaWindow* window, int* stereo);
int set_window_depth_buffer(QarmaWindow* window, int depth_bits);
int get_window_depth_buffer(QarmaWindow* window, int* depth_bits);
int set_window_stencil_buffer(QarmaWindow* window, int stencil_bits);
int get_window_stencil_buffer(QarmaWindow* window, int* stencil_bits);
int set_window_accelerated_context(QarmaWindow* window, int accelerated);
int is_window_accelerated_context(QarmaWindow* window, int* accelerated);
int set_window_debug_context(QarmaWindow* window, int debug);
int is_window_debug_context(QarmaWindow* window, int* debug);
int set_window_shared_context(QarmaWindow* window, int shared);
int is_window_shared_context(QarmaWindow* window, int* shared);
int set_window_multithreaded_context(QarmaWindow* window, int multithreaded);
int is_window_multithreaded_context(QarmaWindow* window, int* multithreaded);
int set_window_context_version(QarmaWindow* window, int major, int minor);
int get_window_context_version(QarmaWindow* window, int* major, int* minor);
int set_window_context_profile(QarmaWindow* window, int profile);
int get_window_context_profile(QarmaWindow* window, int* profile);
int set_window_context_flags(QarmaWindow* window, int flags);
int get_window_context_flags(QarmaWindow* window, int* flags);
int set_window_swap_interval(QarmaWindow* window, int interval);
int get_window_swap_interval(QarmaWindow* window, int* interval);
int poll_window_events(QarmaWindow* window);
int wait_for_window_event(QarmaWindow* window);
int clear_window_events(QarmaWindow* window);
int flush_window_events(QarmaWindow* window);
int register_window_event_callback(QarmaWindow* window, void (*callback)(const char* event));
int unregister_window_event_callback(QarmaWindow* window, void (*callback)(const char* event));
int set_window_user_data(QarmaWindow* window, void* user_data);
int get_window_user_data(QarmaWindow* window, void** user_data);

int set_always_on_top(QarmaWindow *window, bool always_on_top);


