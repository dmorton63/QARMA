#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "mesa_bootstrap.h"
#include "log.h"  // For LOG()
#include "image_loader.h"  // For PNG loading


static Display* x_display;
static Window win;
static EGLDisplay egl_display;
static EGLContext egl_context;
static EGLSurface egl_surface;

// Minimal shader variables for actual rendering
static GLuint program;
static GLuint rect_program;      // Rectangle shader program  
static GLuint text_program;
static GLuint texture_program;   // Texture shader program for images
static GLuint vbo;
static GLuint text_vbo;

// Simple text message storage
static char text_messages[10][64];  // Up to 10 messages, 64 chars each
static int message_count = 0;

// Window state
static int window_width = 640;
static int window_height = 480;
static bool shutdown_requested = false;
static bool show_splash = true;  // Show splash screen initially

// Splash screen image
static Image* splash_image = NULL;
static unsigned int splash_texture = 0;

// Forward declarations
static void render_text_line(const char* text, float x, float y);
static void handle_mouse_click(int x, int y);
static void draw_splash_screen(void);
static void draw_atom_symbol(float center_x, float center_y, float radius);
static void draw_circle(float center_x, float center_y, float radius, float r, float g, float b);
static void render_text_centered(const char* text, float center_x, float y);
static void draw_textured_quad(float x, float y, float width, float height, unsigned int texture);
static void load_splash_image(const char* filename);

