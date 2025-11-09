#pragma once

typedef struct {
    unsigned char* data;
    int width;
    int height;
    int channels;
} Image;

// Load image from file (PNG, JPG, etc.)
Image* load_image(const char* filename);

// Free image data
void free_image(Image* img);

// Create OpenGL texture from image
unsigned int create_texture_from_image(Image* img);