#include "../lib/viewsun_client.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <vector>

// Minimal custom GUI app - draws inside viewsun compositor via custom protocol
// No Wayland/X11 - pure viewsun

int main(int argc, char** argv){
    const char* title = argc>1?argv[1]:"custom_app";
    ViewsunClientLib client;
    if(!client.connect()){ fprintf(stderr,"custom_app: cannot connect to viewsun\n"); return 1; }
    uint32_t winId;
    if(!client.createWindow(640,480,title,&winId)){ fprintf(stderr,"create_window failed\n"); return 1; }
    printf("custom_app: window %d created (%s)\n", winId, title);

    // create shm buffer
    int w=640,h=480,stride=w*4;
    size_t sz=stride*h;
    char shmName[64]; snprintf(shmName,sizeof(shmName),"/viewsun-%d-%d", getpid(), winId);
    int fd=shm_open(shmName,O_CREAT|O_RDWR,0600);
    ftruncate(fd,sz);
    uint32_t* pixels=(uint32_t*)mmap(nullptr,sz,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    // draw based on title: browser/files get real UI, else gradient
    if (strcmp(title,"browser")==0 || strcmp(title,"Browser")==0) {
        for(int i=0;i<w*h;i++) pixels[i]=0xFF2A4A6B;
        // URL bar
        for(int y=8;y<28;y++) for(int x=8;x<w-8;x++) pixels[y*w+x]=0xFFFFFFFF;
        const char* url="https://google.com";
        for(size_t i=0;i<strlen(url);i++) for(int dy=0;dy<8;dy++) for(int dx=0;dx<8;dx++){
            int px=14+i*8+dx, py=12+dy;
            if(px<w&&py<h) pixels[py*w+px]=0xFF000000;
        }
        const char* ph="[internal browser - custom compositor, no Wayland]";
        for(size_t i=0;i<strlen(ph)&&i<50;i++) for(int dy=0;dy<8;dy++) for(int dx=0;dx<8;dx++){
            int px=14+(i%40)*8+dx, py=40+(i/40)*10+dy;
            if(px<w&&py<h) pixels[py*w+px]=0xFFEBDBB2;
        }
    } else if (strcmp(title,"files")==0 || strcmp(title,"Files")==0) {
        for(int i=0;i<w*h;i++) pixels[i]=0xFF3A3A2A;
        // list home
        const char* home=getenv("HOME"); if(!home) home="/";
        std::string cmd="ls -1 \""; cmd+=home; cmd+="\" 2>/dev/null | head -30";
        FILE* f=popen(cmd.c_str(),"r");
        char line[256]; int y=12;
        while(f && fgets(line,sizeof(line),f) && y<h-12){
            line[strcspn(line,"\n")]=0;
            for(size_t i=0;i<strlen(line)&&i<60;i++) for(int dy=0;dy<8;dy++) for(int dx=0;dx<8;dx++){
                int px=12+i*8+dx, py=y+dy;
                if(px<w&&py<h) pixels[py*w+px]=0xFFEBDBB2;
            }
            y+=10;
        }
        if(f) pclose(f);
    } else {
        for(int y=0;y<h;y++) for(int x=0;x<w;x++){
            uint8_t r=(x*255)/w, g=(y*255)/h, b=128;
            pixels[y*w+x]= (0xFFu<<24)|(r<<16)|(g<<8)|b;
        }
        const char* msg="Viewsun Custom App - GUI inside compositor!";
        for(size_t i=0;i<strlen(msg) && i<40;i++){
            for(int dy=0;dy<8;dy++) for(int dx=0;dx<8;dx++) if((i%2)==0) {
                int px=20+i*12+dx, py=20+dy;
                if(px<w&&py<h) pixels[py*w+px]=0xFFFFFFFF;
            }
        }
    }
    client.createBuffer(winId,fd,w,h,stride);
    client.damage(winId,0,0,w,h);
    printf("custom_app: buffer sent, waiting for frames (Super+P to quit viewsun)\n");
    // simple event loop - wait for input/frame
    while(true){
        ViewsunHeader hdr; std::vector<char> pl;
        if(client.pollEvent(hdr,pl)){
            if(hdr.opcode==ViewsunOpcode::InputKey) printf("custom_app: input key\n");
            if(hdr.opcode==ViewsunOpcode::Frame) {
                // could redraw
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 0;
}