// Simple 8x8 bitmap font data for ASCII characters 32-126
static const unsigned char font_8x8[95][8] = {
    // Space (32)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // ! (33)
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},
    // " (34)
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // # (35)
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00},
    // $ (36)
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00},
    // % (37)
    {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00},
    // & (38)
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00},
    // ' (39)
    {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00},
    // ( (40)
    {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00},
    // ) (41)
    {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00},
    // * (42)
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},
    // + (43)
    {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00},
    // , (44)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x06, 0x00},
    // - (45)
    {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00},
    // . (46)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00},
    // / (47)
    {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00},
    // 0 (48)
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00},
    // 1 (49)
    {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00},
    // 2 (50)
    {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00},
    // 3 (51)
    {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00},
    // 4 (52)
    {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00},
    // 5 (53)
    {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00},
    // 6 (54)
    {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00},
    // 7 (55)
    {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00},
    // 8 (56)
    {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00},
    // 9 (57)
    {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00},
    // : (58)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00},
    // ; (59)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x06, 0x00},
    // < (60)
    {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00},
    // = (61)
    {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00},
    // > (62)
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},
    // ? (63)
    {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00},
    // @ (64)
    {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},
    // A (65)
    {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00},
    // B (66)
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00},
    // C (67)
    {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00},
    // D (68)
    {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00},
    // E (69)
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00},
    // F (70)
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00},
    // G (71)
    {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00},
    // H (72)
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00},
    // I (73)
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},
    // J (74)
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00},
    // K (75)
    {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00},
    // L (76)
    {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00},
    // M (77)
    {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00},
    // N (78)
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00},
    // O (79)
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00},
    // P (80)
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00},
    // Q (81)
    {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00},
    // R (82)
    {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00},
    // S (83)
    {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00},
    // T (84)
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},
    // U (85)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00},
    // V (86)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},
    // W (87)
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},
    // X (88)
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00},
    // Y (89)
    {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00},
    // Z (90)
    {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00},
    // [ (91)
    {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00},
    // \ (92)
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00},
    // ] (93)
    {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00},
    // ^ (94)
    {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00},
    // _ (95)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},
    // ` (96)
    {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},
    // a (97)
    {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00},
    // b (98)
    {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00},
    // c (99)
    {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00},
    // d (100)
    {0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00},
    // e (101)
    {0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00},
    // f (102)
    {0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00},
    // g (103)
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F},
    // h (104)
    {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00},
    // i (105)
    {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},
    // j (106)
    {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E},
    // k (107)
    {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00},
    // l (108)
    {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},
    // m (109)
    {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00},
    // n (110)
    {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00},
    // o (111)
    {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00},
    // p (112)
    {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F},
    // q (113)
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78},
    // r (114)
    {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00},
    // s (115)
    {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00},
    // t (116)
    {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00},
    // u (117)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00},
    // v (118)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},
    // w (119)
    {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00},
    // x (120)
    {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00},
    // y (121)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F},
    // z (122)
    {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00},
    // { (123)
    {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00},
    // | (124)
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},
    // } (125)
    {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00},
    // ~ (126)
    {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

// Simplified mesa_bootstrap_init function
void mesa_bootstrap_init(int x, int y, int width, int height) {
    window_width = width;
    window_height = height;
    
    LOG("Initializing mesa_bootstrap with dimensions: %dx%d at position (%d,%d)\n", width, height, x, y);
    
    // Step 1: Create X11 window
    x_display = XOpenDisplay(NULL);
    if (!x_display) {
        LOG("Error: Cannot open X11 display\n");
        exit(1);
    }

    Window root = DefaultRootWindow(x_display);
    win = XCreateSimpleWindow(x_display, root, 0, 0, width, height, 0,
                              BlackPixel(x_display, 0), BlackPixel(x_display, 0));
    
    // Set window title
    XStoreName(x_display, win, "QARMA Operating System v1.1");
    
    // Override window manager - make window fullscreen and undecorated
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    XChangeWindowAttributes(x_display, win, CWOverrideRedirect, &attrs);
    
    // Move window to top-left corner and resize to full screen
    XMoveResizeWindow(x_display, win, 0, 0, width, height);
    
    XSelectInput(x_display, win, ExposureMask | KeyPressMask | ButtonPressMask);
    XMapWindow(x_display, win);
    XFlush(x_display);
    
    // Check actual window position after mapping
    Window root_return;
    int x_return, y_return;
    unsigned int width_return, height_return, border_width_return, depth_return;
    XGetGeometry(x_display, win, &root_return, &x_return, &y_return, 
                 &width_return, &height_return, &border_width_return, &depth_return);
    LOG("Actual window position: (%d, %d), size: %dx%d\n", 
           x_return, y_return, width_return, height_return);
    XFlush(x_display);
    
    LOG("Window created successfully\n");

    // Step 2: Initialize OpenGL with EGL  
    egl_display = eglGetDisplay((EGLNativeDisplayType)x_display);
    eglInitialize(egl_display, NULL, NULL);

    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs;
    eglChooseConfig(egl_display, config_attribs, &config, 1, &num_configs);
    egl_surface = eglCreateWindowSurface(egl_display, config, (EGLNativeWindowType)win, NULL);

    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    egl_context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT, context_attribs);
    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

    // Step 3: Basic OpenGL setup
    glViewport(0, 0, window_width, window_height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    
    // Create basic shaders for rectangle rendering
    const char* simple_vertex = 
        "attribute vec2 position;\n"
        "void main() { gl_Position = vec4(position, 0.0, 1.0); }\n";
    
    const char* simple_fragment = 
        "precision mediump float;\n"
        "uniform vec3 color;\n"
        "void main() { gl_FragColor = vec4(color, 1.0); }\n";
    
    // Create and compile shaders
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &simple_vertex, NULL);
    glCompileShader(vs);
    
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &simple_fragment, NULL);
    glCompileShader(fs);
    
    // Create program for rectangles
    rect_program = glCreateProgram();
    glAttachShader(rect_program, vs);
    glAttachShader(rect_program, fs);
    glLinkProgram(rect_program);
    
    // Check program linking
    GLint link_status;
    glGetProgramiv(rect_program, GL_LINK_STATUS, &link_status);
    if (link_status != GL_TRUE) {
        LOG("ERROR: Failed to link shader program\n");
    } else {
        LOG("Shader program linked successfully\n");
    }
    
    // Create texture shader for PNG images
    const char* texture_vertex = 
        "attribute vec2 position;\n"
        "attribute vec2 texCoord;\n"
        "varying vec2 v_texCoord;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 0.0, 1.0);\n"
        "    v_texCoord = texCoord;\n"
        "}\n";
    
    const char* texture_fragment = 
        "precision mediump float;\n"
        "varying vec2 v_texCoord;\n"
        "uniform sampler2D u_texture;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(u_texture, v_texCoord);\n"
        "}\n";
    
    // Create and compile texture shaders
    GLuint texture_vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(texture_vs, 1, &texture_vertex, NULL);
    glCompileShader(texture_vs);
    
    GLuint texture_fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(texture_fs, 1, &texture_fragment, NULL);
    glCompileShader(texture_fs);
    
    // Create program for textures
    texture_program = glCreateProgram();
    glAttachShader(texture_program, texture_vs);
    glAttachShader(texture_program, texture_fs);
    glLinkProgram(texture_program);
    
    // Check texture program linking
    GLint texture_link_status;
    glGetProgramiv(texture_program, GL_LINK_STATUS, &texture_link_status);
    if (texture_link_status != GL_TRUE) {
        LOG("ERROR: Failed to link texture shader program\n");
    } else {
        LOG("Texture shader program linked successfully\n");
    }
    
    // Create basic triangle program for background
    program = rect_program;  // Use same program for now
    
    // Create VBO
    glGenBuffers(1, &vbo);
    
    // Set up vertex attributes for rectangle rendering
    glUseProgram(rect_program);
    GLint pos_attrib = glGetAttribLocation(rect_program, "position");
    glEnableVertexAttribArray(pos_attrib);
    
    // Create simple text rendering setup (reuse rectangle program)
    text_program = rect_program;  // Use same program for text
    glGenBuffers(1, &text_vbo);
    
    // Test render
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(egl_display, egl_surface);
    
    // Try to load splash screen image
    load_splash_image("src/Images/splash.png");  // Look for splash.png in src/Images/ directory
    
    LOG("Mesa bootstrap initialized successfully\n");
}
void mesa_bootstrap_render(void) {
    // Process X11 events
    while (XPending(x_display)) {
        XEvent event;
        XNextEvent(x_display, &event);
        
        switch (event.type) {
            case ButtonPress:
                if (event.xbutton.button == 1) {  // Left mouse button
                    if (show_splash) {
                        show_splash = false;  // Dismiss splash on any click
                        // Initialize desktop messages
                        mesa_add_text_message("QARMA OS v1.1 - Main Desktop");
                        mesa_add_text_message("");
                        mesa_add_text_message("System Status: Online");
                        mesa_add_text_message("Mouse: Initializing...");
                        mesa_add_text_message("Keyboard: Ready");
                        mesa_add_text_message("");
                        mesa_add_text_message("Click anywhere to interact");
                        mesa_add_text_message("ESC key to shutdown system");
                    } else {
                        handle_mouse_click(event.xbutton.x, event.xbutton.y);
                    }
                }
                break;
            case KeyPress:
                // ESC key to shutdown or dismiss splash
                if (event.xkey.keycode == 9) {  // ESC keycode
                    if (show_splash) {
                        show_splash = false;  // Dismiss splash on ESC
                        // Initialize desktop messages
                        mesa_add_text_message("QARMA OS v1.1 - Main Desktop");
                        mesa_add_text_message("");
                        mesa_add_text_message("System Status: Online");
                        mesa_add_text_message("Mouse: Initializing...");
                        mesa_add_text_message("Keyboard: Ready");
                        mesa_add_text_message("");
                        mesa_add_text_message("Click anywhere to interact");
                        mesa_add_text_message("ESC key to shutdown system");
                    } else {
                        shutdown_requested = true;
                        mesa_add_text_message("Shutdown requested...");
                    }
                }
                break;
        }
    }
    
    glClearColor(0.05f, 0.1f, 0.15f, 1.0f);  // Dark blue background
    glClear(GL_COLOR_BUFFER_BIT);

    if (show_splash) {
        // Show splash screen
        draw_splash_screen();
    } else {
        // Render desktop background gradient (using the triangle as a fullscreen quad)
        glUseProgram(program);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Draw desktop panels and interface elements
        draw_desktop_interface();
    }

    // Temporarily disable complex text rendering
    // draw_text_centered("QARMA Operating System", 0.0f, 0.0f, 1.0f);

    // Disable text message rendering for now to avoid errors
    // TODO: Re-enable once text rendering is properly set up
    
    // Ensure buffer is swapped to display the rendered content
    eglSwapBuffers(egl_display, egl_surface);
    
    // Render taskbar text
    render_text_line("QARMA", 15, window_height - 35);  // Start button label
    render_text_line("System Tray", window_width - 180, window_height - 35);
}

