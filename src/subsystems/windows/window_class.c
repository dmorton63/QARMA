#include "window_class.h"
#include <stdio.h>


int create_window(QarmaWindow* window) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Creating window: %s\n", window->header.title);
	#endif
	return 0;
}
int destroy_window(QarmaWindow* window) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Destroying window: %s\n", window->header.title);
	#endif
	return 0;
}
int focus_window(QarmaWindow* window) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Focusing window: %s\n", window->header.title);
	#endif
	return 0;
}
int add_child_window(ParentWindow* parent, QarmaWindow* child) {
	// Implementation code here
	if (parent->child_count < MAX_WINDOWS) {
		parent->children[parent->child_count++] = child;
		#ifdef DEBUG
		LOG("Added child window: %s\n", child->header.title);
		#endif
		return 0;
	}
	return -1; // Max children reached
}
int remove_child_window(ParentWindow* parent, QarmaWindow* child) {
	// Implementation code here
	for (int i = 0; i < parent->child_count; i++) {
		if (parent->children[i] == child) {
			for (int j = i; j < parent->child_count - 1; j++) {
				parent->children[j] = parent->children[j + 1];
			}
			parent->child_count--;
			#ifdef DEBUG
			LOG("Removed child window: %s\n", child->header.title);
			#endif
			return 0;
		}
	}
	return -1; // Child not found
}
int render_window(QarmaWindow* window) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Rendering window: %s\n", window->header.title);
	#endif
	return 0;
}
int render_parent_window(ParentWindow* parent) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Rendering parent window with %d children\n", parent->child_count);
	#endif	
	for (int i = 0; i < parent->child_count; i++) {
		render_window(parent->children[i]);
	}
	return 0;
}
int initialize_subsystem(Subsystem_ptr* subsystem) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Initializing subsystem: %s\n", subsystem->name);
	#endif
	return subsystem->init();
}
int shutdown_subsystem(Subsystem_ptr* subsystem) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Shutting down subsystem: %s\n", subsystem->name);
	#endif
	subsystem->shutdown();
	return 0;
}
int set_window_title(QarmaWindow* window, const char* title) {
	// Implementation code here
	window->header.title = title;
	#ifdef DEBUG
	LOG("Set window title to: %s\n", title);
	#endif
	return 0;
}
int move_window(QarmaWindow* window, int x, int y) {
	// Implementation code here
	window->position.x = x;
	window->position.y = y;
	#ifdef DEBUG
	LOG("Moved window %s to (%d, %d)\n", window->header.title, x, y);
	#endif
	return 0;
}
	
int resize_window(QarmaWindow* window, int width, int height) {
	// Implementation code here
	window->geometry.width = width;
	window->geometry.height = height;
	#ifdef DEBUG
	LOG("Resized window %s to %dx%d\n", window->header.title, width, height);
	#endif
	return 0;
}
int list_child_windows(ParentWindow* parent, QarmaWindow** out_children, int max_children) {
	// Implementation code here
	int count = (parent->child_count < max_children) ? parent->child_count : max_children;
	for (int i = 0; i < count; i++) {
		out_children[i] = parent->children[i];
	}
	return count;
}
int is_window_focused(ParentWindow* parent, QarmaWindow* window) {
	// Implementation code here
	return (parent->focused == window);
}
int bring_window_to_front(ParentWindow* parent, QarmaWindow* window) {
	// Implementation code here
	for (int i = 0; i < parent->child_count; i++) {
		if (parent->children[i] == window) {
			// Move to front
			for (int j = i; j > 0; j--) {
				parent->children[j] = parent->children[j - 1];
			}
			parent->children[0] = window;
			#ifdef DEBUG
			LOG("Brought window %s to front\n", window->header.title);
			#endif
			return 0;
		}
	}
	return -1; // Window not found
}
int send_input_to_focused_window(ParentWindow* parent, const char* input_event) {
	// Implementation code here
	if (parent->focused) {	
		#ifdef DEBUG
		LOG("Sending input event '%s' to focused window: %s\n", input_event, parent->focused->header.title);
		#endif
		return 0;
	}
	return -1; // No focused window
}

