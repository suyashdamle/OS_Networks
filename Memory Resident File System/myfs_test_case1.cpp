#include "myfs.h"
int main(){
	int err=create_myfs(10);
	// copying the files:
	char str[50],str2[30];
	for(int i=0;i<8;i++){
		bzero(str,50);
		strcpy(str,"random_check_files/");
		bzero(str2,30);
		sprintf(str2,"test_file%d.cpp",i+1);
		strcat(str,str2);
		// obtained the name of the source file
		int err=copy_pc2myfs(str,str2);
	}
	cout<<"\n\n\nshowing the view of the current directory"<<endl;
	ls_myfs();

	while(1){
		cout<<"enter the file name to delete >";
		bzero(str,50);
		scanf("%s",str);
		int err=rm_myfs(str);
		if(err<0){
			cout<<"deleting failed"<<endl;
		}
		ls_myfs();
	}
}