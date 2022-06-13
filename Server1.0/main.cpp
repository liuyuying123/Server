#include "./includes/multipServer.h"
#include "./includes/utils.h"
#include "./includes/Timer.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include "./includes/conn_pool.h"

#define MAX_CLIENTS 1024

int main(int argc,char* argv[]){
    char* my_ip="192.168.15.151";
    sockaddr_in server_address;
    inet_pton(AF_INET,my_ip,&server_address.sin_addr);
    server_address.sin_port=htons(8000);
    server_address.sin_family=AF_INET;
    int server_socket=socket(AF_INET,SOCK_STREAM,0);

    
    if(bind(server_socket,(sockaddr*)&server_address,sizeof(server_address))!=0){
        sys_error("bind",-1);
    }

    //listen
    if(listen(server_socket,32)!=0){
        sys_error("listen",-1);
    }

    multipServer* Server=multipServer::create(server_socket);
    Server->run();
    return 0;


}





