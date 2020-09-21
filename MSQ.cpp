#include <stdlib.h>
#include <atomic>
#include <cstddef>
#include <iostream>
#include <chrono>
#include <thread> 
#include <stdio.h> 
  
#include <time.h> 


#include <cstdint>

#define CAS compare_exchange_strong


struct Node;
std::atomic<Node*> mflag2;
alignas(64)	

struct pointer_t{
	Node* ptr;
	uintptr_t count;
	pointer_t () noexcept{
		ptr = NULL;
		count = 0;
	}
	pointer_t(Node* ptr, uintptr_t count): ptr(ptr), count(count) {}
};

struct Node{
    void* value;
    std::atomic<pointer_t> next;
	Node() noexcept {
		value = NULL;
	}
   
};

struct queue_t{
     std::atomic<pointer_t> Head;
	 std::atomic<pointer_t> Tail;
	 
	 queue_t()noexcept  {}
};




void MInit(queue_t* q)
{
   Node* n = new Node();
   n->value = NULL;
   
   // check
   pointer_t p1,p2;
   p1.ptr=n;
   p1.count = 0;
   p2 = p1;
   q->Head.store(p1);
   q->Tail.store(p2);
   
   
}


void Enqueue(queue_t* Q,void* item)
{
	Node* n = new Node();
	n->value = item;
	pointer_t p;
	p.ptr = NULL;
	n->next.store(p);
	pointer_t tail;
	pointer_t next;
	while(1){
		tail = Q->Tail.load();
		next = tail.ptr->next;
		if(tail.ptr == Q->Tail.load().ptr){
			if(next.ptr == NULL){
				if(tail.ptr->next.CAS(next, pointer_t(n,next.count+1))){
					break;
				}
			}else{
				Q->Tail.CAS(tail, pointer_t(next.ptr,tail.count+1));
			}
		}
	}
	
	Q->Tail.CAS(tail, pointer_t(n,tail.count +1));
		
}
void* Dequeue(queue_t* Q)
{
	void* p = NULL;
	pointer_t head;
	while(1){
		head = Q->Head.load();
		pointer_t tail = Q->Tail.load();
		pointer_t next = head.ptr->next;
		if(head.ptr == Q->Head.load().ptr){
			if(head.ptr == tail.ptr){
				if(next.ptr == NULL){
					return NULL;
				}
				Q->Tail.CAS(tail, pointer_t(next.ptr, tail.count +1));
			}else{
				p = next.ptr->value;
				if(Q->Head.CAS(head, pointer_t(next.ptr, head.count +1 ))){
					break;
				}
			}
		}
	}
	return p;	
}



//test




int* B;


void run(int threadnum,queue_t* Q) {
	
	cpu_set_t set;
	CPU_ZERO(&set);

	CPU_SET(threadnum, &set);
	sched_setaffinity(0, sizeof(set), &set);
	
	int x,y,z;
	while(mflag2.load()== NULL){
		
		switch(rand() % 2) {
			case 0:
				x = rand()%100;
				Enqueue(Q,&x);
				B[threadnum]++;
				break;
			case 1:
				Dequeue(Q);
				B[threadnum]++;
				break;	
		}
	}
    return;
}






int helpfunction(int num_of_threads, int num_of_tests)
{
	int avg,Time;
	
	
	
	
	B = new int[num_of_threads];
	
	int avg1=0;
	int avg2=0;
	//clock_t start;
	Node* n = new Node();
	

	
	srand(time(NULL));
	avg = 0;
	Time = 0;
	for (int i = 0; i < num_of_tests; i++){
		avg1=0;
		
		mflag2.store(NULL);
		queue_t Q;
		MInit(&Q);
		
		
		
		for(int i = 0; i < 50; i++) {
			int x = rand()%100; 
			Enqueue(&Q,&x);
		}
		
		for(int j=0;j<num_of_threads;j++){
			B[j]=0;
		}
		
		std::thread threads[num_of_threads];
		//start = clock();
		
		for(int j = 0; j < num_of_threads; j++) {
			threads[j] = std::thread(run,j,&Q);
		}
		std::this_thread::sleep_for(std::chrono::seconds(2));
		
		mflag2.store(n);
		
		for(int j = 0; j < num_of_threads; j++) {
			threads[j].join();
		}

		
		for(int j=0; j<num_of_threads;j++){
			avg1+=B[j];
			
		}
		
		avg2+= avg1/num_of_tests;
		
		
		
	}
    
	
	return avg2;
}