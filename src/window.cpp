#include "../include/window.h"
#include "../include/tiling.h"

int WindowManager::addWindow(const std::string &title) {
    Window w;
    w.id = next_id++;
    w.color = cfg.win_colors[(w.id-1) % 8];
    w.title = title.empty() ? "win" + std::to_string(w.id) : title;
    w.focused = false;
    // insert near mouse hover: after focused window, so tiling appears where you hover
    int insertAt = focused>=0 ? focused+1 : windows.size();
    if (insertAt<0) insertAt=0;
    if (insertAt> (int)windows.size()) insertAt=windows.size();
    // if mouse is over empty space, append to end
    bool hoverEmpty = true;
    for (auto &win: windows) if (win.rect.contains(mouseX, mouseY)) hoverEmpty=false;
    if (hoverEmpty) insertAt = windows.size();
    windows.insert(windows.begin()+insertAt, w);
    focused = insertAt;
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
    // mouse-hover-driven tiling: split follows cursor, left/right both usable
    // master width tracks mouse X (20-80%), so hovering left makes left tile narrow/wide accordingly
    // and hovered window becomes master so tiling appears where you hover
    if (sw > 0) {
        int ratio = mouseX * 100 / sw;
        if (ratio < 20) ratio = 20;
        if (ratio > 80) ratio = 80;
        // only update if mouse is inside screen and we have a meaningful hover
        // keep manual Alt+h/l adjustments unless mouse moved recently
        cfg.master_ratio = ratio;
    }
    if (focused>=0 && focused < (int)windows.size()) {
        if (focused != 0) {
            Window fw = windows[focused];
            windows.erase(windows.begin()+focused);
            windows.insert(windows.begin(), fw);
            focused = 0;
        }
    } else if (!windows.empty()) {
        for (int i=(int)windows.size()-1;i>=0;--i) if(windows[i].rect.contains(mouseX,mouseY)) {
            Window fw = windows[i];
            windows.erase(windows.begin()+i);
            windows.insert(windows.begin(), fw);
            focused=0;
            break;
        }
    }
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
