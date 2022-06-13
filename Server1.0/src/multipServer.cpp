#include "../includes/multipServer.h"

/*类的静态成员必须在类外初始化*/
multipServer* multipServer::unique_multipServer=nullptr;
int multipServer::sig_fd[2]={0};

void multipServer::sig_handler(int sig_num){
            int old_erron=errno;
            int sig_msg=sig_num;
            if(send(sig_fd[1],(char*)&sig_msg,1,0)<0){
                sys_error("send sig_num",-1);
            }
            errno=old_erron;
        }

/*
构造函数，创建进程，并将子进程中的process_id设置成子进程对应的序号，父进程中的process_id=-1
*/
multipServer::multipServer(int listenfd,int process_count):listen_fd(listenfd),all_process(process_count),process_id(-1),m_stop(false),epoller(Epoller(MAX_EPOLL_NUM)){
    if(all_process>MAX_PROCESS_NUMBER){
        cout<<"Too much process";
        exit(-1);
    }

    /*all_child_process是保存所有子进程信息的child_process数组*/
    all_child_process=new child_process[all_process];
    for(int i=0;i<all_process;i++){
        if(socketpair(AF_LOCAL,SOCK_STREAM,0,all_child_process[i].m_fd)!=0){
            sys_error("socketpair",-1);
        }
        /*创建子进程*/
        all_child_process[i].processid=fork();
        if(all_child_process[i].processid>0){
            close(all_child_process[i].m_fd[0]);
            continue;
        }
        else if(all_child_process[i].processid==0){
            close(all_child_process[i].m_fd[1]);
            process_id=i;
            break;
        }
    }
}

/*
设置信号管道，统一事件源
*/
void multipServer::set_up_sig_pipe(){
    if(socketpair(AF_UNIX,SOCK_STREAM,0,multipServer::sig_fd)!=0){
        sys_error("create sig_pipe",-1);
    }
    epoller.addFd(multipServer::sig_fd[0],EPOLLIN|EPOLLOUT|EPOLLET);
    /*注册信号的回调函数*/
    epoller.setUnblock(multipServer::sig_fd[0]);
    epoller.setUnblock(multipServer::sig_fd[1]);
    add_sig(SIGINT,sig_handler);
    add_sig(SIGTERM,sig_handler);
    add_sig(SIGCHLD,sig_handler);
    add_sig(SIGPIPE,SIG_IGN);
}

/*根据process_id来判断是父进程还是子进程，然后分别调用不同的运行函数*/
void multipServer::run(){
    if(process_id==-1){
        run_parent();
        return;
    }
    run_child();
}

