#pragma once
#include <cstdint>
#include <string>

// Viewsun custom protocol - minimal, no Wayland
// Messages over UNIX socket, fd via SCM_RIGHTS

enum class ViewsunOpcode : uint32_t {
    CreateWindow = 1,
    WindowCreated = 2,
    CreateBuffer = 3,
    Damage = 4,
    SetTitle = 5,
    DestroyWindow = 6,
    Frame = 7,
    InputKey = 8,
    InputButton = 9,
    InputMotion = 10,
    Ping = 11,
    Pong = 12,
};

struct ViewsunHeader {
    ViewsunOpcode opcode;
    uint32_t size; // payload size after header
};

struct ViewsunCreateWindow {
    uint32_t width, height;
    char title[128];
};

struct ViewsunWindowCreated {
    uint32_t id;
};

struct ViewsunCreateBuffer {
    uint32_t window_id;
    uint32_t width, height, stride;
    // fd passed via SCM_RIGHTS ancillary, not in payload
};

struct ViewsunDamage {
    uint32_t window_id;
    int32_t x, y;
    uint32_t width, height;
};

struct ViewsunSetTitle {
    uint32_t window_id;
    char title[128];
};

struct ViewsunDestroyWindow {
    uint32_t window_id;
};

struct ViewsunInputKey {
    uint32_t keycode, state, mod;
};

struct ViewsunInputButton {
    uint32_t button, state;
    int32_t x, y;
};

struct ViewsunInputMotion {
    int32_t x, y;
};

#define VIEWSUN_SOCKET "/tmp/viewsun-0"
#define VIEWUN_MAX_TITLE 128
