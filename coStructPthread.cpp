#
struct pthreadNode
{
    int totalUserNum;
    link_t<client_t*> *clientLink;           //=(link_t<client_t*>*)malloc(sizeof(link_t<client_t*>));
    link_t<int> *newUserLink;
    int tid;
    thread &pself;
    mutex plock;
    pthreadNode()
    {
        clientLink=(link_t<client_t*>*)malloc(sizeof(link_t<client_t*>));
        clientLink->head=clientLink->tail=NULL;
        totalUserNum=0;
        newUserLink=(link_t<int>*)malloc(sizeof(link_t<int>));
        newUserLink->head=newUserLink->tail=NULL;
    }

};
