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
    number = 1;
    udpSender_t udpSender;
    udpSender.create(number,"127.0.0.1",8001,123);
    udpMsgTrans_t msg;
    char STR[]="hello";
    int ret;
    for(int i=0;i<3;i++)
    {
        udpMsgTrans_t msg;
        msg.order=i;
        msg.len=sizeof(STR);
        ret=udpSender.send(&msg,STR,sizeof(STR));
        if(ret==-1)
        {
            cout<<"send err"<<endl;
            return 0;
        }
    }

}
