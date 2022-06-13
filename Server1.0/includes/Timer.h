#ifndef TIMESETTER
#define TIMESETTER
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <time.h>
#include "utils.h"

/*
定时器类，
*/
class Timer{
public:
    /*这个timer是哪个连接的*/
    int client_fd_;
    /*rotation*/
    int rotation_;
    /*slot*/
    int slot_;
    Timer* pre_;
    Timer* next_;
    //constructor
    Timer(int rota,int slo):rotation_(rota),slot_(slo),pre_(nullptr),next_(nullptr){}
};

/*
时间轮类
*/
class TimeWheel{
private:
    static const int N=60; /* 60 round */
    static const int SI=60; /* 60s */
    int cur_slot_;  /*current slot*/
    Timer* Timers_[N];
public:
    TimeWheel():cur_slot_(0){
        for(int i=0;i<N;i++){
            Timers_[i]=nullptr;
        }
    }
    
    Timer* add_Timer(time_t timeout);
    std::vector<int> tick(); /*将要删除的fd全部返回*/
    ~TimeWheel();
    void delete_Timer(Timer* timer);
};

#endif

