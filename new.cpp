/*
There exists bugs in my IDE that Chinese inputing is usually unavailable,
I write note using english instand.
Forgive my Chinglish please.^v^
*/

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

#include<thread>
#include<mutex>

#include <sys/epoll.h>
#ifdef __FreeBSD__
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include<iostream>
#include<queue>
#include"co_routine.cpp"
#define PTHREADNUM 1
#include"mempool.h"

#define LIBCO_ENV 1
#include "udpSocket.cpp"

using namespace mpnsp;
using namespace std;
void debug(char const *c,int i)
{
    static mutex debug_m;
    debug_m.lock();
    printf("%s%d\n",c,i);
    debug_m.unlock();
}
#define MAXUSERNUM 1024*1024
//#define MAXNAMELEN 36
#define AGAINTIMES 3
#define MAX_PASSWD_LEN 40
#define MAX_NUMBER_LEN 10

class pthreadNode_t;
class client_t;
class datebase_t;

#include"coLib.cpp" //包含链表操作、socket收发
inline void coTimeoutSleep_my(int ms)
{
    struct pollfd fds;
    fds.events=0;
    fds.fd=-1;
    co_poll(co_get_epoll_ct(),&fds,1,ms);
}
//#include"coStructPthread.cpp"
client_t* echoNumber2Ptr[MAXUSERNUM];

