#include "../../include/display_server.h"
#include "../../include/backend.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <sys/uio.h>

ViewsunBuffer::~ViewsunBuffer(){ detach(); }
bool ViewsunBuffer::attach(int fd_, uint32_t w, uint32_t h, uint32_t stride_) {
    detach();
    fd=fd_; width=w; height=h; stride=stride_;
    size = stride * height; // stride already bytes (w*4)
    void* map = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
    if(map==MAP_FAILED) { perror("mmap buffer"); return false; }
    pixels = (uint32_t*)map;
    return true;
}
void ViewsunBuffer::detach(){
    if(pixels) munmap(pixels, size);
    if(fd>=0) close(fd);
    pixels=nullptr; fd=-1;
}

DisplayServer& DisplayServer::instance(){ static DisplayServer s; return s; }

bool DisplayServer::init(WindowManager *wm_, int sw_, int sh_){
    wm=wm_; sw=sw_; sh=sh_;
    unlink(VIEWSUN_SOCKET);
    listenFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(listenFd<0) return false;
    fcntl(listenFd, F_SETFL, O_NONBLOCK);
    sockaddr_un addr{}; addr.sun_family=AF_UNIX; strncpy(addr.sun_path, VIEWSUN_SOCKET, sizeof(addr.sun_path)-1);
    if(bind(listenFd,(sockaddr*)&addr,sizeof(addr))<0){ perror("bind viewsun"); return false; }
    if(listen(listenFd,16)<0) return false;
    chmod(VIEWSUN_SOCKET, 0777);
    printf("viewsun: display server listening on %s\n", VIEWSUN_SOCKET);
    return true;
}
void DisplayServer::shutdown(){
    for(auto &c: clients) close(c->fd);
    clients.clear();
    if(listenFd>=0) close(listenFd);
    unlink(VIEWSUN_SOCKET);
    winBufs.clear();
}

bool DisplayServer::sendMsg(int fd, ViewsunOpcode op, const void* data, uint32_t size, int fdToSend){
    ViewsunHeader hdr{op,size};
    struct iovec iov[2]; iov[0]={&hdr,sizeof(hdr)}; iov[1]={(void*)data,size};
    int iovcnt = size?2:1;
    if(fdToSend>=0){
        char cmsgbuf[CMSG_SPACE(sizeof(int))];
        msghdr msg{}; msg.msg_iov=iov; msg.msg_iovlen=iovcnt;
        msg.msg_control=cmsgbuf; msg.msg_controllen=sizeof(cmsgbuf);
        cmsghdr* cmsg=CMSG_FIRSTHDR(&msg); cmsg->cmsg_level=SOL_SOCKET; cmsg->cmsg_type=SCM_RIGHTS; cmsg->cmsg_len=CMSG_LEN(sizeof(int));
        memcpy(CMSG_DATA(cmsg), &fdToSend, sizeof(int));
        msg.msg_controllen=cmsg->cmsg_len;
        return sendmsg(fd,&msg,0)==(ssize_t)(sizeof(hdr)+size);
    } else {
        struct msghdr msg{}; msg.msg_iov=iov; msg.msg_iovlen=iovcnt;
        return sendmsg(fd,&msg,0)==(ssize_t)(sizeof(hdr)+size);
    }
}
bool DisplayServer::sendToClient(ViewsunClient *c, ViewsunOpcode op, const void* data, uint32_t size){
    return sendMsg(c->fd, op, data, size);
}