// Missing function implementations

void mesa_add_text_message(const char* message) {
    if (message_count >= 10) {
        LOG("mesa_add_text_message: Message limit reached (%d), clearing old messages\n", message_count);
        message_count = 0;  // Reset and overwrite old messages
    }
    
    strncpy(text_messages[message_count], message, 63);  // Use existing array size
    text_messages[message_count][63] = '\0';
    message_count++;
    LOG("Adding text message [%d]: '%s'\n", message_count-1, message);
    LOG("Message count now: %d\n", message_count);
}

bool mesa_shutdown_requested(void) {
    return shutdown_requested;
}

void mesa_bootstrap_shutdown(void) {
    LOG("Mesa bootstrap shutting down...\n");
    eglTerminate(egl_display);
    XCloseDisplay(x_display);
}

static void handle_mouse_click(int x, int y) {
    LOG("Mouse click at (%d, %d)\n", x, y);
    
    // Shutdown button is the entire right half of screen for testing
    int button_width = window_width / 2;  // Right half of screen
    int button_height = window_height;    // Full height
    int button_x = window_width / 2;      // Start at middle
    int button_y = 0;                     // Full height
    
    LOG("Button area (X11): x=%d-%d, y=%d-%d\n", 
           button_x, button_x + button_width, button_y, button_y + button_height);
    LOG("Click: x=%d, y=%d\n", x, y);
    
    // Check if click is in button area (using X11 coordinates directly)
    if (x >= button_x && x <= (button_x + button_width) && 
        y >= button_y && y <= (button_y + button_height)) {
        
        LOG("*** SHUTDOWN BUTTON CLICKED! ***\n");
        mesa_add_text_message("SHUTDOWN BUTTON CLICKED! Exiting...");
        shutdown_requested = true;  // Request shutdown
    } else {
        mesa_add_text_message("Mouse clicked!");
    }
}

