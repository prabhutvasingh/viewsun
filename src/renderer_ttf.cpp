#define STB_TRUETYPE_IMPLEMENTATION
#include "../include/stb_truetype.h"
#include "../include/renderer.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

static stbtt_fontinfo fontInfo;
static unsigned char* ttfBuffer = nullptr;
static bool ttfLoaded = false;
static float ttfScale = 0;
static int ttfAscent = 0, ttfDescent = 0, ttfLineGap = 0;

bool initTTF(const char* path, int pixelHeight) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f,0,SEEK_END);
    long sz = ftell(f);
    fseek(f,0,SEEK_SET);
    ttfBuffer = (unsigned char*)malloc(sz);
    fread(ttfBuffer,1,sz,f);
    fclose(f);
    if (!stbtt_InitFont(&fontInfo, ttfBuffer, 0)) { free(ttfBuffer); ttfBuffer=nullptr; return false; }
    ttfScale = stbtt_ScaleForPixelHeight(&fontInfo, pixelHeight);
    stbtt_GetFontVMetrics(&fontInfo, &ttfAscent, &ttfDescent, &ttfLineGap);
    ttfLoaded = true;
    return true;
}

static bool ensureTTF() {
    if (ttfLoaded) return true;
    // try JetBrainsMono Nerd Font Mono Regular as per user neofetch
    const char* candidates[] = {
        "/usr/share/fonts/TTF/JetBrainsMonoNerdFontMono-Regular.ttf",
        "/usr/share/fonts/TTF/JetBrainsMono-Regular.ttf",
        "/usr/share/fonts/JetBrainsMono-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/Hack-Regular.ttf",
        nullptr
    };
    for (int i=0;candidates[i];i++) if (initTTF(candidates[i], 13)) return true;
    return false;
}

// draw TTF char with alpha blending - returns 0 if fallback needed
int drawCharTTF(Framebuffer &fb, int x, int y, char c, uint32_t color) {
    if (!ensureTTF()) return 0;
    if ((unsigned char)c < 32 || (unsigned char)c >= 128) {
        return 0;
    }
    int w,h,xoff,yoff;
    unsigned char* bitmap = stbtt_GetCodepointBitmap(&fontInfo, 0, ttfScale, c, &w, &h, &xoff, &yoff);
    // yoff is relative to baseline, ascent is baseline distance
    int ascent = (int)(ttfAscent * ttfScale);
    int px = x + xoff;
    int py = y + ascent + yoff;
    // blend
    uint8_t sr = (color>>16)&0xFF, sg=(color>>8)&0xFF, sb=color&0xFF;
    for(int row=0;row<h;row++) for(int col=0;col<w;col++) {
        int alpha = bitmap[row*w+col];
        if(alpha==0) continue;
        int fx = px+col, fy = py+row;
        if(fx<0||fx>=fb.width||fy<0||fy>=fb.height) continue;
        uint32_t dst = fb.pixels[fy*fb.stride+fx];
        uint8_t dr=(dst>>16)&0xFF, dg=(dst>>8)&0xFF, db=dst&0xFF;
        uint8_t nr = (sr*alpha + dr*(255-alpha))/255;
        uint8_t ng = (sg*alpha + dg*(255-alpha))/255;
        uint8_t nb = (sb*alpha + db*(255-alpha))/255;
        fb.pixels[fy*fb.stride+fx] = (0xFFu<<24)|(nr<<16)|(ng<<8)|nb;
    }
    stbtt_FreeBitmap(bitmap, nullptr);
    int advance, lsb;
    stbtt_GetCodepointHMetrics(&fontInfo, c, &advance, &lsb);
    int adv = (int)(advance * ttfScale);
    if (adv<7) adv=7;
    return adv;
}

int textWidthTTF(const std::string &s) {
    if (!ensureTTF()) return s.size()*8;
    int w=0;
    for(char c: s) {
        int adv,lsb;
        stbtt_GetCodepointHMetrics(&fontInfo, c, &adv, &lsb);
        w += (int)(adv * ttfScale);
    }
    return w;
}
