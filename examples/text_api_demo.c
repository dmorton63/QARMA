#include "../headers/mesa_bootstrap.h"
#include <stdio.h>

// Example showing how to add text messages to your OpenGL window
void demonstrate_text_api() {
    printf("=== Text Message API Demo ===\n\n");
    
    printf("1. Initialize the window:\n");
    printf("   mesa_bootstrap_init();\n\n");
    
    printf("2. Add text messages:\n");
    printf("   mesa_add_text_message(\"Hello World!\");\n");
    printf("   mesa_add_text_message(\"Line 2\");\n");
    printf("   mesa_add_text_message(\"Line 3\");\n\n");
    
    printf("3. In your render loop:\n");
    printf("   mesa_bootstrap_render(); // Renders triangle + text\n\n");
    
    printf("4. Clear messages when needed:\n");
    printf("   mesa_clear_text_messages();\n\n");
    
    printf("5. Add new messages:\n");
    printf("   mesa_add_text_message(\"New content!\");\n\n");
    
    printf("6. Cleanup:\n");
    printf("   mesa_bootstrap_shutdown();\n\n");
    
    printf("=== Integration with Video Subsystem ===\n\n");
    
    printf("Your video_subsystem.c has been updated to automatically\n");
    printf("add text messages to the OpenGL window when processing:\n\n");
    
    printf("- MSG_RENDER_REQUEST: Adds message payload to window\n");
    printf("- MSG_INPUT_EVENT: Shows 'Input: X' for each character\n\n");
    
    printf("Text appears as white characters on your OpenGL window,\n");
    printf("positioned from top-left, with up to 10 lines visible.\n\n");
    
    printf("=== Features ===\n\n");
    printf("- Up to 10 text lines (64 chars each)\n");
    printf("- White text on dark background\n");
    printf("- Lines positioned from top to bottom\n");
    printf("- Automatic text wrapping at 20 characters per line\n");
    printf("- Simple character rendering using OpenGL quads\n");
    printf("- Integrates with existing triangle rendering\n\n");
}

// int main() {
//     demonstrate_text_api();
//     return 0;
// }