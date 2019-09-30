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

struct task_t
{
    stCoRoutine_t *co;//协程控制字
    int workNumb;//epoll中的socket个数
    int addNumb;//新来的数目
    link_t<int> *acLink;
    linkNode_t<task_t*> *dptLinkNode;//在调度器链表中的位置
    int taskNumb;//协程号
};



template<typename TLink,typename TNode>
void AddHead(TLink * apLink,TNode *ap)
{
    if(apLink==NULL || ap==NULL)
    {
        myErrorOperate("AddHead.apLink==NULL || ap==NULL",__LINE__,1);
    }
    if(apLink->head)
    {
        apLink->head->pPrev=(TNode*)ap;
        ap->pPrev=NULL;
        ap->pNext=apLink->head;
        apLink->head=ap;
    }
    else
    {
        apLink->head=apLink->tail=ap;
        ap->pNext=ap->pPrev=NULL;
    }
    ap->pLink=apLink;
}

int sendToSocket(int fd,char* buff,int len,bool co=1)
{
	if(len==0)
	  return 1;
    int idx=0;
    int ret;
    int again=AGAINTIMES+1;

    while(len>idx && again)
    {
	errno=0;
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
                    co_yield_ct();
                }
                continue;
            }
            return ret;
        }
        idx+=ret;
    }
    return len;
}
int recvFromSocket(int fd,char*buff,int len,bool co=1)
{
    if(len==0)
	  return 1;
	memset(buff , 0 , len);
    int idx=0;
    int ret;
    int again=AGAINTIMES+1;
    while(len>idx && again)
    {
        errno=0;
        ret=recv(fd,buff+idx,len-idx,MSG_WAITALL);
        cout<<"recv.ended"<<endl;
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
                    co_yield_ct();
                }
                continue;
            }
            perror("recv.errno:");
            cout<<fd<<endl;
            return ret;
        }
        idx+=ret;
    }
    if(idx<len || again==0)
    {
        debug("again:",again);
        return -1;
    }
    return len;
}

