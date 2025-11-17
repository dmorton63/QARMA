#ifndef PNG_DECODER_H
#define PNG_DECODER_H

#include "core/stdtools.h"
#include "graphics.h"

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t* pixels;  // RGBA32 format
} png_image_t;

/**
 * Simple PNG decoder for basic RGB/RGBA images
 * Returns decoded image data or NULL on failure
 */
png_image_t* png_decode(const uint8_t* png_data, uint32_t data_size);

/**
 * Free decoded PNG image
 */
void png_free(png_image_t* image);

/**
 * Load the embedded splash PNG
 */
png_image_t* load_splash_image(void);

/**
 * Decode PNG directly to framebuffer (no heap allocation)
 */
void png_decode_to_framebuffer(const uint8_t* png_data, uint32_t data_size,
                               uint32_t* framebuffer, uint32_t fb_width, uint32_t fb_height);

/**
 * Load embedded splash PNG directly to framebuffer
 */
void load_splash_to_framebuffer(uint32_t* framebuffer, uint32_t fb_width, uint32_t fb_height);

#endif // PNG_DECODER_H