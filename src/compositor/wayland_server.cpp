#include "../../include/wayland_server.h"
#include "../../include/window.h"
#include "../../include/display_server.h"
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <vector>
#include <string>

// Minimal Wayland host inside viewsun custom compositor
// Handles wl_compositor + wl_shm + xdg_wm_base + wl_seat (no input yet)
// Firefox/Nautilus will connect via WAYLAND_DISPLAY=viewsun-wayland

struct WaylandBuffer {
    int fd=-1;
    void *data=nullptr;
    int w=0,h=0,stride=0;
    size_t size=0;
};

struct WaylandSurface {
    WaylandBuffer *buffer=nullptr;
    int x=0,y=0;
    std::string title="wayland";
    uint32_t winId=0; // viewsun Window id
};

static WaylandServer *gWay = nullptr;
static wl_display *gDisplay = nullptr;
static std::vector<WaylandSurface*> surfaces;

// wl_compositor
static void compositor_create_surface(wl_client*, wl_resource *res, uint32_t id){
    WaylandSurface *s=new WaylandSurface();
    surfaces.push_back(s);
    wl_resource *surf = wl_resource_create(wl_resource_get_client(res), &wl_surface_interface, wl_resource_get_version(res), id);
    wl_resource_set_user_data(surf, s);
    // noop destroy
}
static void compositor_create_region(wl_client*, wl_resource*, uint32_t){}

static const struct wl_compositor_interface compositor_impl{compositor_create_surface, compositor_create_region};

// wl_shm
static void shm_create_pool(wl_client *cl, wl_resource *res, uint32_t id, int fd, int32_t size){
    WaylandBuffer *pool=new WaylandBuffer();
    pool->fd=dup(fd); pool->size=size;
    pool->data=mmap(nullptr,size,PROT_READ,MAP_SHARED,pool->fd,0);
    wl_resource *poolRes=wl_resource_create(cl, &wl_shm_pool_interface, wl_resource_get_version(res), id);
    wl_resource_set_user_data(poolRes, pool);
}
static const struct wl_shm_interface shm_impl{shm_create_pool};

static void shm_pool_create_buffer(wl_client*, wl_resource *res, uint32_t id, int32_t offset, int32_t w, int32_t h, int32_t stride, uint32_t){
    WaylandBuffer *pool=(WaylandBuffer*)wl_resource_get_user_data(res);
    WaylandBuffer *buf=new WaylandBuffer();
    buf->w=w; buf->h=h; buf->stride=stride;
    buf->size=stride*h;
    buf->fd=dup(pool->fd);
    buf->data=(char*)pool->data+offset;
    // keep pool mmap alive, buffer shares it
    wl_resource *b=wl_resource_create(wl_resource_get_client(res), &wl_buffer_interface, wl_resource_get_version(res), id);
    wl_resource_set_user_data(b, buf);
}
static void shm_pool_destroy(wl_client*, wl_resource *r){ auto *p=(WaylandBuffer*)wl_resource_get_user_data(r); if(p){ if(p->data) munmap(p->data,p->size); close(p->fd); delete p; } wl_resource_destroy(r); }
static void shm_pool_resize(wl_client*, wl_resource*, int32_t){}

static const struct wl_shm_pool_interface shm_pool_impl{shm_pool_create_buffer, shm_pool_destroy, shm_pool_resize};

// xdg_wm_base
static void xdg_get_xdg_surface(wl_client *cl, wl_resource *res, uint32_t id, wl_resource *surfRes){
    WaylandSurface *surf=(WaylandSurface*)wl_resource_get_user_data(surfRes);
    wl_resource *xdgSurf=wl_resource_create(cl, &xdg_surface_interface, wl_resource_get_version(res), id);
    wl_resource_set_user_data(xdgSurf, surf);
    // send configure
    xdg_surface_send_configure(xdgSurf, 0);
}
static void xdg_pong(wl_client*, wl_resource*, uint32_t){}

static const struct xdg_wm_base_interface xdg_impl{xdg_get_xdg_surface, xdg_pong};