void multipServer::run_child(){

    cout<<"in child()"<<endl;
    /*在子进程中创建epollfd，并添加给epoller*/
    epoller.setEpoller(epoll_create(10));
    /*建立信号管道，并注册信号回调函数*/
    set_up_sig_pipe();
    /*在子进程中维护一个连接池,总共有MAX_USER_PER_PROCESS个可用连接*/
    Conn_Pool* ConnPool=Conn_Pool::create(MAX_USER_PER_PROCESS);
    
    int time_alarm=0;
    
    /*加入定时信号*/
    add_sig(SIGALRM,sig_handler);

    /*与父进程通信的管道*/
    int commn_fd=all_child_process[process_id].m_fd[0];
    epoller.addFd(commn_fd,EPOLLIN|EPOLLOUT|EPOLLET);
    epoller.setUnblock(commn_fd); /*将与父进程通信的管道设置成非阻塞的*/
    sockaddr_in client_addr;
    alarm(TIMESLOT);
    int readyNum=0;
    while(!m_stop){
        /*等到事件发生*/
        readyNum=epoller.wait(-1);
        cout<<"ready_num: "<<readyNum<<endl;
        if(readyNum<0&&errno!=EINTR){
            sys_error("epoll_wait",-1);
        }
        for(int event_i=0;event_i<readyNum;event_i++){
            int currFd=epoller.getEventFd(event_i);
            uint32_t currEvents=epoller.getEvents(event_i);

            /*如果是父进程有信息送达*/
            if((currFd==commn_fd) && (currEvents&EPOLLIN)){
                int buf;
                cout<<"new conn"<<endl;
                /*将commn_fd中的信息读走，但是这个信息不重要，是为了让子进程来accept一个新连接*/
                int ret=recv(commn_fd,&buf,sizeof(buf),0);
                if((ret<0&&errno!=EAGAIN)||ret==0){
                    continue;
                }

                bzero(&client_addr,sizeof(client_addr));
                socklen_t addr_len=sizeof(client_addr);
                int client_fd=accept(listen_fd,(sockaddr*)&client_addr,&addr_len);
                if(client_fd<0){
                    cout<<"accpet faild"<<endl;
                    continue;
                }
                cout<<"accept success"<<endl;

                /*有新连接到来，则从连接池中给它分配一个连接,并初始化这个连接*/
                ConnPool->allocate(client_fd);
                epoller.addFd(client_fd,EPOLLIN|EPOLLOUT|EPOLLET|EPOLLRDHUP);
                epoller.setUnblock(client_fd);
                
                /*给这个连接分配一个定时器*/
                ConnPool->addTimer(client_fd,3*TIMESLOT);
            }

            /*如果是信号送达*/
            else if((currFd==multipServer::sig_fd[0])&&(currEvents&EPOLLIN)){
                char sig_buf[128];
                int ret=recv(multipServer::sig_fd[0],sig_buf,sizeof(sig_buf),0);
                if((ret<0&&errno!=EAGAIN)||ret==0){
                    continue;
                }
                /*遍历所有得到的信号值*/
                for(int sig_i=0;sig_i<ret;sig_i++){
                    switch(sig_buf[sig_i]){
                        case SIGINT:
                        case SIGTERM:{
                            /*子进程停止运行*/
                            m_stop=true;
                            break; 
                        }
                        case SIGCHLD:{
                            int stat;
                            int child_pid;
                            while(child_pid=waitpid(0,&stat,WNOHANG)>0){
                                continue;
                            }
                            break;
                        }
                        case SIGALRM:{
                            /*处理定时信号,超时连接由连接池处理*/
                            time_alarm=1;
                            break;
                        }
                        default:break;
                    }
                }
            }

            /*如果是对方主动关闭连接*/
            else if(ConnPool->fdExist(currFd) && currEvents&EPOLLRDHUP){
                cout<<"EPOLLRDHUP关闭连接 ";
                ConnPool->closeConn(currFd);
                epoller.delFd(currFd);
            }

            /*如果是连接socket可读*/
            else if(ConnPool->fdExist(currFd) && currEvents&EPOLLIN){
                /*通过连接池获取该连接*/
                auto Conn_p=ConnPool->getConn(currFd); /*Conn_p是一个shared_ptr智能指针*/
                cout<<"Client_fd have message"<<endl;
                /*如果非阻塞读出错，需要关闭连接，关闭连接由连接池在操作,然后epoller将连接fd从epoll监听列表中删除*/
                if(!Conn_p->unblock_read()){
                    cout<<"读出现错误 ";
                    ConnPool->closeConn(currFd);
                    epoller.delFd(currFd);
                    continue;
                }
                
                /*然后处理刚才读的数据*/
                bool process_Ret=Conn_p->process(epoller);
                /*如果处理输入有问题的话，由连接池关闭连接*/
                if(!process_Ret){
                    ConnPool->closeConn(currFd);
                    epoller.delFd(currFd);
                    continue;
                }

                /*更新该连接的定时器，具体做法是先将原来的定时器删除，再为这个连接重新创建一个定时器*/
                ConnPool->deleteTimer(currFd);
                ConnPool->addTimer(currFd,3*TIMESLOT);
            }

            /*如果连接socket可写*/
            else if((ConnPool->fdExist(currFd) && currEvents&EPOLLOUT)){
                auto Conn_p=ConnPool->getConn(currFd); 
                if (!Conn_p->unblock_write(epoller)){
                    
                    /*如果异步写出现错误，需要断开TCP连接*/
                    cout<<"写出现错误 ";
                    ConnPool->closeConn(currFd);
                    epoller.delFd(currFd);
                    continue;
                }
            }
            else{
                continue;
            }

        }

        /*等到所有事件都处理完了过后再处理定时事件*/
        if(time_alarm==1){
            /*为了在处理超时连接的时候从epoller中删除该连接，所以要传一个Epoller的引用*/
            ConnPool->tick(epoller);
            time_alarm=0;
            alarm(TIMESLOT);
        }
    }
    //最后要关闭资源
    close(commn_fd);

}

