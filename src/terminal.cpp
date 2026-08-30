#include "../include/terminal.h"
#include <unistd.h>
#include <pty.h>
#include <utmp.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <linux/input-event-codes.h>

Terminal::Terminal() {}
Terminal::~Terminal() {
    if (masterFd>=0) close(masterFd);
    if (pid>0) kill(pid, SIGHUP);
}

bool Terminal::spawn(int cols_, int rows_) {
    cols=cols_; rows=rows_;
    struct winsize ws{}; ws.ws_col=cols; ws.ws_row=rows;
    pid = forkpty(&masterFd, nullptr, nullptr, &ws);
    if (pid<0) return false;
    if (pid==0) {
        const char* shell = getenv("SHELL");
        if (!shell) shell="/bin/bash";
        // unset env that would break
        execl(shell, shell, (char*)nullptr);
        _exit(127);
    }
    // parent
    int flags = fcntl(masterFd, F_GETFL, 0);
    fcntl(masterFd, F_SETFL, flags | O_NONBLOCK);
    active=true;
    lines.clear(); curLine.clear();
    lines.push_back("");
    return true;
}

void Terminal::resize(int cols_, int rows_) {
    cols=cols_; rows=rows_;
    if (masterFd>=0) {
        struct winsize ws{}; ws.ws_col=cols; ws.ws_row=rows;
        ioctl(masterFd, TIOCSWINSZ, &ws);
        kill(pid, SIGWINCH);
    }
}

void Terminal::feed(const char* data, size_t len) {
    for(size_t i=0;i<len;i++) {
        char c=data[i];
        if (c=='\r') continue;
        if (c=='\n') {
            lines.push_back(curLine);
            curLine.clear();
            cursorX=0;
            cursorY = lines.size()-1;
            if ((int)lines.size() > rows+100) { // scroll limit
                lines.erase(lines.begin());
                cursorY--;
            }
        } else if (c=='\b' || c==127) {
            if (!curLine.empty()) curLine.pop_back();
        } else if (c>=32 && c<127) {
            curLine.push_back(c);
            if ((int)curLine.size() >= cols) {
                lines.push_back(curLine);
                curLine.clear();
            }
        } else if (c=='\t') {
            curLine += "    ";
        }
        // ignore CSI sequences naively: if ESC, skip until letter
        if (c=='\x1b' && i+1<len && data[i+1]=='[') {
            i+=2;
            while(i<len && data[i]!='m' && data[i]!='H' && data[i]!='J' && data[i]!='K' && data[i]!='h' && data[i]!='l') i++;
        }
    }
}

void Terminal::poll() {
    if (masterFd<0) return;
    char buf[4096];
    ssize_t n;
    while ((n=read(masterFd, buf, sizeof(buf)))>0) {
        feed(buf, n);
    }
}

void Terminal::writeInput(const char* data, size_t len) {
    if (masterFd>=0) write(masterFd, data, len);
}
void Terminal::writeInputChar(char c) { writeInput(&c,1); }
void Terminal::backspace() { writeInput("\x7f",1); }
void Terminal::enter() { writeInput("\n",1); }

char keycodeToChar(int keycode, bool shift) {
    // map linux KEY_* to ASCII, simplified
    if (keycode>=KEY_A && keycode<=KEY_Z) {
        char c = 'a' + (keycode-KEY_A);
        if (shift) c = 'A' + (keycode-KEY_A);
        return c;
    }
    if (keycode>=KEY_1 && keycode<=KEY_9) {
        const char* noShift="123456789";
        const char* shifted="!@#$%^&*(";
        int idx=keycode-KEY_1;
        return shift?shifted[idx]:noShift[idx];
    }
    if (keycode==KEY_0) return shift?')':'0';
    if (keycode==KEY_SPACE) return ' ';
    if (keycode==KEY_DOT) return shift?'>':'.';
    if (keycode==KEY_COMMA) return shift?'<':',';
    if (keycode==KEY_MINUS) return shift?'_':'-';
    if (keycode==KEY_EQUAL) return shift?'+':'=';
    if (keycode==KEY_SLASH) return shift?'?':'/';
    if (keycode==KEY_SEMICOLON) return shift?':':';';
    if (keycode==KEY_APOSTROPHE) return shift?'"':'\'';
    if (keycode==KEY_LEFTBRACE) return shift?'{':'[';
    if (keycode==KEY_RIGHTBRACE) return shift?'}':']';
    if (keycode==KEY_BACKSLASH) return shift?'|':'\\';
    if (keycode==KEY_GRAVE) return shift?'~':'`';
    return 0;
}
