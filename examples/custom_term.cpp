#include "../lib/viewsun_client.h"
#include "../include/terminal.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

// Custom terminal as Wayland-like client - PTY inside viewsun window
int main(){
    ViewsunClientLib client;
    if(!client.connect()) return 1;
    uint32_t winId;
    client.createWindow(640,480,"custom_term",&winId);
    int w=640,h=480,stride=w*4;
    char shmName[64]; snprintf(shmName,sizeof(shmName),"/viewsun-term-%d",getpid());
    int fd=shm_open(shmName,O_CREAT|O_RDWR,0600);
    ftruncate(fd,w*h*4);
    uint32_t* pixels=(uint32_t*)mmap(nullptr,w*h*4,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    // draw term background
    for(int i=0;i<w*h;i++) pixels[i]=0xFF1D2021;
    client.createBuffer(winId,fd,w,h,stride);
    client.damage(winId,0,0,w,h);
    printf("custom_term: window %d\n", winId);
    while(true) sleep(1);
}
