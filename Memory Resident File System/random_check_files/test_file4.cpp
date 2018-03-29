#include<bits/stdc++.h>
#include<pthread.h>
#include<sys/shm.h>
#include<signal.h>
#include<unistd.h>

using namespace std;

/*

g++ thread.cpp -lpthread

*/



/*
STATUS CODES:

N=running - only one at a time
D=ready - maintained in the array itself
B=blocked 
T=terminated

*/



char * STATUS;
char * STATUS_2;
pthread_t *threads;
int N;
void sig_1_handler(int signo){

	//cout<<"REACHED HERE_handler\n"<<flush;
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set,SIGUSR2);
	int sig;
	sigwait(&set,&sig);
	signal(SIGUSR1,sig_1_handler);  

	
}

//void sig_2_handler(int signo){

	/*sigset_t set;
	sigemptyset(&set);
	sigaddset(&set,SIGUSR2);
	int sig;
	sigwait(&set,&sig);*/
	//signal(SIGUSR1,sig_1_handler);  

	
//}

void *thread_task(void* thread_idx){
	//cout<<"REACHED HERE__*\n"<<flush;
	signal(SIGUSR1,sig_1_handler);    //installing signal handler for first signal handler
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set,SIGUSR2);
	pthread_sigmask(SIG_BLOCK,&set,NULL);
	//signal(SIGUSR2,sig_2_handler);    //installing signal handler for second signal handler
	int thread_idx_int;
	thread_idx_int=*((int*)thread_idx);
	//cout<<"REACHED HEREEEEEE\n"<<flush;
	usleep(200);
	srand(thread_idx_int);
	int arr[10000];
	for(int i=0;i<10000;i++){
		arr[i]=rand()%2000;
		usleep(80);           // time delay added to properly demonstrate the working of the scheduler
	}
	usleep(150);			// time delay added to properly demonstrate the working of the scheduler
	sort(arr,arr+10000);
	int sleep_time=2;//rand()%10+1;
	STATUS[thread_idx_int]='B';
	//cout<<"\nthread_idx "<<thread_idx_int<<" GOING TO SLEEP\n"<<flush;
	sleep(sleep_time);

	STATUS[thread_idx_int]='T';
	//cout<<"\nthread_idx "<<thread_idx_int<<" GOING TO TERMINATE\n"<<flush;
	pthread_exit(0);

}

void *scheduler_task(void*){
	int    i_prev=N-1;
    int    i_next=0;
    clock_t tim1,tim2,tim3;
    while(i_prev != i_next){
   
       
        tim1=clock();
        pthread_kill(threads[i_next],SIGUSR2);
        STATUS[i_next]='N';       
        while((int)((clock()-tim1)/CLOCKS_PER_SEC) < 1);
        pthread_kill(threads[i_next],SIGUSR1);
        if(STATUS[i_next]!='B' || STATUS[i_next]!='T')
        	STATUS[i_next]='D';       
        i_prev=i_next;
        i_next=(i_next+1)%(N-1);
        while( STATUS[i_next]=='B' || STATUS[i_next]=='T'){
            i_next=(i_next+1)%(N-1);
        }   
       

    }
    if(STATUS[i_next]!='T'){
        pthread_kill(threads[i_next],SIGUSR2);
        STATUS[i_next]='N';       
        while(STATUS[i_next]!='T');
    }
    pthread_exit(0);

}


void *rept_task(void* old_STATUS_void){
	char *old_STATUS=(char*)old_STATUS_void;
	int terminated_count=0;
	while(terminated_count<N-1){

		for(int i=0;i<N;i++){
			//checking context change: Ready->Running
			if(STATUS[i]=='N'  && old_STATUS[i]=='D'){
				old_STATUS[i]='N';
				cout<<"Thread no "<<i<<" (thread id: "<<threads[i]<<" ): Ready -> Running\n"<<flush;
			}

			//checking context change: Running->Ready
			if(STATUS[i]=='D'  && old_STATUS[i]=='N'){
				old_STATUS[i]='D';
				cout<<"Thread no "<<i<<" (thread id: "<<threads[i]<<" ): Running -> Ready\n"<<flush;
			}

			//checking context change: Running/Ready->Blocked
			if((old_STATUS[i]=='D' || old_STATUS[i]=='N') && STATUS[i]=='B'){
				old_STATUS[i]='B';
				cout<<"Thread no "<<i<<" (thread id: "<<threads[i]<<" ):  Running/Ready -> Blocked"<<endl<<flush;	
			}


			//checking context change: Running/Ready->Terminated
			if((old_STATUS[i]=='B') && STATUS[i]=='T'){
				old_STATUS[i]='T';
				cout<<"Thread no "<<i<<" (thread id: "<<threads[i]<<" ):  Blocked -> Terminated"<<endl<<flush;	
				terminated_count++;
			}
			

		}
	}
	pthread_exit(0);
}


int main(){
	cin>>N;
	STATUS=(char*)malloc((++N)*sizeof(char));
	STATUS_2=(char*)malloc(N*sizeof(char));
	srand(time(0));
	int thread_count=0;
	threads=(pthread_t*)malloc(N*sizeof(pthread_t));
	pthread_attr_t attr[N];

	for(int i=0;i<N;i++){
		pthread_attr_init(&attr[i]);
		pthread_create(&threads[i],&attr[i],thread_task,&thread_count);
		usleep(150);
		pthread_kill(threads[i],SIGUSR1);
		STATUS[i]=STATUS_2[i]='D';
		thread_count++;
	}


	//REPORTER
	pthread_t rept_thread;
	pthread_attr_t attr_rept;
	pthread_attr_init(&attr_rept);
	pthread_create(&rept_thread,&attr_rept,rept_task,STATUS_2);
	

	// SCHEDULER
	pthread_t sch_thread;
	pthread_attr_t attr_sch;
	pthread_attr_init(&attr_sch);
	pthread_create(&sch_thread,&attr_sch,scheduler_task,NULL);


	pthread_join(rept_thread,NULL);

	for(int i=0;i<N;i++){
		pthread_cancel(threads[i]);
	}
	pthread_cancel(sch_thread);

	cout<<" \n\nExiting CPU scheduler "<<endl;

}