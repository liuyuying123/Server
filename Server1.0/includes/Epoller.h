

#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h> /*epoll_ctl()*/
#include <fcntl.h> /*fcntl()*/
#include <unistd.h> /*close()*/
#include <vector>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include "./utils.h"
#include <iostream>


class Epoller{
public:
    explicit Epoller(int maxEvents=1024);
    ~Epoller();

    /*设置epoll_Fd_*/
    void setEpoller(int epollfd);
    /*将文件描述符fd加入到epoll监控列表*/
    bool addFd(int fd,uint32_t events);

    /*修描文件描述符fd对应的事件*/
    bool modFd(int fd, uint32_t events);

    /*将文件描述符fd从epoll监控列表中移除*/
    bool delFd(int fd);

    /*设置文件描述符为非阻塞*/
    void setUnblock(int fd);

    /*用于返回epoll监控的记过，成功时发到你会就绪的文件描述符的个数*/
    int wait(int timewait=-1);

    /*根据序号i，获得第i个就绪的fd*/
    int getEventFd(size_t i) const;

    /*获取events的函数*/
    uint32_t getEvents(size_t i) const;

private:
    int epoll_Fd_; /*epoll文件描述符*/
    std::vector<struct epoll_event> events_; /*就绪的事件*/

};

#endif
