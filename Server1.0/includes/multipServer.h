#ifndef PROCESSPOOL
#define PROCESSPOOL

#include <sys/types.h>
#include "utils.h"
#include <wait.h>
#include <iostream>
#include <signal.h>
#include "./Timer.h"
#include "./http_conn.h"
#include "./Epoller.h"
#include "./conn_pool.h"
#include "./http_conn.h"

using namespace std;

#define TIMESLOT 60


class child_process{
public:
    pid_t processid;

    /*与父进程通信的文件描述符*/
    int m_fd[2];

    child_process():processid(-1){}

};

/*
使用单例模式，保证只会创建出一个线程池
*/

class multipServer{

    private:
        /*进程池中的最大进程数量*/
        static const int MAX_PROCESS_NUMBER=16;
        /*每个进程最多能处理的客户数量*/
        static const int MAX_USER_PER_PROCESS=2048;

        /*epoll最多能监听的连接数量*/
        static const int MAX_EPOLL_NUM=2048;

        /*当前总共有多少个进程*/
        int all_process;
        int listen_fd;

        /*子进程在池中的序号，从0开始*/
        int process_id;
        /*子进程通过m_stop来决定是否停止运行*/
        int m_stop;

        /*进程池中记录着所有子进程的信息*/
        child_process* all_child_process;

        /*唯一的进程池*/
        static multipServer* unique_multipServer;
        
        /*私有构造函数，实现单例模式*/
        multipServer(int listenfd,int process_count=8);

        /*设置信号管道，统一事件源*/
        void set_up_sig_pipe();

        /*子进程运行函数*/
        void run_child();

        /*父进程运行函数*/
        void run_parent();

        /*每一个多进程服务器需要一个Epoller*/
        Epoller epoller;

        //处理信号的管道
        static int sig_fd[2];

        
        /*信号处理函数，将信号值发送给管道sig_fd[1],这样的话可以监听sig_fd[0]上的读事件来统一事件源*/
        static void sig_handler(int sig_num);

    
    public:
        /*通过此函数来获取指向唯一的进程池对象的指针*/
        static multipServer* create(int listenfd,int process_num=8){
            if(!unique_multipServer){
                unique_multipServer=new multipServer(listenfd,process_num);
            }
            return unique_multipServer;
        }

        /*析构函数，释放所有子进程资源*/
        ~multipServer(){
            delete [] all_child_process;
        }

        /*获取子进程id*/
        int get_process_id(){
            return process_id;
        }

        /*运行进程池*/
        void run();
};




#endif