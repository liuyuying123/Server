#ifndef HTTPCONN
#define HTTPCONN

#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string>
#include <sys/mman.h>
#include <sys/uio.h>
#include <stdarg.h>
#include "Epoller.h"
#include "./utils.h"
using namespace std;

/*
一个表示用户连接的class中包含一个定时器
用户连接当中保存对应的连接socket，对方主机ip地址等信息
在第一次使用时再分配资源
*/
class Conn
{
public:
    Conn():client_fd_(-1){
        init_p();
    }

    /*初始化时，将读写缓冲区指针赋值为nullptr*/
    void init_p(){
        read_buf_=nullptr;
        write_buf_=nullptr;
    }

    string show_ip(){
        char ip[32];
        inet_ntop(AF_INET,&client_address_.sin_addr.s_addr,ip,32);
        return string(ip);
    }

     /*重新使用该连接，重新设置client socket,如果读写缓冲指针为空的话，第一次给该Conn分配资源*/
    void init(int cli_fd);
    
    /*获取连接fd*/
    int get_client_fd();


private:
    int client_fd_;  /*connection fd*/
    sockaddr_in client_address_;

    void init();/*清除缓冲区中原本的数据，为了复用此连接*/



/*处理客户的http请求，使用有限状态自动机。以下是专门解析http请求相关的变量和函数*/
public:
    /*读缓冲区的大小*/
    static const int max_read_buf_=2048;
    /*写缓冲区的大小*/
    static const int max_write_buf_=1024;
    /*请求文件的最长文件名*/
    static const int max_filename_=200;
    /*HTTP请求方法*/
    enum METHODS {GET=0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT,PATCH};
    /*解析客户请求时，主状态机所处的状态*/
    enum CHECK_STATE{CHECK_STATE_REQUESTLINE=0,CHECK_STATE_HEADER,CHECK_STATE_CONTENT};
    /*服务器处理HTTP请求的可能结果,NO_REQUEST表示请求不完整*/
    enum HTTP_CODE{NO_REQUEST,GET_REQUEST,BAD_REQUEST,NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,INTERNAL_ERROR,CLOSED_CONNECTION};
    /*行的读取状态*/
    enum LINE_STATUS{LINE_OK=0,LINE_BAD,LINE_OPEN};
   
    /*当连接socket可读时，使用该函数进行处理*/
    bool process(Epoller& epoller);
   
    /*非阻塞读操作*/
    bool unblock_read();
    /*非阻塞写操作*/
    bool unblock_write(Epoller& epoller);


private:
    /*读缓冲区*/
    char* read_buf_;
    /*标识读缓冲区中已经读入的客户数据的最后一个字节的下一个位置*/
    int m_read_index_;
    /*当前正在分析的字符在读缓冲区中的位置*/
    int m_checked_index_;
    /*当前正在解析的行的起始位置*/
    int start_line_;
    /*写缓冲区*/
    char* write_buf_;
    /*写缓冲区中待发送的字节数*/
    int m_write_numbers_;    
    /*主状态机当前所处的状态*/
    CHECK_STATE curr_check_state_;
    /*当前请求方法*/
    METHODS cur_method_;

    /*客户请求的目标文件的完整路径 root+m_url,root是网站根目录*/
    char request_real_file_[max_filename_];
    /*文件名*/
    char* m_url_;
    /*http协议版本号*/
    char* m_version_;
    /*主机名*/
    char* m_host_;
    /*http请求的消息体的长度*/
    int m_content_length_;
    /*http请求是否要保持连接*/
    bool m_linger_;
    /*客户请求的目标文件被mmap到内存中的起始位置*/
    char* mmaped_file_address_;
    /*目标文件的状态*/
    struct stat file_stat_;
    /*采用writev来执行写操作，iv_count表示被写内存块的数量*/
    struct iovec m_iv_[2];
    int iv_count_;

    /*解析http请求*/
    HTTP_CODE process_read();
    /*填充http应答*/
    bool process_write(HTTP_CODE ret);

    /*以下函数被process_read函数调用以分析HTTP请求*/

    /*解析请求行，获得请求方法、目标url以及http版本号 */
    HTTP_CODE parse_request_line(char* text);
    /*解析http请求的一个头部字段*/
    HTTP_CODE parse_headers(char* text); 
    /*解析http的请求正文*/
    HTTP_CODE parse_content(char* text);
    /*执行http请求，在这里是判断用户想要获取的静态资源是否在服务器上，并且用户是否有权限获取，
    如果有权限的话同时将文件mmap进内存中等待发送*/
    HTTP_CODE do_request();
    /*获得一行开始的起始位置*/
    char* get_line(){
        return read_buf_+start_line_;
    }
    /*解析一行*/
    LINE_STATUS parse_line();

    /*以下函数被process_write函数调用以填充HTTP应答*/
    /*在发送完指定文件后将其unmap*/
    void unmap();
    /*将response添加到发送缓冲区中*/
    bool add_response(const char* format,...);
    /*将文件内容添加到发送缓冲区中*/
    bool add_content(const char* content);
    /*向发送缓冲区添加状态行*/
    bool add_status_line(int status,const char* title);
    /*向发送缓冲区添加状态头*/
    bool add_headers(int content_length);
    /*向发送缓冲区添加正文长度*/
    bool add_content_length(int content_length);
    /*向发送缓冲区添加linger信息*/
    bool add_linger();
    /*向发送缓冲区添加空行*/
    bool add_blank_line();
};

#endif
