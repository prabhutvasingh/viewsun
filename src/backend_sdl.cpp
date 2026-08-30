#include "../include/backend.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <linux/input-event-codes.h>

class SDLBackend : public Backend {
    SDL_Window *win = nullptr;
    SDL_Surface *surf = nullptr;
    int W=0,H=0;
    Framebuffer fb{};
public:
    bool init(int w, int h) override {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr,"SDL_Init: %s\n", SDL_GetError()); return false; }
        W=w; H=h;
        win = SDL_CreateWindow("viewsun", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_SHOWN);
        if (!win) { fprintf(stderr,"SDL_CreateWindow: %s\n", SDL_GetError()); return false; }
        surf = SDL_GetWindowSurface(win);
        if (!surf) { fprintf(stderr,"SDL_GetWindowSurface: %s\n", SDL_GetError()); return false; }
        fb.pixels = (uint32_t*)surf->pixels;
        fb.width = surf->w;
        fb.height = surf->h;
        fb.stride = surf->pitch / 4;
        return true;
    }
    void shutdown() override {
        if (win) SDL_DestroyWindow(win);
        SDL_Quit();
        win=nullptr; surf=nullptr;
    }
    Framebuffer framebuffer() override { return fb; }
    void present() override {
        SDL_UpdateWindowSurface(win);
        // re-acquire in case resized (not handled)
        fb.pixels = (uint32_t*)surf->pixels;
    }
    void getScreenSize(int &w,int &h) override { w=W; h=H; }
    bool pollEvent(InputEvent &ev) override {
        SDL_Event e;
        if (!SDL_PollEvent(&e)) return false;
        if (e.type==SDL_QUIT) { ev.type=InputEventType::KeyDown; ev.keycode=KEY_Q; ev.alt=true; ev.shift=true; return true; }
        if (e.type==SDL_MOUSEMOTION) {
            ev.type=InputEventType::MouseMove;
            ev.mx=e.motion.x; ev.my=e.motion.y;
            ev.alt=(SDL_GetModState() & KMOD_ALT); ev.shift=(SDL_GetModState() & KMOD_SHIFT);
            ev.ctrl=(SDL_GetModState() & KMOD_CTRL); ev.super=(SDL_GetModState() & KMOD_GUI);
            return true;
        }
        if (e.type==SDL_MOUSEBUTTONDOWN) {
            ev.type=InputEventType::MouseButton;
            ev.button=e.button.button;
            ev.mx=e.button.x; ev.my=e.button.y;
            ev.alt=(SDL_GetModState() & KMOD_ALT); ev.shift=(SDL_GetModState() & KMOD_SHIFT);
            ev.ctrl=(SDL_GetModState() & KMOD_CTRL); ev.super=(SDL_GetModState() & KMOD_GUI);
            return true;
        }
        if (e.type==SDL_KEYDOWN || e.type==SDL_KEYUP) {
            ev.type = (e.type==SDL_KEYDOWN)?InputEventType::KeyDown:InputEventType::KeyUp;
            bool alt = (SDL_GetModState() & KMOD_ALT);
            bool shift = (SDL_GetModState() & KMOD_SHIFT);
            bool ctrl = (SDL_GetModState() & KMOD_CTRL);
            bool super = (SDL_GetModState() & KMOD_GUI);
            ev.alt=alt; ev.shift=shift; ev.ctrl=ctrl; ev.super=super;
            SDL_Keycode k = e.key.keysym.sym;
            // map to linux KEY_ codes for shared logic
            switch(k) {
                case SDLK_RETURN: ev.keycode=KEY_ENTER; break;
                case SDLK_q: ev.keycode=KEY_Q; break;
                case SDLK_j: ev.keycode=KEY_J; break;
                case SDLK_k: ev.keycode=KEY_K; break;
                case SDLK_h: ev.keycode=KEY_H; break;
                case SDLK_l: ev.keycode=KEY_L; break;
                case SDLK_m: ev.keycode=KEY_M; break;
                case SDLK_b: ev.keycode=KEY_B; break;
                case SDLK_g: ev.keycode=KEY_G; break;
                case SDLK_f: ev.keycode=KEY_F; break;
                case SDLK_p: ev.keycode=KEY_P; break;
                default: ev.keycode=0; break;
            }
            if(ev.type==InputEventType::KeyUp) return false; // ignore keyup for SDL
            return true;
        }
        return false;
    }
};

Backend* createSDLBackend() { return new SDLBackend(); }
