
enum udpType{UDP_MSG,UDP_MSG_CTRL,UDP_MSG_CONFIRM,UDP_PUNCH,UDP_PUNCH_REFUSE,UDP_LOGIN_CTRL,UDP_REGIST_CTRL};
struct udpMsgTrans_t{
    long long pwd_to;//
    long long order;
    int type;
    int from;
    int to;
    int len;
    void *buf;//
};
class udpSender_t{
    char *ip;
    int port;
    sockaddr_in saddr;
    int repeat;
    long long pwd_to;
    bool status=0;
    int number;
    bool bind_status;
    int setTimeout(int timeout_sec,int timeout_usec=0)
    {
        struct timeval time;
        time.tv_sec=timeout_sec;
        time.tv_usec=timeout_usec;
        errno=0;
        return setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&time,sizeof(struct timeval));
    }
    int recvFromUdp(void *buf,int len,sockaddr_in* source)
    {
        if(len<=0)
            return 1;
        int ret;
        int totol=0;
        unsigned int lenth;
        while(totol<len)
        {
            errno=0;
            ret=recvfrom(fd,(char*)buf+totol,len-totol,0,(sockaddr*)source,&lenth);
            if(ret<0)
            {
                return -1;
            }
            if(ret==0 && (errno==EINTR || errno==EAGAIN))
            {
                #ifdef LIBCO_ENV
                co_yield_ct();
                #endif
                continue;
            }
            if(errno==EINPROGRESS)
            {
                return 0;
            }
            totol+=lenth;
        }
        return len;
    }
    int sendToUdp(void *buf,int len)
    {
        if(len==0)
        {
            return 1;
        }
        int ret;
        int totol=0;
        while(totol<len)
        {
            errno=0;
            ret=sendto(fd,(char*)buf+totol,len-totol,0,(sockaddr*)&saddr,sizeof(sockaddr));
            if(ret<0)
            {
                return -1;
            }
            if(ret==0 && (errno==EINTR || errno==EAGAIN))
            {
                continue;
            }
            totol+=ret;
        }
        return len;
    }
private:
    int fd;
public:
    udpSender_t()
    {
        status=0;
    }
public:
    int create(int number,const char *ip,int port,long long pwd,int repeat=3)
    {
        this->number=number;
        inet_pton(AF_INET,ip,&saddr.sin_addr);
        saddr.sin_port=htons(port);
        saddr.sin_family=AF_INET;
        this->repeat=repeat;
        pwd_to=pwd;
        fd=socket(AF_INET,SOCK_DGRAM,0);
        bind_status=0;
        status=1;

        cout<<"udp.create.end"<<endl;
    }
public:
    int send(void *head,void *buf,int len) //
    {
        if(status==0)
            return -1;
    cout<<"send"<<endl;
        int ret;
        ((udpMsgTrans_t*)head)->pwd_to=pwd_to;
        void *tmpbuf=malloc(len+sizeof(udpMsgTrans_t));
        memcpy(tmpbuf,head,sizeof(udpMsgTrans_t));
        memcpy((char*)tmpbuf+sizeof(udpMsgTrans_t),buf,len);
        ret=sendToUdp(tmpbuf,len+sizeof(udpMsgTrans_t));
        free(tmpbuf);
        if(ret<0)
        {
            destroy();
            return -1;
        }
        return ret;
	}
public:
    int sendAndConfirm(void *head,void *buf,int len,int timeout_ms=-1,int repeat=5)
	{
	    if(status==0)
            return -1;
     cout<<"sendAndConfirm"<<endl;
        int ret;
        if(timeout_ms!=-1)
        {
            ret=setTimeout(timeout_ms/1000,timeout_ms%1000*1000);
            if(ret<0)
                return ret;
        }
        int lenth;
        udpMsgTrans_t msg;
        char *body;
        while(repeat>=0)
        {
            ret=send(head,buf,len);
            if(ret<0)
            {
                cout<<"udp.sendAndConfirm.send.failed"<<endl;
                return -1;
            }
            ret=recv(&msg,(void**)&body,&lenth);
            if(ret<0)
            {
                return ret;
            }
            if(ret==0 && errno==EINPROGRESS)
            {
                repeat--;
                continue;
            }
            break;
        }
        errno=0;
        return 0;
	}
public:
    int recvAndConfirm(void *head,void**buf,int *len,int timeout_ms=0,int repeat=5)
    {
        if(status==0)
        {
            return -1;
        }
    cout<<"recvAndConfirm"<<endl;
        int ret;
        if(timeout_ms!=0)
        {
            ret=setTimeout(timeout_ms/1000,timeout_ms%1000*1000);
            if(ret<0)
                return ret;
        }
        while(repeat>=0)
        {
            ret=recv(head,buf,len);
            if(ret<0)
                return ret;
            if(ret==0 && errno==EINPROGRESS)
            {
                repeat--;
                continue;
            }
            udpMsgTrans_t msg;
            msg.len=0;
            msg.order=((udpMsgTrans_t*)head)->order;
            msg.type=UDP_MSG_CONFIRM;
            msg.pwd_to=pwd_to;
            ret=send(head,&msg,sizeof(udpMsgTrans_t));
            if(ret<0)
                return ret;
            break;
        }
        errno=0;
        return 0;
    }
public:
    int recv(void *head,void**buf,int*len)
    {
        if(status==0)
            return -1;
    cout<<"recv"<<endl;
        int ret;
        struct sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(8001);
        sa.sin_addr.s_addr = INADDR_ANY;

        if(bind_status==0)
        {
            bind_status=1;
            ret = bind(fd, (struct sockaddr *)&sa, sizeof(sa));
            if( ret < 0 )
            {
                printf("bind error: %s\n", strerror(errno));
                return 1;
            }
        }

        struct sockaddr_in source;
        bool flag=0;
        if(head==NULL)
        {
            head=malloc(sizeof(udpMsgTrans_t));
            flag=1;
        }
        udpMsgTrans_t &msg=*(udpMsgTrans_t*)head;
        cout<<"recvFromUdp.head.recv"<<endl;
        ret=recvFromUdp(&msg,sizeof(udpMsgTrans_t),&source);
        cout<<"recvFromUdp.head.end"<<endl;
        if(ret==0 && errno==EINPROGRESS)
        {
            cout<<"udp.recv.timeout"<<endl;
            if(flag)
            {
                free(head);
            }
            return 0;
        }
        if(ret<0)
        {
            if(flag)
            {
                free(head);
            }
            destroy();
            return -1;
        }
        if(msg.len<0)
        {
            if(flag)
            {
                free(head);
            }
            return 0;
        }
        if(msg.len==0)
        {
            if(flag)
            {
                free(head);
            }
            return 1;
        }

        *buf=malloc(msg.len);
        *len=msg.len;
        cout<<"recvFromUdp"<<endl;
        ret=recvFromUdp(*buf,*len,&source);
        cout<<"recvFromUdp.end"<<endl;
        if(ret==0 && errno==EINPROGRESS)
        {
            cout<<"udp.recv.timeout"<<endl;
            if(flag)
            {
                free(head);
            }
            return 0;
        }
        if(ret<0)
        {
            if(flag)
            {
                free(head);
            }
            return -1;
        }
        if(flag)
        {
            free(head);
        }
        return 1;
    }
public:
    int destroy()
    {
        status=0;
        close(fd);
    }
};


