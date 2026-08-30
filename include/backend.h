#pragma once
#include "common.h"
#include <vector>
#include <functional>

struct Framebuffer {
    uint32_t *pixels = nullptr;
    int width = 0, height = 0;
    int stride = 0; // pixels per line
};

enum class InputEventType { KeyDown, KeyUp, MouseMove, MouseButton, Text };
struct InputEvent {
    InputEventType type;
    int keycode = 0; // linux input.h KEY_*
    bool alt = false, shift = false, ctrl = false, super = false;
    int mx = 0, my = 0;
    int button = 0;
    char text[32] = {0};
};

// Abstract backend: DRM or SDL
class Backend {
public:
    virtual ~Backend() = default;
    virtual bool init(int width, int height) = 0;
    virtual void shutdown() = 0;
    virtual Framebuffer framebuffer() = 0;
    virtual void present() = 0;
    virtual bool pollEvent(InputEvent &ev) = 0;
    virtual void getScreenSize(int &w, int &h) = 0;
};

Backend* createDRMBackend(const std::string &card = "/dev/dri/card1");
Backend* createSDLBackend();