class pthreadNode_t
{
    link_t<client_t*> *clientLink;           //=(link_t<client_t*>*)malloc(sizeof(link_t<client_t*>));
    thread *pSelf;
    int id;
    stCoRoutine_t *eventSource;
    // co_poll(co_get_epoll_ct(),&pf,1,-1);//yield   同时关心epoll事件，和1000ms的超时事件
    pthread_mutex_t _mutex_signal=PTHREAD_MUTEX_INITIALIZER;
    void addNewFd();
    void pthreadIntFunc(int n)
    {
        int e=errno;
        cout<<"there is a signal:"<<n<<endl;
    }
public:
    pthreadNode_t()=delete;
    link_t<int> *newUserLink;
    int totalUserNum;       //number of all the client with all status.
    int pipefd[2];  //通知有中断或者新成员了，如果是新成员data.fd=-1，中断data.fd=中断号
    mutex plock;
    void lock()
    {
        debug("lock : ",id);
        plock.lock();
    }
    void unlock(int flag=1)
    {
        if(!flag)
            return;
        debug("unlock : ",id);
        plock.unlock();
    }
    bool trylock()
    {
        return plock.try_lock();
    }
    static pthread_cond_t cond;
    pthreadNode_t(int id)
    {
        this->id=id;
        clientLink=(link_t<client_t*>*)malloc(sizeof(link_t<client_t*>));
        clientLink->head=clientLink->tail=NULL;
        totalUserNum=0;
        newUserLink=(link_t<int>*)malloc(sizeof(link_t<int>));
        newUserLink->head=newUserLink->tail=NULL;
        eventSource=(stCoRoutine_t*)malloc(sizeof(stCoRoutine_t));
        int ret=pipe(pipefd);//1 write ; 0 read
        if(ret==-1)
        {
            myErrorOperate("thread.create.pipe.failed",__LINE__);
        }
        pthread_mutex_trylock(&_mutex_signal);  //init the mutex of cond (set lock)
        pSelf=new thread(&pthreadNode_t::runFunc,this);
    }
    static void startAll()
    {
        cout<<"start all"<<endl;
        pthread_cond_broadcast(&cond);
    }
    void stop()
    {
        debug("stop",id);
        pthread_cond_wait(&cond,&_mutex_signal);
        debug("stop.end:",id);
    }
    void runFunc()
    {
        co_create(&eventSource,NULL,eventSourceFunc,(void*)this);
        co_resume(eventSource);
        debug("thread",id);
        lock();
        unlock();
        lock(); //wait for lock is free
        unlock();
        cout<<"thread.runFunc.eventloop"<<endl;
        co_eventloop(co_get_epoll_ct(),NULL,NULL);
    }
    static void* eventSourceFunc(void* args)
    {
        co_enable_hook_sys();
        pthreadNode_t *self=(pthreadNode_t*)args;
        int data;
        int ret;
        struct pollfd fds;
        while(1)
        {
            cout<<"read"<<endl;
            self->checkUserNum();
            cout<<"eventSourceFunc.checkUserNum.end"<<endl;
            fds.events=POLLIN|POLLERR|POLLHUP;
            fds.fd=self->pipefd[0];
            fds.revents=0;
            co_poll(co_get_epoll_ct(),&fds,1,-1);
            cout<<"co_poll.end"<<endl;
            if(fds.revents==0)
            {
                continue;
            }
            if(fds.revents!=POLLIN)
            {
                myErrorOperate("co_poll.revents!=POLLIN",__LINE__,0);
                continue;
            }
            ret=read(self->pipefd[0],(void*)&data,sizeof(int));
            cout<<"read.datain------"<<endl;
            if(ret<=0)
                continue;
            if(data==-1)
            {
                cout<<"eventSourceFunc.addNewFd"<<endl;
                self->addNewFd();
            }
            else
            {
                cout<<"eventSourceFunc.pthreadIntFunc"<<endl;
                self->pthreadIntFunc(data);
            }
        }
    }
    void checkUserNum()
    {
        return;
        while(1)
        {   cout<<"lock"<<endl;
            lock();
        cout<<"locked"<<endl;
            if(totalUserNum==0 && newUserLink->head==NULL)
            {
                debug("check user num.stop : id=",id);
                plock.unlock();
                stop();
                continue;
            }
            unlock();
            break;
        }
        cout<<"check user num.end"<<endl;
    }
    void addClientOffline(int number);
};
pthread_cond_t pthreadNode_t::cond=PTHREAD_COND_INITIALIZER;
mutex gmutex;
class rwlock_t
{
    pthread_rwlock_t rwlock;
public:
    rwlock_t()
    {
        pthread_rwlockattr_t attr;
        pthread_rwlockattr_init(&attr);
        pthread_rwlockattr_setkind_np (&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);//prefer_writer_nonrecursive_np
        int ret=pthread_rwlock_init(&rwlock, &attr);
        if(ret)
        {
            myErrorOperate("rwlock.init.failed",__LINE__,1);
        }
    }
    bool rlock()
    {
        return !pthread_rwlock_rdlock(&rwlock);
    }
    bool wlock()
    {
        return !pthread_rwlock_wrlock(&rwlock);
    }
    bool unlock()
    {
        return pthread_rwlock_unlock(&rwlock);
    }
    bool rtryLock()
    {
        return !pthread_rwlock_tryrdlock(&rwlock);
    }
    bool wtryLock()
    {
        return !pthread_rwlock_trywrlock(&rwlock);
    }
};
rwlock_t rwlock_echoNumber2Ptr;
#include"client_t.cpp"  //每个客户对应的类型
void pthreadNode_t::addClientOffline(int number) //pthread must be locked.
{
    rwlock_echoNumber2Ptr.wlock();
    if(echoNumber2Ptr[number])
    {
        rwlock_echoNumber2Ptr.unlock();
        return;
    }
    debug("addClientOffline:",number);
    linkNode_t<client_t*>*cNode;
    client_t *client;
    client=allocNode<client_t>(id);
    cNode=allocNode<linkNode_t<client_t*>>(id);
    cNode->data=client;
    bool locked=trylock();
    AddHead(clientLink,cNode);

    echoNumber2Ptr[number]=client;
    totalUserNum++;
    client->clientCreate(number,id,cNode,this);
    unlock(locked);
    cout<<"addClientOffline.end"<<endl;
}
void pthreadNode_t::addNewFd()
{
    debug("addNewFd",id);
    lock();
    linkNode_t<int>*node=newUserLink->head;
    linkNode_t<int>*p;
    linkNode_t<client_t*>*cNode;
    client_t *client;
    int fd;
    while(node)
    {
        cout<<"-------------add node "<<endl;
        client=allocNode<client_t>(id);
        cNode=allocNode<linkNode_t<client_t*>>(id);
        fd=node->data;

        cNode->data=client;
        RemoveFromLink<linkNode_t<int>,link_t<int>>(node);
        unlock();
        totalUserNum++;
        client->clientInit(fd,id,cNode,this); //启动客户
        lock();
        AddHead(clientLink,cNode);
        node=newUserLink->head;
    }
    unlock();
    cout<<"addNewNode.end"<<endl;
}

