#include "../include/backend.h"
#include "../include/window.h"
#include "../include/renderer.h"
#include "../include/wallpaper.h"
#include "../include/display_server.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
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
    printf("  Super+Enter internal term  Super+w internal browser  Super+e internal files  Super+P logout\n");
    printf("  Alt+Enter (or Ctrl+n) spawn placeholder  Alt+q close  Alt+Shift+q quit\n");
    printf("  j/k focus  h/l resize master  m/b/g layouts (with Alt/Ctrl/Super, or plain)\n");
    printf("  Mouse: left-click focus, right-click close, move focuses\n");
    printf("\nExamples:\n");
    printf("  viewsun -w ~/wallpaper.jpg\n");
    printf("  viewsun --backend sdl -w /tmp/bg.png\n");
    printf("  sudo viewsun --backend drm -w /usr/share/viewsun/wallpaper.png\n");
}

static void spawn(const char *cmd) {
    pid_t pid = fork();
    if (pid==0) {
        // child - detach
        setsid();
        execl("/bin/sh","sh","-c",cmd,(char*)nullptr);
        _exit(127);
    } else if (pid>0) {
        // reap quickly without blocking
        int status; waitpid(pid,&status,WNOHANG);
    }
}
static void spawnKitty() {
    // try kitty, fallback to gnome-terminal, xterm, foot
    spawn("kitty 2>/dev/null || gnome-terminal 2>/dev/null || xterm 2>/dev/null || foot 2>/dev/null &");
}
static void spawnBrowser() {
    spawn("xdg-open https://google.com 2>/dev/null || firefox 2>/dev/null || chromium 2>/dev/null || google-chrome 2>/dev/null &");
}
static void spawnFileManager() {
    spawn("xdg-open \"$HOME\" 2>/dev/null || nautilus \"$HOME\" 2>/dev/null || dolphin \"$HOME\" 2>/dev/null || thunar \"$HOME\" 2>/dev/null || pcmanfm \"$HOME\" 2>/dev/null &");
}
static void spawnCustom(const char* title){
    pid_t pid=fork();
    if(pid==0){
        const char* app=nullptr;
        if(strcmp(title,"browser")==0) app="/home/avi/Projects/viewsun/examples/custom_browser";
        else if(strcmp(title,"files")==0) app="/home/avi/Projects/viewsun/examples/custom_files";
        else app="/home/avi/Projects/viewsun/examples/custom_app";
        if(access(app,X_OK)!=0){
            if(strcmp(title,"browser")==0) app="/usr/local/lib/viewsun/custom_browser";
            else if(strcmp(title,"files")==0) app="/usr/local/lib/viewsun/custom_files";
            else app="/usr/local/lib/viewsun/custom_app";
        }
        // custom clients understand title as argv[1]
        execl(app,app,title,(char*)nullptr);
        // fallback to placeholder if custom client missing
        _exit(127);
    }
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
    // init custom display server (viewsun compositor)
    auto &ds = DisplayServer::instance();
    if (!ds.init(&wm, sw, sh)) fprintf(stderr,"viewsun: display server failed (custom clients disabled)\n");
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
        ds.pollClients();
        InputEvent ev{};
        while (backend->pollEvent(ev)) {
            // forward input to focused custom client
            ds.sendInputToFocused(ev);
            if (ev.type==InputEventType::Text) {
                Window* f = wm.getFocused();
                if (f && f->isTerm && f->term) f->term->writeInput(ev.text, strlen(ev.text));
                continue;
            }
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
            // prioritize terminal input when focused is term and no WM mod
            Window* fterm = wm.getFocused();
            bool isTermFocused = fterm && fterm->isTerm && fterm->term;
            bool wmMod = ev.alt || ev.super || ev.ctrl;
            if (isTermFocused && !wmMod) {
                if (ev.keycode==KEY_BACKSPACE) { fterm->term->backspace(); continue; }
                if (ev.keycode==KEY_ENTER) { fterm->term->enter(); continue; }
                if (ev.keycode==KEY_TAB) { fterm->term->writeInput("    ",4); continue; }
                if (ev.keycode==KEY_SPACE) { fterm->term->writeInputChar(' '); continue; }
                // let Text events handle actual typing, but also handle via keycode as fallback
                // if we get here with printable, try keycodeToChar
                char c = keycodeToChar(ev.keycode, ev.shift);
                if (c) { fterm->term->writeInputChar(c); continue; }
            }
            bool alt=ev.alt, shift=ev.shift; bool super=ev.super; bool ctrl=ev.ctrl;
            int k=ev.keycode;
            if (super && k==KEY_P) { running=false; } // Windows+P logout
            else if (super && k==KEY_ENTER) { wm.addTerminal(); wm.tile(sw,sh); }
            else if (super && k==KEY_W) { spawnCustom("browser"); }
            else if (super && k==KEY_E) { spawnCustom("files"); }
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
                } else {
                    // plain typing into terminal if focused is term
                    Window* f = wm.getFocused();
                    if (f && f->isTerm && f->term) {
                        char c = keycodeToChar(k, shift);
                        if (c) f->term->writeInputChar(c);
                    }
                }
            } else {
                // route remaining keys to focused terminal (typing)
                Window* f = wm.getFocused();
                if (f && f->isTerm && f->term) {
                    if (k==KEY_BACKSPACE) f->term->backspace();
                    else if (k==KEY_ENTER) f->term->enter();
                    else if (k==KEY_SPACE) f->term->writeInputChar(' ');
                    else {
                        char c = keycodeToChar(k, shift);
                        if (c) f->term->writeInputChar(c);
                        else if (k==KEY_DOT) f->term->writeInputChar(shift?'>':'.');
                    }
                }
            }
        }
        // poll terminals for output
        for (auto &w: wm.windows) if (w.isTerm && w.term) w.term->poll();

        Framebuffer fb = backend->framebuffer();
        render(fb, wm, wm.cfg, wp);
        backend->present();
        ds.broadcastFrame();

        // ~60fps cap, also low cpu when idle
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        // handle case where SDL quit event encoded as Alt+Shift+Q already
    }

    ds.shutdown();
    backend->shutdown();
    delete backend;
    return 0;
}
