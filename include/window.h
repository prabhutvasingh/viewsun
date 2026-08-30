#pragma once
#include "common.h"
#include <vector>

struct Window {
    int id;
    Rect rect;
    uint32_t color;
    bool focused = false;
    std::string title;
};

class WindowManager {
public:
    Config cfg;
    Layout layout = Layout::MasterStack;
    std::vector<Window> windows;
    int focused = -1;
    int next_id = 1;

    int addWindow(const std::string &title = "");
    void removeFocused();
    void focusNext(int dir);
    void focusAt(int x, int y);
    void setLayout(Layout l);
    void resizeMaster(int delta);
    void tile(int screenW, int screenH);
    Window* getFocused();
    int mouseX = 0, mouseY = 0;
};
