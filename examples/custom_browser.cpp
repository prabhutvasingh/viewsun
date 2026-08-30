#include "../lib/viewsun_client.h"
#include "../include/stb_truetype.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>

// Custom browser from scratch - no Wayland, pure viewsun SHM
// Fetches via curl, strips HTML, renders with TTF inside tile

static std::string fetchUrl(const std::string &url){
    std::string cmd="curl -sL --max-time 5 '"+url+"' 2>/dev/null | head -c 8000";
    FILE* f=popen(cmd.c_str(),"r");
    if(!f) return "fetch failed";
    char buf[8001]={0};
    size_t n=fread(buf,1,8000,f);
    pclose(f);
    std::string s(buf,n);
    // strip HTML tags crudely
    std::string out; bool inTag=false;
    for(char c: s){
        if(c=='<') inTag=true;
        else if(c=='>') inTag=false;
        else if(!inTag && c!='\r') out.push_back(c);
    }
    // collapse whitespace
    std::string clean;
    for(char c: out) if(c=='\n' || c=='\t') clean.push_back(' '); else clean.push_back(c);
    if(clean.size()>3000) clean=clean.substr(0,3000);
    if(clean.empty()) clean="(empty or no network)";
    return clean;
}

int main(int argc,char**argv){
    std::string url = argc>1?argv[1]:"https://example.com";
    if(url=="browser") url="https://example.com";
    ViewsunClientLib client;
    if(!client.connect()) return 1;
    uint32_t winId;
    client.createWindow(800,600,"browser",&winId);
    int w=800,h=600,stride=w*4;
    char name[64]; snprintf(name,sizeof(name),"/viewsun-browser-%d",getpid());
    int fd=shm_open(name,O_CREAT|O_RDWR,0600);
    ftruncate(fd,w*h*4);
    uint32_t* px=(uint32_t*)mmap(nullptr,w*h*4,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    // fetch
    std::string body=fetchUrl(url);
    auto draw=[&](bool first){
        for(int i=0;i<w*h;i++) px[i]=0xFFF0F0F0;
        // URL bar
        for(int y=6;y<26;y++) for(int x=6;x<w-6;x++) px[y*w+x]=0xFFFFFFFF;
        for(int y=6;y<26;y++){ px[y*w+6]=0xFF999999; px[y*w+w-7]=0xFF999999; }
        for(int x=6;x<w-6;x++){ px[6*w+x]=0xFF999999; px[25*w+x]=0xFF999999; }
        // crude text draw using 8x8 (TTF not needed for placeholder, use block)
        auto drawText=[&](int x,int y,const std::string &s,uint32_t col){
            for(size_t i=0;i<s.size()&& x+i*7 < w-12;i++) for(int dy=0;dy<7;dy++) for(int dx=0;dx<6;dx++){
                if(s[i]!=' '){ int p=(y+dy)*w + x+i*7+dx; if(p>=0&&p<w*h) px[p]=col; }
            }
        };
        drawText(10,10,url,0xFF000000);
        // body wrapped
        int x=10,y=32, colW=w-20;
        std::string cur;
        for(char c: body){
            if(c==' ' || cur.size()>80){
                if(x+cur.size()*7 > w-10){ x=10; y+=10; }
                if(y>h-20) break;
                drawText(x,y,cur,0xFF222222);
                x+=cur.size()*7+7;
                cur.clear();
                if(c!=' ') cur.push_back(c);
            } else cur.push_back(c);
        }
        if(!cur.empty()) drawText(x,y,cur,0xFF222222);
        // footer
        drawText(10,h-12,"Custom Viewsun Browser - no Wayland, pure SHM",0xFF666666);
    };
    draw(true);
    client.createBuffer(winId,fd,w,h,stride);
    client.damage(winId,0,0,w,h);
    printf("custom_browser: %s window %d\n", url.c_str(), winId);
    while(true){
        ViewsunHeader hdr; std::vector<char> pl;
        if(client.pollEvent(hdr,pl)){
            if(hdr.opcode==ViewsunOpcode::InputKey){
                // Enter fetches again, etc
            }
        }
        usleep(100000);
    }
}
