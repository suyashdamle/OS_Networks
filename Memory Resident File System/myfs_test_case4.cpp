#include "myfs.h"
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>       /* clock_t, clock, CLOCKS_PER_SEC */
#include <math.h>     
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
int main(){
	key_t key;
	//creating a shared memory segment of enough size to store the whole shared memory segment
	int shm_myfs=shmget(key, (2*1024*1024+100), IPC_CREAT | 0666);
	if(shm_myfs<0){
		cout<<"could not get shared memory"<<endl;
	}
   	int *shmPT=(int*)shmat(shm_myfs,NULL,0);
   	NUMBER_BLOCKS=ceil((2*1024*1024)/BLOCK_SIZE);             // size in MB

	int (*myfs[NUMBER_BLOCKS]);
	for(int i=0;i<NUMBER_BLOCKS;i++){
		myfs[i]=shmPT+i*(BLOCK_SIZE);
	}
	
	// creating an mrfs of size 10 MB in the shared memory segment
	int err=create_myfs(2,myfs);       // forcing creation of the file system in the shared memory

	//already in root
	mkdir_myfs("mydocs");
	mkdir_myfs("mycode");
	//ls_myfs();

	chdir_myfs("mydocs");
	mkdir_myfs("mytext");
	mkdir_myfs("mypapers");
	//ls_myfs();

	// creating P1- add file in mytext
	int x=fork();
	if(x==0){
		// P1
		int *shmPT=(int*)shmat(shm_myfs,NULL,0);
		//int (*MRFS[NUMBER_BLOCKS]);
		for(int i=0;i<NUMBER_BLOCKS;i++){
			MRFS[i]=shmPT+i*(BLOCK_SIZE);
		}

		//ls_myfs();
		chdir_myfs("mytext");
		char str[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		//cout<<"printing the string:"<<str<<endl;
		int fd1=open_myfs("myfile.txt",'w');
		int err=write_myfs(fd1,53,str);
		err=close_myfs(fd1);
		//ls_myfs();
		return 0;
	}
	else{
		// P2
		int *shmPT=(int*)shmat(shm_myfs,NULL,0);
		//int (*MRFS[NUMBER_BLOCKS]);
		for(int i=0;i<NUMBER_BLOCKS;i++){
			MRFS[i]=shmPT+i*(BLOCK_SIZE);
		}
		//ls_myfs();
		chdir_myfs("..");         // reached the root
		chdir_myfs("mycode");
		//ls_myfs();
		copy_pc2myfs("random_check_files/test_file1.cpp","test_file1.cpp");


		wait(NULL);
		//verifying the correctness of the process:
		cout<<"printing file system directories: \n"<<endl;

		chdir_myfs("..");              // back to root
		cout<<"\n\nTHE ROOT DIRECTORY"<<endl;
		ls_myfs();
		chdir_myfs("mydocs");
		cout<<"\n\nroot/mydocs DIRECTORY"<<endl;
		ls_myfs();
		chdir_myfs("mytext");
		cout<<"\n\nroot/mydocs/mytext DIRECTORY"<<endl;
		ls_myfs();

		cout<<"\n\nprinting the alphabet file: "<<endl;
		showfile_myfs("myfile.txt");

		chdir_myfs("..");   //in mydocs
		chdir_myfs("..");   //in myroot
		chdir_myfs("mycode");   //in mycode

		cout<<"\n\nroot/mycode DIRECTORY"<<endl;
		ls_myfs();

		cout<<"\n\nprinting the copied file: "<<endl;
		showfile_myfs("test_file1.cpp");		

	}

}
