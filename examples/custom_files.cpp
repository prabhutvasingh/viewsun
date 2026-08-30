#include "../lib/viewsun_client.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>

int main(){
    ViewsunClientLib client;
    if(!client.connect()) return 1;
    uint32_t winId;
    client.createWindow(640,480,"files",&winId);
    int w=640,h=480,stride=w*4;
    char name[64]; snprintf(name,sizeof(name),"/viewsun-files-%d",getpid());
    int fd=shm_open(name,O_CREAT|O_RDWR,0600);
    ftruncate(fd,w*h*4);
    uint32_t* px=(uint32_t*)mmap(nullptr,w*h*4,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    const char* home=getenv("HOME"); if(!home) home="/";
    std::string path=home;
    auto redraw=[&](){
        for(int i=0;i<w*h;i++) px[i]=0xFFF5F5F0;
        // path bar
        for(int y=6;y<22;y++) for(int x=6;x<w-6;x++) px[y*w+x]=0xFF3A3A2A;
        auto drawText=[&](int x,int y,const std::string &s,uint32_t col){
            for(size_t i=0;i<s.size()&& x+i*7 < w-10;i++) for(int dy=0;dy<7;dy++) for(int dx=0;dx<5;dx++){
                int p=(y+dy)*w + x+i*7+dx; if(p>=0&&p<w*h) if(s[i]!=' ') px[p]=col;
            }
        };
        drawText(10,8,path,0xFFEBDBB2);
        // files
        std::vector<std::string> files;
        try{ for(auto &e: std::filesystem::directory_iterator(path)){ std::string n=e.path().filename().string(); if(e.is_directory()) n+="/"; files.push_back(n);} }catch(...){}
        std::sort(files.begin(),files.end());
        int y=28;
        for(size_t i=0;i<files.size()&& y<h-12;i++){
            uint32_t col = files[i].back()=='/' ? 0xFF8BA4FF : 0xFF222222;
            // icon
            for(int dy=0;dy<8;dy++) for(int dx=0;dx<8;dx++){ int p=(y+dy)*w+10+dx; px[p]= files[i].back()=='/'?0xFF458588:0xFFA89984; }
            drawText(22,y,files[i],col);
            y+=10;
        }
        if(files.empty()) drawText(10,28,"(empty)",0xFF928374);
    };
    redraw();
    client.createBuffer(winId,fd,w,h,stride);
    client.damage(winId,0,0,w,h);
    printf("custom_files: %s window %d\n", path.c_str(), winId);
    while(true){
        ViewsunHeader hdr; std::vector<char> pl;
        if(client.pollEvent(hdr,pl)){
            if(hdr.opcode==ViewsunOpcode::InputKey){
                // simple: Enter goes up, etc - not implemented for brevity
            }
        }
        usleep(100000);
    }
}
