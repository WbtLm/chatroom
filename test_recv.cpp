#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <bits/stdc++.h>
using namespace std;
#include "udpSocket.cpp"

int main()
{
    int number;
    cout<<"input number:"<<endl;
    number = 0;
    udpSender_t udpSender;
    udpSender.create(number,"127.0.0.1",8001,123);
    udpMsgTrans_t msg;
    char STR[]="hello";
    int ret;
    for(int i=0;i<100;i++)
    {
        ret=udpSender.recv(&msg,&msg.buf,&msg.len);
        if(ret==-1)
        {
            cout<<"recv err"<<endl;
            return 0;
        }
        cout<<"len="<<msg.len<<endl;
        for(int i=0;i<msg.len;i++)
            cout<<*(char*)(msg.buf+i);
     //   printf("%s\n",msg.buf);
    }

}
