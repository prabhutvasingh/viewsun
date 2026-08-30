#include "../include/backend.h"
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstdio>
#include <cstring>
#include <poll.h>
#include <linux/input.h>
#include <dirent.h>
#include <vector>

class DRMBackend : public Backend {
    int fd=-1;
    drmModeConnector *conn=nullptr;
    drmModeCrtc *savedCrtc=nullptr;
    uint32_t connId=0, crtcId=0;
    drmModeModeInfo mode{};
    struct DumbBO { uint32_t handle, pitch, size, fb; uint32_t *map; } bo{};
    Framebuffer fb{};
    int W=0,H=0;
    std::vector<int> inputFds;
    bool hasEvdev=false;

    bool findConnector() {
        drmModeRes *res = drmModeGetResources(fd);
        if (!res) return false;
        for (int i=0;i<res->count_connectors;i++) {
            drmModeConnector *c = drmModeGetConnector(fd, res->connectors[i]);
            if (!c) continue;
            if (c->connection==DRM_MODE_CONNECTED && c->count_modes>0) {
                conn=c; connId=c->connector_id;
                mode=c->modes[0];
                // prefer highest resolution
                for(int j=1;j<c->count_modes;j++) if(c->modes[j].hdisplay*c->modes[j].vdisplay > mode.hdisplay*mode.vdisplay) mode=c->modes[j];
                // find crtc
                drmModeEncoder *enc = drmModeGetEncoder(fd, c->encoder_id);
                if (enc) {
                    crtcId = enc->crtc_id;
                    drmModeFreeEncoder(enc);
                } else if (res->count_crtcs>0) crtcId=res->crtcs[0];
                drmModeFreeResources(res);
                return true;
            }
            drmModeFreeConnector(c);
        }
        drmModeFreeResources(res);
        return false;
    }

    bool createDumb() {
        struct drm_mode_create_dumb creq{}; creq.width=W; creq.height=H; creq.bpp=32;
        if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq)!=0) { perror("CREATE_DUMB"); return false; }
        bo.handle=creq.handle; bo.pitch=creq.pitch; bo.size=creq.size;
        uint32_t fbId;
        if (drmModeAddFB(fd, W, H, 24,32, bo.pitch, bo.handle, &fbId)!=0) { perror("AddFB"); return false; }
        bo.fb=fbId;
        struct drm_mode_map_dumb mreq{}; mreq.handle=bo.handle;
        if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq)!=0) { perror("MAP_DUMB"); return false; }
        void *map = mmap(0, bo.size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, mreq.offset);
        if (map==MAP_FAILED) { perror("mmap"); return false; }
        bo.map=(uint32_t*)map;
        fb.pixels=bo.map; fb.width=W; fb.height=H; fb.stride=bo.pitch/4;
        return true;
    }

    void openInputs() {
        // open all /dev/input/event*
        for(int i=0;i<32;i++) {
            char p[64]; snprintf(p,sizeof(p),"/dev/input/event%d",i);
            int f=open(p, O_RDONLY|O_NONBLOCK);
            if(f>=0) inputFds.push_back(f);
        }
        hasEvdev=!inputFds.empty();
    }