int get_window_geometry(QarmaWindow* window, int* x, int* y, int* width, int* height) {
	// Implementation code here
	*x = window->position.x;
	*y = window->position.y;
	*width = window->geometry.width;
	*height = window->geometry.height;
	return 0;
}
int set_window_geometry(QarmaWindow* window, int x, int y, int width, int height) {
	// Implementation code here
	window->position.x = x;
	window->position.y = y;
	window->geometry.width = width;
	window->geometry.height = height;
	#ifdef DEBUG
	LOG("Set window %s geometry to (%d, %d, %dx%d)\n", window->header.title, x, y, width, height);
	#endif
	return 0;
}
int toggle_window_fullscreen(QarmaWindow* window) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Toggling fullscreen for window: %s\n", window->header.title);
	#endif
	return 0;
}
int minimize_window(QarmaWindow* window) {
	// Implementation code here
	window->style.minimized = true;
	#ifdef DEBUG
	LOG("Minimizing window: %s\n", window->header.title);
	#endif
	return 0;
}
int maximize_window(QarmaWindow* window) {
	// Implementation code here
	window->style.maximized = true;
	#ifdef DEBUG
	LOG("Maximizing window: %s\n", window->header.title);
	#endif
	return 0;
}
int restore_window(QarmaWindow* window) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Restoring window: %s\n", window->header.title);
	#endif	
	return 0;
}
bool is_window_minimized(QarmaWindow* window) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Checking if window %s is minimized\n", window->header.title);
	#endif
	return window->style.minimized;
}
bool is_window_maximized(QarmaWindow* window) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Checking if window %s is maximized\n", window->header.title);
	#endif
	return window->style.maximized;
}
bool is_window_fullscreen(QarmaWindow* window) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Checking if window %s is fullscreen\n", window->header.title);
	#endif
	return window->style.fullscreen;
}
int set_window_opacity(QarmaWindow* window, float opacity) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Setting window %s opacity to %f\n", window->header.title, opacity);
	#endif
	return 0;
}
int get_window_opacity(QarmaWindow* window, float* opacity) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Getting window %s opacity\n", window->header.title);
	#endif
	*opacity = 1.0f; // Placeholder
	return 0;
}
int set_window_background_color(QarmaWindow* window, int r, int g, int b) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Setting window %s background color to RGB(%d, %d, %d)\n", window->header.title, r, g, b);
	#endif
	return 0;
}
int get_window_background_color(QarmaWindow* window, int* r, int* g, int* b) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Getting window %s background color\n", window->header.title);
	#endif	
	*r = 255; *g = 255; *b = 255; // Placeholder
	return 0;
}
int capture_window_screenshot(QarmaWindow* window, const char* filepath) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Capturing screenshot of window %s to %s\n", window->header.title, filepath);
	#endif
	return 0;
}
int set_window_always_on_top(QarmaWindow* window, int always_on_top) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Setting window %s always on top to %d\n", window->header.title, always_on_top);
	#endif
	return 0;
}
bool is_window_always_on_top(QarmaWindow* window) {
	// Implementation code here
	#ifdef DEBUG
	LOG("Checking if window %s is always on top\n", window->header.title);
	#endif	
	window->style.always_on_top = 0; // Placeholder
	return window->style.always_on_top;
}

int set_window_resizable(QarmaWindow* window, bool resizable)
{
    window->style.resizable = resizable;
    #ifdef DEBUG
    LOG("Setting window %s resizable to %d\n", window->header.title, resizable);
    #endif
	return 0;
}

bool is_window_resizable(QarmaWindow* window)
{		
    if(!window) {
		return false;
	}
    #ifdef DEBUG
    LOG("Checking if window %s is resizable: %d\n", window->header.title, window->style.resizable);
    #endif	
	return window->style.resizable;
}

int set_window_borderless(QarmaWindow* window, bool borderless)	
{
	window->style.borderless = borderless;
    #ifdef DEBUG
    LOG("Setting window %s borderless to %d\n", window->header.title, borderless);
    #endif	

	return 0;
}

bool is_window_borderless(QarmaWindow* window)
{
    if(!window) {
		return false;
	}
    #ifdef DEBUG
    LOG("Checking if window %s is borderless: %d\n", window->header.title, window->style.borderless);
    #endif
	return window->style.borderless;
}

int set_window_icon(QarmaWindow* window, const char* icon_path)
{
    window->icon_path = icon_path;
    #ifdef DEBUG
    LOG("Setting window %s icon to %s\n", window->header.title, icon_path);
    #endif

	return 0;
}

int get_window_icon(QarmaWindow* window, char* out_icon_path, int max_length)
{
    snprintf(out_icon_path, max_length, "%s", window->icon_path);
    #ifdef DEBUG
    LOG("Getting window %s icon path: %s\n", window->header.title, out_icon_path);
    #endif
	return 0;
}