ssize_t DisplayServer::recvMsg(int fd, ViewsunHeader &hdr, std::vector<char> &payload, int &outFd){
    outFd=-1;
    char cmsgbuf[CMSG_SPACE(sizeof(int))];
    iovec iov[1]; iov[0]={&hdr,sizeof(hdr)};
    msghdr msg{}; msg.msg_iov=iov; msg.msg_iovlen=1; msg.msg_control=cmsgbuf; msg.msg_controllen=sizeof(cmsgbuf);
    ssize_t n=recvmsg(fd,&msg,0);
    if(n!=sizeof(hdr)) return n;
    if(hdr.size>0){
        payload.resize(hdr.size);
        // payload may be in same recv? Actually header and payload are separate sends? We send header+payload as one sendmsg with 2 iovs, so recvmsg will get both? But we only read header first. So need to handle.
        // Simplify: header and payload sent together, but we read header first, then read payload separately via recv
        // For simplicity, assume payload is sent immediately after header in same stream, so read it
        ssize_t r=recv(fd,payload.data(),hdr.size,MSG_WAITALL);
        if(r!=(ssize_t)hdr.size) return -1;
    }
    // check for fd
    for(cmsghdr* cmsg=CMSG_FIRSTHDR(&msg); cmsg; cmsg=CMSG_NXTHDR(&msg,cmsg)){
        if(cmsg->cmsg_level==SOL_SOCKET && cmsg->cmsg_type==SCM_RIGHTS){
            memcpy(&outFd, CMSG_DATA(cmsg), sizeof(int));
        }
    }
    return sizeof(hdr)+hdr.size;
}

void DisplayServer::onCreateWindow(ViewsunClient *c, uint32_t w, uint32_t h, const std::string &title){
    // create custom window via WindowManager
    Window win;
    win.id = wm->next_id++;
    win.color = 0xFF1D2021;
    win.title = title.empty()?"app":title;
    win.type = WinType::Custom;
    win.focused=false;
    bool left = (wm->lastSw>0 && wm->mouseX < wm->lastSw/2);
    int insertAt = left ? std::min(wm->cfg.master_count, (int)wm->windows.size()) : wm->windows.size();
    wm->windows.insert(wm->windows.begin()+insertAt, std::move(win));
    if(left) wm->cfg.master_count++;
    if(wm->cfg.master_count > (int)wm->windows.size()) wm->cfg.master_count = wm->windows.size();
    wm->focused = insertAt;
    for(auto &ww: wm->windows) ww.focused=false;
    wm->windows[wm->focused].focused=true;
    int id = wm->windows[wm->focused].id;
    c->windows.push_back(id);
    ViewsunWindowCreated ev{(uint32_t)id};
    sendToClient(c, ViewsunOpcode::WindowCreated, &ev, sizeof(ev));
    wm->tile(sw,sh);
    printf("viewsun: client %d created custom window %d (%s) %dx%d\n", c->fd, id, title.c_str(), w,h);
}

void DisplayServer::onCreateBuffer(ViewsunClient *c, uint32_t winId, int fd, uint32_t w, uint32_t h, uint32_t stride){
    auto buf = std::make_unique<ViewsunBuffer>();
    if(!buf->attach(fd,w,h,stride)) return;
    // find existing
    for(auto &wb: winBufs) if(wb.winId==winId){ wb.buf=std::move(buf); wb.owner=c; return; }
    winBufs.push_back({winId, std::move(buf), c});
    printf("viewsun: buffer %dx%d for win %d\n", w,h,winId);
}

void DisplayServer::onDamage(ViewsunClient *c, uint32_t winId, int x,int y,uint32_t w,uint32_t h){
    // mark damaged, compositor will blit on next frame
    (void)c; (void)x; (void)y; (void)w; (void)h;
    // no-op, frame will be requested
}

ViewsunBuffer* DisplayServer::getBufferForWindow(uint32_t winId){
    for(auto &wb: winBufs) if(wb.winId==winId) return wb.buf.get();
    return nullptr;
}

