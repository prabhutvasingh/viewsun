#include "../include/renderer.h"
#include "../include/font8x8.h"
#include <algorithm>
#include <cstring>
#include <ctime>
#include <cstdio>

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

static int drawChar(Framebuffer &fb, int x, int y, char c, uint32_t color) {
    // try TTF first - if font loaded, drawCharTTF handles drawing and returns advance
    // check if TTF available by trying to draw; if returns 0 means fallback
    int adv = drawCharTTF(fb, x, y, c, color);
    if (adv != 0) return adv;
    // fallback bitmap 8x8
    unsigned char uc = (unsigned char)c;
    if (uc>=128) return 8;
    auto &g = font8x8_basic[uc];
    for(int row=0;row<8;row++) {
        unsigned char bits = g[row];
        for(int col=0;col<8;col++) {
            if (bits & (1 << col)) {
                int px = x+col, py = y+row;
                if(px>=0 && px<fb.width && py>=0 && py<fb.height) fb.pixels[py*fb.stride+px]=color;
            }
        }
    }
    return 8;
}

static void drawText(Framebuffer &fb, int x, int y, const std::string &s, uint32_t color) {
    int curX = x;
    for(char c: s) curX += drawChar(fb, curX, y, c, color);
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
        // internal terminal buffer
        if (w.type==WinType::Terminal && w.term) {
            int rows = (w.rect.h - 20)/8;
            int y0 = w.rect.y + 18; int x0 = w.rect.x + 2;
            int start = 0;
            if ((int)w.term->lines.size() > rows-1) start = w.term->lines.size() - (rows-1);
            for (int r=0; r<rows-1 && (start+r) < (int)w.term->lines.size(); ++r) drawText(fb, x0, y0 + r*8, w.term->lines[start+r], 0xFFEBDBB2);
            std::string cur = w.term->curLine + (w.focused?"_":"");
            drawText(fb, x0, y0 + (rows-1)*8, cur, w.focused?0xFFA89984:0xFFEBDBB2);
        } else if (w.type==WinType::Browser) {
            int y0 = w.rect.y + 18; int x0 = w.rect.x + 4;
            drawText(fb, x0, y0, "URL: " + w.url, 0xFFEBDBB2);
            drawRect(fb, x0, y0+10, w.rect.w-8, 1, 0xFF665C54);
            drawText(fb, x0, y0+14, "[internal browser placeholder]", 0xFFA89984);
            drawText(fb, x0, y0+22, "Super+W spawns here, Enter to open", 0xFFBDAE93);
            drawText(fb, x0, y0+30, "external: xdg-open still available", 0xFF928374);
        } else if (w.type==WinType::FileManager) {
            int y0 = w.rect.y + 18; int x0 = w.rect.x + 4;
            drawText(fb, x0, y0, w.fmPath, 0xFFA89984);
            drawRect(fb, x0, y0+10, w.rect.w-8, 1, 0xFF665C54);
            int rows = (w.rect.h - 32)/8;
            for (int i=0;i<rows && i < (int)w.fmFiles.size(); ++i) {
                uint32_t col = (i==0 && w.focused)?0xFFFBF1C7:0xFFEBDBB2;
                std::string name = w.fmFiles[i];
                if ((int)name.size()> (w.rect.w-12)/8) name = name.substr(0,(w.rect.w-12)/8-3)+"...";
                drawText(fb, x0, y0+14+i*8, name, col);
            }
        }
    }
    // status line bottom with time/date
    char timebuf[64];
    time_t now = time(nullptr);
    struct tm *tm = localtime(&now);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %a %H:%M:%S", tm);
    std::string status = " LAYOUT: ";
    switch(wm.layout){case Layout::MasterStack: status+="MASTER"; break; case Layout::BSP: status+="BSP"; break; case Layout::Grid: status+="GRID"; break;}
    status += " | WINS: " + std::to_string(wm.windows.size());
    status += " | ";
    status += timebuf;
    status += " | Super+Enter kitty Super+W browser Super+E files Super+P logout";
    // crude status bg
    drawRect(fb, 0, fb.height-18, fb.width, 18, 0xFF1D2021);
    drawText(fb, 8, fb.height-14, status, 0xFFA89984);
    // right-align full date if space (fallback)
    // ensure time visible even on small widths by truncating status left
    if ((int)status.size()*8 > fb.width-16) {
        // truncate left part, keep time at end
        std::string shortStatus = std::string(timebuf) + " | WINS:" + std::to_string(wm.windows.size());
        drawRect(fb, 0, fb.height-18, fb.width, 18, 0xFF1D2021);
        drawText(fb, 8, fb.height-14, shortStatus, 0xFFA89984);
    }
    // cursor on top
    drawCursor(fb, wm.mouseX, wm.mouseY);
}