int set_window_cursor(QarmaWindow* window, const char* cursor_type)
{
    window->cursor_type = cursor_type;
    #ifdef DEBUG
    LOG("Setting window %s cursor to %s\n", window->header.title, cursor_type);
    #endif
	return 0;
}

int get_window_cursor(QarmaWindow* window, char* out_cursor_type, int max_length)
{
    snprintf(out_cursor_type, max_length, "%s", window->cursor_type);
    #ifdef DEBUG
    LOG("Getting window %s cursor type: %s\n", window->header.title, out_cursor_type);
    #endif	
	return 0;
}

int set_window_tooltip(QarmaWindow* window, const char* tooltip)
{
	window->tooltip = tooltip;
    #ifdef DEBUG
    LOG("Setting window %s tooltip to %s\n", window->header.title, tooltip);
    #endif
	return 0;
}

int get_window_tooltip(QarmaWindow* window, char* out_tooltip, int max_length)
{
    snprintf(out_tooltip, max_length, "%s", window->tooltip);
    #ifdef DEBUG
    LOG("Getting window %s tooltip: %s\n", window->header.title, out_tooltip);
    #endif
	return 0;
}

int enable_window_vsync(QarmaWindow* window, int enable)
{
    #ifdef DEBUG
    LOG("Enabling window %s vsync: %d\n", window->header.title, enable);
    #endif
	return 0;
}

int is_window_vsync_enabled(QarmaWindow* window, int* enabled)
{
    *enabled = 1; // Placeholder
    #ifdef DEBUG
    LOG("Checking if window %s vsync is enabled: %d\n", window->header.title, *enabled);
    #endif
	return 0;
}

int set_window_brightness(QarmaWindow* window, float brightness)
{
    window->display_settings.brightness = brightness;
    #ifdef DEBUG
    LOG("Setting window %s brightness to %f\n", window->header.title, brightness);
    #endif
	return 0;
}

int get_window_brightness(QarmaWindow* window, float* brightness)
{
    *brightness = window->display_settings.brightness;
    #ifdef DEBUG
    LOG("Getting window %s brightness: %f\n", window->header.title, *brightness);
    #endif
	return 0;
}

int set_window_contrast(QarmaWindow* window, float contrast)
{
    window->display_settings.contrast = contrast;
    #ifdef DEBUG
    LOG("Setting window %s contrast to %f\n", window->header.title, contrast);
    #endif
	return 0;
}

int get_window_contrast(QarmaWindow* window, float* contrast)
{
    *contrast = window->display_settings.contrast;
    #ifdef DEBUG
    LOG("Getting window %s contrast: %f\n", window->header.title, *contrast);
    #endif
	return 0;
}

int set_window_saturation(QarmaWindow* window, float saturation)
{
	window->display_settings.saturation = saturation;
    #ifdef DEBUG
    LOG("Setting window %s saturation to %f\n", window->header.title, saturation);
    #endif
	return 0;
}

int get_window_saturation(QarmaWindow* window, float* saturation)
{
    *saturation = window->display_settings.saturation;
    #ifdef DEBUG
    LOG("Getting window %s saturation: %f\n", window->header.title, *saturation);
    #endif
	return 0;
}

int set_window_hue(QarmaWindow* window, float hue)
{
	window->display_settings.hue = hue;
    #ifdef DEBUG
    LOG("Setting window %s hue to %f\n", window->header.title, hue);
    #endif
	return 0;
}

int get_window_hue(QarmaWindow* window, float* hue)
{
    *hue = window->display_settings.hue;
    #ifdef DEBUG
    LOG("Getting window %s hue: %f\n", window->header.title, *hue);
    #endif
	return 0;
}

int set_window_gamma(QarmaWindow* window, float gamma)
{
    window->display_settings.gamma = gamma;
    #ifdef DEBUG
    LOG("Setting window %s gamma to %f\n", window->header.title, gamma);
    #endif
	return 0;
}

int get_window_gamma(QarmaWindow* window, float* gamma)
{
    *gamma = window->display_settings.gamma;
    #ifdef DEBUG
    LOG("Getting window %s gamma: %f\n", window->header.title, *gamma);
    #endif
	return 0;
}

int set_window_refresh_rate(QarmaWindow* window, int refresh_rate)
{
	window->display_settings.refresh_rate = refresh_rate;
    #ifdef DEBUG
    LOG("Setting window %s refresh rate to %d\n", window->header.title, refresh_rate);
    #endif
	return 0;
}

int get_window_refresh_rate(QarmaWindow* window, int* refresh_rate)
{
	*refresh_rate = window->display_settings.refresh_rate;
	#ifdef DEBUG
	LOG("Getting window %s refresh rate: %d\n", window->header.title, *refresh_rate);
	#endif
	return 0;
}

