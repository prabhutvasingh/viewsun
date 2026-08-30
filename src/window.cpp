#include "../include/window.h"
#include "../include/tiling.h"

int WindowManager::addWindow(const std::string &title) {
    Window w;
    w.id = next_id++;
    w.color = cfg.win_colors[(w.id-1) % 8];
    w.title = title.empty() ? "win" + std::to_string(w.id) : title;
    w.focused = false;
    // tile wherever mouse hovers: left half -> left master (stack left), right half -> right stack
    bool left = (lastSw > 0 && mouseX < lastSw/2);
    int insertAt;
    if (left) {
        // insert into master area - grow left stack
        insertAt = cfg.master_count; // after existing left tiles
        if (insertAt > (int)windows.size()) insertAt = windows.size();
        windows.insert(windows.begin()+insertAt, w);
        cfg.master_count++;
        if (cfg.master_count > (int)windows.size()) cfg.master_count = windows.size();
        focused = insertAt;
    } else {
        insertAt = windows.size();
        windows.insert(windows.begin()+insertAt, w);
        focused = insertAt;
    }
    for (auto &win: windows) win.focused = false;
    windows[focused].focused = true;
    return w.id;
}

void WindowManager::removeFocused() {
    if (windows.empty() || focused <0 || focused >= (int)windows.size()) return;
    bool wasMaster = focused < cfg.master_count;
    windows.erase(windows.begin()+focused);
    if (wasMaster && cfg.master_count > 1) cfg.master_count--;
    if (cfg.master_count > (int)windows.size()) cfg.master_count = windows.size();
    if (cfg.master_count < 1 && !windows.empty()) cfg.master_count = 1;
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

void WindowManager::focusAt(int x, int y) {
    mouseX = x; mouseY = y;
    if (windows.empty()) return;
    for (int i = (int)windows.size()-1; i>=0; --i) {
        if (windows[i].rect.contains(x,y)) {
            for (auto &w: windows) w.focused = false;
            focused = i;
            windows[focused].focused = true;
            return;
        }
    }
}

void WindowManager::setLayout(Layout l) { layout = l; }

void WindowManager::resizeMaster(int delta) {
    cfg.master_ratio += delta;
    if (cfg.master_ratio < 10) cfg.master_ratio = 10;
    if (cfg.master_ratio > 90) cfg.master_ratio = 90;
}

void WindowManager::tile(int sw, int sh) {
    if (windows.empty()) return;
    lastSw = sw; lastSh = sh;
    // keep tiling as-is, do not auto-resize on hover - resizing was the bug
    // left/right placement is handled in addWindow via mouseX < sw/2
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