static void render_text_line(const char* text, float x, float y) {
    if (!text) return;
    
    // Bitmap font rendering using the 8x8 font array with 2x scaling
    glUseProgram(rect_program);
    
    int color_location = glGetUniformLocation(rect_program, "color");
    glUniform3f(color_location, 1.0f, 1.0f, 1.0f);  // White text
    
    float pixel_scale = 2.0f;  // 2x scaling for better visibility
    
    // Convert pixel coordinates to normalized coordinates
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (c < 32 || c > 126) continue; // Only printable ASCII characters
        
        int font_index = c - 32;  // Font array starts at space (ASCII 32)
        float char_x_pixel = x + (i * 8 * pixel_scale);  // 8 pixels per character * scale
        float char_y_pixel = y;
        
        // Render each bit of the character as a small rectangle
        for (int row = 0; row < 8; row++) {
            unsigned char row_data = font_8x8[font_index][row];
            for (int col = 0; col < 8; col++) {
                if (row_data & (1 << col)) {  // Check if pixel is set (corrected bit order)
                    float px = char_x_pixel + (col * pixel_scale);
                    float py = char_y_pixel + (row * pixel_scale);
                    
                    // Convert to normalized coordinates
                    float px_norm = (px / window_width) * 2.0f - 1.0f;
                    float py_norm = 1.0f - (py / window_height) * 2.0f;
                    float pw_norm = (pixel_scale / window_width) * 2.0f;
                    float ph_norm = (pixel_scale / window_height) * 2.0f;
                    
                    // Create pixel rectangle vertices
                    float pixel_vertices[] = {
                        px_norm, py_norm - ph_norm,               // Bottom left
                        px_norm + pw_norm, py_norm - ph_norm,     // Bottom right
                        px_norm, py_norm,                         // Top left
                        px_norm + pw_norm, py_norm,               // Top right
                        px_norm, py_norm,                         // Top left (repeated)
                        px_norm + pw_norm, py_norm - ph_norm      // Bottom right (repeated)
                    };
                    
                    glBindBuffer(GL_ARRAY_BUFFER, vbo);
                    glBufferData(GL_ARRAY_BUFFER, sizeof(pixel_vertices), pixel_vertices, GL_DYNAMIC_DRAW);
                    
                    GLint pos_attrib = glGetAttribLocation(rect_program, "position");
                    glEnableVertexAttribArray(pos_attrib);
                    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
                    
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
            }
        }
    }
}

