#include "../includes/http_conn.h"
#include <iostream>
using namespace std;

/*定义HTTP响应的一些状态信息*/
const char* ok_200_title="OK";
const char* error_400_title="Bad Request";
const char* error_400_content="Your request has bad syntax or is inherently impossible to satisify.\n";
const char* error_403_title="Forbidden";
const char* error_403_content="You don't have permission to get file from this server.\n";
const char* error_404_title="Not Found";
const char* error_404_content="The requested file was not found on this server.\n";
const char* error_500_title="Internal Error";
const char* error_500_content="There was an unusual problem serving the requested file.\n";

const char* server_root="/var/www/resources";



void Conn::init(int cli_fd){
    /*如果读写缓冲区为空，则分配资源*/
    if(!read_buf_&&!write_buf_){
        read_buf_=new char[max_read_buf_]();
        write_buf_=new char[max_write_buf_]();
    }

    client_fd_=cli_fd;

    /*初始化http状态等信息为初始状态*/
    init();
}

/*
    解析读缓冲区中的一行，一个字符一个字符解析
    m_checked_index_:表示正在解析的字符
    m_read_index_: 已经读入缓冲区的字符的下一个位置
*/
Conn::LINE_STATUS Conn::parse_line(){
    char temp;
    for(;m_checked_index_<m_read_index_;++m_checked_index_){
        temp=read_buf_[m_checked_index_];
        if(temp=='\r'){

            /*如果当前检测的字符是读缓冲区的最后一个字符*/
            if(m_checked_index_+1==m_read_index_){
                return LINE_OPEN;
            }
            else if(read_buf_[m_checked_index_+1]=='\n'){
                read_buf_[m_checked_index_++]='\0';
                read_buf_[m_checked_index_++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp=='\n'){
            /*处理上一次读了\r的情况，上次\n还未读入缓冲区中*/
            if(m_checked_index_>1 && (read_buf_[m_checked_index_-1]=='\r')){
                read_buf_[m_checked_index_-1]='\0';
                read_buf_[m_checked_index_++]='\0';
                return LINE_OK;
            }
            else{
                return LINE_BAD;
            }

        }
    }
    /*这次读取的过程中没有发现"\r"或者"\n" */
    return LINE_OPEN;
}

/*
非阻塞读，将连接socker上的内容读到读缓冲区中
返回值为是否成功阻塞读
当读出错或者对方主动关闭连接时返回false
*/
bool Conn::unblock_read(){
    
    if(m_read_index_>=max_read_buf_){
        return false;
    }

    int read_size=0;
    while(true){
        read_size=recv(client_fd_,read_buf_+m_read_index_,max_read_buf_-m_read_index_,0);
        if(read_size==-1){
            if(errno==EAGAIN||errno==EWOULDBLOCK){
                break;
            }
            return false;
        }
        else if(read_size==0){ /*对方主动关闭连接*/
            return false;
        }
        m_read_index_+=read_size;
    }
    cout<<"读入的数据："<<read_buf_<<endl;
    return true;
}

/*
解析请求行，获得请求方法、目标url以及http版本号 
*/
Conn::HTTP_CODE Conn::parse_request_line(char* text){

    /*size_t strspn(const char *str1, const char *str2) 检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标*/
    
    m_url_=strpbrk(text," \t");
   
    if(!m_url_){
        return BAD_REQUEST;
    }

    *m_url_='\0';
    m_url_++;
     
    char* method=text;
   
    /*目前只能处理GET请求*/
    if(strcasecmp(method,"GET")==0){
        cur_method_=GET;
    }
    else{
        return BAD_REQUEST;
    }

    /*找到m_url中第一个不是\t的字符的下标*/
    m_url_+=strspn(m_url_," \t");

    m_version_=strpbrk(m_url_," \t");
    if(!m_version_){
        return BAD_REQUEST;
    }
    *m_version_='\0';
    m_version_++;

    /*跳过\t*/
    m_version_+=strspn(m_version_," \t");
    if(strcasecmp(m_version_,"HTTP/1.1")!=0){
        return BAD_REQUEST;
    }

    if(strncasecmp(m_url_,"HTTP://",7)==0){
        m_url_+=7;
        m_url_=strchr(m_url_,'/');
    }
   
    if(m_url_[0]!='/'||!m_url_){
        return BAD_REQUEST;
    }

    curr_check_state_=CHECK_STATE_HEADER;
    return NO_REQUEST;
}

/*
    解析http请求的一个头部字段
    目前只支持Connection、Content-Length、Host:
*/
Conn::HTTP_CODE Conn::parse_headers(char* text){

    /*如果遇到了空行，则表示请求头的所有字段都已经解析完成了，如果有请求内容的话就继续解析请求内容，否则返回解析结束*/
    if(text[0]=='\0'||strlen(text)==0){
       
        if(m_content_length_!=0){
            curr_check_state_=CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        else{
            return GET_REQUEST;
        }
    }
    
    /*Connection*/
    if(strncasecmp(text,"Connection:",11)==0){
        text+=11;
        text+=strspn(text," \t");
        if(strncasecmp(text,"Keep-alive",10)==0){
            m_linger_=true;
        }
    }

    /*Content-Length*/
    else if(strncasecmp(text,"Content-Length:",15)==0){
        text+=15;
        text+=strspn(text," \t");
        m_content_length_=atol(text);
        
    }
    else if(strncasecmp(text,"Host:",5)==0){
        text+=5;
        text+=strspn(text," \t");
        m_host_=text;
    }
    else{
        cout<<"ooop! unknow header: "<<text<<endl;
    }

    return NO_REQUEST;
    
}

/*
解析请求内容，这里只判断内容被读完了没有

*/
Conn::HTTP_CODE Conn::parse_content(char* text){
    if(m_read_index_>=(m_checked_index_+m_content_length_)){
        text[m_content_length_]='\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;

}


/*
解析http请求，主状态机
*/
Conn::HTTP_CODE Conn::process_read(){
    LINE_STATUS line_stu=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;
    char* text=0;
    /*一次异步读数据很可能没有读完，没有读完时从状态机处于NO_REQUEST状态*/
    while(((curr_check_state_==CHECK_STATE_CONTENT)&&line_stu==LINE_OK)||((line_stu=parse_line())==LINE_OK)){
        text=get_line();/*一行*/
        start_line_=m_checked_index_;
        switch(curr_check_state_){
            case(CHECK_STATE_REQUESTLINE):{
                ret=parse_request_line(text);
                if(ret==BAD_REQUEST){
                    return BAD_REQUEST;
                }
                break;
            }
            case(CHECK_STATE_HEADER):{
                ret=parse_headers(text);
                if(ret==GET_REQUEST){
                    return do_request();
                }
                else if(ret==BAD_REQUEST){
                    return BAD_REQUEST;
                }
                break;
            }
            case(CHECK_STATE_CONTENT):{
                ret=parse_content(text);
                if(ret==GET_REQUEST){
                    return do_request();
                }
                /*表示请求正文还没有被读取完，但是这时候读缓冲区是没有读满的,还要继续读*/
                line_stu=LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }

    return NO_REQUEST;

}

Conn::HTTP_CODE Conn::do_request(){
    
    strncpy(request_real_file_,server_root,strlen(server_root));
    
    int len=strlen(server_root);
    strncpy(request_real_file_+len,m_url_,strlen(m_url_));

    int ret=stat(request_real_file_,&file_stat_);
    if(ret<0){
        return NO_RESOURCE;
    }
    /*如果其他用户对此文件没有读权限*/
    if(!(file_stat_.st_mode&S_IROTH)){
        return FORBIDDEN_REQUEST;
    }

    /*如果此文件是个目录*/
    if(S_ISDIR(file_stat_.st_mode)){
        return BAD_REQUEST;
    }

    /*将文件内容mmap进内存*/
    int fd=open(request_real_file_,O_RDONLY);
    mmaped_file_address_=(char*)mmap(NULL,file_stat_.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    if(mmaped_file_address_==MAP_FAILED){
        return INTERNAL_ERROR;
    }
    close(fd);
    /*文件成功mmap进内存，等待发送*/
    return FILE_REQUEST;

}


void Conn::unmap(){
    if(mmaped_file_address_){
        if(munmap(mmaped_file_address_,file_stat_.st_size)!=0){
            sys_error("munmap",-1);
        }
        mmaped_file_address_=nullptr;
    }
}

/*
利用变长参数将响应头、响应行以及响应内容都添加到发送缓存中
*/
bool Conn::add_response(const char* format,...){
    if(m_write_numbers_>=max_write_buf_){
        return false;
    }
    va_list arg_list;
    va_start(arg_list,format);
    int len=vsnprintf(write_buf_+m_write_numbers_,max_write_buf_-m_write_numbers_-1,format,arg_list);
    if(len>=max_write_buf_-m_write_numbers_-1){
        return false;
    }
    m_write_numbers_+=len;
    va_end(arg_list);
    return true;
}

bool Conn::add_content(const char* content){
    return add_response("%s",content);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              
}

bool Conn::add_status_line(int status,const char* title){
    return add_response("%s %d %s \r\n","HTTP/1.1",status,title);
}

bool Conn::add_headers(int content_length){
    return add_content_length(content_length)&&add_linger()&&add_blank_line();
}

bool Conn::add_content_length(int content_length){
    return add_response("Content-Length: %d\r\n",content_length);
}

bool Conn::add_linger(){
    return add_response("Connection: %s\r\n",(m_linger_==true)?"Keep-Alive":"close");
}

bool Conn::add_blank_line(){
    return add_response("%s","\r\n");
}

bool Conn::process_write(HTTP_CODE ret){
    switch(ret){
        case INTERNAL_ERROR:{
            add_status_line(500,error_500_title);
            add_headers(strlen(error_500_content));
            if(!add_content(error_500_content)){
                return false;
            }
            break;
        }
        case BAD_REQUEST:{
            add_status_line(400,error_400_title);
            add_headers(strlen(error_400_content));
            if(!add_content(error_400_content)){
                return false;
            }
            break;
        }
        case NO_RESOURCE:{
            add_status_line(404,error_404_title);
            add_headers(strlen(error_404_content));
            if(!add_content(error_404_content)){
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST:{
            add_status_line(403,error_403_title);
            add_headers(strlen(error_403_content));
            if(!add_content(error_403_content)){
                return false;
            }
            break;
        }
        case FILE_REQUEST:{
            add_status_line(200,ok_200_title);
            if(file_stat_.st_size!=0){
                add_headers(file_stat_.st_size);
                m_iv_[0].iov_base=write_buf_;
                m_iv_[0].iov_len=m_write_numbers_;
                m_iv_[1].iov_base=mmaped_file_address_;
                m_iv_[1].iov_len=file_stat_.st_size;
                iv_count_=2;
            
                return true;
            }
            else{
                const char* ok_string="<html><body> </body></html>";
                add_headers(strlen(ok_string));
                if(!add_content(ok_string)){
                    return false;
                }
            }
            break;
        }
        default:{
            return false;
        }

    }
    m_iv_[0].iov_base=write_buf_;
    m_iv_[0].iov_len=m_write_numbers_;
    iv_count_=1;
    return true;
}

/*
非阻塞写操作
传入一个Epoller的引用也是为了修改监听事件
返回值为false表示需要关闭该连接
*/
bool Conn::unblock_write(Epoller& epoller){
    int temp=0;
    int bytes_have_write=0;
    int bytes_have_not_write=m_write_numbers_;

    if(bytes_have_not_write==0){
        /*这个连接请求的数据已经发送完了*/
        return true;
    }

    cout<<"in unblock_write: m_write_numbers: "<<m_write_numbers_<<endl;

    while(1){
        temp=writev(client_fd_,m_iv_,iv_count_);
        /*如果此时的tcp写缓冲已经满了，等待tcp缓冲有空间再继续发送*/
        if(temp<=-1){
            if(errno==EAGAIN){
                epoller.modFd(client_fd_,EPOLLOUT|EPOLLONESHOT|EPOLLRDHUP|EPOLLET);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_not_write-=temp;
        bytes_have_write+=temp;

        /*此时表示请求行、请求头都已经发送出去了*/
        if(bytes_have_not_write<=bytes_have_write){

            unmap();
            /*判断是否保持连接*/
            if(m_linger_){
                init();/*重新初始化这个连接，以便接受下一次的请求消息*/
                epoller.modFd(client_fd_,EPOLLIN); /*等待这个连接的下一次数据到来*/
                return true;
            }
            else{
                return false; /*返回false则表示应该关闭这个连接*/
            }

        }
    }

}

/*
    重新初始化连接状态，http请求初始化以及将读写缓冲区清空
*/
void Conn::init(){

    curr_check_state_=CHECK_STATE_REQUESTLINE;
    m_linger_=false;
    cur_method_=GET;
    m_url_=0;
    m_version_=0;
    m_content_length_=0;
    m_host_=0;
    start_line_=0;
    m_checked_index_=0;
    mmaped_file_address_=nullptr;
    m_read_index_=0;
    m_write_numbers_=0;
    bzero(read_buf_,sizeof(read_buf_));
    bzero(write_buf_,sizeof(write_buf_));
    bzero(request_real_file_,sizeof(request_real_file_));
}

int Conn::get_client_fd(){
    return client_fd_;
}

/*
处理客户请求
传一个Epoller的引用是为了修改监听的事件
返回值为false代表需要关闭这个连接
*/
bool Conn::process(Epoller& epoller){
    HTTP_CODE read_ret;
    read_ret=process_read();
    /*如果没有读完一个完整的HTTP请求，则再次注册读事件继续读*/
    if(read_ret==NO_REQUEST){
        epoller.modFd(client_fd_,EPOLLIN|EPOLLONESHOT|EPOLLRDHUP|EPOLLET);
        return true;/*如果消息还没有到完的话，则直接返回,等到又有数据到达了再次读入*/
    }

    bool write_ret=process_write(read_ret);
    
    /*如果处理写出错的话，要让连接池关闭这个连接*/
    if(!write_ret){
        return false;
    }
    epoller.modFd(client_fd_,EPOLLOUT|EPOLLONESHOT|EPOLLRDHUP|EPOLLET);
    cout<<"process end"<<endl;
    return true;
}

