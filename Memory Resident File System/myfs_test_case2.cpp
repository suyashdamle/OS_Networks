#include "myfs.h"

int main(){
	int err=create_myfs(10);//got the file system in the MRFS pointer
	int fd1=open_myfs("mytest.txt",'w');
	if(fd1<0)exit(-1);
	char str[500];
	bzero(str,500);
	srand(time(0));
	char str2[20],str3[20];
	int n;
	for(int i=0;i<100;i++){
		n=rand()%1000;
		memcpy(str+4*i,&n,4);
	}
	// got the list of 100 random integers in str.
	int length_written=400;
	length_written=write_myfs(fd1,400,str);
	err=close_myfs(fd1);

	cout<<"after creating first file: "<<endl;
	err=ls_myfs();
	int N;
	cout<<"enter the value of N: ";
	cin>>N;
	
	for(int i=0;i<N;i++){
		bzero(str2,20);
		strcpy(str2,"mytest-");
		bzero(str3,20);
		sprintf(str3,"%d",i+1);
		strcat(str2,str3);
		strcat(str2,".txt");
		//copying
		fd1=open_myfs("mytest.txt",'r');
		int fd_new=open_myfs(str2,'w');
		bzero(str,500);
		int read=read_myfs(fd1,400,str);
		int written=write_myfs(fd_new,400,str);
		err=close_myfs(fd_new);
	}
	cout<<"\n\n\nls at the end: "<<endl;
	err=ls_myfs();

	//dumping into the file:mydump-32.backup
	dump_myfs("mydump-32.backup");

	vector<int> file_blocks_list=get_file_block_list(1);
	cout<<"printing file blocks: "<<endl;
	for(int i=0;i<file_blocks_list.size();i++){
		cout<<file_blocks_list[i]<<" ";
	}cout<<endl;
	//cout<<MRFS[69][0]<<endl;
	cout<<"\n\n\nCOMPLETED EXECUTION OF CODE\n\n\n"<<flush;
	return 0;
}