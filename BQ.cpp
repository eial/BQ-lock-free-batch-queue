#include "BQ.hpp"
#include <cstddef>
#include <iostream>

thread_local ThreadData threadData;

#define CAS compare_exchange_strong
std::atomic<PtrCnt> SQTail;
alignas(64)
std::atomic<PtrCntOrAnn> SQHead;
alignas(64)
void EnqueueToShared(void* item);
void* DequeueFromShared();
PtrCnt HelpAnnAndGetHead();
Node* ExecuteBatch(BatchRequest req);
void ExecuteAnn(Ann* ann);
void AddToEnqsList(void* item);
void PairFuturesWithResults(Node* oldHeadNode);
std::pair<unsigned int, Node*> ExecuteDeqsBatch();
void PairDeqFuturesWithResults(Node* oldHeadNode, unsigned int successfulDeqsNum);
/// Helping Functions
void UpdateHead(Ann* ann);
Node* GetNthNode(Node* node, unsigned int n);
Future* RecordOpAndGetFuture(Type);
void ExecuteAllPending();

///implementations
void EnqueueToShared(void* item)
{
		
    Node* newNode = new Node(item, NULL);
    Node* x;
    while(1){
        PtrCnt tailAndCnt = SQTail.load();
        x = NULL;
        if(tailAndCnt.node->next.CAS(x, newNode)){
         
            PtrCnt newTail(newNode, tailAndCnt.cnt+1);
            SQTail.CAS(tailAndCnt, newTail);
            break;
        }
       
        PtrCntOrAnn head = SQHead.load();
        if((head.wrap.tag & 1) == 1){    //the head is announcment
            ExecuteAnn(head.wrap.ann);
        }
        else{
            PtrCnt newtail(tailAndCnt.node->next.load(), tailAndCnt.cnt+1);
            SQTail.CAS(tailAndCnt, newtail); //the head is ptrcnt
      
        }
    }
  
}

void* DequeueFromShared(){

    while(1){
        PtrCnt headAndCnt = HelpAnnAndGetHead();
        Node* headNextNode = headAndCnt.node->next.load();
        if(headNextNode == NULL){
            return NULL;
        }
        PtrCntOrAnn newHead(headNextNode, headAndCnt.cnt+1);
        PtrCntOrAnn expectedHeadAndCnt(headAndCnt); // convert from PtrCnt
        if(SQHead.CAS(expectedHeadAndCnt, newHead)){
            void* retVal = headNextNode->item;
			return retVal;
        }
    }
}

PtrCnt HelpAnnAndGetHead()
{	
    while(1){
        PtrCntOrAnn head = SQHead.load();
        if((head.wrap.tag & 1) == 0){
            return head.ptrCnt;
        }
        ExecuteAnn(head.wrap.ann);  // there is an ann in the head, do it! (a thread helps another thread)
    }
}

Node* ExecuteBatch(BatchRequest batchRequest)
{

    Ann* ann = new Ann(batchRequest);
    PtrCnt oldHeadAndCnt;
    while(1){
        oldHeadAndCnt = HelpAnnAndGetHead();
        ann->oldHead.store(oldHeadAndCnt);  //save the old head in the ann so we can execute it after it being dequeued
        PtrCntOrAnn expectedOldHeadAndCnt(oldHeadAndCnt);
        if(SQHead.CAS(expectedOldHeadAndCnt,PtrCntOrAnn(ann))){  // the head now points to the ann
            break;
        }
    }
    ExecuteAnn(ann);
    return oldHeadAndCnt.node;
}

