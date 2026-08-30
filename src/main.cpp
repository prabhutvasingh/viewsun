#include "../include/backend.h"
#include "../include/window.h"
#include "../include/renderer.h"
#include "../include/wallpaper.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <linux/input-event-codes.h>
#include <chrono>
#include <thread>

static void print_help(const char* prog) {
    printf("viewsun %s - fully custom tiling compositor (no X11/Wayland)\n", VIEWSUN_VERSION);
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -w <path>            set wallpaper image (png/jpg/bmp)  (also: viewsun -w /path)\n");
    printf("  --wallpaper <path>   same as -w\n");
    printf("  --backend <drm|sdl|auto>  select backend (default auto: try drm, fall back to sdl)\n");
    printf("  --card <path>        DRM card path (default /dev/dri/card1, fallback card0)\n");
    printf("  --size WxH           force resolution (sdl) e.g. --size 1280x720\n");
    printf("  -h, --help           show this help\n");
    printf("  -v, --version        show version\n");
    printf("\nControls:\n");
    printf("  Alt+Enter (or Ctrl+Enter/Super+Enter, or Ctrl+n) spawn window\n");
    printf("  Alt+q (or Ctrl+q/Super+q, or Ctrl+c/x, or right-click) close\n");
    printf("  Alt+Shift+q quit  Super+P (Windows+P) logout\n");
    printf("  j/k focus  h/l resize master  m/b/g layouts (with Alt/Ctrl/Super, or plain)\n");
    printf("  Mouse: left-click focus, right-click close, move focuses\n");
    printf("\nExamples:\n");
    printf("  viewsun -w ~/wallpaper.jpg\n");
    printf("  viewsun --backend sdl -w /tmp/bg.png\n");
    printf("  sudo viewsun --backend drm -w /usr/share/viewsun/wallpaper.png\n");
}

static void print_version() { printf("viewsun %s\n", VIEWSUN_VERSION); }

