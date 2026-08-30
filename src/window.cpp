#include "../include/window.h"
#include "../include/tiling.h"
#include <filesystem>
#include <algorithm>

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
        windows.insert(windows.begin()+insertAt, std::move(w));
        cfg.master_count++;
        if (cfg.master_count > (int)windows.size()) cfg.master_count = windows.size();
        focused = insertAt;
    } else {
        insertAt = windows.size();
        windows.insert(windows.begin()+insertAt, std::move(w));
        focused = insertAt;
    }
    for (auto &win: windows) win.focused = false;
    windows[focused].focused = true;
    return w.id;
}

int WindowManager::addTerminal() {
    Window w;
    w.id = next_id++;
    w.color = 0xFF1D2021;
    w.title = "term" + std::to_string(w.id);
    w.focused = false;
    w.type = WinType::Terminal;
    w.isTerm = true;
    w.term = std::make_unique<Terminal>();
    int cols = 80, rows = 24;
    if (lastSw>0 && lastSh>0) { cols = (lastSw/2) / 8; rows = (lastSh/3) / 8; if (cols<20) cols=80; if (rows<5) rows=24; }
    if (!w.term->spawn(cols, rows)) {
        // spawn failed, fallback to placeholder
        w.isTerm = false;
        w.type = WinType::Placeholder;
        w.term.reset();
    }
    bool left = (lastSw > 0 && mouseX < lastSw/2);
    int insertAt = left ? std::min(cfg.master_count, (int)windows.size()) : windows.size();
    windows.insert(windows.begin()+insertAt, std::move(w));
    if (left) cfg.master_count++;
    if (cfg.master_count > (int)windows.size()) cfg.master_count = windows.size();
    focused = insertAt;
    for (auto &win: windows) win.focused = false;
    windows[focused].focused = true;
    return windows[focused].id;
}

static std::vector<std::string> listDir(const std::string &path) {
    std::vector<std::string> out;
    try {
        for (auto &e: std::filesystem::directory_iterator(path)) {
            std::string name = e.path().filename().string();
            if (e.is_directory()) name += "/";
            out.push_back(name);
        }
    } catch(...) {}
    std::sort(out.begin(), out.end());
    if (out.empty()) out.push_back("(empty)");
    return out;
}

int WindowManager::addBrowser() {
    Window w;
    w.id = next_id++;
    w.color = 0xFF2A4A6B;
    w.title = "browser" + std::to_string(w.id);
    w.type = WinType::Browser;
    w.url = "https://google.com";
    w.focused=false;
    bool left = (lastSw > 0 && mouseX < lastSw/2);
    int insertAt = left ? std::min(cfg.master_count, (int)windows.size()) : windows.size();
    windows.insert(windows.begin()+insertAt, std::move(w));
    if (left) cfg.master_count++;
    if (cfg.master_count > (int)windows.size()) cfg.master_count = windows.size();
    focused = insertAt;
    for (auto &win: windows) win.focused = false;
    windows[focused].focused = true;
    return windows[focused].id;
}

int WindowManager::addFileManager() {
    Window w;
    w.id = next_id++;
    w.color = 0xFF3A3A2A;
    w.title = "files" + std::to_string(w.id);
    w.type = WinType::FileManager;
    const char* home = getenv("HOME");
    w.fmPath = home ? home : "/";
    w.fmFiles = listDir(w.fmPath);
    w.focused=false;
    bool left = (lastSw > 0 && mouseX < lastSw/2);
    int insertAt = left ? std::min(cfg.master_count, (int)windows.size()) : windows.size();
    windows.insert(windows.begin()+insertAt, std::move(w));
    if (left) cfg.master_count++;
    if (cfg.master_count > (int)windows.size()) cfg.master_count = windows.size();
    focused = insertAt;
    for (auto &win: windows) win.focused = false;
    windows[focused].focused = true;
    return windows[focused].id;
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
    // resize terms to new rect - safe guard null/dangling
    for (auto &w: windows) if (w.isTerm && w.term && w.term->masterFd>=0) {
        // ensure rect valid before resize
        if (w.rect.w < 16 || w.rect.h < 24) continue;
        int cols = (w.rect.w - 4) / 8;
        int rows = (w.rect.h - 20) / 8;
        if (cols<10) cols=10;
        if (rows<2) rows=2;
        if (cols!=w.term->cols || rows!=w.term->rows) {
            // protect against use-after-move corruption
            if (w.term) w.term->resize(cols, rows);
        }
    }
}

Window* WindowManager::getFocused() {
    if (focused<0 || focused >= (int)windows.size()) return nullptr;
    return &windows[focused];
}