void draw_desktop_interface(void) {
    // Draw simple colored rectangles for GUI elements
    LOG("Drawing desktop interface...\n");
    
    // Taskbar (bottom of screen)
    glUseProgram(rect_program);
    
    // Set up vertex attributes
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLint pos_attrib = glGetAttribLocation(rect_program, "position");
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    
    // Create taskbar vertices (bottom 50 pixels of screen, converted to normalized coordinates)
    float taskbar_y_norm = -1.0f + (50.0f / window_height) * 2.0f;  // Convert to [-1,1] range
    float taskbar_vertices[] = {
        -1.0f, -1.0f,           // Bottom left
         1.0f, -1.0f,           // Bottom right  
        -1.0f, taskbar_y_norm,  // Top left
         1.0f, taskbar_y_norm,  // Top right
        -1.0f, taskbar_y_norm,  // Top left (repeated)
         1.0f, -1.0f            // Bottom right (repeated)
    };
    
    glBufferData(GL_ARRAY_BUFFER, sizeof(taskbar_vertices), taskbar_vertices, GL_DYNAMIC_DRAW);
    
    // Set bright gray color for taskbar (much more visible)
    int color_location = glGetUniformLocation(rect_program, "color");
    glUniform3f(color_location, 0.7f, 0.7f, 0.8f);  // Bright gray
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
    LOG("Drew taskbar\n");
    
    // Draw title bar (top 50 pixels of screen, converted to normalized coordinates)
    float titlebar_y_norm = 1.0f - (50.0f / window_height) * 2.0f;  // Convert to [-1,1] range
    float titlebar_vertices[] = {
        -1.0f, titlebar_y_norm,     // Bottom left
         1.0f, titlebar_y_norm,     // Bottom right
        -1.0f, 1.0f,                // Top left  
         1.0f, 1.0f,                // Top right
        -1.0f, 1.0f,                // Top left (repeated)
         1.0f, titlebar_y_norm      // Bottom right (repeated)
    };
    
    glBufferData(GL_ARRAY_BUFFER, sizeof(titlebar_vertices), titlebar_vertices, GL_DYNAMIC_DRAW);
    
    // Set bright blue color for title bar
    glUniform3f(color_location, 0.3f, 0.5f, 0.9f);  // Bright blue
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
    LOG("Drew title bar\n");
    
    // Draw shutdown button (top-right corner) - Visual red rectangle
    float button_width_norm = (200.0f / window_width) * 2.0f;  // Convert 200 pixels to normalized
    float shutdown_button_vertices[] = {
        1.0f - button_width_norm, titlebar_y_norm,     // Bottom left
        1.0f, titlebar_y_norm,                         // Bottom right
        1.0f - button_width_norm, 1.0f,                // Top left  
        1.0f, 1.0f,                                    // Top right
        1.0f - button_width_norm, 1.0f,                // Top left (repeated)
        1.0f, titlebar_y_norm                          // Bottom right (repeated)
    };
    
    glBufferData(GL_ARRAY_BUFFER, sizeof(shutdown_button_vertices), shutdown_button_vertices, GL_DYNAMIC_DRAW);
    
    // Set bright red color for shutdown button
    glUniform3f(color_location, 1.0f, 0.3f, 0.3f);  // Bright red
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
    LOG("Drew shutdown button\n");
    
    // Add title text to the title bar
    render_text_line("WELCOME TO QARMA OS Version 1.1", 20.0f, 25.0f);  // 20px from left, 25px from top
    
    // Add text to the shutdown button
    float button_text_x = window_width - 80.0f;  // Position in the shutdown button
    render_text_line("EXIT", button_text_x, 25.0f);  // "EXIT" text on the button
}

