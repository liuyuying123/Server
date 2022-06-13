
#include "../includes/utils.h"
#include <iostream>
using namespace std;

void sys_error(char* error_info,int error_num){
    perror(error_info);
    exit(error_num);
}


void add_sig(int sig_num,void (*handler)(int),bool restart){
    struct sigaction sig;
    bzero(&sig,sizeof(sig));
    if(sigfillset(&sig.sa_mask)!=0){
        sys_error("sigfillset",-1);
    }
    sig.sa_handler=handler;
    if(restart){
        sig.sa_flags|=SA_RESTART;
    }
    
    if(sigaction(sig_num,&sig,nullptr)!=0){
        sys_error("sigaction",-1);
    }
    
}

