#ifndef UTILS
#define UTILS

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
using namespace std;


//callback function
//cut off  timeout client connection


void sys_error(char* error_info,int error_num);


void add_sig(int sig_num,void (*handler)(int),bool restart=true);

#endif