void ExecuteAnn(Ann* ann)
{
 

    PtrCnt annOldTailAndCnt;
    while(1){
        PtrCnt tailAndCnt = SQTail.load();
        annOldTailAndCnt = ann->oldTail.load();
        if(annOldTailAndCnt.node != NULL){ // another thread linked the batch.
            break;
        }
        Node* null = NULL;
        tailAndCnt.node->next.CAS(null, ann->batchReq.firstEnq); //linking the batch
        if(tailAndCnt.node->next.load() == ann->batchReq.firstEnq){ // the CAS worked thus this was really the tail.
            annOldTailAndCnt = tailAndCnt; // the ann now holds the old tail of the queue
            ann->oldTail.store(tailAndCnt);
            break;
        }
        else{ // the tailAndCnt wasnt really the tail.
            SQTail.CAS(tailAndCnt, PtrCnt(tailAndCnt.node->next, tailAndCnt.cnt+1)); //move on to the next node
        }
    }
    PtrCnt newTailAndCnt(ann->batchReq.lastEnq, annOldTailAndCnt.cnt + ann->batchReq.enqsNum); //the new tail
    SQTail.CAS(annOldTailAndCnt, newTailAndCnt); // update the SQtail , if cas doesnt work then it was already updated (another thread did it)
 
    UpdateHead(ann);
   
}

void UpdateHead(Ann* ann)
{

    unsigned int oldQueueSize = ann->oldTail.load().cnt - ann->oldHead.load().cnt;
    
    unsigned int successfulDeqsNum = ann->batchReq.deqsNum;
    if(ann->batchReq.excessDeqsNum > oldQueueSize){ // there are more excess dequeues than nodes in the old queue
        successfulDeqsNum -= ann->batchReq.excessDeqsNum - oldQueueSize; // remove the excess deqs that are impossible to do. because there are more excess than already in the queue
    }
    PtrCntOrAnn annHelper(ann);
    PtrCnt oh = ann->oldHead.load();
    PtrCntOrAnn annOldHeadHelper(oh);
    if(successfulDeqsNum == 0){
        SQHead.CAS(annHelper, annOldHeadHelper);
        return;
    }
    Node* newHeadNode;
    if(oldQueueSize > successfulDeqsNum){
        newHeadNode = GetNthNode(ann->oldHead.load().node, successfulDeqsNum); //dequeues from the old queue
    }
    else{
        newHeadNode = GetNthNode(ann->oldTail.load().node, successfulDeqsNum - oldQueueSize); // dequeue also from the old queue and also from added batch
    }
    PtrCnt newHeadPtrCnt(newHeadNode,ann->oldHead.load().cnt + successfulDeqsNum);
    SQHead.CAS(annHelper, PtrCntOrAnn(newHeadPtrCnt)); // update the head
}

Node* GetNthNode(Node* node, unsigned int n)
{
    for(int i = 0; i < n; ++i){
        node = node->next.load();
    }
    return node;
}

/**
 *
 * @param item
 */
void Enqueue(void* item)
{
    if(threadData.opsQueue.empty()){
        EnqueueToShared(item);
    }else{
        Evaluate(FutureEnqueue(item));
    }
}

void* Dequeue()
{
    if(threadData.opsQueue.empty()){
        return DequeueFromShared();
    }else{
        return Evaluate(FutureDequeue());
    }
}

Future* FutureEnqueue(void* item)
{
    AddToEnqsList(item);
    ++threadData.enqsNum;
    return RecordOpAndGetFuture(ENQ);
}
void AddToEnqsList(void* item)
{
    Node* node = new Node(item,NULL);
    if(threadData.enqsHead == NULL){
        threadData.enqsHead = node;
    }else{
        threadData.enqsTail->next.store(node);
    }
    threadData.enqsTail = node;
}

Future* RecordOpAndGetFuture(Type futureOpType)
{
    Future* future = new Future();
    threadData.opsQueue.push(FutureOp(futureOpType,future));
    return future;
}

Future* FutureDequeue()
{

    ++threadData.deqsNum;
    if(threadData.deqsNum > threadData.enqsNum){
        threadData.excessDeqsNum++;
    }
    return RecordOpAndGetFuture(DEQ);
}

void Init()
{
    PtrCntOrAnn head;
    PtrCnt tail;
    Node* dummy = new Node(0, NULL);
    head.ptrCnt.node = dummy;
    tail.node = dummy;
    head.ptrCnt.cnt = 0;
    tail.cnt = 0;
    SQHead.store(head);
    SQTail.store(tail);
}