int main(int argc, char** argv) {
    Config cfg;
    int forceW=0, forceH=0;

    for (int i=1;i<argc;i++) {
        std::string a = argv[i];
        if (a=="-h" || a=="--help") { print_help(argv[0]); return 0; }
        else if (a=="-v" || a=="--version") { print_version(); return 0; }
        else if (a=="-w" || a=="--wallpaper") {
            if (i+1>=argc) { fprintf(stderr,"viewsun: -w requires a path\n"); return 1; }
            cfg.wallpaper_path = argv[++i];
        } else if (a=="--backend") {
            if (i+1>=argc) { fprintf(stderr,"viewsun: --backend requires arg\n"); return 1; }
            cfg.backend = argv[++i];
        } else if (a=="--card") {
            if (i+1>=argc) { fprintf(stderr,"viewsun: --card requires path\n"); return 1; }
            cfg.drm_card = argv[++i];
            setenv("VIEWSUN_CARD", cfg.drm_card.c_str(), 1);
        } else if (a=="--size") {
            if (i+1>=argc) { fprintf(stderr,"viewsun: --size requires WxH\n"); return 1; }
            std::string s=argv[++i];
            sscanf(s.c_str(), "%dx%d", &forceW, &forceH);
        } else if (a[0]!='-' && cfg.wallpaper_path.empty()) {
            // allow positional: viewsun /path/to/wallpaper.png (convenience)
            // but only if file exists? treat as wallpaper if ends with image ext
            if (a.find(".png")!=std::string::npos || a.find(".jpg")!=std::string::npos || a.find(".jpeg")!=std::string::npos || a.find(".bmp")!=std::string::npos) {
                cfg.wallpaper_path = a;
            } else {
                fprintf(stderr,"viewsun: unknown argument '%s' (use -w for wallpaper)\n", a.c_str());
                print_help(argv[0]); return 1;
            }
        } else {
            fprintf(stderr,"viewsun: unknown argument '%s'\n", a.c_str());
            print_help(argv[0]); return 1;
        }
    }

    if (!cfg.drm_card.empty()) setenv("VIEWSUN_CARD", cfg.drm_card.c_str(), 1);

    // wallpaper
    Wallpaper wp;
    if (!cfg.wallpaper_path.empty()) {
        if (!wp.load(cfg.wallpaper_path)) {
            fprintf(stderr,"viewsun: continuing without wallpaper\n");
        }
    } else {
        // try default location
        const char* home = getenv("HOME");
        std::string def = home ? std::string(home)+"/.config/viewsun/wallpaper.png" : "";
        if (!def.empty() && access(def.c_str(), R_OK)==0) wp.load(def);
    }

    // backend selection
    Backend* backend = nullptr;
    auto tryDRM = [&]()->Backend* {
        Backend* b = createDRMBackend(cfg.drm_card);
        int w = forceW?forceW:0, h=forceH?forceH:0;
        if (b->init(w,h)) return b;
        delete b; return nullptr;
    };
    auto trySDL = [&]()->Backend* {
        Backend* b = createSDLBackend();
        int w = forceW?forceW:1280, h=forceH?forceH:720;
        if (b->init(w,h)) return b;
        delete b; return nullptr;
    };

    if (cfg.backend=="drm") backend = tryDRM();
    else if (cfg.backend=="sdl") backend = trySDL();
    else { // auto
        backend = tryDRM();
        if (!backend) {
            fprintf(stderr,"viewsun: DRM not available, falling back to SDL\n");
            backend = trySDL();
        }
    }

    if (!backend) { fprintf(stderr,"viewsun: failed to init any backend\n"); return 1; }

    int sw, sh;
    backend->getScreenSize(sw, sh);
    printf("viewsun: running %dx%d backend=%s wallpaper=%s\n", sw, sh, cfg.backend.c_str(), cfg.wallpaper_path.empty()?"(none)":cfg.wallpaper_path.c_str());

    WindowManager wm;
    wm.cfg = cfg;
    // keep wallpaper path in config for renderer fallback color
    wm.addWindow("WIN1");
    wm.addWindow("WIN2");
    wm.addWindow("WIN3");
    wm.tile(sw,sh);

    bool running=true;
    auto last = std::chrono::steady_clock::now();
    // init mouse in center
    wm.mouseX = sw/2; wm.mouseY = sh/2;
    while (running) {
        InputEvent ev{};
        while (backend->pollEvent(ev)) {
            if (ev.type==InputEventType::MouseMove) {
                wm.focusAt(ev.mx, ev.my);
                // do not retile on hover - avoids resize-only bug
                continue;
            }
            if (ev.type==InputEventType::MouseButton) {
                wm.focusAt(ev.mx, ev.my);
                if (ev.button==1) {
                    // left click already focused, retile to update border
                    wm.tile(sw,sh);
                } else if (ev.button==3) {
                    wm.removeFocused(); wm.tile(sw,sh);
                }
                continue;
            }
            if (ev.type!=InputEventType::KeyDown) continue;
            bool alt=ev.alt, shift=ev.shift; bool super=ev.super; bool ctrl=ev.ctrl;
            int k=ev.keycode;
            // Debug log for GNOME: show key if unknown
            // fprintf(stderr,"key %d alt=%d super=%d ctrl=%d shift=%d\n",k,alt,super,ctrl,shift);
            if (super && k==KEY_P) { running=false; } // Windows+P logout
            else if ((alt || super || ctrl) && k==KEY_ENTER) { wm.addWindow(); wm.tile(sw,sh); }
            else if ((alt || super || ctrl) && k==KEY_N) { wm.addWindow(); wm.tile(sw,sh); } // fallback n
            else if ((alt && shift) && k==KEY_Q) { running=false; }
            else if ((alt || super || ctrl) && k==KEY_Q) { wm.removeFocused(); wm.tile(sw,sh); }
            else if ((alt || super || ctrl) && k==KEY_C) { wm.removeFocused(); wm.tile(sw,sh); }
            else if ((alt || super || ctrl) && k==KEY_X) { wm.removeFocused(); wm.tile(sw,sh); }
            else if (k==KEY_J || k==KEY_K || k==KEY_H || k==KEY_L || k==KEY_M || k==KEY_B || k==KEY_G) {
                // allow with or without mod inside GNOME where Alt grabbed
                bool mod = alt || super || ctrl;
                if (!mod && wm.windows.size()<=3) mod=true; // allow plain keys when no mod (GNOME steals Alt)
                if (mod) {
                    if (k==KEY_J) { wm.focusNext(1); wm.tile(sw,sh); }
                    else if (k==KEY_K) { wm.focusNext(-1); wm.tile(sw,sh); }
                    else if (k==KEY_H) { wm.resizeMaster(-5); wm.tile(sw,sh); }
                    else if (k==KEY_L) { wm.resizeMaster(5); wm.tile(sw,sh); }
                    else if (k==KEY_M) { wm.setLayout(Layout::MasterStack); wm.tile(sw,sh); }
                    else if (k==KEY_B) { wm.setLayout(Layout::BSP); wm.tile(sw,sh); }
                    else if (k==KEY_G) { wm.setLayout(Layout::Grid); wm.tile(sw,sh); }
                }
            }
        }

        Framebuffer fb = backend->framebuffer();
        render(fb, wm, wm.cfg, wp);
        backend->present();

        // ~60fps cap, also low cpu when idle
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        // handle case where SDL quit event encoded as Alt+Shift+Q already
    }

    backend->shutdown();
    delete backend;
    return 0;
}
