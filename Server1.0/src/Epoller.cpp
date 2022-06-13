#include "../includes/Epoller.h"

Epoller::Epoller(int maxEvents):epoll_Fd_(-1),events_(maxEvents){}

void Epoller::setEpoller(int epollfd){
    epoll_Fd_=epollfd;
    assert(epoll_Fd_>=0 && events_.size()>0);
}

Epoller::~Epoller(){
    close(epoll_Fd_);
}

bool Epoller::addFd(int fd, uint32_t events){
    if(fd<0) return false;
    epoll_event ev={0};
    ev.data.fd=fd;
    ev.events=events;
    return epoll_ctl(epoll_Fd_,EPOLL_CTL_ADD,fd,&ev)==0;
}

bool Epoller::modFd(int fd, uint32_t events){
    if(fd<0)  return false;
    epoll_event ev={0};
    ev.data.fd=fd;
    ev.events=events|EPOLLET|EPOLLONESHOT;
    return epoll_ctl(epoll_Fd_,EPOLL_CTL_MOD,fd,&ev)==0;
}

bool Epoller::delFd(int fd){
    if(fd<0) return false;
    epoll_event ev={0};
    return epoll_ctl(epoll_Fd_,EPOLL_CTL_DEL,fd,&ev)==0;
}


void Epoller::setUnblock(int fd){
    int old_mode=fcntl(fd,F_GETFL);
    int new_mode=old_mode|O_NONBLOCK;

    if(fcntl(fd,F_SETFL,new_mode)!=0){
        sys_error("fcntl",-1);
    }
}

 int Epoller::wait(int timewait){
     return epoll_wait(epoll_Fd_,&events_[0],static_cast<int>(events_.size()+1),timewait);
 }

int Epoller::getEventFd(size_t i) const{
    assert(i<events_.size() && i>=0);
    return events_[i].data.fd;
}

uint32_t Epoller::getEvents(size_t i) const{
    assert(i<events_.size() && i>=0);
    return events_[i].events;
}
