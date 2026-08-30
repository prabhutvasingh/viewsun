#pragma once
#include <cstdint>
#include <string>
#include <cmath>

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
    // true RGB - generate via HSV, not 8 colour limit
    static uint32_t trueColor(int id) {
        // golden ratio hue for distinct true RGB
        float h = fmod(id * 137.508f, 360.0f);
        float s = 0.6f + fmod(id*0.13f, 0.3f);
        float v = 0.85f;
        float c = v*s; float x = c*(1 - fabs(fmod(h/60.0f,2)-1)); float m=v-c;
        float r=0,g=0,b=0;
        if(h<60){r=c;g=x;} else if(h<120){r=x;g=c;} else if(h<180){g=c;b=x;} else if(h<240){g=x;b=c;} else if(h<300){r=x;b=c;} else {r=c;b=x;}
        uint8_t R=(r+m)*255, G=(g+m)*255, B=(b+m)*255;
        return (0xFFu<<24)|(R<<16)|(G<<8)|B;
    }
    std::string wallpaper_path = "";
    std::string backend = "auto"; // auto, drm, sdl
    std::string drm_card = "/dev/dri/card1";
};
