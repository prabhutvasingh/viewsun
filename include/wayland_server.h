#pragma once
#include "window.h"
#include <wayland-server.h>
#include "xdg-shell-server-protocol.h"

class WaylandServer {
public:
    wl_display *display = nullptr;
    wl_global *compositor = nullptr, *shm = nullptr, *xdg = nullptr;
    WindowManager *wm = nullptr;
    int sw=0, sh=0;
public:
    bool init(WindowManager *wm_, int sw_, int sh_);
    void shutdown();
    void poll(); // dispatch
    const char* socketName();
};
