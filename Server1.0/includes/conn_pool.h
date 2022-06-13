#ifndef CONNPOOL
#define CONNPOOL


#include <memory>
#include <map>
#include <set>
#include "utils.h"
#include <unistd.h>
#include "Timer.h"
#include "http_conn.h"

/*
连接池负责管理一个工作进程中的所有tcp连接，并且处理超时连接
使用单例模式，只能创建一个连接池
map<int,Conn*>为listen fd->Conn*的映射
set<Conn*> 保存了连接池中所有Conn数据的地址
使用智能指针来保存Conn
给每个一连接分配一个时间轮定时器，使用map<int,timmer>来保存
*/

class Conn_Pool{

public:
    /*静态函数*/
    static Conn_Pool* create(int num=2000){
        if(!Unique_connpool_){
            Unique_connpool_=new Conn_Pool(num);
        }

        return Unique_connpool_;
    }

    ~Conn_Pool();

    /*判断当前fd是否是在mappings中*/
    bool fdExist(int fd){
        if(mappings_.find(fd)!=mappings_.end()){
            return true;
        }
        return false;
    }

    /*取出一个空闲连接，分配给刚刚accept的连接socket,返回连接*/
    shared_ptr<Conn> allocate(int client_fd);

    /*通过fd获取某个连接*/
    shared_ptr<Conn> getConn(int fd);

    void closeConn(int fd);

    /*给这个连接分配一个定时器*/
    void addTimer(int fd,time_t timeout);

    /*删除指定连接的定时器*/
    void deleteTimer(int fd);

    /*连接池中实现tick函数提供外层接口*/
    void tick(Epoller& epoller);


private:
    Conn_Pool(int num=2000):conn_count_(num){
        init();
    }

    static Conn_Pool* Unique_connpool_;

    /*一个连接池中默认有2000个连接*/
    int conn_count_;

    /*在使用的连接*/
    map<int,shared_ptr<Conn>> mappings_;

    /*连接池中有一个时间轮来处理超时连接*/
    TimeWheel* timeWheel_;

    /*保存定时器信息，它会自动析构啊*/
    map<int,Timer*> allTimers_;

    /*未使用的连接，当连接断开时，要将它加到conns当中*/
    set<shared_ptr<Conn>> conns_;

    /*创建conn_count个空闲连接*/
    void init();


    /*如果client fd断开，要将连接返回给空闲连接*/
    void Return(int client_fd);
    
};


#endif