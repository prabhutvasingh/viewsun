#include "viewsun_client.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <sys/uio.h>
#include <fcntl.h>
#include <vector>

bool ViewsunClientLib::connect(){
    fd=socket(AF_UNIX,SOCK_STREAM,0);
    if(fd<0) return false;
    sockaddr_un addr{}; addr.sun_family=AF_UNIX; strncpy(addr.sun_path, VIEWSUN_SOCKET, sizeof(addr.sun_path)-1);
    if(::connect(fd,(sockaddr*)&addr,sizeof(addr))<0){ close(fd); fd=-1; return false; }
    return true;
}
void ViewsunClientLib::disconnect(){ if(fd>=0) close(fd); fd=-1; }

static bool sendMsg(int fd, ViewsunOpcode op, const void* data, uint32_t size, int fdToSend=-1){
    ViewsunHeader hdr{op,size};
    iovec iov[2]; iov[0]={&hdr,sizeof(hdr)}; iov[1]={(void*)data,size};
    int cnt=size?2:1;
    if(fdToSend>=0){
        char cmsgbuf[CMSG_SPACE(sizeof(int))];
        msghdr msg{}; msg.msg_iov=iov; msg.msg_iovlen=cnt; msg.msg_control=cmsgbuf; msg.msg_controllen=sizeof(cmsgbuf);
        cmsghdr* cmsg=CMSG_FIRSTHDR(&msg); cmsg->cmsg_level=SOL_SOCKET; cmsg->cmsg_type=SCM_RIGHTS; cmsg->cmsg_len=CMSG_LEN(sizeof(int));
        memcpy(CMSG_DATA(cmsg),&fdToSend,sizeof(int)); msg.msg_controllen=cmsg->cmsg_len;
        return sendmsg(fd,&msg,0)==(ssize_t)(sizeof(hdr)+size);
    } else {
        msghdr msg{}; msg.msg_iov=iov; msg.msg_iovlen=cnt;
        return sendmsg(fd,&msg,0)==(ssize_t)(sizeof(hdr)+size);
    }
}

bool ViewsunClientLib::createWindow(uint32_t w,uint32_t h,const char* title,uint32_t *outId){
    ViewsunCreateWindow req{}; req.width=w; req.height=h; strncpy(req.title,title,sizeof(req.title)-1);
    if(!sendMsg(fd,ViewsunOpcode::CreateWindow,&req,sizeof(req))) return false;
    // wait for WindowCreated
    ViewsunHeader hdr; std::vector<char> payload;
    // simple recv
    if(recv(fd,&hdr,sizeof(hdr),MSG_WAITALL)!=sizeof(hdr)) return false;
    if(hdr.opcode!=ViewsunOpcode::WindowCreated) return false;
    std::vector<char> pl(hdr.size);
    if(hdr.size) recv(fd,pl.data(),hdr.size,MSG_WAITALL);
    auto *ev=(ViewsunWindowCreated*)pl.data();
    *outId=ev->id;
    return true;
}
bool ViewsunClientLib::createBuffer(uint32_t winId,int shmFd,uint32_t w,uint32_t h,uint32_t stride){
    ViewsunCreateBuffer req{}; req.window_id=winId; req.width=w; req.height=h; req.stride=stride;
    return sendMsg(fd,ViewsunOpcode::CreateBuffer,&req,sizeof(req),shmFd);
}
bool ViewsunClientLib::damage(uint32_t winId,int x,int y,uint32_t w,uint32_t h){
    ViewsunDamage req{winId,x,y,w,h};
    return sendMsg(fd,ViewsunOpcode::Damage,&req,sizeof(req));
}
bool ViewsunClientLib::setTitle(uint32_t winId,const char* title){
    ViewsunSetTitle req{}; req.window_id=winId; strncpy(req.title,title,sizeof(req.title)-1);
    return sendMsg(fd,ViewsunOpcode::SetTitle,&req,sizeof(req));
}
bool ViewsunClientLib::pollEvent(ViewsunHeader &hdr,std::vector<char> &payload){
    // non-blocking
    int flags=fcntl(fd,F_GETFL,0); fcntl(fd,F_SETFL,flags|O_NONBLOCK);
    ssize_t n=recv(fd,&hdr,sizeof(hdr),0);
    if(n!=sizeof(hdr)) return false;
    if(hdr.size){ payload.resize(hdr.size); recv(fd,payload.data(),hdr.size,MSG_WAITALL); }
    else payload.clear();
    return true;
}
bool ViewsunClientLib::waitFrame(){
    // blocking wait for Frame
    ViewsunHeader hdr;
    if(recv(fd,&hdr,sizeof(hdr),MSG_WAITALL)!=sizeof(hdr)) return false;
    if(hdr.opcode==ViewsunOpcode::Frame){
        if(hdr.size) { std::vector<char> tmp(hdr.size); recv(fd,tmp.data(),hdr.size,MSG_WAITALL); }
        return true;
    }
    return false;
}
