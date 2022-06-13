#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "./includes/utils.h"
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <vector>
#include <iostream>
using namespace std;
#define SERVER_PORT 8000
#define MAXLINE 128



int main(){
    char* my_ip="192.168.15.151";
    sockaddr_in server_address;
    inet_pton(AF_INET,my_ip,&server_address.sin_addr);
    char address[32];
    server_address.sin_family=AF_INET;
    //inet_addr("127.0.0.1");
    server_address.sin_port=htons(SERVER_PORT);

    int client_fd=socket(AF_INET,SOCK_STREAM,0);

    int ret=connect(client_fd,(sockaddr*)&server_address,sizeof(server_address));
    if(ret!=0){
        cout<<"errno: "<<errno<<endl;
    }
    char write_buf[MAXLINE];
    char read_buf[MAXLINE];
    while(1){
        bzero(write_buf,MAXLINE);
        bzero(read_buf,sizeof(read_buf));
        // cout<<"user input: ";
        cin>>write_buf;
        //cout<<write_buf<<endl;
        cout<<errno<<endl;
        if(strcmp(write_buf,"q")==0){
        
            exit(0);
        }
        int n=send(client_fd,write_buf,strlen(write_buf),0);
        cout<<write_buf<<endl;

    }
    return 0;
}