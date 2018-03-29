#include "myfs.h"
int main(){
	int n;
	int err=restore_myfs("mydump-32.backup");
	int fd=open_myfs("mytest.txt",'r');
	char str[500];
	bzero(str,500);
	err=read_myfs(fd,400,str);
	err=close_myfs(fd);
	int arr[100];
	for(int i=0;i<100;i++){
		memcpy(&n,str+4*i,4);
		//cout<<n<<endl;
		arr[i]=n;
	}
	sort(arr,arr+100);
	fd=open_myfs("sorted.txt",'w');
	for(int i=0;i<100;i++)
		memcpy(str+4*i,&arr[i],4);
	err=write_myfs(fd,400,str);
	err=close_myfs(fd);
	ls_myfs();
	cout<<endl;

	cout<<"displaying file sorted.txt after converting to int "<<endl;
	fd=open_myfs("sorted.txt",'r');
	err=read_myfs(fd,400,str);
	for(int i=0;i<100;i++){
		memcpy(&n,str+4*i,4);
		cout<<n<<", ";
	}cout<<endl;

}