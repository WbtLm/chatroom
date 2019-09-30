#include"redisOperat.cpp"

struct client_t
{
public:
    client_t()=default;
    int status=0;
//  bool tcp_enable;
    enum clientEm{CLIENT_UNINIT=0,CLIENT_INIT=1,CLIENT_ACTIVE=2,CLIENT_DESTROY=3};
    enum msgType{CM_STRING=0,CM_MSG_CTRL=1,LOGIN_AC=2,LOGIN_REQUEST,LOGIN_REFUSE,LOGIN_RELOGIN,REGIST_REQUEST,REGIST_AC,REGIST_REFUSE,UDP_PUNCH,UDP_PUNCH_REFUSE};//普通字符串消息，控制,
private:
    #define MAX_MSG_LEN 1024
    #define MAX_PASSWD_LEN 40
 //   sockaddr saddr_client;
    int tid;
    mutex *client_lock; //!!! must be point !!! because this is struct rather than class.So the creater of class mutex has to be called obviously.
    inline static client_t* getNumberToPtr(int n)
    {
       // echoNumber2Ptr_rw_lock.lock();
        rwlock_echoNumber2Ptr.rlock();
        client_t*p=echoNumber2Ptr[n];
        rwlock_echoNumber2Ptr.unlock();
        return p;
    }
    inline static void setNumberToPtr(int n,client_t* p)
    {
        rwlock_echoNumber2Ptr.wlock();
        echoNumber2Ptr[n]=p;
        rwlock_echoNumber2Ptr.unlock();
    }
   // int idx;//client存放的下标
    int number;//client的qq号
    int mfd; //main_socket_fd
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
    link_t<cTransMsg_t*>*msgQue;
    link_t<int>*friendList;
    int friendNum;
    linkNode_t<client_t*>*selfNode;
    stCoRoutine_t *coSelf;
    pthreadNode_t *selfpThread;
    inline void lock_client()
    {
      //  debug("client_lock",__LINE__);
        client_lock->lock();
    }
    inline bool trylock_client()
    {
        return client_lock->try_lock();
    }
    inline void unlock_client(int flag=1)
    {
        if(!flag)
            return;
      //  debug("client_unlock",__LINE__);
        client_lock->unlock();
    }
    int saveMsgToDB()
    {
        int cnt=0;
        debug("saveMsgToDb:",number);
        int ret;//=redis_db->redisDBConnect();
        cTransMsg_t *node;
        linkNode_t<cTransMsg_t*>*p;
        while(1)
        {
            cout<<"saving..."<<endl;
            lock_client();
            if(msgQue->head==NULL)
            {
                unlock_client();
                break;
            }
            p=msgQue->head;
            RemoveFromLink<linkNode_t<cTransMsg_t*>,link_t<cTransMsg_t*>>(p);
            unlock_client();
            node=p->data;
            char *buf=(char*)malloc(sizeof(cTransMsg_t)+node->len);
            memcpy(buf,node,sizeof(cTransMsg_t));
            memcpy(buf+sizeof(cTransMsg_t),node->buf,node->len);
            ret=redis_db->updateMsg(number,buf,sizeof(cTransMsg_t)+node->len);
            free(buf);
            if(ret==1)
            {
                cout<<"success"<<endl;
                cnt++;
            }
            else if(ret==0)
            {
                cout<<"failed"<<endl;
            }
            else
                cout<<"failed.db.connect.unavailable"<<endl;
        }
        cout<<"return"<<endl;
        return cnt;
    }
    int readMsgFromDB()
    {   //read msgs form db and add msgs to the msgQue.
        cTransMsg_t *msg;
        int cnt=0;
        int ret;
        char *buf;
        int len;
        lock_client();
        int flagCnt=0;
        while(1)
        {
            ret=redis_db->popMsg(number,(void**)&buf,&len);
            if(ret!=1)
            {
                if(ret==-1)
                {
                    cout<<"redis.popMsg.failed.connection.err then reconnect"<<endl;
                    if(flagCnt==1)
                    {
                        break;
                    }
                    redis_db->redisDBConnect();
                    continue;
                }
                break;
            }
            cnt++;
            msg=allocNode<cTransMsg_t>(tid);
            memcpy(msg,buf,sizeof(cTransMsg_t));
            msg->buf=(char*)malloc(len);
            memcpy(msg->buf,buf+sizeof(cTransMsg_t),len);
            linkNode_t<cTransMsg_t*>*node=allocNode<linkNode_t<cTransMsg_t*>>(tid);
            node->pLink=NULL;
            node->data=msg;
            free(buf);
            AddTail(msgQue,node);
        }
        unlock_client();
        return cnt;
    }
    void clientOff()    //user exit but do not delete the client_t and index in link
    {
        lock_client();          //then lock the client
        //status=0 is in the destroy
        //time out create....000000000000000000
        unlock_client();
    }
  //  int lastFriend;    //保存上一次发消息的朋友
private:
    udpMsgTrans_t* toUdpMsgStruct(cTransMsg_t *tcpMsg)
    {
        udpMsgTrans_t *udpMsg=(udpMsgTrans_t*)malloc(sizeof(udpMsgTrans_t));
        /*
        typedef struct cTransMsg_t
        {
            int from;
            int to;
            int len;
            int type;
            char *buf;
        }cTransMsg_t;
enum msgType{CM_STRING=0,CM_MSG_CTRL=1,LOGIN_AC=2,LOGIN_REQUEST,LOGIN_REFUSE,LOGIN_RELOGIN,REGIST_REQUEST,REGIST_AC,REGIST_REFUSE,UDP_PUNCH}
        struct udpMsgTrans_t{
            long long pwd_to;
            long long order;
            int type;
            int from;
            int to;
            int len;
            void *buf;
        };
enum udpType{UDP_MSG,UDP_MSG_CTRL,UDP_MSG_CONFIRM,UDP_PUNCH,UDP_LOGIN_CTRL,UDP_REGIST_CTRL};
        */
        memset(udpMsg,0,sizeof(udpMsgTrans_t));
        udpMsg->from=tcpMsg->from;
        udpMsg->to=tcpMsg->to;
        udpMsg->len=tcpMsg->len;
        static const int toUdpTypeEcho[]={
            UDP_MSG,        //CM_STRING
            UDP_MSG_CTRL,   //CM_MSG_CTRL
            UDP_LOGIN_CTRL,       //LOGIN_AC
            UDP_LOGIN_CTRL,       //LOGIN_REQUEST
            UDP_LOGIN_CTRL,       //LOGIN_REFUSE
            UDP_LOGIN_CTRL,       //LOGIN_RELOGIN
            UDP_REGIST_CTRL,       //REGIST_REQUEST
            UDP_REGIST_CTRL,       //REGIST_AC
            UDP_REGIST_CTRL,       //REGIST_REFUSE
            UDP_PUNCH              //UDP_PUNCH
        };
        tcpMsg->type=toUdpTypeEcho[tcpMsg->type];

    }
    static int login(int mfd,client_t* self)
    {
        cout<<"login"<<endl;
        int ret;
        cTransMsg_t in;
        cTransMsg_t &msg=in;
        cout<<"recv cTrans..."<<endl;
        struct pollfd fds;
        fds.events=POLLIN|POLLHUP|POLLERR;
        fds.fd=self->mfd;
        co_poll(co_get_epoll_ct(),&fds,1,60*1000);
        if(fds.revents==0)
        {
            cout<<"timeout."<<endl;
            return 0;
        }
        ret=recvFromSocket(mfd,(char*)&in,sizeof(cTransMsg_t));
        if(ret==0)
        {
            cout<<"no name client exit"<<endl;
            return 0;
        }
        if(ret<0)
        {
            cout<<"no name client err"<<endl;
            return -1;
        }
        char buf[MAX_PASSWD_LEN];
        cout<<"login.switch"<<endl;
        switch(in.type)
        {
            case LOGIN_REQUEST: cout<<"login.login"<<endl;
                ret=recvFromSocket(mfd,(char*)buf,MAX_PASSWD_LEN);
                in.buf=buf;
                if(ret<=0)
                {
                    cout<<"recv err"<<endl;
                }
                if(in.buf[MAX_PASSWD_LEN-1])
                {
                    cout<<"no name client in.passwd illegail.too_long"<<endl;
                    cout<<in.buf<<endl;
                    return 0;
                }
                if(in.from<0 || in.from>=MAXUSERNUM)
                {
                    cout<<"in.from out of boundary"<<endl;
                    return 0;
                }
                ret=cmpPwd(&in);
                if(ret==1)
                {
                    cTransMsg_t ac;
                    ac.type=LOGIN_AC;
                    cout<<"sendto AC------"<<endl;
                    ret=sendToSocket(mfd,(char*)&ac,sizeof(cTransMsg_t));
                    cout<<1<<endl;
                    if(ret<=0)
                    {
                        cout<<"send ac failed"<<endl;
                        return -1;
                    }
                    self->number=in.from;
                    debug("number=",in.from);
                    client_t *ori;

                    ori=getNumberToPtr(in.from);
                    //if ori is online.then send a msg to it.
                    if(ori && ori->status==CLIENT_ACTIVE)
                    {
                        ori->sendMsgTo(ori->number,NULL,0,LOGIN_RELOGIN); //send msg before echo is reseted!
                    }
                    setNumberToPtr(in.from,self);
                    cout<<"send ac.success then check the ori"<<endl;
                    if(ori) //if ori is not null,then destroy ori.
                    {
                        ori->destroy();     //destroy will check if there exist msg to save to db.
                        freeNode(ori,ori->tid); //destroy do not reset tid.So tid is available.
                    }
                    return 1;
                }
                return 0;
            case REGIST_REQUEST:  cout<<"login.regist"<<endl;

                int ret;
                int i;
                ret=recvFromSocket(mfd,(char*)buf,msg.len);
                msg.buf=buf;
                if(ret<=0)
                {
                    return -2;
                }
                ret=redis_db->getMaxNumberOfUser(i);
                cout<<"maxNumberOfUser="<<i<<endl;
                if(ret<=0)
                {
                    return -2;
                }
                for(;i<MAXUSERNUM;i++)
                {
                    cout<<"i++="<<i<<endl;
                    ret=redis_db->readUserPasswd(i,buf,MAX_PASSWD_LEN);
                    if(ret==-1)
                    {
                        cout<<"regist.db.err"<<endl;
                        return -2;
                    }
                    else if(ret==1)
                    {
                        continue;
                    }
                    break;
                }
                cout<<"regist with number="<<i<<endl;
                cout<<"pwd="<<msg.buf<<endl;
                ret=redis_db->setUserPasswd(i,msg.buf,msg.len);
                cout<<"msg.len="<<msg.len<<endl;
                if(ret<=0)
                {
                    cout<<"save pwd err"<<endl;
                    return -2;
                }
                ret=redis_db->setMaxNumberOfUser(i+1);
                if(ret<=0)
                {
                    myErrorOperate("setMaxNumberOfUser err",__LINE__,0);
                }
                msg.from=i;
                msg.type=REGIST_AC;
                ret=sendToSocket(mfd,(char*)&msg,sizeof(cTransMsg_t));
                if(ret<=0)
                {
                    cout<<"send Regist.number failed"<<endl;
                    return -2;
                }
                cout<<"regist success with number="<<i<<endl;
                return 2;
              //  break;
            default:
                return 0;
        }

    }
    static int cmpPwd(cTransMsg_t *in)
    {
        int ret;
        char pwdBuf[MAX_PASSWD_LEN+1];
        ret=redis_db->readUserPasswd(in->from,pwdBuf,MAX_PASSWD_LEN);
        if(ret==-1)
        {
            debug("read passwd failed:",in->from);
            return -1;
        }
        if(ret==0)
        {
            debug("number is not exists",in->from);
            return 0;
        }
        if(strcmp(pwdBuf,in->buf)==0)
        {
            cout<<"cmp success"<<endl;
            return 1;
        }
        cout<<in->buf<<"  &&  "<< pwdBuf << endl;
        return 0;
    }
    static void* clientRun(void *args)
    {
        co_enable_hook_sys();
        client_t *self=(client_t*)args;
        int mfd=self->mfd;
        int ret;
        debug("client_t.clientRun.mfd=",mfd);

        ret=login(mfd,self);   //log in. and regist
        if(ret==-2)//regist
        {
            cTransMsg_t msg;
            msg.type=REGIST_REFUSE;
            ret=sendToSocket(mfd,(char*)&msg,sizeof(cTransMsg_t));
            if(ret<=0)
            {
                cout<<"redist.send_refuse.failed"<<endl;
            }
            self->number=0;
            self->destroy();
            freeNode(self,self->tid);
            return NULL;
        }
        if(ret==2)//regist
        {
            debug("regist success with number=",mfd);
            self->number=0;
            self->destroy();
            freeNode(self,self->tid);
            return NULL;
        }
        if(ret!=1)
        {
            if(ret!=-1)
            {
                cTransMsg_t ac;
                ac.type=LOGIN_REFUSE;
                cout<<"send refused-------"<<endl;
                ret=sendToSocket(mfd,(char*)&ac,sizeof(ac));
                cout<<"sendto socket.end"<<endl;
                if(ret<=0)
                {
                    cout<<"send refuse failed"<<endl;
                }
            }
            debug("status-------------------",self->status);
            self->clientOff();
            return NULL;
        }
        self->status=CLIENT_ACTIVE;
        debug("login.ac with number=:",self->number);
        cout<<"read msg from db"<<endl;
        ret=self->readMsgFromDB();
        if(ret)
        {
            debug("there are msgs in db:",ret);
            self->sendToUser();
        }
        cout<<"readfriends"<<endl;
        ret=self->readFriends();
        if(ret<0)
        {
            myErrorOperate("readFriends.failed",__LINE__);
        }
        cout<<"while"<<endl;

        struct pollfd fds;
        while(1)    //监听事件
        {
            fds.events=POLLIN|POLLERR|POLLHUP;
            fds.fd=self->mfd;
            cout<<"clientRun.co_poll"<<endl;
            cout<<self->mfd<<endl;
            co_poll(co_get_epoll_ct(),&fds,1,-1);   //yield
            cout<<"clientRun.co_poll.active"<<endl;
            if(fds.revents&POLLIN)
            {
                cTransMsg_t *msg=allocNode<cTransMsg_t>(self->tid);
                ret=recvFromSocket(self->mfd,(char*)msg,sizeof(cTransMsg_t));
                if(ret<=0)
                {
                    cout<<"user.offline(recv.msgHead.err)"<<endl;
                    cout<<self->number<<endl;
                    self->clientOff();
                    return NULL;
                }
                if(msg->type==CM_STRING)
                {
                    if(msg->len<=0)
                    {
                        debug("msg.len.err(len<=0):",self->number);
                        continue;
                    }
                    msg->buf=(char*)malloc(msg->len+1);
                    ret=recvFromSocket(mfd,msg->buf,msg->len);
                    if(ret<=0)
                    {
                        debug("msg.len.err2(len<=0):",self->number);
                        continue;
                    }
                    msg->buf[msg->len]=0;
                    cout<<"------<";
                    debug(msg->buf,self->number);

                    self->sendMsgTo(msg);
                    cout<<"clientRun.freeNode"<<endl;
                    freeNode(msg,self->tid);

                }
                else if(msg->type==UDP_PUNCH)
                {
                //    if(numberToPtr())
                }
            }
            else if((fds.revents&POLLHUP) || (fds.revents&POLLERR))
            {
                cout<<"user.offline(hup or err)"<<endl;
                cout<<self->number<<endl;
                self->clientOff();
                return NULL;
            }
        }
    }
    stCoRoutine_t *coCtrlTimeout;
    static void* msgCtrlTimeout(void *arg)
    {
        return NULL;
        client_t*self=(client_t*)arg;
        co_enable_hook_sys();
        int cnt=360;
        co_yield_ct();
        while(1)
        {
            cout<<"msgCtrlTimeout.time out-------------"<<endl;
            coTimeoutSleep_my(1000);// 1 second for check msg
            cout<<"msgCtrlTimeout.over"<<endl;
            self->client_lock->lock();
            cout<<1<<endl;
            if(self->msgQue->head)
            {
                self->saveMsgToDB();
                cnt=360;
            }
            else
            {
                cnt--;
            }
            if(cnt==0)
            {
                self->destroy();
                self->client_lock->unlock();
                freeNode(self,self->tid);
                return NULL;
            }
            self->client_lock->unlock();
        }
    }
public:
    int clientInit(int fd,int tid,linkNode_t<client_t*> *p,pthreadNode_t* pthread)
    {   //init and login
        cout<<"clientInit"<<endl;
        client_lock=new mutex();
        client_lock->lock();
        int ret;
        status=CLIENT_INIT;
        selfpThread=pthread;
        selfNode=p;
        this->tid=tid;
        friendList=allocNode<link_t<int>>(tid);
        memset(friendList,0,sizeof(link_t<int>));
        friendNum=0;
        mfd=fd;
        msgQue=allocNode<link_t<cTransMsg_t*>>(tid);
        memset(msgQue,0,sizeof(cTransMsg_t));
        coSelf=allocNode<stCoRoutine_t>(tid);
        co_create(&coSelf,NULL,clientRun,this);
        client_lock->unlock();
        co_resume(coSelf);
    }
    int clientCreate(int numb,int tid,linkNode_t<client_t*> *p,pthreadNode_t* pthread)
    {   //init without login
        number=numb;
        client_lock=new mutex();
        //echoNumber2Ptr[number]=client; :: in the addClientOffline
        client_lock->lock();
        status=0;
        selfpThread=pthread;
        selfNode=p;
        int ret;
        friendList=allocNode<link_t<int>>(tid);//must seted or destroy will be stack error.
        memset(friendList,0,sizeof(link_t<int>));
        friendNum=0;
        this->tid=tid;
        msgQue=allocNode<link_t<cTransMsg_t*>>(tid);
        memset(msgQue,0,sizeof(cTransMsg_t));
        coCtrlTimeout=allocNode<stCoRoutine_t>(tid);
        co_create(&coCtrlTimeout,NULL,msgCtrlTimeout,this);
        client_lock->unlock();
        co_resume(coCtrlTimeout);
    }
/*    int clientLoading(int fd)
    {   //login and read ifd (example:friend list).
        client_lock->lock();
        status=CLIENT_INIT;
        mfd=fd;
        friendList=allocNode<link_t<int>>(tid);
        memset(friendList,0,sizeof(link_t<int>));
        friendNum=0;
        coSelf=allocNode<stCoRoutine_t>(tid);
        co_create(&coSelf,NULL,clientRun,this);
        client_lock->unlock();
        co_resume(coSelf);
    }*/
    int readFriends()
    {
        return 0;
       /* char path[65],real[73]="friend_data_"; //friend_data_QQNumber
        sprintf(path,"%d",number);
        strcat(real,path);
        FILE *fd=fopen(real,"rt");
        if(fd==NULL)
        {
            cout<<"readfriends.NULL.fail"<<endl;
            perror("p:");
            myErrorOperate("fopen.failed",__LINE__);
        }
        fscanf(fd,"%d",&friendNum);
        for(int i=0;i<friendNum;i++)
        {
            linkNode_t<int>*node=(linkNode_t<int>*)allocNode<linkNode_t<int>>(tid);
            node->pLink=NULL;
            fscanf(fd,"%d",&node->data);
            AddTail(friendList,node);
        }
        fclose(fd);
        return friendNum;*/
    }
    inline int sendMsgTo(cTransMsg_t *msg)
    {
        return sendMsgTo(msg->to,msg->buf,msg->len,msg->type);
    }
    int sendMsgTo(int toNumber,char* msg,int lenth,int type=0/*默认发文字消息*/)
    {           /*write to db*/
        cout<<"sendMsgTo"<<endl;
        if(toNumber<0 || toNumber>=MAXUSERNUM)
        {
            return -1;
        }
        bool lockforclient;
        client_t* to=getNumberToPtr(toNumber);

        //if not exist,then create. (to==null or status==destroy)
        if(to)
        {
            cout<<"user is offline and client_t is inited."<<endl;
            lockforclient=to->trylock_client();  //client.lock second
            cout<<"client.lock"<<endl;
            if(to->status==CLIENT_DESTROY)
            {
                to->unlock_client(lockforclient);
                //reload the client;
                selfpThread->addClientOffline(toNumber);
                to=getNumberToPtr(toNumber);
            }
            else
            {
                to->unlock_client(lockforclient);
            }
        }
        else
        {
            cout<<"user is offline and client_t is not inited.so create the client_t"<<endl;
            selfpThread->addClientOffline(toNumber);
            cout<<"out"<<endl;
            to=getNumberToPtr(toNumber);
        }

        //to is the target by now
        cTransMsg_t* msgTmp=(cTransMsg_t*)allocNode<cTransMsg_t>(tid);
        msgTmp->buf=msg;
        msgTmp->len=lenth;
        msgTmp->from=number;
        msgTmp->type=type;
        linkNode_t<cTransMsg_t*> *nodeTmp=allocNode<linkNode_t<cTransMsg_t*>>(tid);
        nodeTmp->data=msgTmp;
        nodeTmp->pLink=NULL;
        AddTail(to->msgQue,nodeTmp);
        //let client(to_number).sendToUser
        to->sendToUser();
        cout<<"sendMsgTo over"<<endl;
        return 1;
    }
 /*   void cancelClientInPthread() //cancel the client_t in pthread and destroy
    {
        cout<<"user offline"<<endl;
        if(status==CLIENT_ACTIVE)
        {
            clientOff();
        }
        cout<<"offline 1"<<endl;
        selfpThread->lock();
        RemoveFromLink<linkNode_t<client_t*>,link_t<client_t*>>(selfNode);
        if(status==CLIENT_ACTIVE)
        {
            selfpThread->totalUserNum--;
        }
        selfpThread->unlock();
        cout<<"destroy.... "<<endl;
        destroy();
    }*/
    int sendToUser() //trylock
    {

        linkNode_t<cTransMsg_t*> *node;
        cTransMsg_t *msgHead;
        int ret;
        debug("sendToUser with status=",status);
        bool lockforclient=0;
        while(1)
        {
            lockforclient=trylock_client();
            if(!(msgQue->head))
            {
                unlock_client(lockforclient);
                break;
            }
            node=msgQue->head;
            RemoveFromLink<linkNode_t<cTransMsg_t*>,link_t<cTransMsg_t*>>(node);
            unlock_client(lockforclient);
            msgHead=node->data;
            msgHead->from=number;

            ret=sendToSocket(mfd,(char*)msgHead,sizeof(cTransMsg_t));
            if(ret<=0)
            {
                cout<<"sendto.clientoff"<<endl;
                clientOff();
                lockforclient=trylock_client();
                AddHead(msgQue,node);
                unlock_client(lockforclient);
                if(status==CLIENT_ACTIVE)
                {
                    clientOff();
                }
                cout<<"saveMsgToDB head"<<endl;
                saveMsgToDB();
                break; //no free
            }
            else
                cout<<"send msghead success"<<endl;
            ret=sendToSocket(mfd,msgHead->buf,msgHead->len);
            if(ret<=0)
            {
                clientOff();
                AddHead(msgQue,node);
                //no free
                if(status==CLIENT_ACTIVE)
                {
                    clientOff();
                }
                cout<<"saveMsgToDB"<<endl;
                saveMsgToDB();
                break; //no free
            }
            else
                debug("send msgbody success with len=",msgHead->len);
            freeNode(node,tid);
        }
        cout<<"send to user over"<<endl;
    }
    void destroy()
    {

    }
    ~client_t()//thread.lock,client.lock
    {
        int s=0;
        cout<<"destroy(~)-----------------------"<<endl;
        sendToUser();  //sendToUser has to be first ,then reset echo.
        cout<<1<<endl;

        if(getNumberToPtr(number)==this)
        {
            setNumberToPtr(number,NULL);
        }
        selfpThread->lock();
        client_lock->lock();
        if(status==CLIENT_ACTIVE)
        {
            s=1;
        }
        selfpThread->totalUserNum--;
        status=CLIENT_DESTROY;
        client_lock->unlock();
        selfpThread->unlock();

        cout<<1<<endl;
        while(msgQue->head)
        {
            cout<<2<<endl;
            auto head=msgQue->head;
            RemoveFromLink<linkNode_t<cTransMsg_t*>,link_t<cTransMsg_t*>>(head);
            cout<<head<<endl;
            freeNode(head,tid);
        }
        cout<<3<<endl;
        freeNode(msgQue,tid);
        cout<<"a"<<endl;

        if(friendList)
        {
            while(friendList->head)
            {
                cout<<"b"<<endl;
                auto head=friendList->head;
                RemoveFromLink<linkNode_t<int>,link_t<int>>(head);
                cout<<"c"<<endl;
                freeNode(head,tid);
            }
            cout<<4<<endl;
            freeNode(friendList,tid);
        }
        cout<<5<<endl;
        if(mfd==0)
        {
            myErrorOperate("close.mfd==0",__LINE__);
        }
        close(mfd);
        delete client_lock;  //lock is deleted after numberToPtr is seted NULL.
        debug("client_t.destroy.close:",mfd);
        cout<<"destroy.exit"<<endl;

    }

};
