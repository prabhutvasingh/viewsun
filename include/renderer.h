#pragma once
#include "backend.h"
#include "window.h"
#include "wallpaper.h"

void render(Framebuffer &fb, WindowManager &wm, const Config &cfg, Wallpaper &wp);
void drawRect(Framebuffer &fb, int x,int y,int w,int h, uint32_t color);
void drawRectBorder(Framebuffer &fb, Rect r, uint32_t color, int border);
void fill(Framebuffer &fb, uint32_t color);
