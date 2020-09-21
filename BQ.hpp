#ifndef UNTITLED18_BQ_HPP
#define UNTITLED18_BQ_HPP

#include <stdlib.h>
#include <atomic>
#include <queue>
using std::queue;

enum Type{ENQ, DEQ};

struct Future {
    void* result;
    bool isDone;
    Future():isDone(false), result(NULL){}
};

struct Node{
    void* item;
    std::atomic<Node*> next;
    Node(void* item, Node* Next){
        this->item = item;
        next.store(Next);
    }
};

struct BatchRequest{
    Node* firstEnq;
    Node* lastEnq;
    unsigned int enqsNum;
    unsigned int deqsNum;
    unsigned int excessDeqsNum;
    BatchRequest(Node* firstEnq,Node* lastEnq, unsigned int enqsNum, unsigned int deqsNum, unsigned int excessDeqsNum):
    firstEnq(firstEnq),lastEnq(lastEnq),enqsNum(enqsNum),deqsNum(deqsNum),excessDeqsNum(excessDeqsNum){}

    BatchRequest() = default;
};

struct PtrCnt{
    Node* node;
    uintptr_t cnt;
    PtrCnt() = default;
    PtrCnt(Node* node, unsigned int cnt): node(node), cnt(cnt){}
};

struct Ann{
    BatchRequest batchReq;
    std::atomic <PtrCnt> oldHead;
    std::atomic <PtrCnt> oldTail;
    Ann(BatchRequest batchR){
        batchReq = batchR;
        PtrCnt tail((Node*)0, 40);
        PtrCnt head((Node*)0, 40);
        oldHead.store(head);
        oldTail.store(tail);
    };
};

union PtrCntOrAnn{
    PtrCnt ptrCnt;
    struct {
        uintptr_t tag;
        Ann* ann;
    } wrap;
    PtrCntOrAnn() = default;
    PtrCntOrAnn(Node* node, unsigned int cnt): ptrCnt(node, cnt){} ///HOW THE FUCKK HE KNOWS IF ITS PTRCNT ? WHY NO PUT ZERO??
    PtrCntOrAnn(PtrCnt& ptrCnt): ptrCnt(ptrCnt){}
    PtrCntOrAnn(Ann* ann): wrap{1,ann}{}

};

struct FutureOp{
    Type type;
    Future* future;
    FutureOp() = default;
    FutureOp(Type type,Future* future): type(type), future(future){}
};

struct ThreadData{
    queue<FutureOp> opsQueue;
    Node* enqsHead;
    Node* enqsTail;
    unsigned int enqsNum;
    unsigned int deqsNum;
    unsigned int excessDeqsNum;
};

/**** Interface Functions ****/

void Enqueue(void* item);

void* Dequeue();

Future* FutureEnqueue(void* item);

Future* FutureDequeue();

void Init();

void* Evaluate(Future* future);

void Init_thread();




#endif //UNTITLED18_BQ_HPP
