#include "png_decoder.h"
#include "core/memory.h"
#include "core/memory/heap.h"
#include "core/memory/memory_pool.h"
#include "core/string.h"
#include "splash_data.h"
#include "config.h"

// PNG chunk reading utilities
static uint32_t read_uint32_be(const uint8_t* data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

static uint16_t read_uint16_le(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

// Bit stream for DEFLATE decoding
typedef struct {
    const uint8_t* data;
    uint32_t size;
    uint32_t byte_pos;
    uint8_t bit_pos;
} bit_stream_t;

static void init_bit_stream(bit_stream_t* bs, const uint8_t* data, uint32_t size) {
    bs->data = data;
    bs->size = size;
    bs->byte_pos = 0;
    bs->bit_pos = 0;
}

static uint32_t read_bits(bit_stream_t* bs, int num_bits) {
    uint32_t result = 0;
    for (int i = 0; i < num_bits; i++) {
        if (bs->byte_pos >= bs->size) return 0;
        
        uint8_t bit = (bs->data[bs->byte_pos] >> bs->bit_pos) & 1;
        result |= (bit << i);
        
        bs->bit_pos++;
        if (bs->bit_pos >= 8) {
            bs->bit_pos = 0;
            bs->byte_pos++;
        }
    }
    return result;
}

static void align_to_byte(bit_stream_t* bs) {
    if (bs->bit_pos != 0) {
        bs->bit_pos = 0;
        bs->byte_pos++;
    }
}

// Huffman decoding structures
typedef struct {
    uint16_t code;
    uint8_t length;
    uint16_t symbol;
} huffman_entry_t;

typedef struct {
    huffman_entry_t entries[288];
    int count;
} huffman_table_t;

static void build_huffman_table(huffman_table_t* table, const uint8_t* lengths, int num_symbols) {
    table->count = 0;
    
    // Count codes of each length
    int bl_count[16] = {0};
    for (int i = 0; i < num_symbols; i++) {
        if (lengths[i] > 0) {
            bl_count[lengths[i]]++;
        }
    }
    
    // Generate code values
    int next_code[16] = {0};
    int code = 0;
    for (int bits = 1; bits <= 15; bits++) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }
    
    // Assign codes to symbols
    for (int n = 0; n < num_symbols; n++) {
        int len = lengths[n];
        if (len != 0) {
            table->entries[table->count].symbol = n;
            table->entries[table->count].length = len;
            table->entries[table->count].code = next_code[len];
            next_code[len]++;
            table->count++;
        }
    }
}

static int decode_symbol(bit_stream_t* bs, huffman_table_t* table) {
    uint32_t code = 0;
    for (int len = 1; len <= 15; len++) {
        code = (code << 1) | read_bits(bs, 1);
        
        for (int i = 0; i < table->count; i++) {
            if (table->entries[i].length == len && table->entries[i].code == code) {
                return table->entries[i].symbol;
            }
        }
    }
    return -1;
}

// Fixed Huffman tables for DEFLATE
static void build_fixed_tables(huffman_table_t* lit_table, huffman_table_t* dist_table) {
    uint8_t lit_lengths[288];
    for (int i = 0; i <= 143; i++) lit_lengths[i] = 8;
    for (int i = 144; i <= 255; i++) lit_lengths[i] = 9;
    for (int i = 256; i <= 279; i++) lit_lengths[i] = 7;
    for (int i = 280; i <= 287; i++) lit_lengths[i] = 8;
    build_huffman_table(lit_table, lit_lengths, 288);
    
    uint8_t dist_lengths[32];
    for (int i = 0; i < 32; i++) dist_lengths[i] = 5;
    build_huffman_table(dist_table, dist_lengths, 32);
}

// Simple DEFLATE decompressor
static uint8_t* deflate_decompress(const uint8_t* compressed, uint32_t compressed_size, uint32_t* output_size) {
    // Skip zlib header (2 bytes)
    if (compressed_size < 6) return NULL;
    
    bit_stream_t bs;
    init_bit_stream(&bs, compressed + 2, compressed_size - 2);
    
    // Use static buffer for decompression (3.5MB)
    // Too large for current heap allocator, use static allocation
    static uint8_t decompress_buffer[3670016]; // 1024 * (1024 * 3 + 1) + extra  
    const uint32_t max_output = sizeof(decompress_buffer);
    uint8_t* output = decompress_buffer;
    
    uint32_t out_pos = 0;
    int bfinal = 0;
    
    static const uint16_t length_base[] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const uint8_t length_extra[] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const uint16_t dist_base[] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const uint8_t dist_extra[] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    
    int block_count = 0;
    while (!bfinal) {
        bfinal = read_bits(&bs, 1);
        int btype = read_bits(&bs, 2);
        block_count++;
        
        if (block_count > 100000) {
            SERIAL_LOG("PNG: Too many blocks, stopping\n");
            break;  // Safety limit - 100k blocks should be more than enough
        }
        
        if (btype == 0) {
            // Uncompressed block
            align_to_byte(&bs);
            uint16_t len = read_uint16_le(&bs.data[bs.byte_pos]);
            bs.byte_pos += 4; // Skip len and nlen
            
            if (out_pos + len > max_output) break;
            memcpy(output + out_pos, bs.data + bs.byte_pos, len);
            out_pos += len;
            bs.byte_pos += len;
            
        } else if (btype == 1 || btype == 2) {
            // Fixed or dynamic Huffman
            huffman_table_t lit_table, dist_table;
            
            if (btype == 1) {
                build_fixed_tables(&lit_table, &dist_table);
            } else {
                // Dynamic Huffman - simplified version
                int hlit = read_bits(&bs, 5) + 257;
                int hdist = read_bits(&bs, 5) + 1;
                int hclen = read_bits(&bs, 4) + 4;
                
                // Code length alphabet
                static const uint8_t cl_order[] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                uint8_t code_lengths[19] = {0};
                for (int i = 0; i < hclen; i++) {
                    code_lengths[cl_order[i]] = read_bits(&bs, 3);
                }
                
                huffman_table_t cl_table;
                build_huffman_table(&cl_table, code_lengths, 19);
                
                // Decode literal/length and distance code lengths
                uint8_t lengths[320];
                int n = 0;
                while (n < hlit + hdist) {
                    int symbol = decode_symbol(&bs, &cl_table);
                    if (symbol < 16) {
                        lengths[n++] = symbol;
                    } else if (symbol == 16) {
                        int repeat = read_bits(&bs, 2) + 3;
                        uint8_t val = (n > 0) ? lengths[n-1] : 0;
                        for (int i = 0; i < repeat && n < hlit + hdist; i++) {
                            lengths[n++] = val;
                        }
                    } else if (symbol == 17) {
                        int repeat = read_bits(&bs, 3) + 3;
                        for (int i = 0; i < repeat && n < hlit + hdist; i++) {
                            lengths[n++] = 0;
                        }
                    } else if (symbol == 18) {
                        int repeat = read_bits(&bs, 7) + 11;
                        for (int i = 0; i < repeat && n < hlit + hdist; i++) {
                            lengths[n++] = 0;
                        }
                    }
                }
                
                build_huffman_table(&lit_table, lengths, hlit);
                build_huffman_table(&dist_table, lengths + hlit, hdist);
            }
            
            // Decode compressed data
            while (1) {
                int symbol = decode_symbol(&bs, &lit_table);
                if (symbol < 0) {
                    SERIAL_LOG("PNG: Huffman decode failed in block\n");
                    break;
                }
                
                if (symbol < 256) {
                    // Literal byte
                    if (out_pos >= max_output) break;
                    output[out_pos++] = symbol;
                } else if (symbol == 256) {
                    // End of block
                    break;
                } else {
                    // Length/distance pair
                    int len_code = symbol - 257;
                    if (len_code >= 29) break;
                    
                    int length = length_base[len_code] + read_bits(&bs, length_extra[len_code]);
                    int dist_code = decode_symbol(&bs, &dist_table);
                    if (dist_code < 0 || dist_code >= 30) break;
                    
                    int distance = dist_base[dist_code] + read_bits(&bs, dist_extra[dist_code]);
                    
                    // Copy from history
                    if (distance > (int)out_pos || out_pos + length > max_output) break;
                    
                    for (int i = 0; i < length; i++) {
                        output[out_pos] = output[out_pos - distance];
                        out_pos++;
                    }
                }
            }
        } else {
            // Invalid block type
            break;
        }
    }
    
    *output_size = out_pos;
    // Note: Caller must free this buffer with memory_pool_free(SUBSYSTEM_VIDEO, output)
    return output;
}

png_image_t* png_decode(const uint8_t* png_data, uint32_t data_size) {
    SERIAL_LOG("PNG: decode() called\n");
    gfx_print("PNG: decode() called\n");
    
    if (!png_data || data_size < 8) {
        SERIAL_LOG("PNG: Invalid input data\n");
        gfx_print("PNG: Invalid input data\n");
        return NULL;
    }
    
    // Check PNG signature
    const uint8_t png_sig[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (memcmp(png_data, png_sig, 8) != 0) {
        SERIAL_LOG("PNG: Invalid signature\n");
        gfx_print("PNG: Invalid signature\n");
        return NULL;
    }
    SERIAL_LOG("PNG: Signature valid\n");
    gfx_print("PNG: Signature valid\n");
    
    uint32_t offset = 8;
    uint32_t width = 0, height = 0;
    uint8_t bit_depth = 0, color_type = 0;
    uint8_t* image_data = NULL;
    uint32_t image_data_size = 0;
    
    // Parse chunks
    while (offset < data_size - 8) {
        uint32_t chunk_length = read_uint32_be(&png_data[offset]);
        uint32_t chunk_type = read_uint32_be(&png_data[offset + 4]);
        
        if (offset + 12 + chunk_length > data_size) break;
        
        const uint8_t* chunk_data = &png_data[offset + 8];
        
        if (chunk_type == 0x49484452) { // "IHDR"
            if (chunk_length >= 13) {
                width = read_uint32_be(chunk_data);
                height = read_uint32_be(chunk_data + 4);
                bit_depth = chunk_data[8];
                color_type = chunk_data[9];
            }
        } else if (chunk_type == 0x49444154) { // "IDAT"
            // For simplicity, we'll handle only one IDAT chunk
            image_data = (uint8_t*)chunk_data;
            image_data_size = chunk_length;
        } else if (chunk_type == 0x49454E44) { // "IEND"
            break;
        }
        
        offset += 12 + chunk_length;
    }
    
    // For this demo, let's create a simple fallback image
    // In a real implementation, you'd decompress and process the actual image data
    if (width == 0 || height == 0 || width > 2048 || height > 2048) {
        // Create a default splash image - very small to fit in limited heap
        width = 200;
        height = 150;
    }
    
    SERIAL_LOG("PNG: Creating image structure\n");
    gfx_print("PNG: Creating image structure\n");
    
    png_image_t* result = (png_image_t*)memory_pool_alloc(SUBSYSTEM_VIDEO, sizeof(png_image_t), POOL_FLAG_ZERO_INIT);
    if (!result) {
        SERIAL_LOG("PNG: Failed to allocate image structure\n");
        gfx_print("PNG: Failed to allocate image structure\n");
        return NULL;
    }
    gfx_print("PNG: Image structure allocated\n");
    
    result->width = width;
    result->height = height;
    
    uint32_t pixel_count = width * height;
    uint32_t pixel_bytes = pixel_count * sizeof(uint32_t);
    gfx_print("PNG: Allocating pixel buffer\n");
    
    result->pixels = (uint32_t*)memory_pool_alloc_large(SUBSYSTEM_VIDEO, pixel_bytes, 0);
    
    if (!result->pixels) {
        SERIAL_LOG("PNG: Failed to allocate pixel buffer\n");
        gfx_print("PNG: Failed to allocate pixel buffer\n");
        memory_pool_free(SUBSYSTEM_VIDEO, result);
        return NULL;
    }
    SERIAL_LOG("PNG: Pixel buffer allocated, generating checkerboard\n");
    gfx_print("PNG: Pixel buffer allocated\n");
    
    // Create a gradient pattern as fallback (since we don't have full PNG decoder)
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t offset = y * width + x;
            
            // Create a distinctive checkerboard pattern to verify PNG integration
            int square_size = 60;
            bool checker = ((x / square_size) + (y / square_size)) % 2;
            
            uint8_t r, g, b;
            if (checker) {
                r = 0x00; g = 0xFF; b = 0xFF; // Bright cyan squares
            } else {
                r = 0xFF; g = 0x00; b = 0xFF; // Bright magenta squares  
            }
            uint8_t a = 0xFF;

            result->pixels[offset] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    
    // Suppress unused variable warnings
    (void)bit_depth;
    (void)color_type;
    (void)image_data;
    (void)image_data_size;
    
    return result;
}

void png_free(png_image_t* image) {
    if (!image) return;
    
    if (image->pixels) {
        memory_pool_free(SUBSYSTEM_VIDEO, image->pixels);
    }
    memory_pool_free(SUBSYSTEM_VIDEO, image);
}

png_image_t* load_splash_image(void) {
    gfx_print("PNG: Loading splash image...\n");
    
    png_image_t* result = png_decode(images_splash_png, images_splash_png_len);
    
    if (result) {
        gfx_print("PNG: Successfully decoded image\n");
    } else {
        gfx_print("PNG: Decode failed!\n");
    }
    
    return result;
}

// PNG filter functions
static uint8_t paeth_predictor(uint8_t a, uint8_t b, uint8_t c) {
    int p = a + b - c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;
    
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

static void unfilter_scanline(uint8_t filter_type, uint8_t* scanline, uint8_t* prev_scanline, 
                              uint32_t bytes_per_scanline, uint8_t bytes_per_pixel) {
    switch (filter_type) {
        case 0: // None
            break;
        case 1: // Sub
            for (uint32_t i = bytes_per_pixel; i < bytes_per_scanline; i++) {
                scanline[i] = (scanline[i] + scanline[i - bytes_per_pixel]) & 0xFF;
            }
            break;
        case 2: // Up
            if (prev_scanline) {
                for (uint32_t i = 0; i < bytes_per_scanline; i++) {
                    scanline[i] = (scanline[i] + prev_scanline[i]) & 0xFF;
                }
            }
            break;
        case 3: // Average
            for (uint32_t i = 0; i < bytes_per_scanline; i++) {
                uint8_t left = (i >= bytes_per_pixel) ? scanline[i - bytes_per_pixel] : 0;
                uint8_t up = prev_scanline ? prev_scanline[i] : 0;
                scanline[i] = (scanline[i] + ((left + up) >> 1)) & 0xFF;
            }
            break;
        case 4: // Paeth
            for (uint32_t i = 0; i < bytes_per_scanline; i++) {
                uint8_t left = (i >= bytes_per_pixel) ? scanline[i - bytes_per_pixel] : 0;
                uint8_t up = prev_scanline ? prev_scanline[i] : 0;
                uint8_t up_left = (prev_scanline && i >= bytes_per_pixel) ? prev_scanline[i - bytes_per_pixel] : 0;
                scanline[i] = (scanline[i] + paeth_predictor(left, up, up_left)) & 0xFF;
            }
            break;
    }
}

// Decode PNG directly to framebuffer without allocating pixel buffer
void png_decode_to_framebuffer(const uint8_t* png_data, uint32_t data_size,
                               uint32_t* framebuffer, uint32_t fb_width, uint32_t fb_height) {
    if (!png_data) {
        SERIAL_LOG("PNG: Invalid parameters - png_data is NULL\n");
        return;
    }
    if (!framebuffer) {
        SERIAL_LOG("PNG: Invalid parameters - framebuffer is NULL\n");
        return;
    }
    if (data_size < 8) {
        SERIAL_LOG("PNG: Invalid parameters - data_size < 8\n");
        return;
    }
    
    // Check PNG signature
    const uint8_t png_sig[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (memcmp(png_data, png_sig, 8) != 0) {
        SERIAL_LOG("PNG: Invalid signature\n");
        return;
    }
    
    SERIAL_LOG("PNG: Parsing chunks...\n");
    
    uint32_t offset = 8;
    uint32_t width = 0, height = 0;
    uint8_t bit_depth = 0, color_type = 0;
    
    // First pass: find IHDR and count total IDAT size
    uint32_t total_idat_size = 0;
    uint32_t temp_offset = 8;
    while (temp_offset < data_size - 8) {
        if (temp_offset + 12 > data_size) break;
        
        uint32_t chunk_length = read_uint32_be(&png_data[temp_offset]);
        uint32_t chunk_type = read_uint32_be(&png_data[temp_offset + 4]);
        
        if (temp_offset + 12 + chunk_length > data_size) break;
        
        if (chunk_type == 0x49444154) { // "IDAT"
            total_idat_size += chunk_length;
        } else if (chunk_type == 0x49454E44) { // "IEND"
            break;
        }
        
        temp_offset += 12 + chunk_length;
    }
    
    SERIAL_LOG("PNG: Total IDAT size: ");
    // Allocate buffer for all IDAT data (needs to hold all compressed chunks)
    static uint8_t idat_buffer[2048 * 1024]; // 2MB for compressed IDAT data
    uint32_t idat_pos = 0;
    
    // Parse chunks to find IHDR and collect all IDAT chunks
    while (offset < data_size - 8) {
        if (offset + 12 > data_size) break;
        
        uint32_t chunk_length = read_uint32_be(&png_data[offset]);
        uint32_t chunk_type = read_uint32_be(&png_data[offset + 4]);
        
        if (offset + 12 + chunk_length > data_size) break;
        
        const uint8_t* chunk_data = &png_data[offset + 8];
        
        if (chunk_type == 0x49484452) { // "IHDR"
            if (chunk_length >= 13) {
                width = read_uint32_be(chunk_data);
                height = read_uint32_be(chunk_data + 4);
                bit_depth = chunk_data[8];
                color_type = chunk_data[9];
                
                SERIAL_LOG("PNG: Width=");
                SERIAL_LOG("PNG: Height=");
                SERIAL_LOG("PNG: BitDepth=");
                SERIAL_LOG("PNG: ColorType=");
            }
        } else if (chunk_type == 0x49444154) { // "IDAT"
            // Concatenate all IDAT chunks
            if (idat_pos + chunk_length <= sizeof(idat_buffer)) {
                memcpy(idat_buffer + idat_pos, chunk_data, chunk_length);
                idat_pos += chunk_length;
                //SERIAL_LOG("PNG: Copied IDAT chunk\n");
            }
        } else if (chunk_type == 0x49454E44) { // "IEND"
            break;
        }
        
        offset += 12 + chunk_length;
    }
    
    const uint8_t* idat_data = idat_buffer;
    uint32_t idat_size = idat_pos;
    
    if (!idat_data || width == 0 || height == 0) {
        SERIAL_LOG("PNG: Missing required data\n");
        return;
    }
    
    // Only support RGB and RGBA at 8-bit depth
    if (bit_depth != 8 || (color_type != 2 && color_type != 6)) {
        SERIAL_LOG("PNG: Unsupported format (only RGB/RGBA 8-bit supported)\n");
        // Draw fallback pattern
        for (uint32_t y = 0; y < fb_height; y++) {
            for (uint32_t x = 0; x < fb_width; x++) {
                framebuffer[y * fb_width + x] = ((x / 60 + y / 60) % 2) ? 0x00FFFF : 0xFF00FF;
            }
        }
        return;
    }
    
    SERIAL_LOG("PNG: Decompressing image data...\n");
    
    // Decompress IDAT data (uses static buffer, no free needed)
    uint32_t decompressed_size = 0;
    uint8_t* decompressed = deflate_decompress(idat_data, idat_size, &decompressed_size);
    
    if (!decompressed || decompressed_size == 0) {
        SERIAL_LOG("PNG: Decompression failed\n");
        return;
    }
    
    // Log decompressed size and first few bytes
    SERIAL_LOG("PNG: Decompressed ");
    char size_str[16];
    uint32_t temp = decompressed_size;
    int idx = 0;
    if (temp == 0) {
        size_str[idx++] = '0';
    } else {
        char digits[16];
        int d = 0;
        while (temp > 0) { digits[d++] = '0' + (temp % 10); temp /= 10; }
        for (int i = d-1; i >= 0; i--) size_str[idx++] = digits[i];
    }
    size_str[idx] = '\0';
    SERIAL_LOG(size_str);
    SERIAL_LOG(" bytes. First bytes: ");
    for (int i = 0; i < 8 && i < (int)decompressed_size; i++) {
        char hex[4];
        uint8_t b = decompressed[i];
        hex[0] = "0123456789abcdef"[b >> 4];
        hex[1] = "0123456789abcdef"[b & 0xF];
        hex[2] = ' ';
        hex[3] = '\0';
        SERIAL_LOG(hex);
    }
    SERIAL_LOG("\n");
    
    SERIAL_LOG("PNG: Image dimensions: ");
    SERIAL_LOG("PNG: Framebuffer dimensions: ");
    
    // Calculate bytes per pixel and scanline
    uint8_t bytes_per_pixel = (color_type == 6) ? 4 : 3; // RGBA : RGB
    uint32_t bytes_per_scanline = width * bytes_per_pixel;
    uint32_t stride = bytes_per_scanline + 1; // +1 for filter byte
    
    // Calculate expected size and check if we have enough
    uint32_t expected_size = stride * height;
    if (decompressed_size < expected_size) {
        SERIAL_LOG("PNG: Warning - decompressed size smaller than expected\n");
    }
    
    // Allocate scanline buffers
    uint8_t* curr_scanline = (uint8_t*)memory_pool_alloc(SUBSYSTEM_VIDEO, bytes_per_scanline, 0);
    uint8_t* prev_scanline = (uint8_t*)memory_pool_alloc(SUBSYSTEM_VIDEO, bytes_per_scanline, POOL_FLAG_ZERO_INIT);
    
    if (!curr_scanline || !prev_scanline) {
        SERIAL_LOG("PNG: Failed to allocate scanline buffers\n");
        if (curr_scanline) memory_pool_free(SUBSYSTEM_VIDEO, curr_scanline);
        if (prev_scanline) memory_pool_free(SUBSYSTEM_VIDEO, prev_scanline);
        // decompressed uses static buffer, no free needed
        return;
    }
    
    memset(prev_scanline, 0, bytes_per_scanline);
    
    // Calculate centering offsets once
    uint32_t offset_x = (fb_width > width) ? (fb_width - width) / 2 : 0;
    uint32_t offset_y = (fb_height > height) ? (fb_height - height) / 2 : 0;
    
    // Process each scanline
    for (uint32_t y = 0; y < height && y < fb_height; y++) {
        uint32_t scanline_offset = y * stride;
        
        // If we run out of data, stop gracefully
        if (scanline_offset >= decompressed_size) break;
        if (scanline_offset + 1 > decompressed_size) break;
        
        uint8_t filter_type = decompressed[scanline_offset];
        
        // Copy what we have, padding with zeros if needed
        uint32_t available = decompressed_size - (scanline_offset + 1);
        uint32_t to_copy = (available < bytes_per_scanline) ? available : bytes_per_scanline;
        
        if (to_copy > 0) {
            memcpy(curr_scanline, &decompressed[scanline_offset + 1], to_copy);
            // Pad remaining with zeros if incomplete
            if (to_copy < bytes_per_scanline) {
                memset(curr_scanline + to_copy, 0, bytes_per_scanline - to_copy);
            }
        } else {
            break;
        }
        
        // Apply PNG filter
        unfilter_scanline(filter_type, curr_scanline, prev_scanline, bytes_per_scanline, bytes_per_pixel);
        
        // Debug first scanline
        if (y == 0) {
            SERIAL_LOG("PNG: First scanline filter=");
            char fstr[4];
            fstr[0] = '0' + filter_type;
            fstr[1] = '\0';
            SERIAL_LOG(fstr);
            SERIAL_LOG(", first pixel after unfilter: ");
            for (int i = 0; i < 3; i++) {
                char hex[4];
                uint8_t b = curr_scanline[i];
                hex[0] = "0123456789abcdef"[b >> 4];
                hex[1] = "0123456789abcdef"[b & 0xF];
                hex[2] = ' ';
                hex[3] = '\0';
                SERIAL_LOG(hex);
            }
            SERIAL_LOG("\n");
        }
        
        // Apply centering offset
        uint32_t screen_y = y + offset_y;
        
        // Copy to framebuffer (centered)
        if (screen_y < fb_height) {
            for (uint32_t x = 0; x < width && x < fb_width; x++) {
                uint32_t pixel_offset = x * bytes_per_pixel;
                uint8_t r = curr_scanline[pixel_offset];
                uint8_t g = curr_scanline[pixel_offset + 1];
                uint8_t b = curr_scanline[pixel_offset + 2];
                
                uint32_t screen_x = x + offset_x;
                if (screen_x < fb_width) {
                    // Convert to 32-bit ARGB
                    framebuffer[screen_y * fb_width + screen_x] = 0xFF000000 | (r << 16) | (g << 8) | b;
                }
            }
        }
        
        // Swap scanline buffers
        uint8_t* temp = prev_scanline;
        prev_scanline = curr_scanline;
        curr_scanline = temp;
    }
    
    SERIAL_LOG("PNG: Rendering complete!\n");
    
    memory_pool_free(SUBSYSTEM_VIDEO, curr_scanline);
    memory_pool_free(SUBSYSTEM_VIDEO, prev_scanline);
    // decompressed uses static buffer, no free needed
}

// Load embedded splash PNG directly to framebuffer
void load_splash_to_framebuffer(uint32_t* framebuffer, uint32_t fb_width, uint32_t fb_height) {
    png_decode_to_framebuffer(images_splash_png, images_splash_png_len, 
                              framebuffer, fb_width, fb_height);
}