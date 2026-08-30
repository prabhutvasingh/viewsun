#define STB_TRUETYPE_IMPLEMENTATION
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
    // load TTF for sharp text (JetBrainsMono)
    stbtt_fontinfo font; unsigned char* ttfBuf=nullptr; float scale=0; int ascent=0;
    auto loadFont=[&]()->bool{
        const char* p="/usr/share/fonts/TTF/JetBrainsMonoNerdFontMono-Regular.ttf";
        FILE*f=fopen(p,"rb"); if(!f) return false;
        fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
        ttfBuf=(unsigned char*)malloc(sz); fread(ttfBuf,1,sz,f); fclose(f);
        if(!stbtt_InitFont(&font, ttfBuf,0)) return false;
        scale=stbtt_ScaleForPixelHeight(&font,13);
        int descent,lineGap; stbtt_GetFontVMetrics(&font,&ascent,&descent,&lineGap);
        return true;
    };
    bool hasFont=loadFont();
    auto drawTextTTF=[&](int x,int y,const std::string &s,uint32_t col){
        if(!hasFont){
            for(size_t i=0;i<s.size()&& x+(int)i*7 < w-12;i++) for(int dy=0;dy<7;dy++) for(int dx=0;dx<6;dx++){
                if(s[i]!=' '){ int p=(y+dy)*w + x+i*7+dx; if(p>=0&&p<w*h) px[p]=col; }
            }
            return;
        }
        uint8_t sr=(col>>16)&0xFF, sg=(col>>8)&0xFF, sb=col&0xFF;
        int curX=x;
        for(char c: s){
            if(curX+8 >= w-10) break;
            if(c<32||c>=127){ curX+=7; continue; }
            int w2,h2,xoff,yoff; unsigned char* bmp=stbtt_GetCodepointBitmap(&font,0,scale,c,&w2,&h2,&xoff,&yoff);
            int yoff2 = (int)(ascent*scale)+yoff;
            for(int row=0;row<h2;row++) for(int col2=0;col2<w2;col2++){
                int a=bmp[row*w2+col2]; if(a==0) continue;
                int fx=curX+xoff+col2, fy=y+yoff2+row;
                if(fx<0||fx>=w||fy<0||fy>=h) continue;
                uint32_t dst=px[fy*w+fx]; uint8_t dr=(dst>>16)&0xFF,dg=(dst>>8)&0xFF,db=dst&0xFF;
                uint8_t nr=(sr*a+dr*(255-a))/255, ng=(sg*a+dg*(255-a))/255, nb=(sb*a+db*(255-a))/255;
                px[fy*w+fx]=(0xFFu<<24)|(nr<<16)|(ng<<8)|nb;
            }
            stbtt_FreeBitmap(bmp,nullptr);
            int adv,lsb; stbtt_GetCodepointHMetrics(&font,c,&adv,&lsb);
            curX+=(int)(adv*scale);
        }
    };
    std::string body=fetchUrl(url);
    auto draw=[&](bool first){
        for(int i=0;i<w*h;i++) px[i]=0xFFF0F0F0;
        for(int y=6;y<26;y++) for(int x=6;x<w-6;x++) px[y*w+x]=0xFFFFFFFF;
        for(int y=6;y<26;y++){ px[y*w+6]=0xFF999999; px[y*w+w-7]=0xFF999999; }
        for(int x=6;x<w-6;x++){ px[6*w+x]=0xFF999999; px[25*w+x]=0xFF999999; }
        drawTextTTF(10,10,url,0xFF000000);
        int x=10,y=32;
        std::string cur;
        for(char c: body){
            if(c==' ' || cur.size()>60){
                int tw=0; for(char cc: cur){ int adv,lsb; stbtt_GetCodepointHMetrics(&font,cc,&adv,&lsb); tw+=(int)(adv*scale); }
                if(x+tw > w-10){ x=10; y+=14; }
                if(y>h-20) break;
                drawTextTTF(x,y,cur,0xFF222222);
                x+=tw+7;
                cur.clear();
                if(c!=' ') cur.push_back(c);
            } else cur.push_back(c);
        }
        if(!cur.empty()) drawTextTTF(x,y,cur,0xFF222222);
        drawTextTTF(10,h-12,"Custom Viewsun Browser - no Wayland, pure SHM",0xFF666666);
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
