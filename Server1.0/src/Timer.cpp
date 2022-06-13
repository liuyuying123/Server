#include "../includes/Timer.h"
#include <iostream>
using namespace std;


/*向时间轮中添加一个定时器*/
Timer* TimeWheel::add_Timer(time_t timeout){
    int all_slot=0;
    if(timeout<0){
        return nullptr;
    }
    if(timeout<=SI){
        all_slot=1;
    }
    else{
        all_slot=timeout/SI;
    }
    int this_rotation=all_slot/N;
    int this_slot=(cur_slot_+(all_slot%N))%N;
    Timer* new_Timer=new Timer(this_rotation,this_slot);

    /*add this timer into the TimeWheel*/
    if(Timers_[this_slot]){
        new_Timer->next_=Timers_[this_slot];
        Timers_[this_slot]->pre_=new_Timer;
        Timers_[this_slot]=new_Timer;
    }
    else{
        Timers_[this_slot]=new_Timer;
    }
    return new_Timer;
}


vector<int> TimeWheel::tick(){
    Timer* temp=Timers_[cur_slot_];
    vector<int> Fds;
    while(temp){
        if(temp->rotation_>0){
            temp->rotation_--;
            temp=temp->next_;
            continue;
        }
        else{
            /*将要删除的连接fd都返回*/
            Fds.push_back(temp->client_fd_);
            
            /*接下来删除该定时器*/

            /*if temp is head node*/
            if(temp==Timers_[cur_slot_]){
                Timers_[cur_slot_]=Timers_[cur_slot_]->next_;
                delete temp;
                temp=Timers_[cur_slot_];
            }
            else{

                /*temp not is head node*/
                Timer* temp2=temp;
                temp->pre_->next_=temp->next_;
                if(temp->next_){
                    temp->next_->pre_=temp->pre_;
                }
                temp=temp->next_;
                delete temp2;
            }
        }
    }
    
    cur_slot_=(++cur_slot_)%N;
    return Fds;
    
}

TimeWheel::~TimeWheel(){

    /*delete all the timmer*/
    for(int i=0;i<N;i++){
        Timer* temp=Timers_[i];
        while(temp){
            Timer* temp2=temp;
            temp=temp->next_;
            delete temp2;
        }
    }
}

void TimeWheel::delete_Timer(Timer* timer){

    /*delete a specific timer from TimeWheel*/
    int this_slot=timer->slot_;
    Timer* temp=Timers_[this_slot];
    if(timer==temp){

        /*if timer is head*/
        Timers_[this_slot]=temp->next_;
        if(Timers_[this_slot]){
            Timers_[this_slot]->pre_=nullptr;
        }
        delete temp;
        return;
    }
    else{
        while(temp){
            if(temp!=timer){
                temp=temp->next_;
            }
            else{
                temp->pre_->next_=temp->next_;
                if(temp->next_){
                    temp->next_->pre_=temp->pre_;
                }
                delete temp;
                return ;
            }
        }
    }
}