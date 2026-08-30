#pragma once
#include "../include/viewsun_protocol.h"
#include <cstdint>
#include <vector>

class ViewsunClientLib {
    int fd=-1;
public:
    bool connect();
    void disconnect();
    int getFd(){ return fd; }
    bool createWindow(uint32_t w, uint32_t h, const char* title, uint32_t *outId);
    bool createBuffer(uint32_t winId, int shmFd, uint32_t w, uint32_t h, uint32_t stride);
    bool damage(uint32_t winId, int x,int y,uint32_t w,uint32_t h);
    bool setTitle(uint32_t winId, const char* title);
    bool pollEvent(ViewsunHeader &hdr, std::vector<char> &payload);
    bool waitFrame();
};