void multipServer::run_parent(){

    /*在父进程中创建epollfd，并添加给epoller*/
    epoller.setEpoller(epoll_create(5));
    set_up_sig_pipe();
    epoller.setUnblock(listen_fd);
    epoller.addFd(listen_fd,EPOLLIN|EPOLLOUT|EPOLLET);
    int current_process=0;
    int conn=1;
    int readyNum=0;
    while(!m_stop){
        readyNum=epoller.wait(-1);
        if(readyNum<0){
            continue;
        }
        for(int event_i=0;event_i<readyNum;event_i++){
            int currFd=epoller.getEventFd(event_i);
            uint32_t currEvents=epoller.getEvents(event_i);
            
            /*如果是有新连接，则选择一个子进程进行处理*/
            if(currFd==listen_fd && currEvents&EPOLLIN){
                int choice_id=current_process;
                do{
                    if(all_child_process[choice_id].processid!=-1){
                        break;
                    }
                    choice_id=(choice_id+1)%all_process;

                }while(choice_id!=current_process);

                if(all_child_process[choice_id].processid==-1){
                    m_stop=true;
                    break;
                }

                current_process=(choice_id+1)%all_process;
                if(send(all_child_process[choice_id].m_fd[1],(char*)&conn,sizeof(conn),0)<0){
                    sys_error("Main process send message to child process",-1);
                }
            }

            /*接下来处理父进程收到的信号*/
            else if(currFd==multipServer::sig_fd[0] && currEvents&EPOLLIN){
                char sig_value[128];
                int total_sig=read(multipServer::sig_fd[0],sig_value,sizeof(sig_value));
                if(total_sig<=0){
                    continue;
                }
                for(int sig_i=0;sig_i<total_sig;sig_i++){
                    switch(sig_value[sig_i]){
                        case SIGINT:
                        case SIGTERM:{
                            /*父进程杀掉所有的子进程*/
                            for(int child_i=0;child_i<all_process;child_i++){
                                if(all_child_process[child_i].processid!=-1){
                                    kill(all_child_process[child_i].processid,SIGTERM);

                                    /*关闭文件描述符*/
                                    close(all_child_process[child_i].m_fd[1]);
                                }
                            }
                            m_stop=true;
                            break;

                        }
                        case SIGCHLD:{

                            /*有子进程已经退出*/
                            int dead_child_pid;
                            int stat;
                            while(dead_child_pid=waitpid(0,&stat,WNOHANG)>0){
                                for(int child_i=0;child_i<all_process;child_i++){
                                    if(all_child_process[child_i].processid==dead_child_pid){
                                        all_child_process[child_i].processid=-1;
                                        close(all_child_process[child_i].m_fd[1]);
                                    }
                                }
                            }
                            //如果所有的子进程都退出了，则父进程也停止并退出
                            m_stop=true;
                            for(int child_i=0;child_i<all_process;child_i++){
                                if(all_child_process[child_i].processid!=-1){
                                    m_stop=false;
                                    break;
                                }
                            }
                            break;

                        }
                        default:break;
                    }
                }
            }
            else{
                continue;
            }
        }

    }
    close(listen_fd); 
}

