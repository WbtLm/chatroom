#include <iostream>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
using namespace std;
#include "udpSocket.cpp"

#define IP_SERVER "127.1.1.1"  //"118.89.231.196"
#define MAX_PASSWD_LEN 40
struct client_t;
#define MAX_NUMBER_LEN 10

int myErrorOperate(char const * const error_str,int error_line,int error_exit=1)
{
    perror(error_str);
    printf("%d\n",error_line);
    if(error_exit==1)
        exit(1);
    return 0;
}

template<typename dataT>struct link_t;
template<typename dataT>struct linkNode_t;

template<typename dataT>
struct linkNode_t
{
    struct link_t<dataT> *pLink;
    struct linkNode_t *pNext;
    struct linkNode_t *pPrev;
    dataT data;
};
template<typename dataT>
struct link_t
{
    linkNode_t<dataT> *head;
    linkNode_t<dataT> *tail;
};


typedef struct login_t{
    int numb;
    char passwd[28];
}login_t;
typedef struct cTransMsg_t
{
    int from;
    int to;
    int len;
    int type;
    char *buf;
}cTransMsg_t;
int sendToSocket(int fd,char* buff,int len,bool co=0)
{
    if(len==0)
    {
        return 1;
    }
    int idx=0;
    int ret;
    int again=3+1;
    errno=0;
    while(len>idx && again)
    {
        ret=send(fd,buff+idx,len-idx,0);
        if(ret<=0)
        {
            if(errno==EINTR || errno==EAGAIN)
            {
                if(errno==EAGAIN)
                {
                    again--;
                }
                if(co)
                {
             //       co_yield_ct();
                }
                continue;
            }
            return ret;
        }
        idx+=ret;
    }
    return len;
}
int recvFromSocket(int fd,char*buff,int len,bool co=0)
{
    if(len==0)
    {
        return 1;
    }
    memset(buff , 0 , len);
    int idx=0;
    int ret;
    int again=3+1;
    while(len>idx && again)
    {
        errno=0;
        ret=recv(fd,buff+idx,len-idx,MSG_WAITALL);
        if(ret<=0)
        {
            if(errno==EINTR || errno==EAGAIN)
            {
     //           cout<<"rcv err"<<errno<<endl;
                if(errno==EAGAIN)
                {
                    again--;
                }
                if(co)
                {
                 //   co_yield_ct();
                }
                continue;
            }
            return ret;
        }
        idx+=ret;
    }
    if(idx<len || again==0)
    {
        return -1;
    }
    return len;
}
enum clientEm{CLIENT_UNINIT=0,CLIENT_INIT=1,CLIENT_ACTIVE=2,CLIENT_DESTROY=3};
enum msgType{CM_STRING=0,CM_CTRL=1,LOGIN_AC=2,LOGIN_REQUEST,LOGIN_REFUSE,LOGIN_RELOGIN,REGIST_REQUEST,REGIST_AC,REGIST_REFUSE};//普通字符串消息，控制,
struct client_t{
    int status;
    int mfd;
    int number;
    char passwd[28];
    link_t<int>*friendList;
    int friendNum;
    client_t()
    {
        status=CLIENT_UNINIT;
    }
};
client_t client;
pid_t childPid;
int createSocket(int &mfd)
{
    mfd=socket(AF_INET,SOCK_STREAM,0);
    if(mfd<0)
    {
        perror("socket:");
        return -1;
    }
    sockaddr_in saddr={0};
    saddr.sin_family=AF_INET;
    saddr.sin_port=htons(8001);
    inet_pton(AF_INET,IP_SERVER,&saddr.sin_addr);
    int ret,len;
    ret=connect(mfd,(sockaddr*)&saddr,sizeof(sockaddr));
    if(ret<0)
    {
        myErrorOperate("connnect.failed",__LINE__);
        return 0;
    }
    cout<<ret<<endl;
    cout<<"connect success."<<endl;
    return 1;
}
int clientLogin(client_t* client)
{
    int &mfd=client->mfd;
    int ret;
    ret=createSocket(mfd);
    if(ret!=1)
    {
        return ret;
    }
    cTransMsg_t login;
    cout<<"input your number:"<<endl;
    cin>>login.from;
    getchar();
    cout<<"input your passwd:"<<endl;
    char buf[MAX_PASSWD_LEN];
    login.buf=buf;
    cin.getline(login.buf,MAX_PASSWD_LEN);
    cout<<"login...please wait..."<<endl;
    char sendBuf[MAX_PASSWD_LEN+sizeof(cTransMsg_t)]={0};
    login.len=MAX_PASSWD_LEN;
    login.type=LOGIN_REQUEST;
    memcpy(sendBuf,&login,sizeof(cTransMsg_t));
    memcpy(sendBuf+sizeof(cTransMsg_t),buf,MAX_PASSWD_LEN);
    ret=sendToSocket(mfd,(char*)sendBuf,MAX_PASSWD_LEN+sizeof(cTransMsg_t));
    if(ret<=0)
    {
        cout<<"login err(login_t.send failed)"<<endl;
        return -1;
    }
    cTransMsg_t msg;
    ret=recvFromSocket(mfd,(char*)&msg,sizeof(cTransMsg_t));
    if(ret<=0)
    {
        cout<<"recv login_msg failed"<<endl;
        return -1;
    }
    if(msg.type==LOGIN_REFUSE)
    {
        cout<<"login refused.Check your number and passwd,please."<<endl;
        return 0;
    }
    else if(msg.type!=LOGIN_AC)
    {
        cout<<"login_msg.type recved but err"<<endl;
        return -1;
    }
    cout<<"Login success.You are online."<<endl;
    return 1;
}
void fatherExit() // kill child
{
    killpg(childPid,SIGINT);
}
void _sighandler(int arg)
{
    int ret;
    if(arg==SIGCHLD)
    {
        wait(&ret);
        cout<<"exit..."<<endl;
        exit(0);
    }
    else if(arg==SIGINT)
    {
        fatherExit();
    }
}
void fatherSetSignal()
{
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_handler=_sighandler;
    sa.sa_flags=SA_RESTART;  //|SA_NODEFER
    //sigfillset(&sa.sa_mask);
    sigfillset(&sa.sa_mask);

   // signal(SIGINT,sig_handle);
    int ret=sigaction(SIGINT,&sa,NULL);
    if(ret==-1)
    {
        cout<<"father set signal err"<<endl;
        return;
    }
    ret=sigaction(SIGCHLD,&sa,NULL);
    if(ret==-1)
    {
        cout<<"father set signal err"<<endl;
        return;
    }
    cout<<"father set signal successed"<<endl;
}
void clientRcv(int mfd) //father
{
    int ret;
    cTransMsg_t msg;
    while(1)
    {
        ret=recvFromSocket(mfd,(char*)&msg,sizeof(cTransMsg_t));
        if(ret<=0)
        {
            cout<<"recv msg failed"<<endl;
            fatherExit();
            return;
        }
        if(msg.type==LOGIN_RELOGIN)
        {
            cout<<"you have login in other place"<<endl; //I am father
            fatherExit();
            return;
        }
        cout<<"len="<<msg.len<<endl;
        msg.buf=(char*)malloc(msg.len+1);
        ret=recvFromSocket(mfd,msg.buf,msg.len);
        msg.buf[msg.len]=0;
        cout<<"recv with len="<<msg.len<<" from="<<msg.from<<"---->"<<msg.buf<<endl;
        free(msg.buf);
    }
}
int clientSendMsg(client_t* client,int toNumber,char*buf,int len,int type=0)
{
    cTransMsg_t msg;
    msg.type=0;
    msg.len=len;
    msg.to=toNumber;
    printf("sending...\n");
    int ret;
    ret=sendToSocket(client->mfd,(char*)&msg,sizeof(cTransMsg_t));
    if(ret<=0)
    {
        cout<<"err 1"<<endl;
        return -1;
    }
    ret=sendToSocket(client->mfd,buf,len);
    if(ret<=0)
    {
        cout<<"err 2"<<endl;
        return -1;
    }
    cout<<"message sended:"<<len<<endl;
    return 1;
}
void chatWith(int withNumber)
{
    char tmp[128];
    while(1)
    {
        cout<<"I to "<<withNumber<<":";
        cin.getline(tmp,sizeof(tmp));
        if(strcmp(tmp,":quit")==0)
            break;
        int msglen=strlen(tmp)+1;
        clientSendMsg(&client,withNumber,tmp,msglen);
    }
}
void login()
{
    int ret;
    ret=clientLogin(&client);
    if(ret<=0)
    {
        return;
    }
    ret=fork();
    char buf[MAX_NUMBER_LEN];
    int withNumber;
    if(ret==0)
    {
        char tmp[128];
        char toNumberBuf[10];
        bool flag;
        while(1)
        {
            while(1)
            {
                cout<<"choose a friend:";
                cin.getline(buf,sizeof(buf));
                int len=strlen(buf);
                int i;
                for(i=0;i<len;i++)
                {
                    if(!isdigit(buf[i]))
                    {
                        break;
                    }
                }
                if(i!=len)
                {
                    cout<<"illegial input."<<endl;
                    continue;
                }
                break;
            }
            withNumber=atoi(buf);
            chatWith(withNumber);
            continue;
        }
        fatherExit(); //kill child;
    }
    else    //father
    {
        childPid=ret;
        fatherSetSignal();
        clientRcv(client.mfd);
    }
    close(client.mfd);
}
void regist()
{
    char buf[MAX_PASSWD_LEN+1];
    char rebuf[MAX_PASSWD_LEN+1];
    while(1)
    {
        cout<<"input your passwd with lenth<="<<MAX_PASSWD_LEN<<endl;

        cin.getline(buf,MAX_PASSWD_LEN);
        cout<<"input your passswd again:"<<endl;
        cin.getline(rebuf,MAX_PASSWD_LEN);
        if(strcmp(buf,rebuf)!=0)
        {
            cout<<"the two of passwd is not the same"<<endl;
            continue;
        }
        break;
    }
    cout<<"sending request..."<<endl;
    int mfd,ret;

    ret=createSocket(mfd);
    if(ret!=1)
    {
        cout<<"connect server failed"<<endl;
        return;
    }
    cTransMsg_t msg;
   // msg.buf=buf;
    msg.type=REGIST_REQUEST;
    msg.len=strlen(buf);
    cout<<"lenth of pwd="<<msg.len<<endl;
    char sendBuf[MAX_PASSWD_LEN+sizeof(cTransMsg_t)];
    memcpy(sendBuf,&msg,sizeof(cTransMsg_t));
    memcpy(sendBuf+sizeof(cTransMsg_t),buf,msg.len);
    ret=sendToSocket(mfd,(char*)&sendBuf,msg.len+sizeof(cTransMsg_t));
    if(ret<=0)
    {
        cout<<"send failed"<<endl;
        close(mfd);
        return;
    }
    cout<<"send success.Recv..."<<endl;

    ret=recvFromSocket(mfd,(char*)&msg,sizeof(cTransMsg_t));
    if(ret<=0)
    {
        cout<<"recv failed"<<endl;
        close(mfd);
        return;
    }
    if(msg.type!=REGIST_AC)
    {
        cout<<"recv.type err"<<endl;
        cout<<msg.type<<endl;
        cout<<msg.from<<endl;
        close(mfd);
        return;
    }
    cout<<"You have regist success!Please hold your number:"<<msg.from<<endl;
    close(mfd);
    return;
}
int main()
{
    int ret;
    while(1)
    {
        cout<<"1.login"<<endl;
        cout<<"2.regist"<<endl;
        ret=getchar();
        getchar();
        switch(ret)
        {
            case '1':login();break;
            case '2':regist();break;
            default:
                cout<<"illegial"<<endl;
                getchar();
        }
    }
    return 0;
}