int set_window_aspect_ratio(QarmaWindow* window, float aspect_ratio)
{
    window->display_settings.aspect_ratio = aspect_ratio;
    LOG("Setting window %s aspect ratio to %f\n", window->header.title, aspect_ratio);
	return 0;
}

int get_window_aspect_ratio(QarmaWindow* window, float* aspect_ratio)
{
    *aspect_ratio = window->display_settings.aspect_ratio;
    #ifdef DEBUG
    LOG("Getting window %s aspect ratio: %f\n", window->header.title, *aspect_ratio);
    #endif
	return 0;
}

int set_window_multisampling(QarmaWindow* window, int samples)
{
	window->context.samples = samples;
    #ifdef DEBUG
    LOG("Setting window %s multisampling to %d\n", window->header.title, samples);
    #endif
	return 0;
}

int get_window_multisampling(QarmaWindow* window, int* samples)
{
    *samples = window->context.samples;
    #ifdef DEBUG
    LOG("Getting window %s multisampling: %d\n", window->header.title, *samples);
    #endif
	return 0;
}

int set_window_stereo(QarmaWindow* window, int stereo)
{
	window->stereo = stereo;
    #ifdef DEBUG
    LOG("Setting window %s stereo to %d\n", window->header.title, stereo);
    #endif
	return 0;
}

int is_window_stereo(QarmaWindow* window, int* stereo)
{
    *stereo = window->stereo;
    #ifdef DEBUG
    LOG("Checking if window %s stereo is enabled: %d\n", window->header.title, *stereo);
    #endif
	return 0;
}

int set_window_depth_buffer(QarmaWindow* window, int depth_bits)
{
	window->depth_bits = depth_bits;
    #ifdef DEBUG
    LOG("Setting window %s depth buffer to %d bits\n", window->header.title, depth_bits);
    #endif
	return 0;
}

int get_window_depth_buffer(QarmaWindow* window, int* depth_bits)
{
    *depth_bits = window->depth_bits;
    #ifdef DEBUG
    LOG("Getting window %s depth buffer: %d bits\n", window->header.title, *depth_bits);
    #endif
	return 0;
}

int get_window_stencil_buffer(QarmaWindow* window, int* stencil_bits)
{
    *stencil_bits = window->stencil_bits;
    #ifdef DEBUG
    LOG("Getting window %s stencil buffer: %d bits\n", window->header.title, *stencil_bits);
    #endif
	return 0;
}


int set_window_accelerated_context(QarmaWindow* window, int accelerated)
{
	window->context.accelerated = accelerated;
    #ifdef DEBUG
    LOG("Setting window %s accelerated context to %d\n", window->header.title, accelerated);
    #endif
	return 0;
}

int is_window_accelerated_context(QarmaWindow* window, int* accelerated)
{
    *accelerated = window->context.accelerated;
    #ifdef DEBUG
    LOG("Checking if window %s has accelerated context: %d\n", window->header.title, *accelerated);
    #endif
	return 0;
}

int set_window_debug_context(QarmaWindow* window, int debug)
{
	window->debug = debug;
	return 0;
}

int is_window_debug_context(QarmaWindow* window, int* debug)
{
    *debug = window->debug;
    #ifdef DEBUG
    LOG("Checking if window %s has debug context: %d\n", window->header.title, *debug);
    #endif
	return 0;
}

int set_window_shared_context(QarmaWindow* window, int shared)
{
	window->shared = shared;
    #ifdef DEBUG
    LOG("Setting window %s shared context to %d\n", window->header.title, shared);
    #endif
	return 0;
}

int is_window_shared_context(QarmaWindow* window, int* shared)
{
    *shared = window->shared;
    #ifdef DEBUG
    LOG("Checking if window %s has shared context: %d\n", window->header.title, *shared);
    #endif
	return 0;
}

int set_window_multithreaded_context(QarmaWindow* window, int multithreaded)
{
	window->multithreaded = multithreaded;
    #ifdef DEBUG
    LOG("Setting window %s multithreaded context to %d\n", window->header.title, multithreaded);
    #endif
	return 0;
}

int is_window_multithreaded_context(QarmaWindow* window, int* multithreaded)
{
    *multithreaded = window->multithreaded;
    #ifdef DEBUG
    LOG("Checking if window %s has multithreaded context: %d\n", window->header.title, *multithreaded);
    #endif
	return 0;
}

