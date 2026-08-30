#pragma once
#include "window.h"
#include "viewsun_protocol.h"
#include "backend.h"
#include <vector>
#include <memory>
#include <functional>
#include <poll.h>
#include <sys/stat.h>

struct ViewsunBuffer {
    int fd = -1;
    uint32_t *pixels = nullptr;
    uint32_t width=0, height=0, stride=0;
    size_t size=0;
    ~ViewsunBuffer();
    bool attach(int fd_, uint32_t w, uint32_t h, uint32_t stride_);
    void detach();
};

struct ViewsunClient {
    int fd = -1;
    uint32_t nextWindowId = 1;
    std::vector<uint32_t> windows; // ids owned
};

class DisplayServer {
public:
    int listenFd = -1;
    std::vector<std::unique_ptr<ViewsunClient>> clients;
    std::vector<pollfd> pfds;
    WindowManager *wm = nullptr;
    int sw=0, sh=0;

    bool init(WindowManager *wm_, int sw_, int sh_);
    void shutdown();
    void pollClients(); // non-blocking
    void handleClient(ViewsunClient *c);
    bool sendToClient(ViewsunClient *c, ViewsunOpcode op, const void* data, uint32_t size);
    void broadcastFrame();
    void sendInputToFocused(const InputEvent &ev);
    ViewsunBuffer* getBufferForWindow(uint32_t winId);
    // called when client creates window
    void onCreateWindow(ViewsunClient *c, uint32_t w, uint32_t h, const std::string &title);
    void onCreateBuffer(ViewsunClient *c, uint32_t winId, int fd, uint32_t w, uint32_t h, uint32_t stride);
    void onDamage(ViewsunClient *c, uint32_t winId, int x,int y,uint32_t w,uint32_t h);
    // map windowId -> buffer
    struct WinBuf { uint32_t winId; std::unique_ptr<ViewsunBuffer> buf; ViewsunClient* owner; };
    std::vector<WinBuf> winBufs;

    static DisplayServer& instance();
private:
    bool acceptNew();
    ssize_t recvMsg(int fd, ViewsunHeader &hdr, std::vector<char> &payload, int &outFd);
    bool sendMsg(int fd, ViewsunOpcode op, const void* data, uint32_t size, int fdToSend=-1);
};
