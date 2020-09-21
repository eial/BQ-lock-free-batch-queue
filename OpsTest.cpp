#include "BQ.hpp"
#include "MSQ.hpp"
#include <chrono>
#include <thread> 
#include <stdio.h> 
#include <stdlib.h>   
#include <time.h> 
#include <cstddef>
#include <iostream>
#include <atomic>
#include <cstdint>
#include <fstream>


#define CAS compare_exchange_strong
using namespace std;
std::atomic<Node*> mflag1;
alignas(64)	

int cnt=0;
int* A;


void run(int ops_per_batch,int threadnum) {
	
	cpu_set_t set;
	CPU_ZERO(&set);
	
	CPU_SET(threadnum, &set);
	sched_setaffinity(0, sizeof(set), &set);
	
	int x,y,z;
	
	while(mflag1.load()== NULL){
		
		switch(rand() % 3) {
			case 0:
				x = rand()%100;
				Enqueue(&x);
				A[threadnum]++;
				break;
			case 1:
				Dequeue();
				A[threadnum]++;
				break;
			case 2:
				
				for(int j=0 ; j< ops_per_batch - 1 ; j++) {
					switch(rand()%2){	
						case 0 : z = rand()%100; 
								FutureEnqueue(&z);
								A[threadnum]++;
								break;
						case 1: FutureDequeue();
								A[threadnum]++;
								break;
					}
				}
				y= rand()%100;
				Enqueue(&y);
				A[threadnum]++;
				break;
		}
		
	}


    return;
}









int main(int argc, char** argv)
{
	int num_of_threads,ops_per_batch,num_of_tests,Time;
	
	num_of_threads = atoi(argv[1]);
	ops_per_batch = atoi(argv[2]);
	
	
	num_of_tests = atoi(argv[3]);
	A = new int[num_of_threads];
	
	
	
	if(num_of_threads == 1){
		
		ofstream myfile ("results.csv");
		
		myfile << "threads,BQ,MSQ,batch,name\n";
		
	}
	
	
	
	int avg1=0;
	int avg2=0;
	//clock_t start;
	Node* n = new Node(NULL,NULL);
	
	
	
	for (int i = 0; i < num_of_tests; i++){
		
		avg1=0;
		mflag1.store(NULL);
    
		Init();
		Init_thread();
		
		
		
		for(int j = 0; j < 50; j++) {
			int x = rand()%100; 
			Enqueue(&x);
		}
		
		for(int j=0;j<num_of_threads;j++){
			A[j]=0;
		}
		
		std::thread threads[num_of_threads];
		//start = clock();
		
		for(int j = 0; j < num_of_threads; j++) {
			threads[j] = std::thread(run, ops_per_batch,j);
		}
		
		std::this_thread::sleep_for(std::chrono::seconds(2));
		mflag1.store(n);
		
		for(int j = 0; j < num_of_threads; j++) {
			threads[j].join();
		}
		
		
		for(int j=0; j<num_of_threads;j++){
			avg1+=A[j];
			
		}
		
		avg2+= avg1/num_of_tests;

		
			
	}
	ofstream myfile ("results.csv", ios_base::app);
	int avg4 = helpfunction(num_of_threads,num_of_tests);
	 myfile << num_of_threads << " , " << avg2 << " , " << avg4 << " ";
	 if(num_of_threads == 1){
		 myfile << ", " << ops_per_batch << "-long batches, " << ops_per_batch << "_long_batch_Graph";
	 }
	 if(num_of_threads == 2){
		myfile << ", , " << ops_per_batch << "_throughput";
	 }
	 myfile << "\n";
		 
	 myfile.close();
	 
	 return 0;
	
}