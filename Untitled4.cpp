
#include<iostream>
#include<thread>
using namespace std;

class A{

public:

    void fun(int a,int b){

        cout<<"this is A thread!"<<a<<std::endl;
    }
    A()
    {
        thread *p=new thread(A::fun,this,3,4);
    }
};

int main(){

    int k=0;

    A a;

    thread t(&A::fun,a,k,k+1);
    t.join();

}