pthreadNode_t *pNode[PTHREADNUM];
thread *listenThread;
int listenfd;
void* listener()
{
    co_enable_hook_sys();
    cout<<"listener.thread.created"<<endl;
    listenfd=socket(AF_INET,SOCK_STREAM,0);
    if(listenfd<0)
    {
        myErrorOperate("listener.socket.failed",__LINE__);
    }
    int val=1;
    int ret=setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,(void*)&val,sizeof(int));
    if(ret<0)
    {
        close(listenfd);
        myErrorOperate("listener.setsocketopt.failed",__LINE__);
    }
    struct sockaddr_in saddr={0};
    saddr.sin_family=AF_INET;
    saddr.sin_port=htons(8001);
    saddr.sin_addr.s_addr=htonl(INADDR_ANY);
    ret=bind(listenfd,(struct sockaddr*)&saddr,sizeof(struct sockaddr));
    if(ret<0)
    {
        myErrorOperate("listener.bind.failed",__LINE__);
    }
    ret=listen(listenfd,20);
    if(ret<0)
    {
        myErrorOperate("listener.listen.failed",__LINE__);
    }
    gmutex.unlock();
    int fd;
    unsigned int saddr_len;
    int minv,mini;
    while(1)
    {
        saddr_len=sizeof(saddr);
        fd=accept(listenfd,(struct sockaddr*)&saddr,&saddr_len);
        debug("ac.fd=-------------------------",fd);
        minv=INT_MAX;
        for(int i=0;i<PTHREADNUM;i++)
        {
            if(pNode[i]->totalUserNum < minv)
            {
                minv=pNode[i]->totalUserNum;
                mini=i;
            }
        }
        debug("listener.mini=",mini);
        pNode[mini]->lock();
        linkNode_t<int>*p=allocNode<linkNode_t<int>>(mini);
        p->data=fd;
        p->pLink=NULL;
        AddTail(pNode[mini]->newUserLink,p);
        int v=-1;
        write(pNode[mini]->pipefd[1],&v,sizeof(int));
        pNode[mini]->unlock();
        if(minv==0)
        {
            pthreadNode_t::startAll();
        }
        debug("listener.minv=",minv);
        cout<<pNode[mini]->newUserLink->head<<endl;
    }
}
void mainInit()
{
    redis_db=new redisDB_t();

    struct sigaction sa={0};    //这三行:设置向已关闭socket发送信息时默认行为。防止向已关闭socket发送数据导致服务器崩溃;
    sa.sa_handler = SIG_IGN;
    //忽略剩下的两个成员：
    //sa_flags , sa_mask
    sigaction( SIGPIPE, &sa, 0 );
    cout<<"create thread..."<<endl;
    for(int i=0;i<PTHREADNUM;i++)
    {
        pNode[i]=new pthreadNode_t(i);
    }
    //By now,all worker threads have been created.So create the listener thread is safty.
    gmutex.lock();
    listenThread = new thread(listener);
    gmutex.lock(); //wait for listener init.
    gmutex.unlock();
    cout<<"init complete."<<endl;
}
int main()
{
    char a[1000];
    mainInit();
    //main thread listen
 //   while()
    int op;
    string s;
    while(1)
    {
        op=getchar();
        if(!isdigit(op))
        {
            cout<<"illegal input"<<endl;
            while(getchar()!='\n');
            continue;
        }
        op-='0';
        if(op>=0 && op<PTHREADNUM)
        {
            printf("Thread:%d\n",op);
            debug("the number of user in this pthread is:",pNode[op]->totalUserNum);
            printf("link.head=%p\n",pNode[op]->newUserLink->head);
            printf("link=%p\n",pNode[op]->newUserLink);
            cout<<echoNumber2Ptr[0]<<endl;
            cout<<echoNumber2Ptr[1]<<endl;
        }
        else
        {
            debug("illegal number:",op);
        }
        while(getchar()!='\n');
    }
}
