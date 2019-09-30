
#include<hiredis/hiredis.h>
//#include<bits/stdc++.h>
//using namespace std;

class redisDB_t{
    redisContext *redis=NULL;
    int status=0;
public:
    redisDB_t()
    {
        status=0;
        redis=NULL;
        redisDBConnect();
    }
    int redisDBConnect()
    {
        cout<<"redisDBConnect.connect..."<<endl;
        if(status==1)
        {
            cout<<"have already connected."<<endl;
            return 1;
        }
        status=0;
        cout<<"redisConnect"<<endl;
        redis=redisConnect("127.0.0.1",6379);
        cout<<1<<endl;
        if(redis->err)
        {
            redisFree(redis);
            cout<<"redis.connect fail"<<endl;
       //     myErrorOperate("redis.connect fail",__LINE__);
            return 0;
        }
        cout<<"redis connect success"<<endl;
        status=1;
        return 1;
    }
    int popMsg(int number,void** buf,int *len)
    {
     //   redisDBConnect();
        if(status==0)
            return -1;
        redisReply *reply;
        char command[30];
        strcpy(command,"lpop msglst_");
        char numberString[10];
        sprintf(numberString,"%d",number);
        strcat(command,numberString);
        cout<<"redis.pop.command..."<<endl;
        reply=(redisReply*)redisCommand(redis,command);
        cout<<"redis.pop.rediscommand.end"<<endl;
        if(reply==NULL)
        {
            cout<<1<<endl;
            status=0;
            redisFree(redis);
            return -1;
        }
        if(reply->type==REDIS_REPLY_NIL)
        {
            cout<<2<<endl;
            freeReplyObject(reply);
            return 0;
        }
        if(reply->type != REDIS_REPLY_STRING)
        {
            cout<<"type="<<reply->type<<endl;
            freeReplyObject(reply);
            return 0;
        }
        *buf=malloc(reply->len);
        memcpy(*buf,reply->str,reply->len);
        *len=reply->len;
        freeReplyObject(reply);
        cout<<"redis.pop.end"<<endl;
        return 1;
    }
    int updateMsg(int number,char const *buf,int len)
    {
     //   redisDBConnect();
        if(status==0)
            return -1;
        redisReply *reply;
        char *command=(char*)malloc(sizeof("msglst_")+len+5);//command is key of key-value
        strcpy(command,"msglst_");//rpush msglst_QQNumber
        char numberString[MAX_NUMBER_LEN];
        sprintf(numberString,"%d",number);
        strcat(command,numberString);
        char const *comlst[3]={"rpush",command,buf};
        size_t comlen[3]={
            sizeof("rpush")-1,
            strlen(command),
            (size_t)len
        };
        reply=(redisReply*)redisCommandArgv(redis,3,(const char**)comlst,(const size_t*)comlen);
        if(reply==NULL)
        {
            status=0;
            redisFree(redis);
            return -1;
        }
        if(reply->type==REDIS_REPLY_INTEGER && reply->integer>0)
        {
            freeReplyObject(reply);
            cout<<"save to db : success !"<<endl;
            return 1;
        }
        cout<<reply->integer<<endl;
        return 0;
    }
    void closeRedis()
    {
        status=0;
        redisFree(redis);
    }
    inline bool getUserPasswd(int number,char*buf,int size)
    {
        return readUserPasswd(number,buf,size);
    }
    bool readUserPasswd(int number,char *buf,int size)
    {
        if(status==0)
            return -1;
        redisReply * reply;
        int len;
        char numberBuf[MAX_NUMBER_LEN+1];
        sprintf(numberBuf,"%d",number);
        // command = "hexists";
        char const *hName="pwd_data";

        {
            char const *comlst[3]={"hExists",hName,numberBuf};
            size_t comlen[3]={
                sizeof("hExists")-1,
                strlen(hName),
                strlen(numberBuf)
            };
            reply=(redisReply*)redisCommandArgv(redis,3,(const char**)comlst,(const size_t*)comlen);
            if(reply==NULL)
            {
                status=0;
                redisFree(redis);
                return -1;
            }
            if(reply->type==REDIS_REPLY_INTEGER && reply->integer>0)
            {
                cout<<"query Exists_pwd from db success!"<<endl;
                freeReplyObject(reply);
            }
            else
            {
                cout<<reply->integer<<endl;
                return 0;
            }
        }
        {
            char const *comlst[3]={"hget",hName,numberBuf};
            size_t comlen[3]={
                sizeof("hget")-1,
                strlen(hName),
                strlen(numberBuf)
            };
            reply=(redisReply*)redisCommandArgv(redis,3,(const char **)comlst,(const size_t*)comlen);
            if(reply==NULL)
            {
                status=0;
                redisFree(redis);
                return -1;
            }
            if(reply->type==REDIS_REPLY_STRING)
            {
                int len=min((int)reply->len,size);
                memcpy(buf,reply->str,len);
                buf[len]=0;
               // cout<<reply->str<<endl;
               // cout<<buf<<endl;
                freeReplyObject(reply);
                return 1;
            }
            else
            {
                freeReplyObject(reply);
                return 0;
            }
        }

    }
    int setMaxNumberOfUser(int number)
    {
        if(status==0)
        {
            return -1;
        }
        char buf[sizeof("set max_number_of_user ")+MAX_NUMBER_LEN];
        sprintf(buf,"set max_number_of_user %d",number);
        redisReply *reply;
        reply=(redisReply*)redisCommand(redis,buf);
        if(reply==NULL)
        {
            cout<<"set max_number_of_user err"<<endl;
            return -1;
        }
        if(reply->type==REDIS_REPLY_STATUS && strcmp(reply->str,"OK")==0)
        {
            return 1;
        }
        return 0;
    }
    int getMaxNumberOfUser(int &number)
    {
        if(status==0)
            return -1;
        redisReply *reply;
        reply=(redisReply*)redisCommand(redis,"get max_number_of_user");
        if(reply==NULL)
            return -1;
        if(reply->type==REDIS_REPLY_STRING)
        {
            number=atoi(reply->str);
            freeReplyObject(reply);
            return 1;
        }
        if(reply->type==REDIS_REPLY_NIL)
        {
            setMaxNumberOfUser(1);
            number=1;
            return 1;
        }
        cout<<"type="<<reply->type<<endl;//4 is nil
        freeReplyObject(reply);
        return 0;
    }
    bool setUserPasswd(int number,char *buf,int size)
    {
        if(status==0)
            return -1;
        redisReply * reply;
        int len;
        char numberBuf[MAX_NUMBER_LEN+1];
        sprintf(numberBuf,"%d",number);
        // command = "hset";
        char const *hName="pwd_data";

        {
            char const *comlst[4]={"hset",hName,numberBuf,buf};
            size_t comlen[4]={
                sizeof("hset")-1,
                strlen(hName),
                strlen(numberBuf),
                size
            };
            reply=(redisReply*)redisCommandArgv(redis,4,(const char**)comlst,(const size_t*)comlen);
            if(reply==NULL)
            {
                status=0;
                redisFree(redis);
                return -1;
            }
            if(reply->type==REDIS_REPLY_INTEGER && reply->integer>0)
            {
                cout<<"set pwd to db success!"<<endl;
                cout<<"integer="<<reply->integer<<endl;
                cout<<"number="<<number<<endl;
                cout<<"passwd="<<buf<<endl;
                freeReplyObject(reply);
                return 1;
            }
            else
            {
                cout<<"set pwd to db failed"<<endl;
                cout<<comlst[0]<<comlst[1]<<comlst[2]<<comlst[3]<<endl;
                cout<<comlen[0]<<comlen[1]<<comlen[2]<<comlen[3]<<endl;
                cout<<"integer="<<reply->integer<<endl;
                cout<<"number="<<number<<endl;
                cout<<"passwd="<<buf<<endl;
                return -1;
            }
        }

    }
};
redisDB_t *redis_db;


/*
int main()
{
    redisDB_t db;
    int ret=db.redisDBConnect();
    if(ret==0)
    {
        cout<<"connect failed"<<endl;
        return 0;
    }
    char buf[100]={"21  asdf  fsd 2"};  //buf can be struct or any binary data.
    int len=strlen(buf);
    ret=db.updateMsg(0,buf,len);
    if(ret!=1)
    {
        cout<<"update failed"<<endl;
        cout<<ret<<endl;
    }
    char *p;
    ret=db.popMsg(0,(void**)&p,&len);
    if(ret!=1)
    {
        cout<<"query failed"<<endl;
        cout<<ret<<endl;
    }
    p[len]=0;
    cout<<"msg="<<p<<endl;

}*/

