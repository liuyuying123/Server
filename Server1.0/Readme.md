# 单Reactor/多线程模型实现，同时包含定时器处理超时连接  
设置信号管道，统一事件源：set_up_sig_pipe()函数
processpool的构造函数，创建进程，并将子进程中的process_id设置成子进程对应的序号，父进程中的process_id=-1
all_child_process是保存所有子进程信息的child_process数组
socketpair创建的是双向的管道，所以在父进程中关闭fd[1],在子进程中关闭fd[0]
  在创建子进程的过程中，如果创建后在父进程中返回，则关闭fd[1]，然后继续创建子进程；如果是在子进程中返回，则关闭fd[0]，将processid设置成该子进程的序号i,然后退出构造函数。  

  在每个进程中，sig_pipe是独有的，子进程和父进程各自分别执行set_up_sig_pipe()函数来实现统一事件源。当进程收到指定信号时，信号处理函数将该信号值发送到管道的写端，这样进程通过监听管道读端上的可读事件就可以像处理正常事件那样处理信号了。每个进程中的epollfd也是特有的。
将信号加入统一事件源，创建一个add_sig函数，将信号处理函数和信号进行绑定  

  当父进程创建完子进程后，建立信号管道，然后将信号管道和监听socket都加入到自己的epoll_fd当中，当有新连接到的时候从子进程中去选择一个进程来accpet这个连接socket,通过父子进程之间通信的管道来传递，进程的选择使用Round Robin算法，也就是轮流选择子进程。  

  父子进程都调用run函数，根据process_id来判断执行run_parent()还是执行run_child(),在父进程中，process_id=-1，在子进程中process_id是子进程在进程池当中的编号。在run_parent()函数中，父进程等待新连接到达并将新连接交给某个特定的子进程去处理；在run_child()函数中，子进程接收到父进程从通信管道传来的信息，去accpet接受一个新连接，并将这个连接加入到自己的epoll_fd中进行监听，子进程还负责监听连接socket上的读事件，处理请求后将消息发送给客户端；子进程也要统一事件源来处理信号。  
  超时连接应该在子进程当中处理。

接下来处理父进程收到的信号：  
当父进程收到SIGINT或者SIGTERM信号时，代表要关闭服务器，于是父进程杀掉所有的子进程。依次向还存活着的子进程发送SIGTERM信号。
当父进程收到SIGCHLD信号时，代表有子进程已经退出了，所以循环waitpid子进程，将all_child_process中的子进程process_id设置为-1，并关闭和子进程进行通信的文件描述符。如果所有的子进程都退出了，则父进程也停止并退出。  

在子进程中，先建立信号管道，然后将与父进程进行通信的管道加入监听。process_id是本子进程在进程池当中的序号。  
如果是父进程有信息送达，则将commn_fd中的信息读走，但是这个信息不重要；然后去accpet监听socket,将新到的连接socket加入epoll监听中。并初始化这个客户连接，在子进程中，每个子进程都维护一个users数组，每一个user代表一个客户连接，其中包含epoll_fd,client_fd和client_addr。每个客户连接中包含处理客户需求的代码。users的下标代表client_fd,这样方便查找。  
如果是信号送达，如果子进程收到SIGINT或者SIGTERM信号，表示子进程应该终止，子进程关闭所有与客户端的连接，并释放users数组，关闭相应的文件描述符。 
如果是连接socket有信息送达，则调用user的process函数来处理客户请求。   
最后要关闭资源。  定时器应该加在哪里呢？？？  
## 定时器的实现  
负责处理客户连接的子进程需要处理定时事件，也是统一事件源的方式，当收到SIGALARM信号时要执行定时器的tick()函数，当收到新连接时，给新连接设一个定时器，并将定时器加入时间轮，当客户连接在定时器到时之前发送来新数据，则更新该连接的定时时间。加入定时信号

处理定时信号，定时信号应该不紧急，我们设立一个有定时信号到达的标志，在处理定时信号的时候只将该标志设置为1，等到所有事件都处理完了过后再处理定时事件。  
在表示一个客户连接的user_data当中应该包含一个定时器。
子进程的时间轮

如果子进程收到父进程发送的信息时，在accpet了客户端连接过后，要为这个新连接创建一个定时器并加入时间轮当中。  
如果客户连接又到达了，则更新该连接的定时器，具体做法是先将原来的定时器删除，再为这个连接重新创建一个定时器  
还的有一个断开连接的回调函数，当tick()函数删除所有超时连接的时候，会调用。在这个函数当中，我们将连接socket从epoll监听队列中删除，并将这个连接对应的users数组中的client_fd设置为-1;
静态变量必须要初始化
参数的默认值只能在申明处定义。
## 记录出错
- 在Delete_timer函数中，当删除节点是头节点的时候，如果删除过后头节点为nullptr，则不能将此头节点的pre置为nullptr,不然会出错，在多进程的情况下很难debug!
- debug了两天，怎么说呢，解析http请求的逻辑很重要，还有就是读写事件的操作逻辑，epolloneshot在多进程的环境下依旧逃不开，要等到收到了数据，处理了过后才可以注册socket上的可写事件，然后再异步写数据到socket。
- 静态变量和成员不要放在头文件当中，并且类中的静态成员一定要在类外进行初始化























