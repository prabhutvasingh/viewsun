#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image.h"
#include "../include/wallpaper.h"
#include "../include/backend.h"
#include <cstdio>
#include <algorithm>
#include <cstring>

void Wallpaper::free() {
    if (pixels) { stbi_image_free(pixels); pixels = nullptr; } // actually we allocate separate, but use stbi free for now - we'll copy
    // we allocate with new, so handle both: if loaded via stb, pixels points to stb allocation; free with stbi_image_free
    // to avoid double confusion, we will always use stbi_image_free
    w = h = 0;
    loaded = false;
}

void Wallpaper::clear() { free(); }

bool Wallpaper::load(const std::string &path) {
    free();
    int ch;
    int rw, rh;
    unsigned char *data = stbi_load(path.c_str(), &rw, &rh, &ch, 4);
    if (!data) {
        fprintf(stderr, "viewsun: failed to load wallpaper '%s': %s\n", path.c_str(), stbi_failure_reason());
        return false;
    }
    // stb gives RGBA, we need ARGB/BGRA? Our framebuffer is XRGB 0xFFRRGGBB as uint32_t little endian is BGRA in memory? Let's store as 0xAARRGGBB and write directly assuming same format.
    // Convert RGBA -> 0xFFRRGGBB (ignore alpha)
    w = rw; h = rh;
    pixels = (uint32_t*)malloc(w*h*4);
    if (!pixels) { stbi_image_free(data); return false; }
    for (int i=0;i<w*h;i++) {
        uint8_t r = data[i*4+0];
        uint8_t g = data[i*4+1];
        uint8_t b = data[i*4+2];
        uint8_t a = data[i*4+3];
        // pack as 0xAABBGGRR? No, for little endian uint32_t 0xAARRGGBB when viewed as bytes is BB GG RR AA.
        // Our renderer writes 0xFFRRGGBB expecting that reinterpreted as little endian will be B,G,R,0xFF in memory which matches DRM XRGB.
        // DRM dumb buffer is XRGB8888: [B][G][R][X] in memory. So 0xFFRRGGBB is correct.
        pixels[i] = (0xFFu<<24) | (r<<16) | (g<<8) | b;
        (void)a;
    }
    stbi_image_free(data);
    loaded = true;
    printf("viewsun: wallpaper loaded %s (%dx%d)\n", path.c_str(), w, h);
    return true;
}

void drawWallpaper(Framebuffer &fb, Wallpaper &wp, uint32_t fallback) {
    if (!wp.loaded || !wp.pixels) {
        // fallback solid
        for (int y=0;y<fb.height;y++) {
            uint32_t *row = fb.pixels + y*fb.stride;
            for (int x=0;x<fb.width;x++) row[x]=fallback;
        }
        return;
    }
    // aspect fill: scale to cover framebuffer, center crop
    // compute scale
    float scale = std::max((float)fb.width / wp.w, (float)fb.height / wp.h);
    int sw = (int)(wp.w * scale);
    int sh = (int)(wp.h * scale);
    int ox = (fb.width - sw)/2;
    int oy = (fb.height - sh)/2;

    for (int y=0; y<fb.height; y++) {
        uint32_t *row = fb.pixels + y*fb.stride;
        for (int x=0; x<fb.width; x++) {
            // map fb (x,y) -> wallpaper (sx,sy)
            float sx = (x - ox) / scale;
            float sy = (y - oy) / scale;
            int ix = (int)sx;
            int iy = (int)sy;
            if (ix<0 || iy<0 || ix>=wp.w || iy>=wp.h) {
                row[x] = fallback;
            } else {
                // nearest neighbor (fast). Could do bilinear but ok for v0.1
                row[x] = wp.pixels[iy*wp.w + ix];
            }
        }
    }
}
