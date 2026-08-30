#include "../include/renderer.h"
#include "../include/font8x8.h"
#include <algorithm>
#include <cstring>

void fill(Framebuffer &fb, uint32_t color) {
    for (int y=0;y<fb.height;y++) {
        uint32_t *row = fb.pixels + y*fb.stride;
        for (int x=0;x<fb.width;x++) row[x]=color;
    }
}

void drawRect(Framebuffer &fb, int x,int y,int w,int h, uint32_t color) {
    int x0 = std::max(x, 0), y0 = std::max(y, 0);
    int x1 = std::min(x+w, fb.width), y1 = std::min(y+h, fb.height);
    for (int yy=y0; yy<y1; yy++) {
        uint32_t *row = fb.pixels + yy*fb.stride;
        for (int xx=x0; xx<x1; xx++) row[xx]=color;
    }
}

void drawRectBorder(Framebuffer &fb, Rect r, uint32_t color, int border) {
    // top
    drawRect(fb, r.x, r.y, r.w, border, color);
    // bottom
    drawRect(fb, r.x, r.y+r.h-border, r.w, border, color);
    // left
    drawRect(fb, r.x, r.y, border, r.h, color);
    // right
    drawRect(fb, r.x+r.w-border, r.y, border, r.h, color);
}

static void drawChar(Framebuffer &fb, int x, int y, char c, uint32_t color) {
    if (c <0 || c>=128) return;
    // font8x8_basic from public domain VGA font, covers full ASCII 0-127
    auto &g = font8x8_basic[(int)c];
    // if undefined, use blank
    bool defined = false;
    for(int i=0;i<8;i++) if(g[i]) defined=true;
    if(!defined) return;
    for(int row=0;row<8;row++) {
        for(int col=0;col<8;col++) {
            if (g[row] & (1<<(7-col))) {
                int px = x+col, py = y+row;
                if(px>=0 && px<fb.width && py>=0 && py<fb.height) fb.pixels[py*fb.stride+px]=color;
            }
        }
    }
}

static void drawText(Framebuffer &fb, int x, int y, const std::string &s, uint32_t color) {
    for(size_t i=0;i<s.size();i++) drawChar(fb, x+i*8, y, s[i], color);
}

void drawCursor(Framebuffer &fb, int x, int y) {
    // simple arrow cursor 12x18
    uint32_t white = 0xFFFFFFFF, black = 0xFF000000;
    // shadow
    drawRect(fb, x+1, y+1, 10, 15, black);
    drawRect(fb, x, y, 10, 15, white);
    // arrow shape
    for(int i=0;i<10;i++) drawRect(fb, x+i, y+i, 2, 2, black);
}

void render(Framebuffer &fb, WindowManager &wm, const Config &cfg, Wallpaper &wp) {
    drawWallpaper(fb, wp, cfg.bg);
    for (auto &w: wm.windows) {
        uint32_t borderCol = w.focused ? cfg.border_focus : cfg.border_normal;
        // interior
        drawRect(fb, w.rect.x, w.rect.y, w.rect.w, w.rect.h, w.color);
        // inset to show border: draw border around, then darken interior border? Simple: border overlay
        drawRectBorder(fb, w.rect, borderCol, cfg.border);
        // title bar (top 16px)
        uint32_t titleBg = w.focused ? 0xFF3C3836 : 0xFF32302F;
        drawRect(fb, w.rect.x, w.rect.y, w.rect.w, 16, titleBg);
        drawText(fb, w.rect.x+4, w.rect.y+4, w.title, 0xFFEBDBB2);
        // inner border for title
        drawRectBorder(fb, {w.rect.x, w.rect.y, w.rect.w, 16}, borderCol, 1);
    }
    // status line bottom
    std::string status = " LAYOUT: ";
    switch(wm.layout){case Layout::MasterStack: status+="MASTER"; break; case Layout::BSP: status+="BSP"; break; case Layout::Grid: status+="GRID"; break;}
    status += " | WINS: " + std::to_string(wm.windows.size());
    status += " | MASTER:" + std::to_string(cfg.master_ratio) + "%";
    status += " | Alt+Enter new  Alt+q close  Super+P logout  Click focus  Alt+hjkl focus/resize  Alt+mbg layout";
    // crude status bg
    drawRect(fb, 0, fb.height-18, fb.width, 18, 0xFF1D2021);
    drawText(fb, 8, fb.height-14, status, 0xFFA89984);
    // cursor on top
    drawCursor(fb, wm.mouseX, wm.mouseY);
}