// Helper function to render text centered horizontally
static void render_text_centered(const char* text, float center_x, float y) {
    if (!text) return;
    
    // Estimate text width (8 pixels per character with 2x scaling = 16 pixels per char)
    float text_width = strlen(text) * 16.0f;
    float start_x = center_x - (text_width / 2.0f);
    
    render_text_line(text, start_x, y);
}

// Helper function to draw a circle using triangles
static void draw_circle(float center_x, float center_y, float radius, float r, float g, float b) {
    glUseProgram(rect_program);
    
    int color_location = glGetUniformLocation(rect_program, "color");
    glUniform3f(color_location, r, g, b);
    
    const int segments = 32;  // Number of triangle segments to approximate circle
    float angle_step = (2.0f * 3.14159f) / segments;
    
    // Convert pixel coordinates to normalized coordinates
    float center_x_norm = (center_x / window_width) * 2.0f - 1.0f;
    float center_y_norm = 1.0f - (center_y / window_height) * 2.0f;
    float radius_x_norm = (radius / window_width) * 2.0f;
    float radius_y_norm = (radius / window_height) * 2.0f;
    
    // Draw circle as triangular fan
    for (int i = 0; i < segments; i++) {
        float angle1 = i * angle_step;
        float angle2 = (i + 1) * angle_step;
        
        float vertices[] = {
            center_x_norm, center_y_norm,  // Center point
            center_x_norm + cos(angle1) * radius_x_norm, center_y_norm + sin(angle1) * radius_y_norm,  // Point 1
            center_x_norm + cos(angle2) * radius_x_norm, center_y_norm + sin(angle2) * radius_y_norm   // Point 2
        };
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        
        GLint pos_attrib = glGetAttribLocation(rect_program, "position");
        glEnableVertexAttribArray(pos_attrib);
        glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
        
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
}

// Draw an atom symbol with nucleus and electron orbits
static void draw_atom_symbol(float center_x, float center_y, float radius) {
    // Draw nucleus (center circle) - bright white
    draw_circle(center_x, center_y, radius * 0.15f, 1.0f, 1.0f, 1.0f);
    
    // Draw electron orbits (elliptical rings)
    const int num_orbits = 3;
    for (int orbit = 0; orbit < num_orbits; orbit++) {
        float orbit_radius = radius * (0.4f + orbit * 0.2f);
        float orbit_color = 0.7f - orbit * 0.15f;  // Fade from bright to dim
        
        // Draw orbit ring using small circles
        const int orbit_points = 48;
        for (int i = 0; i < orbit_points; i++) {
            float angle = (i * 2.0f * 3.14159f) / orbit_points;
            
            // Create elliptical orbits at different angles
            float tilt = orbit * 0.6f;  // Different tilt for each orbit
            float orbit_x = center_x + cos(angle + tilt) * orbit_radius;
            float orbit_y = center_y + sin(angle + tilt) * orbit_radius * 0.6f;  // Flatten orbits
            
            // Draw small dots for orbit path
            if (i % 4 == 0) {  // Only draw every 4th point for dotted effect
                draw_circle(orbit_x, orbit_y, radius * 0.02f, orbit_color, orbit_color, 1.0f);
            }
        }
    }
}

// Draw the splash screen with atom symbol and information
static void draw_splash_screen(void) {
    // Draw semi-transparent overlay background
    glUseProgram(rect_program);
    
    int color_location = glGetUniformLocation(rect_program, "color");
    glUniform3f(color_location, 0.1f, 0.1f, 0.2f);  // Dark blue overlay
    
    // Full screen overlay with transparency effect
    float overlay_vertices[] = {
        -1.0f, -1.0f,  // Bottom left
         1.0f, -1.0f,  // Bottom right
        -1.0f,  1.0f,  // Top left
         1.0f,  1.0f,  // Top right
        -1.0f,  1.0f,  // Top left (repeated)
         1.0f, -1.0f   // Bottom right (repeated)
    };
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(overlay_vertices), overlay_vertices, GL_DYNAMIC_DRAW);
    
    GLint pos_attrib = glGetAttribLocation(rect_program, "position");
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // Calculate center positions
    float center_x = window_width / 2.0f;
    float center_y = window_height / 2.0f;
    
    // Draw splash image or fallback to procedural atom
    if (splash_texture != 0 && splash_image != NULL) {
        // Use the PNG image - scale it appropriately
        float image_scale = 0.3f;  // Scale factor for the image
        float img_width = splash_image->width * image_scale;
        float img_height = splash_image->height * image_scale;
        float img_x = center_x - img_width / 2.0f;
        float img_y = center_y - 150.0f - img_height / 2.0f;
        
        draw_textured_quad(img_x, img_y, img_width, img_height, splash_texture);
    } else {
        // Fallback to procedural atom symbol
        draw_atom_symbol(center_x, center_y - 100.0f, 120.0f);
    }
    
    // Application information text, centered below atom/image
    render_text_centered("QARMA OPERATING SYSTEM", center_x, center_y + 80.0f);
    render_text_centered("Version 1.1.0", center_x, center_y + 120.0f);
    render_text_centered("", center_x, center_y + 160.0f);  // Empty line
    render_text_centered("Multi-Core CPU Management Framework", center_x, center_y + 180.0f);
    render_text_centered("with Real-time Subsystem Dispatch", center_x, center_y + 200.0f);
    render_text_centered("", center_x, center_y + 240.0f);  // Empty line
    render_text_centered("Author: David K. Morton", center_x, center_y + 260.0f);
    render_text_centered("Built with OpenGL ES + EGL + Mesa", center_x, center_y + 280.0f);
    render_text_centered("", center_x, center_y + 320.0f);  // Empty line
    render_text_centered("Press ESC or click to continue...", center_x, center_y + 340.0f);
}

