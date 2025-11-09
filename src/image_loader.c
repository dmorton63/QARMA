#include "image_loader.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <GLES2/gl2.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Image* load_image(const char* filename) {
    LOG("Attempting to load image: %s\n", filename);
    
    Image* img = malloc(sizeof(Image));
    if (!img) {
        LOG("Failed to allocate memory for image\n");
        return NULL;
    }
    
    // Load image using stb_image
    img->data = stbi_load(filename, &img->width, &img->height, &img->channels, 4); // Force RGBA
    img->channels = 4; // Always use RGBA
    
    if (!img->data) {
        LOG("Failed to load image: %s\n", filename);
        LOG("STB Error: %s\n", stbi_failure_reason());
        free(img);
        return NULL;
    }
    
    // Check if image is too large (limit to 4K resolution for safety)
    if (img->width > 4096 || img->height > 4096) {
        LOG("Image too large: %dx%d (max 4096x4096)\n", img->width, img->height);
        stbi_image_free(img->data);
        free(img);
        return NULL;
    }
    
    LOG("Successfully loaded image: %s (%dx%d, %d channels, %d bytes)\n", 
        filename, img->width, img->height, img->channels, img->width * img->height * img->channels);
    return img;
}

void free_image(Image* img) {
    if (img) {
        if (img->data) {
            stbi_image_free(img->data);
        }
        free(img);
    }
}

unsigned int create_texture_from_image(Image* img) {
    if (!img || !img->data) {
        LOG("Cannot create texture: invalid image data\n");
        return 0;
    }
    
    LOG("Creating OpenGL texture for %dx%d image\n", img->width, img->height);
    
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img->width, img->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img->data);
    
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG("OpenGL error creating texture: 0x%x\n", error);
        glDeleteTextures(1, &texture);
        return 0;
    }
    
    LOG("Successfully created OpenGL texture ID %u from image (%dx%d)\n", texture, img->width, img->height);
    return texture;
}