int set_window_context_version(QarmaWindow* window, int major, int minor)
{
	window->major = major;
	window->minor = minor;
    #ifdef DEBUG
    LOG("Setting window %s context version to %d.%d\n", window->header.title, major, minor);
    #endif
	return 0;
}

int get_window_context_version(QarmaWindow* window, int* major, int* minor)
{
    *major = window->major;
    *minor = window->minor;
    #ifdef DEBUG
    LOG("Getting window %s context version: %d.%d\n", window->header.title, *major, *minor);
    #endif
	return 0;
}

int set_window_context_profile(QarmaWindow* window, int profile)
{
	window->context.profile = profile;
    #ifdef DEBUG
    LOG("Setting window %s context profile to %d\n", window->header.title, profile);
    #endif
    return 0;
}

int get_window_context_profile(QarmaWindow* window, int* profile)
{
    *profile = window->context.profile;
    #ifdef DEBUG
    LOG("Getting window %s context profile: %d\n", window->header.title, *profile);
    #endif
	return 0;
}

int set_window_context_flags(QarmaWindow* window, int flags)
{
	window->context.flags = flags;
    #ifdef DEBUG
    LOG("Setting window %s context flags to %d\n", window->header.title, flags);
    #endif
	return 0;
}

int get_window_context_flags(QarmaWindow* window, int* flags)
{
    *flags = window->context.flags;
    #ifdef DEBUG
    LOG("Getting window %s context flags: %d\n", window->header.title, *flags);
    #endif
	return 0;
}

int set_window_swap_interval(QarmaWindow* window, int interval)
{
	window->interval = interval;
    #ifdef DEBUG
    LOG("Setting window %s swap interval to %d\n", window->header.title, interval);
    #endif
	return 0;
}

int get_window_swap_interval(QarmaWindow* window, int* interval)
{
    *interval = window->interval;
    #ifdef DEBUG
    LOG("Getting window %s swap interval: %d\n", window->header.title, *interval);
    #endif
	return 0;
}

int poll_window_events(QarmaWindow* window)
{
    #ifdef DEBUG	
    LOG("Polling window %s events\n", window->header.title);
    #endif
	return 0;
}

int wait_for_window_event(QarmaWindow* window)
{
    #ifdef DEBUG
    LOG("Waiting for window %s events\n", window->header.title);
    #endif
	return 0;
}

int clear_window_events(QarmaWindow* window)
{
    #ifdef DEBUG
    LOG("Clearing window %s events\n", window->header.title);
    #endif
	return 0;
}

int flush_window_events(QarmaWindow* window)
{
    #ifdef DEBUG
    LOG("Flushing window %s events\n", window->header.title);
    #endif
	return 0;
}

int register_window_event_callback(QarmaWindow* window, void(*callback)(const char* event))
{
    #ifdef DEBUG
    LOG("Registering event callback for window %s\n", window->header.title);
    #endif
    // Note: This is a simplified implementation. The actual callback registration
    // would need to create a WindowEventCallback struct and link it properly.
    // For now, this function is just a placeholder.
    (void)window;    // Mark as unused to avoid warnings
    (void)callback;  // Mark as unused to avoid warnings
    return 0;
}

int unregister_window_event_callback(QarmaWindow* window, void(*callback)(const char* event))
{
    // Note: This is a simplified implementation. The actual callback unregistration
    // would need to find and remove the WindowEventCallback from the linked list.
    // For now, this function is just a placeholder.
    (void)window;    // Mark as unused to avoid warnings  
    (void)callback;  // Mark as unused to avoid warnings
    #ifdef DEBUG
    LOG("Unregistering event callback for window %s\n", window->header.title);
    #endif
	return 0;
}

int set_window_user_data(QarmaWindow* window, void* user_data)
{
	window->user_data = user_data;
    #ifdef DEBUG
    LOG("Setting user data for window %s\n", window->header.title);
    #endif
	return 0;
}

int get_window_user_data(QarmaWindow* window, void** user_data)
{
    *user_data = window->user_data;
    #ifdef DEBUG
    LOG("Getting user data for window %s\n", window->header.title);
    #endif
	return 0;
}


int set_always_on_top(QarmaWindow* window, bool always_on_top) {
	if(!window) return -1; // or handle error differently
	window->style.always_on_top = always_on_top;
	LOG("Setting window %s always on top to %d\n", window->header.title, always_on_top);
	return 0;
}

// bool is_window_always_on_top(QarmaWindow* window) {
//     if (!window) return false; // or handle error differently
// 	printf("Setting window %s always on top to %d\n", window->header.title, window->style.always_on_top);
//     return window->style.always_on_top;
// }