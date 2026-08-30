#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct TermSize { int cols=80, rows=24; };

class Terminal {
public:
    int masterFd = -1;
    pid_t pid = -1;
    std::vector<std::string> lines;
    std::string curLine;
    int cursorX = 0, cursorY = 0;
    int cols = 80, rows = 24;
    bool active = false;

    Terminal();
    ~Terminal();
    bool spawn(int cols_, int rows_);
    void resize(int cols_, int rows_);
    void feed(const char* data, size_t len);
    void writeInput(const char* data, size_t len);
    void writeInputChar(char c);
    void poll(); // non-blocking read from pty
    void backspace();
    void enter();
};

char keycodeToChar(int keycode, bool shift);