void Init_thread()
{
	threadData.enqsHead = NULL;
	threadData.enqsTail = NULL;
	threadData.enqsNum = 0;
	threadData.deqsNum = 0;
	threadData.excessDeqsNum = 0;
}

void*  Evaluate(Future* future)
{
    if(!future->isDone){
        ExecuteAllPending();
    }
    return future->result;
}

void ExecuteAllPending()
{
    if(threadData.enqsNum==0){
        //No enqueues. execute a dequeues only batch
        std::pair<unsigned int, Node*> result = ExecuteDeqsBatch();
        PairDeqFuturesWithResults(result.second,result.first);
    }else{
        // Execute a batch operation with at least one Enqueue
        Node* oldHeadNode = ExecuteBatch(BatchRequest(threadData.enqsHead,threadData.enqsTail,threadData.enqsNum,threadData.deqsNum,threadData.excessDeqsNum));
        PairFuturesWithResults(oldHeadNode);
        threadData.enqsHead = NULL;
        threadData.enqsTail = NULL;
        threadData.enqsNum = 0;
    }
    threadData.deqsNum = 0;
    threadData.excessDeqsNum = 0;
}

// oldHeadNode is the old head of the shared Queue
void PairFuturesWithResults(Node* oldHeadNode)
{
    Node* nextEnqNode = threadData.enqsHead;
    Node* currentHead = oldHeadNode;
    bool noMoreSuccessfulDeqs = false;
    while(!threadData.opsQueue.empty()){
        FutureOp op = threadData.opsQueue.front();
        threadData.opsQueue.pop();
        if(op.type == ENQ){
            nextEnqNode = nextEnqNode->next.load();
        }else{
            if(noMoreSuccessfulDeqs || currentHead->next.load() == nextEnqNode){
                // the Queue is currently empty
                // this is a excess dequeue
                op.future->result = NULL;
            }else{
				Node* nodePrecedingDeqNode = currentHead;
                currentHead = currentHead->next.load();
                if(currentHead==threadData.enqsTail){
                    // we reached the tail and this is the last dequeue that has a result
                    noMoreSuccessfulDeqs = true;
                }
                op.future->result = currentHead->item;
            }
        }
        op.future->isDone = true;
    }
}
std::pair<unsigned int, Node*> ExecuteDeqsBatch()
{
    PtrCnt oldHeadAndCnt;
    Node* newHeadNode ;
    unsigned int successfulDeqsNum;
    while(true){
        oldHeadAndCnt = HelpAnnAndGetHead();
        newHeadNode = oldHeadAndCnt.node;
        successfulDeqsNum = 0;
        Node* headNextNode = NULL;
        for(int i=0;i<threadData.deqsNum;++i){
            headNextNode = newHeadNode->next.load();
            if(headNextNode == NULL){
                break;
            }
            ++successfulDeqsNum;
            newHeadNode = headNextNode;
        }
        if(successfulDeqsNum == 0){
            break;
        }
        PtrCntOrAnn oldHeadAndCntHelper(oldHeadAndCnt);
        if(SQHead.CAS(oldHeadAndCntHelper,PtrCntOrAnn(newHeadNode,oldHeadAndCnt.cnt+successfulDeqsNum))){
            break;
        }
    }
    return std::pair<unsigned int, Node*>(successfulDeqsNum,oldHeadAndCnt.node);
}
void PairDeqFuturesWithResults(Node* oldHeadNode, unsigned int successfulDeqsNum)
{
    Node* currentHead = oldHeadNode;
    FutureOp op;
    for(int i=0;i<successfulDeqsNum;i++){
		Node* nodePrecedingDeqNode = currentHead;
        currentHead = currentHead->next.load();
        op = threadData.opsQueue.front();
        threadData.opsQueue.pop();
        op.future->result = currentHead->item;
        op.future->isDone=true;
    }
    int excess = threadData.deqsNum - successfulDeqsNum;
    for(int i=0;i< excess ;i++){
        op = threadData.opsQueue.front();
        threadData.opsQueue.pop();
        op.future->result=NULL;
        op.future->isDone=true;
    }
}