#pragma once
#include "common.h"
#include "terminal.h"
#include <vector>
#include <memory>

enum class WinType { Placeholder, Terminal, Browser, FileManager };

struct Window {
    int id;
    Rect rect;
    uint32_t color;
    bool focused = false;
    std::string title;
    WinType type = WinType::Placeholder;
    bool isTerm = false;
    std::unique_ptr<Terminal> term;
    // browser
    std::string url = "https://google.com";
    // file manager
    std::string fmPath;
    std::vector<std::string> fmFiles;
    int fmScroll = 0;
};

class WindowManager {
public:
    Config cfg;
    Layout layout = Layout::MasterStack;
    std::vector<Window> windows;
    int focused = -1;
    int next_id = 1;

    int addWindow(const std::string &title = "");
    int addTerminal(); // spawns PTY term inside window
    int addBrowser(); // internal browser placeholder
    int addFileManager(); // internal file manager
    void removeFocused();
    void focusNext(int dir);
    void focusAt(int x, int y);
    void setLayout(Layout l);
    void resizeMaster(int delta);
    void tile(int screenW, int screenH);
    Window* getFocused();
    int mouseX = 0, mouseY = 0;
    int lastSw = 0, lastSh = 0;
};