void DisplayServer::handleClient(ViewsunClient *c){
    ViewsunHeader hdr; std::vector<char> payload; int fd=-1;
    ssize_t n=recvMsg(c->fd, hdr, payload, fd);
    if(n<=0){
        if(n==0 || (errno!=EAGAIN && errno!=EWOULDBLOCK)){
            // disconnect
            printf("viewsun: client %d disconnect\n", c->fd);
            close(c->fd); c->fd=-1;
        }
        return;
    }
    switch(hdr.opcode){
        case ViewsunOpcode::CreateWindow: {
            auto *req=(ViewsunCreateWindow*)payload.data();
            std::string title(req->title);
            onCreateWindow(c, req->width, req->height, title);
            break;
        }
        case ViewsunOpcode::CreateBuffer: {
            auto *req=(ViewsunCreateBuffer*)payload.data();
            onCreateBuffer(c, req->window_id, fd, req->width, req->height, req->stride);
            if(fd>=0) close(fd); // dup'ed in attach
            break;
        }
        case ViewsunOpcode::Damage: {
            auto *req=(ViewsunDamage*)payload.data();
            onDamage(c, req->window_id, req->x, req->y, req->width, req->height);
            break;
        }
        case ViewsunOpcode::SetTitle: {
            auto *req=(ViewsunSetTitle*)payload.data();
            // find window and set title
            for(auto &w: wm->windows) if(w.id==(int)req->window_id) w.title=req->title;
            break;
        }
        case ViewsunOpcode::DestroyWindow: {
            auto *req=(ViewsunDestroyWindow*)payload.data();
            for(auto it=wm->windows.begin(); it!=wm->windows.end(); ++it) if(it->id==(int)req->window_id){ wm->windows.erase(it); break; }
            wm->tile(sw,sh);
            break;
        }
        case ViewsunOpcode::Pong: break;
        default: break;
    }
}

bool DisplayServer::acceptNew(){
    int fd=accept(listenFd,nullptr,nullptr);
    if(fd<0) return false;
    fcntl(fd,F_SETFL,O_NONBLOCK);
    auto c=std::make_unique<ViewsunClient>();
    c->fd=fd;
    clients.push_back(std::move(c));
    printf("viewsun: new client fd %d\n", fd);
    return true;
}

void DisplayServer::pollClients(){
    // accept
    while(acceptNew());
    // poll clients
    for(auto it=clients.begin(); it!=clients.end();){
        if((*it)->fd<0){
            it=clients.erase(it);
            continue;
        }
        // check readable
        pollfd pfd{(*it)->fd, POLLIN, 0};
        if(poll(&pfd,1,0)>0 && (pfd.revents & POLLIN)){
            handleClient(it->get());
        }
        if((*it)->fd<0) it=clients.erase(it);
        else ++it;
    }
}

void DisplayServer::broadcastFrame(){
    for(auto &c: clients) sendToClient(c.get(), ViewsunOpcode::Frame, nullptr,0);
}

void DisplayServer::sendInputToFocused(const InputEvent &ev){
    Window* f=wm->getFocused();
    if(!f) return;
    // find owner
    for(auto &wb: winBufs) if(wb.winId==(uint32_t)f->id){
        ViewsunClient* c=wb.owner;
        if(!c) return;
        if(ev.type==InputEventType::KeyDown || ev.type==InputEventType::KeyUp){
            ViewsunInputKey ik{ (uint32_t)ev.keycode, ev.type==InputEventType::KeyDown?1u:0u, (uint32_t)(ev.alt?1:0 | ev.shift?2:0 | ev.ctrl?4:0 | ev.super?8:0) };
            sendToClient(c, ViewsunOpcode::InputKey, &ik, sizeof(ik));
        } else if(ev.type==InputEventType::MouseButton){
            ViewsunInputButton ib{(uint32_t)ev.button, 1u, ev.mx - f->rect.x, ev.my - f->rect.y};
            sendToClient(c, ViewsunOpcode::InputButton, &ib, sizeof(ib));
        } else if(ev.type==InputEventType::MouseMove){
            ViewsunInputMotion im{ev.mx - f->rect.x, ev.my - f->rect.y};
            sendToClient(c, ViewsunOpcode::InputMotion, &im, sizeof(im));
        }
        break;
    }
}