public:
    bool init(int w,int h) override {
        const char* card = getenv("VIEWSUN_CARD") ? getenv("VIEWSUN_CARD") : "/dev/dri/card1";
        // try card1 then card0
        fd=open(card,O_RDWR|O_CLOEXEC);
        if(fd<0) { fd=open("/dev/dri/card0",O_RDWR|O_CLOEXEC); }
        if(fd<0) { perror("open drm card"); return false; }
        if (!findConnector()) {
            // fallback to requested size
            if(w>0 && h>0) { W=w; H=h; mode.hdisplay=W; mode.vdisplay=H; }
            else { fprintf(stderr,"viewsun: no connected connector\n"); return false; }
        } else {
            W=mode.hdisplay; H=mode.vdisplay;
            if(w>0 && h>0) { W=w; H=h; } // override if provided
        }
        // save crtc
        savedCrtc = drmModeGetCrtc(fd, crtcId);
        if (!createDumb()) return false;
        if (drmModeSetCrtc(fd, crtcId, bo.fb, 0,0, &connId,1,&mode)!=0) {
            perror("SetCrtc"); // may fail if master busy
            // still continue for testing? For now fail
            return false;
        }
        openInputs();
        printf("viewsun: DRM %dx%d @ %dHz on connector %u crtc %u\n", W,H,mode.vrefresh, connId, crtcId);
        return true;
    }
    void shutdown() override {
        if (savedCrtc) { drmModeSetCrtc(fd, savedCrtc->crtc_id, savedCrtc->buffer_id, savedCrtc->x,savedCrtc->y, &connId,1,&savedCrtc->mode); drmModeFreeCrtc(savedCrtc); }
        if (bo.map) munmap(bo.map, bo.size);
        if (bo.fb) drmModeRmFB(fd, bo.fb);
        if (bo.handle) { struct drm_mode_destroy_dumb dreq{}; dreq.handle=bo.handle; drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB,&dreq); }
        if (conn) drmModeFreeConnector(conn);
        for(int f: inputFds) close(f);
        if(fd>=0) close(fd);
    }
    Framebuffer framebuffer() override { return fb; }
    void present() override { /* dumb buffer is directly scanned out, no double buffer - already visible */ }
    void getScreenSize(int &w,int &h) override { w=W; h=H; }

    bool pollEvent(InputEvent &ev) override {
        if (inputFds.empty()) return false;
        static int curX = -1, curY = -1;
        static int accDx = 0, accDy = 0;
        static bool altDown=false, shiftDown=false, ctrlDown=false, superDown=false;
        if (curX==-1) { curX=W/2; curY=H/2; }
        struct input_event ie;
        // Use poll to avoid blocking, check all fds
        for(int fd: inputFds) {
            while (true) {
                ssize_t n=read(fd,&ie,sizeof(ie));
                if(n!=sizeof(ie)) break;
                if(ie.type==EV_KEY) {
                    if(ie.code==KEY_LEFTALT || ie.code==KEY_RIGHTALT) { altDown = (ie.value!=0); continue; }
                    if(ie.code==KEY_LEFTSHIFT || ie.code==KEY_RIGHTSHIFT) { shiftDown=(ie.value!=0); continue; }
                    if(ie.code==KEY_LEFTCTRL || ie.code==KEY_RIGHTCTRL) { ctrlDown=(ie.value!=0); continue; }
                    if(ie.code==KEY_LEFTMETA || ie.code==KEY_RIGHTMETA) { superDown=(ie.value!=0); continue; }
                    if(ie.code==BTN_LEFT || ie.code==BTN_RIGHT || ie.code==BTN_MIDDLE) {
                        if(ie.value==1) { // press
                            ev.type = InputEventType::MouseButton;
                            ev.button = (ie.code==BTN_LEFT?1:(ie.code==BTN_RIGHT?3:2));
                            ev.mx = curX; ev.my = curY;
                            ev.alt=altDown; ev.shift=shiftDown; ev.ctrl=ctrlDown; ev.super=superDown;
                            return true;
                        }
                        continue;
                    }
                    // regular key
                    ev.type = (ie.value==0)?InputEventType::KeyUp:InputEventType::KeyDown;
                    ev.keycode=ie.code;
                    ev.alt=altDown; ev.shift=shiftDown; ev.ctrl=ctrlDown; ev.super=superDown;
                    if(ie.value==0) continue; // only emit KeyDown for actions
                    return true;
                } else if(ie.type==EV_REL) {
                    if(ie.code==REL_X) accDx += ie.value;
                    else if(ie.code==REL_Y) accDy += ie.value;
                } else if(ie.type==EV_ABS) {
                    if(ie.code==ABS_X) { curX = ie.value * W / 4096; } // crude scale
                    else if(ie.code==ABS_Y) { curY = ie.value * H / 4096; }
                } else if(ie.type==EV_SYN && ie.code==SYN_REPORT) {
                    if(accDx!=0 || accDy!=0) {
                        curX += accDx; curY += accDy;
                        if(curX<0) curX=0; if(curX>=W) curX=W-1;
                        if(curY<0) curY=0; if(curY>=H) curY=H-1;
                        accDx=0; accDy=0;
                        ev.type = InputEventType::MouseMove;
                        ev.mx = curX; ev.my = curY;
                        ev.alt=altDown; ev.shift=shiftDown; ev.ctrl=ctrlDown; ev.super=superDown;
                        return true;
                    }
                }
            }
        }
        // if we accumulated REL but no SYN (some devices), emit now
        if(accDx!=0 || accDy!=0) {
            curX += accDx; curY += accDy;
            if(curX<0) curX=0; if(curX>=W) curX=W-1;
            if(curY<0) curY=0; if(curY>=H) curY=H-1;
            accDx=0; accDy=0;
            ev.type = InputEventType::MouseMove;
            ev.mx = curX; ev.my = curY;
            ev.alt=altDown; ev.shift=shiftDown; ev.ctrl=ctrlDown; ev.super=superDown;
            return true;
        }
        return false;
    }
};

Backend* createDRMBackend(const std::string &card) {
    // card param ignored, uses env or auto
    (void)card;
    return new DRMBackend();
}