// xdg_surface
static void xdg_surface_get_toplevel(wl_client *cl, wl_resource *res, uint32_t id){
    WaylandSurface *surf=(WaylandSurface*)wl_resource_get_user_data(res);
    wl_resource *top=wl_resource_create(cl, &xdg_toplevel_interface, wl_resource_get_version(res), id);
    wl_resource_set_user_data(top, surf);
    // create viewsun window for this surface
    if(gWay){
        // title not yet known, use wayland
        surf->title="firefox";
        // create window via WindowManager (deferred to main loop? do now)
        // we need to access WindowManager via gWay->wm
        // create window 800x600
        Window win;
        win.id = gWay->wm->next_id++;
        win.title = surf->title;
        win.type = WinType::Custom;
        win.color = 0xFF1D2021;
        bool left = (gWay->wm->lastSw>0 && gWay->wm->mouseX < gWay->wm->lastSw/2);
        int at = left? std::min(gWay->wm->cfg.master_count,(int)gWay->wm->windows.size()): gWay->wm->windows.size();
        gWay->wm->windows.insert(gWay->wm->windows.begin()+at, std::move(win));
        if(left) gWay->wm->cfg.master_count++;
        gWay->wm->focused=at;
        for(auto &w: gWay->wm->windows) w.focused=false;
        gWay->wm->windows[at].focused=true;
        surf->winId = gWay->wm->windows[at].id;
        gWay->wm->tile(gWay->sw, gWay->sh);
        printf("wayland: xdg_toplevel -> viewsun window %d\n", surf->winId);
    }
    xdg_toplevel_send_configure(top, 0,0, nullptr);
}
static void xdg_surface_ack_configure(wl_client*, wl_resource*, uint32_t){}
static void xdg_surface_set_window_geometry(wl_client*, wl_resource*, int32_t,int32_t,int32_t,int32_t){}
static void xdg_surface_destroy(wl_client*, wl_resource *r){ wl_resource_destroy(r); }

static const struct xdg_surface_interface xdg_surface_impl{xdg_surface_destroy, xdg_surface_get_toplevel, xdg_surface_ack_configure, xdg_surface_set_window_geometry};

// xdg_toplevel
static void xdg_toplevel_set_title(wl_client*, wl_resource *res, const char *title){
    WaylandSurface *s=(WaylandSurface*)wl_resource_get_user_data(res);
    s->title=title?title:"wayland";
    if(gWay) for(auto &w: gWay->wm->windows) if(w.id==(int)s->winId) w.title=s->title;
}
static void xdg_toplevel_set_app_id(wl_client*, wl_resource*, const char*){}
static void xdg_toplevel_show_window_menu(wl_client*, wl_resource*, wl_resource*, uint32_t,int32_t,int32_t){}
static void xdg_toplevel_move(wl_client*, wl_resource*, wl_resource*, uint32_t){}
static void xdg_toplevel_resize(wl_client*, wl_resource*, wl_resource*, uint32_t,uint32_t){}
static void xdg_toplevel_set_max_size(wl_client*, wl_resource*, int32_t,int32_t){}
static void xdg_toplevel_set_min_size(wl_client*, wl_resource*, int32_t,int32_t){}
static void xdg_toplevel_set_maximized(wl_client*, wl_resource*){}
static void xdg_toplevel_unset_maximized(wl_client*, wl_resource*){}
static void xdg_toplevel_set_fullscreen(wl_client*, wl_resource*, wl_resource*){}
static void xdg_toplevel_unset_fullscreen(wl_client*, wl_resource*){}
static void xdg_toplevel_set_minimized(wl_client*, wl_resource*){}

static const struct xdg_toplevel_interface xdg_toplevel_impl{
    xdg_toplevel_set_title, xdg_toplevel_set_app_id, xdg_toplevel_show_window_menu,
    xdg_toplevel_move, xdg_toplevel_resize, xdg_toplevel_set_max_size, xdg_toplevel_set_min_size,
    xdg_toplevel_set_maximized, xdg_toplevel_unset_maximized, xdg_toplevel_set_fullscreen,
    xdg_toplevel_unset_fullscreen, xdg_toplevel_set_minimized
};

