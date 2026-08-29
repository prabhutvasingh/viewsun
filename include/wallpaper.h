#pragma once
#include <string>
#include <cstdint>

struct Wallpaper {
    uint32_t* pixels = nullptr; // ARGB32
    int w = 0, h = 0;
    bool loaded = false;
    ~Wallpaper() { free(); }
    bool load(const std::string &path);
    void free();
    void clear();
};

// draw wallpaper onto framebuffer with aspect-fill/center
void drawWallpaper(struct Framebuffer &fb, Wallpaper &wp, uint32_t fallback);
