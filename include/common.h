#pragma once
#include <cstdint>
#include <string>

#define VIEWSUN_VERSION "0.1.0"

struct Rect {
    int x = 0, y = 0;
    int w = 0, h = 0;
    bool contains(int px, int py) const { return px >= x && px < x+w && py >= y && py < y+h; }
};

enum class Layout { MasterStack, BSP, Grid };

struct Config {
    int gap = 8;
    int border = 2;
    int master_ratio = 60; // percent
    int master_count = 1;
    uint32_t bg = 0xFF282828;
    uint32_t border_focus = 0xFF458588;
    uint32_t border_normal = 0xFF504945;
    uint32_t win_colors[8] = {0xFFCC241D,0xFF98971A,0xFFD79921,0xFF458588,0xFFB16286,0xFF689D6A,0xFFA89984,0xFF928374};
    std::string wallpaper_path = "";
    std::string backend = "auto"; // auto, drm, sdl
    std::string drm_card = "/dev/dri/card1";
};
