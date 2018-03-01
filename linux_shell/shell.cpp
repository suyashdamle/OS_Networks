#include <unistd.h>
#include<bits/stdc++.h>
#include <fcntl.h> 
#include <sys/wait.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <bits/stdc++.h>

using namespace std;

#define KMAG  "\x1B[1;33m"
#define CYAN "\x1B[1;36m"
#define RESET "\x1B[0m"



int main(){
	string comm,comm2;
	int x;char mode;
	while(1){
		cout<<(CYAN);
		cout<<"\n\nyour loc:<"<<get_current_dir_name()<<">";
		cout<<RESET;	


		printf(KMAG);
		cout<<"\nCustom Shell : Indicate your choice: "<<endl;
		cout<<"\tA.Run an internal command"<<endl;
		cout<<"\tB.Run an external command"<<endl;
		cout<<"\tC.Run an external command by redirecting standard input from a file"<<endl; 
		cout<<"\tD.Run an external command by redirecting standard output to a file "<<endl;
		cout<<"\tE.Run an external command in the background"<<endl; 
		cout<<"\tF.Run several external commands in the pipe mode"<<endl; 
		cout<<"\tG.Quit the shell\n\t>"; 
		printf(RESET);

		scanf(" %c",&mode);
		if(mode=='G')exit(0);

		cout<<"Enter your command : (mode = "<<mode<<") ->";
		
		getchar();
		getline(cin,comm);
		if(mode=='A'){
			// NO fork in this case
			string str;
			istringstream i_str_2(comm);
			i_str_2>>str;
			if(str=="chdir" || str=="cd"){
				i_str_2>>str;
				int ret=chdir(str.c_str());
				if(ret<0)
					cout<<"chdir unsuccessful"<<endl;
				continue;
			}
			if(str=="mkdir"){
				i_str_2>>str;
				int ret=mkdir(str.c_str(),ACCESSPERMS);
				if(ret<0)
					cout<<"mkdir unsuccessful"<<endl;
				continue;
			}

			if(str=="rmdir"){
				i_str_2>>str;
				int ret=rmdir(str.c_str());
				if(ret<0)
					cout<<"rmdir unsuccessful"<<endl;
				continue;
			}

		}









		
		else if (mode=='F'){    
				int process_no=0;
				int total_process=0;
				char string_part[comm.length()];
				char new_string_part[comm.length()];
				strcpy(string_part,comm.c_str());
				strcpy(new_string_part,comm.c_str());

				comm2=comm;
				istringstream i_str(comm);
				istringstream i_str_2(comm2);
				string word;
				
				char *string_part_1=strtok(new_string_part, "|");
			    while(string_part_1!=NULL){
			    	total_process++;
			    	string_part_1=strtok(NULL, "|");
			    }


				fflush(stdout);
				pid_t pids[total_process+1];
				int **pipes_all=(int**)malloc(total_process*sizeof(int*));
				for(int i=0;i<=total_process;i++){
					pipes_all[i]=(int*)malloc(2*sizeof(int));
				}
				char* string_part_2=strtok(string_part, "|");
			    while(string_part_2!=NULL){
			    	fflush(stdout);
			 		process_no++;
			 		pipe(pipes_all[process_no]);
			 		pids[process_no]=fork();
			 		string str_2(string_part_2);
			 		string str_1(string_part_2);
			 		i_str_2.str(str_2);
			 		i_str.str(str_1);
					if(pids[process_no]==0){
						
						int num=0;
					    while(i_str_2>>word){
							num++;
						}
						char** arguments_1=(char**)malloc((num+1)*sizeof(char*));
						num=0;
						while(i_str>>word){
							arguments_1[num++]=(char*)malloc((word.length()+1)*sizeof(char));
							strcpy(arguments_1[num-1],word.c_str());
							strcat(arguments_1[num-1],"\0");
						}
						arguments_1[num]=(char*)NULL;
						/*char *arguments_1[]={"./test.out",(char*)NULL};
						char* arguments_2[]={"sort",(char*)NULL};*/
						if(process_no!=1){
							fflush(stdout);
							dup2(pipes_all[process_no-1][0],STDIN_FILENO);
							close(pipes_all[process_no-1][1]);
						}
						if(process_no!=total_process){
							fflush(stdout);
							dup2(pipes_all[process_no][1],STDOUT_FILENO);//dup2(pipe_1[0],STDIN_FILENO);
							close(pipes_all[process_no][0]);
						}


						int ret=execvp(arguments_1[0],arguments_1);
						
						if(ret>0)
							cout<<"process done in :"<<string_part_2<<endl;
						else
							cout<<"execvp failed in "<<string_part_2<<endl;
						fflush(stdout);
						return 1;
					}
					if(pids[process_no]!=0){
						string_part_2=strtok(NULL,"|");
						continue;
					}
				}
				for(int i=1;i<=total_process;i++){
					int status;
					close(pipes_all[i][0]);
					close(pipes_all[i][1]);
					while(wait(NULL)!=pids[i]);
				}
			}










		else if(comm=="quit" || mode=='G'){exit(0);}

		else{       // normal single forking required
			comm2=comm;
			istringstream i_str(comm);
			istringstream i_str_2(comm);
			string word;
			x=fork();
			int file_desc;
			if(x==0){
				int stdin_copy,stdout_copy;
				if(mode=='D'){  // external with output redirection
					fflush(stdout);
					char string_part[comm.length()];
					strcpy(string_part,comm.c_str());
					char *string_part_1=strtok(string_part,">");
					char *out_file_name=strtok(NULL,">");
					stdout_copy=dup(STDOUT_FILENO);
				    file_desc = open(out_file_name, O_WRONLY| O_CREAT);
				    dup2(file_desc,STDOUT_FILENO);
				    i_str_2.str(string(string_part_1));
				    i_str.str(string(string_part_1));
				}

				else if(mode=='C'){  // external with input redirection
					char string_part[comm.length()];
					strcpy(string_part,comm.c_str());
					char *string_part_1=strtok(string_part,"<");
					while(string_part_1!=NULL){
						x=fork();
					}
					char *in_file_name=strtok(NULL,"<");
					stdin_copy=dup(STDIN_FILENO);
				    file_desc = open(in_file_name, O_RDONLY);
				    dup2(file_desc,STDIN_FILENO);
				    i_str_2.str(string(string_part_1));
				    i_str.str(string(string_part_1 ));
					
					
				}
				else if(mode=='E'){
					char string_part[comm.length()];
					strcpy(string_part,comm.c_str());
					char *string_part_1=strtok(string_part,"&");
					char *string_part_2=strtok(NULL,"&");
				    i_str_2.str(string(string_part_1));
				    i_str.str(string(string_part_1 ));
					
				}

				else if(mode=='B'){}
				else{
					cout<<"Invalid input "<<endl;
					continue;
				}

				

				//child process - does the job
				int num=0;
				const char* file_name;
				while(i_str_2>>word){
					num++;
				}
				char** arguments=(char**)malloc((num+1)*sizeof(char*));
				num=0;
				while(i_str>>word){
					arguments[num++]=(char*)malloc((word.length()+1)*sizeof(char));
					strcpy(arguments[num-1],word.c_str());
					strcat(arguments[num-1],"\0");
				}
				arguments[num]=(char*)NULL;
				int ret=execvp(arguments[0],arguments);
				cout<<ret<<endl;
				if(ret<0)
					cout<<"** ERROR: execvp() failed to execute"<<endl;

				fflush(stdout);

				if(mode=='C'){
					dup2(stdin_copy,STDIN_FILENO);
				}

				if(mode=='D'){
					fflush(stdout);
					dup2(stdout_copy,STDOUT_FILENO);
				}
				_Exit(0);
			}
			else{
				int status;
				if(mode=='E')continue;
				while(wait(&status)!=x){//cout<<"still waiting"<<endl;
				}
				//cout<<"killing previous thread"<<endl;
				//kill(x,SIGTERM);
			}
		}

	}
}
