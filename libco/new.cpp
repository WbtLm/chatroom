#include "co_routine.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <stack>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

#include <sys/epoll.h>
#ifdef __FreeBSD__
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include<iostream>
using namespace std;

struct task_t
{
    stCoRoutine_t *co;
    int fd;
};
typedef struct task_t task_t;

int myErrorOperate(char const * const error_str,int error_line,int error_exit=1)
{
    perror(error_str);
    printf("%d\n",error_line);
    if(error_exit==1)
        exit(1);
    return 0;
}

static void* mcoListen(void *args)
{
    co_enable_hook_sys();
    int lsEpFd=epoll_create(100);
    if(lsEpFd<0)
    {
        myErrorOperate("create listen_epfd err",__LINE__);//exit
    }
    int lsSocketFd;
    if((lsSocketFd=socket(AF_INET,SOCK_STREAM,0))<0)
    {
   //     free(lsEpFd);
        myErrorOperate("create listen_socket fd err.",__LINE__);//exit
    }
    //set socket opt
    int ret,val=1;
    ret=setsockopt(lsSocketFd,SOL_SOCKET,SO_REUSEADDR,(void*)&val,sizeof(val));
        //reuse addr
    if(ret<0)
    {
        myErrorOperate("set SO_REUSEADDR err.",__LINE__,0);
    }

    struct sockaddr_in saddr;
    memset(&saddr,0,sizeof(sockaddr_in));
    saddr.sin_family=AF_INET;
    saddr.sin_addr.s_addr=INADDR_ANY;
    saddr.sin_port=8002;
    ret=bind(lsSocketFd,(struct sockaddr*)&saddr,sizeof(struct sockaddr_in));
    if(ret<0)
    {
        myErrorOperate("lsten socket bind err.",__LINE__,1);//exit
    }
    ret=listen(lsSocketFd,20);//20,backlog  ???
    if(ret<0)
    {
        myErrorOperate("listen err.",__LINE__,1);//exit
    }
    cout<<"Accepting connections..."<<endl;
    socklen_t acSocketFd,saddrLen;
    for(;;)
    {
        saddrLen=sizeof(saddr);
        acSocketFd=accept(lsSocketFd,(struct sockaddr*)&saddr,&saddrLen);
        if(acSocketFd<0)
        {
            myErrorOperate("accept err.",__LINE__,0);
            continue;
        }
    }
}
int main() {
    stCoRoutine_t *coLs;
    co_create(&coLs,NULL,mcoListen,NULL);
    co_resume(coLs);
    cout<<"listen co init complete."<<endl;
    co_eventloop(co_get_epoll_ct(),0,0);
}












