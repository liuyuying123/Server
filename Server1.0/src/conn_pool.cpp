#include "../includes/conn_pool.h"
#include <iostream>
using namespace std;
Conn_Pool* Conn_Pool::Unique_connpool_=nullptr;

void Conn_Pool::init(){

    /*先创建conn_count_个连接*/
    for(int i=0;i<conn_count_;i++){
        conns_.emplace(make_shared<Conn>());
        
    }

    /*创建一个时间轮*/
    timeWheel_=new TimeWheel;

}

/*析构函数，清理资源*/
Conn_Pool::~Conn_Pool(){
    delete timeWheel_;
}

shared_ptr<Conn> Conn_Pool::allocate(int client_fd){
    cout<<"分配连接："<<client_fd<<endl;
    auto free_p=*(conns_.begin());
    /*将free_p从conns_中移除*/
    conns_.erase(free_p);
    mappings_[client_fd]=free_p;
   /*在此时初始化连接的socket_fd,以及缓冲区等*/
    free_p->init(client_fd);
    return free_p;
}


void Conn_Pool::Return(int client_fd){
    if(mappings_.find(client_fd)==mappings_.end()){
        sys_error("Not a valid connection",-1);
    }
    auto free_p=mappings_[client_fd];
    mappings_.erase(client_fd);
    conns_.insert(free_p);/*不在这里操作epollfd*/
}

shared_ptr<Conn> Conn_Pool::getConn(int fd){
    if(!mappings_[fd]){
        /*如果此连接不是合法连接*/
        sys_error("Not a valid connection",-1);
    }
    return mappings_[fd];
}


void Conn_Pool::closeConn(int fd){
    cout<<"关闭连接："<<fd<<endl;
    Return(fd);
    close(fd);
    deleteTimer(fd);
}

void Conn_Pool::addTimer(int fd,time_t timeout){
    Timer* newTimer=timeWheel_->add_Timer(timeout);
    newTimer->client_fd_=fd;
    allTimers_[fd]=newTimer;
}

void Conn_Pool::deleteTimer(int fd){
    if(allTimers_.find(fd)!=allTimers_.end()){

        /*还要删除时间轮中对应的定时器*/
        timeWheel_->delete_Timer(allTimers_[fd]);

        /*删除allTimers_中对应的数据*/
        allTimers_.erase(fd);
    }
}


void Conn_Pool::tick(Epoller& epoller){
    /*获得所有超时连接*/
    auto Fds=timeWheel_->tick();
    for(auto fd:Fds){
        cout<<fd;
        closeConn(fd);
        epoller.delFd(fd);
    }
    cout<<endl;
}