// Load splash screen image from file
static void load_splash_image(const char* filename) {
    splash_image = load_image(filename);
    if (splash_image) {
        splash_texture = create_texture_from_image(splash_image);
        LOG("Splash image loaded successfully: %s\n", filename);
    } else {
        LOG("Could not load splash image: %s (will use procedural atom)\n", filename);
    }
}

// Draw a textured quad for displaying images
static void draw_textured_quad(float x, float y, float width, float height, unsigned int texture) {
    if (texture == 0) return;
    
    // Convert pixel coordinates to normalized coordinates  
    float x1_norm = (x / window_width) * 2.0f - 1.0f;
    float y1_norm = 1.0f - (y / window_height) * 2.0f;
    float x2_norm = ((x + width) / window_width) * 2.0f - 1.0f;
    float y2_norm = 1.0f - ((y + height) / window_height) * 2.0f;
    
    // Vertex data: position (x,y) + texture coordinates (u,v)
    float vertices[] = {
        // Position      // TexCoords
        x1_norm, y2_norm,  0.0f, 1.0f,  // Bottom left
        x2_norm, y2_norm,  1.0f, 1.0f,  // Bottom right  
        x1_norm, y1_norm,  0.0f, 0.0f,  // Top left
        x2_norm, y1_norm,  1.0f, 0.0f,  // Top right
        x1_norm, y1_norm,  0.0f, 0.0f,  // Top left (repeated)
        x2_norm, y2_norm,  1.0f, 1.0f   // Bottom right (repeated)
    };
    
    glUseProgram(texture_program);  // Use texture shader program
    
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // Set the texture uniform
    GLint texture_uniform = glGetUniformLocation(texture_program, "u_texture");
    glUniform1i(texture_uniform, 0);  // Use texture unit 0
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    
    // Position attribute
    GLint pos_attrib = glGetAttribLocation(texture_program, "position");
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    
    // Texture coordinate attribute  
    GLint tex_attrib = glGetAttribLocation(texture_program, "texCoord");
    if (tex_attrib >= 0) {
        glEnableVertexAttribArray(tex_attrib);
        glVertexAttribPointer(tex_attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    }
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // Clean up
    glDisableVertexAttribArray(pos_attrib);
    if (tex_attrib >= 0) {
        glDisableVertexAttribArray(tex_attrib);
    }
}

void draw_text_centered(const char* msg, float center_x, float center_y, float scale) {
    // Simplified text rendering - temporarily disabled
    (void)msg; (void)center_x; (void)center_y; (void)scale; // Suppress warnings
    return;
}
    