// wl_surface
static void surface_destroy(wl_client*, wl_resource *res){ wl_resource_destroy(res); }
static void surface_attach(wl_client*, wl_resource *res, wl_resource *buf, int32_t x,int32_t y){
    WaylandSurface *s=(WaylandSurface*)wl_resource_get_user_data(res);
    if(buf){
        WaylandBuffer *b=(WaylandBuffer*)wl_resource_get_user_data(buf);
        s->buffer=b; s->x=x; s->y=y;
        // attach to viewsun window buffer for blit
        if(gWay && s->winId){
            // find winBuf and attach
            for(auto &wb: gWay->wm->windows) if(wb.id==(int)s->winId){
                // create ViewsunBuffer wrapping shm
                // Find DisplayServer winBufs
                // Use DisplayServer instance
                // We need to create ViewsunBuffer from WaylandBuffer
                // For now, directly store WaylandBuffer as ViewsunBuffer via hack: create ViewsunBuffer that points to same mmap
                // Do via DisplayServer::winBufs
                auto &ds = DisplayServer::instance();
                // remove old
                for(auto &wbb: ds.winBufs) if(wbb.winId==s->winId) wbb.buf.reset();
                auto vb=std::make_unique<ViewsunBuffer>();
                vb->fd=dup(b->fd); vb->width=b->w; vb->height=b->h; vb->stride=b->stride; vb->size=b->stride*b->h;
                vb->pixels=(uint32_t*)mmap(nullptr,vb->size,PROT_READ,MAP_SHARED,vb->fd,0);
                // find or create
                bool found=false;
                for(auto &wbb: ds.winBufs) if(wbb.winId==s->winId){ wbb.buf=std::move(vb); found=true; break; }
                if(!found) ds.winBufs.push_back({s->winId, std::move(vb), nullptr});
                break;
            }
        }
    } else {
        s->buffer=nullptr;
    }
}
static void surface_damage(wl_client*, wl_resource*, int32_t,int32_t,int32_t,int32_t){}
static void surface_frame(wl_client*, wl_resource*, uint32_t cb){
    wl_resource *cbRes=wl_resource_create(wl_resource_get_client(wl_resource_create(nullptr,nullptr,0,0)), &wl_callback_interface, 1, cb);
    wl_callback_send_done(cbRes, 0);
    wl_resource_destroy(cbRes);
}
static void surface_set_opaque_region(wl_client*, wl_resource*, wl_resource*){}
static void surface_set_input_region(wl_client*, wl_resource*, wl_resource*){}
static void surface_commit(wl_client*, wl_resource *res){
    WaylandSurface *s=(WaylandSurface*)wl_resource_get_user_data(res);
    (void)s;
    // damage already handled via attach
}
static void surface_set_buffer_transform(wl_client*, wl_resource*, int32_t){}
static void surface_set_buffer_scale(wl_client*, wl_resource*, int32_t){}
static void surface_damage_buffer(wl_client*, wl_resource*, int32_t,int32_t,int32_t,int32_t){}
static void surface_offset(wl_client*, wl_resource*, int32_t,int32_t){}

static void surface_get_release(wl_client*, wl_resource*, uint32_t callback){
    wl_resource *r=wl_resource_create(wl_resource_get_client(wl_resource_create(nullptr,nullptr,0,0)), &wl_callback_interface, 1, callback);
    wl_callback_send_done(r, 0);
    wl_resource_destroy(r);
}
static const struct wl_surface_interface surface_impl{
    surface_destroy, surface_attach, surface_damage, surface_frame, surface_set_opaque_region,
    surface_set_input_region, surface_commit, surface_set_buffer_transform,
    surface_set_buffer_scale, surface_damage_buffer, surface_offset, surface_get_release
};

// globals bind
static void bind_compositor(wl_client *cl, void*, uint32_t v, uint32_t id){
    wl_resource *r=wl_resource_create(cl, &wl_compositor_interface, v, id);
    wl_resource_set_implementation(r, &compositor_impl, nullptr, nullptr);
}
static void bind_shm(wl_client *cl, void*, uint32_t v, uint32_t id){
    wl_resource *r=wl_resource_create(cl, &wl_shm_interface, v, id);
    wl_resource_set_implementation(r, &shm_impl, nullptr, nullptr);
}
static void bind_xdg(wl_client *cl, void*, uint32_t v, uint32_t id){
    wl_resource *r=wl_resource_create(cl, &xdg_wm_base_interface, v, id);
    wl_resource_set_implementation(r, &xdg_impl, nullptr, nullptr);
}

bool WaylandServer::init(WindowManager *wm_, int sw_, int sh_){
    wm=wm_; sw=sw_; sh=sh_; gWay=this;
    display=wl_display_create();
    if(!display) return false;
    wl_global_create(display, &wl_compositor_interface, 4, nullptr, bind_compositor);
    wl_global_create(display, &wl_shm_interface, 1, nullptr, bind_shm);
    wl_global_create(display, &xdg_wm_base_interface, 1, nullptr, bind_xdg);
    const char* sock="viewsun-wayland";
    if(wl_display_add_socket(display, sock)!=0){ perror("wayland socket"); return false; }
    // also add /tmp/viewsun-wayland for firefox WAYLAND_DISPLAY
    printf("wayland: listening on %s (WAYLAND_DISPLAY=viewsun-wayland)\n", sock);
    gDisplay=display;
    // set env for children
    setenv("WAYLAND_DISPLAY","viewsun-wayland",1);
    return true;
}
void WaylandServer::shutdown(){
    if(display) wl_display_destroy(display);
    display=nullptr;
}
void WaylandServer::poll(){
    if(display) wl_display_flush_clients(display);
    // dispatch with 0 timeout
    if(display) wl_event_loop_dispatch(wl_display_get_event_loop(display), 0);
}
const char* WaylandServer::socketName(){ return "viewsun-wayland"; }
