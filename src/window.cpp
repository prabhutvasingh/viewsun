#include "../include/window.h"
#include "../include/tiling.h"

int WindowManager::addWindow(const std::string &title) {
    Window w;
    w.id = next_id++;
    w.color = cfg.win_colors[(w.id-1) % 8];
    w.title = title.empty() ? "win" + std::to_string(w.id) : title;
    w.focused = false;
    windows.push_back(w);
    focused = windows.size()-1;
    for (auto &win: windows) win.focused = false;
    windows[focused].focused = true;
    return w.id;
}

void WindowManager::removeFocused() {
    if (windows.empty() || focused <0 || focused >= (int)windows.size()) return;
    windows.erase(windows.begin()+focused);
    if (windows.empty()) focused = -1;
    else {
        if (focused >= (int)windows.size()) focused = windows.size()-1;
        for (auto &w: windows) w.focused = false;
        windows[focused].focused = true;
    }
}

void WindowManager::focusNext(int dir) {
    if (windows.empty()) return;
    windows[focused].focused = false;
    focused = (focused + dir + windows.size()) % windows.size();
    windows[focused].focused = true;
}

void WindowManager::setLayout(Layout l) { layout = l; }

void WindowManager::resizeMaster(int delta) {
    cfg.master_ratio += delta;
    if (cfg.master_ratio < 10) cfg.master_ratio = 10;
    if (cfg.master_ratio > 90) cfg.master_ratio = 90;
}

void WindowManager::tile(int sw, int sh) {
    if (windows.empty()) return;
    switch(layout) {
        case Layout::MasterStack: tileMasterStack(windows, sw, sh, cfg); break;
        case Layout::BSP: tileBSP(windows, sw, sh, cfg); break;
        case Layout::Grid: tileGrid(windows, sw, sh, cfg); break;
    }
    for (auto &w: windows) w.focused = false;
    if (focused>=0 && focused < (int)windows.size()) windows[focused].focused = true;
}

Window* WindowManager::getFocused() {
    if (focused<0 || focused >= (int)windows.size()) return nullptr;
    return &windows[focused];
}
