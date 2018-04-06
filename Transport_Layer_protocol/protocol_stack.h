#include <bits/stdc++.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <pthread.h> 
#include <sys/stat.h>
#include <sys/wait.h>
#include <math.h>
#include <arpa/inet.h>
#include <signal.h>
#include <poll.h>

using namespace std;

//////////////////////  SENDER'S PARAMETERS    ////////////////////////////////////////

#define BUFSIZE 1024
#define TIMEOUT_PERIOD 5
#define MAX_WINDOW_SIZE 500          // a circular buffer(EACH of 1024 characters) of this size is maintained - it could not be exceeded
#define SENDER_BUFFER_SIZE (100*1024)
bool timeout_occured=false;
//bool congestion_control_running=false;   // whether or not the congestion control thread is running
bool sending_initialized=false;
bool triple_dupack=false;

int sender_window=1024;
int threshold=30*1024;                              // the threshold of TCP Toho
int currptr=-1,baseptr=0,old_baseptr=-1;
int sender_buff_st=-1;
int sender_buff_end=-1;
int ack_count=0;
int present_count=0;
int sender_socket_fd=0;

pthread_mutex_t sender_buffer_lock;    // the Mutex lock for the sender - buffer operations
pthread_mutex_t ack_count_lock;
pthread_mutex_t pointers_lock;
pthread_t udp_receive_threadid;
pthread_t congestion_control_threadid;
bool window_first=true;

socklen_t serverlen;
struct sockaddr_in serveraddr;

char* sender_buffer;

/////////////////////////////////////  RECEIVER'S PARAMETERS  ///////////////////////////////////////////

#define RECV_BUFFER_SIZE (500*1024)
//bool congestion_control_running=false;   // whether or not the congestion control thread is running
int recv_window=1024;
//int currptr=0,baseptr=0,old_baseptr=0;
int recv_buff_st=0;
int recv_buff_end=0;
int last_ack_byte=-1;                              // last byte number
float drop_prob;
int appRecv_bytes_read=0;

pthread_t buffer_controller_threadid;
pthread_mutex_t buffer_lock;
pthread_mutex_t recv_buffer_param_lock;

vector<bool> byte_filled(RECV_BUFFER_SIZE,0);

socklen_t clientlen;
struct sockaddr_in clientaddr;
char * recv_data_buffer;
bool first_execution=true;

//////////////////////////////////////////////////////////////////////////////////////////////////////////


bool buff_controller_running=false;


void error(const char *msg) {
    perror(msg);
    exit(0);
}


void mysig(int sig)
{
    pid_t pid;
    printf("A time-out occurred\n");
    timeout_occured = true;
    window_first=true;
    
}


void * buffer_controller(void *data);

char * create_packet(int start_byte,int number_bytes,int finish_at=-1){
    char *str=(char *)malloc(BUFSIZE*sizeof(char));
    bzero(str,BUFSIZE);
    int k=start_byte;
    int i;

    for(i=0;i<number_bytes-1;i++){
        //cout<<"i,k,finish_at::"<<i<<" "<<k<<" "<<finish_at<<endl;
        str[i]=sender_buffer[k];
        if(k==finish_at){
            i++;
            //cout<<"k,finish_at"<<k<<" "<<finish_at<<endl;
            break;

        }
        //cout<<"chk create_packet:*:0>>\n"<<str[i]<<"\n";
        k=(k+1)%SENDER_BUFFER_SIZE;
        
    }
    //cout<<"\n\ninside create_packet i,k,finish_at::"<<i<<" "<<k<<" "<<finish_at<<endl;
    str[i]='\0';
    //cout<<"\nstart_byte,strlen(str)  "<<start_byte<<" "<<strlen(str)<<'\n';//<<" "<<str<<endl;
    return str;
                
}




void * congestion_control(void *data){
    int n_send,n_recv;
    char buf[BUFSIZE];
    //int sender_socket_fd=*((int*)data);
    int start_from=0;
    int finish_at=-1;
    while(1){
        //cout<<"\npointers_lock::1::start\n";
        pthread_mutex_lock(&pointers_lock);
        {   
            int z=sender_buff_end-sender_buff_st+1;
            int buff_content=(sender_buff_end>sender_buff_st)?(z):(SENDER_BUFFER_SIZE-z+2);
            if(sender_buff_end<0 || sender_buff_st<0)
                buff_content=0;
            baseptr=(currptr+sender_window);        // both currptr and baseptr are in terms of the number of bytes sent/ack'ed
        }
        pthread_mutex_unlock(&pointers_lock);
        //cout<<"\npointers_lock::1::end\n";
        //cout<<"\nbaseptr "<<baseptr<<"old_baseptr "<<old_baseptr<<"sender_buff_end "<<sender_buff_end<<"\n";
        
        start_from=(finish_at+1)%SENDER_BUFFER_SIZE;

        // checking whether there is anything to send...

        if(timeout_occured){
            cout<<"detected timeout here"<<endl<<flush;
            window_first=true;
            goto timeout_handler;
            //cout<<"start_from, sender_buff_end-sender_buff_st: "<<start_from<<", "<<sender_buff_end<<endl;
        }
        if(start_from<sender_buff_end && (sender_buff_end+1)%SENDER_BUFFER_SIZE!=sender_buff_st){
                 // the below part runs in only 1 of the first 3 cases   
                //cout<<"baseptr : old_baseptr"<<baseptr<<" : "<<old_baseptr<<endl<<flush;
                if(baseptr>old_baseptr && sender_buff_end>=0){                    // ie, if there has been some change in the window
                    //cout<<"baseptr, old_baseptr: "<<baseptr<<" "<<old_baseptr<<endl;
                    //baseptr=(currptr+sender_window);        // both currptr and baseptr are in terms of the number of bytes sent/ack'ed
                    if(sender_buff_st<sender_buff_end){
                        finish_at=min(sender_buff_end,(baseptr%SENDER_BUFFER_SIZE)-1);
                    }
                    else{
                        if(sender_buff_st<baseptr)
                           finish_at=max(sender_buff_end,(baseptr%SENDER_BUFFER_SIZE)-1);
                       else
                           finish_at=min(sender_buff_end,(baseptr%SENDER_BUFFER_SIZE)-1); 
                    }
                    int k=start_from;
                    
                    // sending the new window\n
                    //cout<<"\nSending the NEW WINDOW: "<<start_from<<" "<<finish_at<<endl;
                    while(1){
                        char *str=create_packet(k,1013,finish_at);
                        k=(k+strlen(str))%SENDER_BUFFER_SIZE;
                        cout<<"k, finish_at"<<k<<" "<<finish_at<<endl;
                        char buf[1024];
                        sprintf(buf,"DATA:%d:%s",present_count,str);
                        present_count+=strlen(str);
                        cout<<"sending "<<strlen(str)<<" no of characters"<<endl;
                        n_send= sendto(sender_socket_fd, buf, strlen(buf),0,(struct sockaddr*)&serveraddr,serverlen);
                        if (n_send < 0) 
                            error("ERROR sending message");
                        //cout<<"chk:^:1>>\n"<<buf<<"\n";
                        if(window_first){
                            alarm(TIMEOUT_PERIOD);
                            timeout_occured=false;
                        }
                        cout<<"SENT:"<<endl;//<< buf
                        window_first=false;
                        if(k>=finish_at)
                            break;
                        if(timeout_occured)break;
                    }
                    old_baseptr=baseptr;
                }
            
                else if(triple_dupack){
                    cout<<"\n\n\nhandling triple_dupack"<<endl;
                    threshold=threshold/2;
                    sender_window=threshold+1;
        
                    char *str=create_packet(sender_buff_st,1013);
                    sprintf(buf,"DATA:%d:%s",currptr+1,str);
                    //cout<<"n_send::"<<buf<<endl;
                    n_send= sendto(sender_socket_fd, buf, strlen(buf),0,(struct sockaddr*)&serveraddr,serverlen);
                    if (n_send < 0) 
                        error("ERROR sending message");
                    cout<<"triple_dupack: sending:  "<<endl;//<<buf
                    pthread_mutex_lock(&ack_count_lock);
                    {
                        triple_dupack=false;
                        ack_count=0;
                    }
                    pthread_mutex_unlock(&ack_count_lock);
                }
        
                else if(timeout_occured){
                    timeout_handler:
                    cout<<"entered in timeout handler"<<endl<<flush;
                    cout<<"expected condition: "<<baseptr<<" "<<old_baseptr<<endl;
                    threshold=threshold/2;
        
                    pthread_mutex_lock(&pointers_lock);
                    {
                        sender_window=1013;           // set at 1 MSS
                        old_baseptr=baseptr=currptr+sender_window;
                    }
                    pthread_mutex_unlock(&pointers_lock);
        
                    int start_from=(currptr+1)%SENDER_BUFFER_SIZE;
                    int finish_at;
                    //--------------------------------------------------------------------------
                    if(sender_buff_st<=sender_buff_end){
                        finish_at=min(sender_buff_end,(baseptr%SENDER_BUFFER_SIZE)-1);
                    }
                    else{
                        if(sender_buff_st<baseptr)
                           finish_at=max(sender_buff_end,(baseptr%SENDER_BUFFER_SIZE)-1);
                       else
                           finish_at=min(sender_buff_end,(baseptr%SENDER_BUFFER_SIZE)-1); 
                    }
                    //--------------------------------------------------------------------------
                    int k;
                    // sending the new window
                    while(1){
                        k=start_from;
                        present_count=start_from;//present_count=baseptr;
                        char *str=create_packet(k,1013);
                        k=(k+strlen(str))%SENDER_BUFFER_SIZE;
                        char buf[1024];
                        sprintf(buf,"DATA:%d:%s",present_count,str);
                        present_count+=strlen(str);
                        //cout<<"n_send::"<<buf<<endl;
                        n_send= sendto(sender_socket_fd, buf, strlen(buf),0,(struct sockaddr*)&serveraddr,serverlen);
                        if (n_send < 0) 
                            error("ERROR sending message");
                        if(window_first){
                            alarm(TIMEOUT_PERIOD);
                            timeout_occured=false;
                            window_first=false;
                        }
                        cout<<"start_from, finish_at at timeout_handler: "<<start_from<<", "<<finish_at<<endl;
                        cout<<"RESENDING :"<<endl;//<<buf
                        cout<<"k= "<<k<<endl;
                        if(k>=finish_at)
                            break;
                    }
        
                }
                
            }
            /*
            else{
                return &sender_window;
            }*/
        }
    
}


void update_window(int y,int z){
    int new_ack=y,tmp1,tmp2;    
    tmp1=new_ack%SENDER_BUFFER_SIZE; 
    tmp1=(tmp1+1)%SENDER_BUFFER_SIZE;                                   // making space in the buffer
    int receiver_window=z;
    //packet_start=new_ack+1;
    //tmp2=packet_start;
    if(new_ack>=currptr){                                                     // some new packet acknowledged
        pthread_mutex_lock(&pointers_lock);
        {

            sender_buff_st=min(tmp1,sender_buff_end);

            if(sender_window<threshold)
                sender_window=min((sender_window+new_ack-currptr),receiver_window);            // taking care of the buffer space on the receiver side
            else
                sender_window+=1024;   //increasing by 1 MSS
            currptr=new_ack;
        }
        pthread_mutex_unlock(&pointers_lock);
    }
    else{
        pthread_mutex_lock(&ack_count_lock);
        {
            ack_count++;
            if(ack_count>=3)
                triple_dupack=true;
        }
        pthread_mutex_unlock(&ack_count_lock);
        usleep(100);
    }
    cout<<"\n\nUPDATING WINDOW: new window: "<<sender_window<<endl;

}
int parse_packets(char buf[]){
    int z,y,t=0;
    if(buf[0]=='A' && buf[1]=='C' && buf[2]=='K'){
        sscanf(buf,"ACK:%d %d",&y,&z);
        update_window(y,z);
    }
    //while(currptr<baseptr){//read_size>=1                //file_scanner(file_to_send,1013,buf)
      //      n_recv=recvfrom(sender_socket_fd, buf, BUFSIZE,0,(struct sockaddr*)&serveraddr,&serverlen);
    return(1);
}
/*
void* udp_receive(void *data){
    //int sender_socket_fd=*((int*)data);

    while(1){
        int n_recv;
        char buf[BUFSIZE],buf_2[BUFSIZE];
        int t=0;
        n_recv=recvfrom(sender_socket_fd, buf, BUFSIZE,0,(struct sockaddr*)&serveraddr,&serverlen);
        if(n_recv<0)continue;
        if(n_recv>0){
            cout<<"ACK received : "<<buf<<endl;
        }
        cout<<"\n\n\n\nreceived ACK::"<<buf<<"\n\n"<<endl;
        //cout<<"n_recv here: "<<n_recv<<":: ::"<<buf<<endl<<flush;
        t=parse_packets(buf);
        if(t==0)
            error("\nError in parse_packets\n");
    }
}*/




void appSend(int sockfd,char *data_to_send){
    //cout<<"data here: "<<data_to_send<<endl<<flush;
    if(!sending_initialized){
        sender_buffer=(char*)malloc(SENDER_BUFFER_SIZE*sizeof(char));
        pthread_create(&congestion_control_threadid, NULL,&congestion_control,&sender_socket_fd);
        //pthread_create(&udp_receive_threadid,NULL,&udp_receive,&sender_socket_fd);
        sending_initialized=true;
        pthread_mutex_init(&ack_count_lock, NULL);
        pthread_mutex_init(&pointers_lock, NULL);
    }
    if(!buff_controller_running){
    	clientaddr=serveraddr;
    	clientlen=serverlen;
    	pthread_create(&buffer_controller_threadid,NULL,&buffer_controller,&sender_socket_fd);
    	buff_controller_running=true;
    }

    int z=sender_buff_end-sender_buff_st+1;
    int buff_space=(sender_buff_end>sender_buff_st)?(SENDER_BUFFER_SIZE-z):(z);
    if(sender_buff_st==-1){//initialization
        sender_buff_st=0;
        buff_space=SENDER_BUFFER_SIZE;
    }
    // BUSY WAITING
    while(strlen(data_to_send)>buff_space){
        z=sender_buff_end-sender_buff_st+1;
        int buff_space=(sender_buff_end>sender_buff_st)?(SENDER_BUFFER_SIZE-z):(z);
    }

    for(int k=0;k<strlen(data_to_send);k++){
        sender_buffer[(sender_buff_end+1+k)%SENDER_BUFFER_SIZE]=data_to_send[k];
    }
    sender_buff_end=(sender_buff_end+strlen(data_to_send))%SENDER_BUFFER_SIZE;
}

int establish_connection(char *hostname, int portno){    
    struct hostent *server;
    int  n_send,n_recv;
    signal(SIGALRM,mysig);
    char buf[BUFSIZE],buf_2[BUFSIZE];
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    /* check command line arguments */
   
    /* socket: create the socket */
    sender_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sender_socket_fd < 0) 
        error("ERROR opening socket");

    /*
    *
    * NOTE: the setsocketopt() does the task of waiting till time-out. After expiry of the 
    *       timeout, it exists and then re-transmits again
    */
    
    setsockopt(sender_socket_fd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv));
    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
      (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* get a message from the user */
    serverlen = sizeof(serveraddr);
    return sender_socket_fd;

}

int ftp_send_handshake(const char* file_loc,long int file_size){
    char buf[BUFSIZE];
    int n_send,n_recv=-1;
    while(n_recv<0){
        /* send file name and size to the server */
        sprintf(buf,"%s %ld",file_loc,file_size);
        cout<<"sending : "<<buf<<endl;
        cout<<"to : "<<sender_socket_fd<<endl;
        n_send= sendto(sender_socket_fd, buf, strlen(buf),0,(struct sockaddr*)&serveraddr,serverlen); 
        if (n_send < 0) 
          error("ERROR sending size to server");


        /* print the server's reply */
        bzero(buf, BUFSIZE);
        n_recv= recvfrom(sender_socket_fd, buf, BUFSIZE,0,(struct sockaddr*)&serveraddr,&serverlen);
    }
    printf("received from server: %s\n\n",buf);

    
    char new_file_loc[200];
    long int new_file_size;
    sscanf(buf,"%s %ld",new_file_loc,&new_file_size);
    if(strcmp(new_file_loc,file_loc)!=0 || new_file_size!=file_size){
        printf("ERROR: handshaking failed\n");
        return 0;
    }
    return 1;
}

int ftp_send_md5_checksum(char* file_loc){
    char buf[BUFSIZE];
    int n_recv,n_send;
    bool matched=false;
    char md5_command[500];
    char md5_file[200];
    sprintf(md5_command,"md5sum %s > MD5_%s",file_loc,file_loc);
    sprintf(md5_file,"MD5_%s",file_loc);
    
    system(md5_command);
    FILE * checksum=fopen(md5_file,"r");
    char buf_org[BUFSIZE];
    fscanf(checksum,"%s",buf_org);
    bzero(buf,BUFSIZE);
    char buf3[1024];
    n_recv=-1;
    while(n_recv<0){
        bzero(buf3,BUFSIZE);
        n_recv= recvfrom(sender_socket_fd, buf, BUFSIZE,0,(struct sockaddr*)&serveraddr,&serverlen);
        sscanf(buf,"CHECKSUM:%s",buf3);
        //cout<<"\n buf in md5: "<<buf<<'\n';
        if(strlen(buf3)==0)
            n_recv=-1;
    }
    if (n_recv < 0) 
      error("ERROR reading from socket");
    //cout<<"n_recv: "<<n_recv<<endl<<flush;
    fflush(stdout);
    printf("RECEIVED: %s\n",buf3);
    printf("ORIGINAL: %s\n",buf_org);
    fflush(stdout);
    if(strcmp(buf3,buf_org)==0){
        printf("MD5 Matched...exiting\n");
        matched=true;
    }
    else{
        matched=false;
        printf("MD5 NOT Matched...exiting\n");
    }

    fclose(checksum);
    sprintf(md5_command,"rm MD5_%s",file_loc);
    system(md5_command);           //deleting the created file of md5 sum

    if(matched)
        return 1;
    return 0;
}





////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////






void sendACK(int byte_idx,int socket_fd){
  int n_send;
  char buf[BUFSIZE];
  bzero(buf,BUFSIZE);
  int z=recv_buff_end-recv_buff_st+1;
  int recv_buff_space=(recv_buff_end>recv_buff_st)?(RECV_BUFFER_SIZE-z):(z);
  sprintf(buf,"ACK:%d %d",byte_idx,recv_buff_space);
  cout<<"\n\n\n\nsending ACK: "<<buf<<"\nZ:"<<z<<endl;
  n_send = sendto(socket_fd, buf, strlen(buf),0,(struct sockaddr *) &clientaddr, clientlen); 
  if (n_send < 0) 
    error("ERROR writing to socket");

}






void * buffer_controller(void *data){
  cout<<"\n\nBUFFER CONTROLLER THREAD RUNNING\n\n"<<endl;
  int socket_fd=(*(int *)data);cout<<"socket_fd: "<<socket_fd<<endl;
  char buf[BUFSIZE];
  char buf_2[BUFSIZE];
  bool first_packet=true;
  int to_be_acked=-1;
  int n_recv,received_byte_count=-1,bytes_received=0;
  srand(clock());
  
  while(1){
    bzero(buf,BUFSIZE);
    n_recv=recvfrom(socket_fd, buf, BUFSIZE,0,(struct sockaddr*)&clientaddr,&clientlen);//(struct sockaddr*)&serveraddr,&serverlen)
    if(n_recv<=0)continue;
    //someting received
    bzero(buf_2,BUFSIZE);





    ///////////////////// DECIDING WHETHER IT IS ACK OR DATA   //////////////////////////////////

    if((buf[0]=='A' && buf[1]=='C' && buf[2]=='K') || (buf[0]=='C' && buf[1]=='H' && buf[2]=='E') ){
    	///////////////////    IF IT IS ACK  OR CHECKSUM ///////////////////////////////////

    	cout<<"\n\n\n\nreceived ACK::"<<buf<<"\n\n"<<endl;
        //cout<<"n_recv here: "<<n_recv<<":: ::"<<buf<<endl<<flush;
        int t=parse_packets(buf);
        if(t==0)
            error("\nError in parse_packets\n");
        continue;

    }



    ///////////////// IF IT IS DATA //////////////////////////////////////////////
    else if(buf[0]=='D' && buf[1]=='A' && buf[2]=='T' && buf[3]=='A'){          
        sscanf(buf,"DATA:%d:%1014[^\b]",&received_byte_count,buf_2);//1014[^\a]
        cout<<"received_byte_count: "<<received_byte_count<<endl;
        bytes_received=strlen(buf_2);
        cout<<"received bytes "<<bytes_received<<endl<<flush;
        //if(bytes_received==0)continue;
    
        // deciding whether to drop this packet on the basis of drop prob
        float prob=(float)(rand()%100+rand()%100)/200.0;
        if(prob<drop_prob){
          cout<<"dropping packet"<<endl;
          continue;
        }
    
        if(received_byte_count<=last_ack_byte){
          sendACK(last_ack_byte,socket_fd);
          continue;
        }
    
        /// -----------------------  CODE TO PUT DATA INTO BUFFER  ----------------------------------------
    
        int start_from=(received_byte_count%RECV_BUFFER_SIZE);
        int finish_at=(start_from+bytes_received-1)%RECV_BUFFER_SIZE;;
        cout<<"start_from, finish_at: "<<start_from<<" "<<finish_at<<endl;
        cout<<"recv_buff_st, recv_buff_end: "<<recv_buff_st<<" "<<recv_buff_end<<endl;
    
        if(recv_buff_end != -1 && recv_buff_st!=-1 ){
          if(recv_buff_st>=recv_buff_end && !first_packet){
                
                //while(!(recv_buff_end>=start_from || recv_buff_st<=start_from)){continue;}
                while(finish_at>=recv_buff_st){
                  continue;
                }
                
    
                //finish_at=min(recv_buff_end,((bytes_received+start_from-1)%RECV_BUFFER_SIZE));
          }
          else{
              if(finish_at<start_from){
                while(finish_at>=recv_buff_st){continue;}
              }
                 //finish_at=max(recv_buff_end,(baseptr%RECV_BUFFER_SIZE)-1);
             else{
              if(start_from<recv_buff_st)
                while(recv_buff_st<=finish_at){continue;}
              }  //finish_at=min(recv_buff_end,(baseptr%RECV_BUFFER_SIZE)-1); 
    
          }
        }
        cout<<"before recv_buff_end update: "<<start_from<<" "<<finish_at<<" "<<recv_buff_end<<endl;
        int j=0;
        for (int i = start_from; i!=finish_at+1 ; i=(i+1)%RECV_BUFFER_SIZE)
        {
          byte_filled[i]=1;
    
          recv_data_buffer[i]=buf_2[j];
          j++;
        }
        while(byte_filled[(recv_buff_end)%RECV_BUFFER_SIZE]==true){
          recv_buff_end=(1+recv_buff_end)%RECV_BUFFER_SIZE;
          if(recv_buff_end==(recv_buff_st-1)||(recv_buff_end==RECV_BUFFER_SIZE-1 && recv_buff_st==0))
            break;
        }
        cout<<"after recv_buff_end update: ,last_ack_byte"<<recv_buff_end<<" "<<last_ack_byte<<endl;
    
    
        /// -------------------------------------------------------------------------------------------------------
    
    
        to_be_acked=last_ack_byte;
        if(received_byte_count>last_ack_byte){ // new bytes received here
          // search for the max. acknowledgable byte
          cout<<"new bytes received : ACKing"<<endl;
          for(int i=(last_ack_byte+1)%RECV_BUFFER_SIZE;i!=recv_buff_end+1;i=(i+1)%RECV_BUFFER_SIZE){
            if(byte_filled[i]==1){
              to_be_acked++;
            } 
            else
              break;
          }
          cout<<"before calling sendACK: "<<recv_buff_st<<" "<<recv_buff_end<<"to_be_acked: "<<to_be_acked<<endl;
          sendACK(to_be_acked,socket_fd);
          cout<<"ack sent"<<endl;
          last_ack_byte=to_be_acked;
          }
    
        // sending the ACK of the last continuous byte received
        else{
          sendACK(last_ack_byte,socket_fd); 
        }
    
        first_packet=false;
      } 
    }
}



void appRecv(char *data,int socket_fd,int len){
  int local_recv_buff_st=recv_buff_st;
  if(first_execution){
    recv_data_buffer=(char*)malloc(RECV_BUFFER_SIZE*sizeof(char));
    if(recv_data_buffer==NULL){
      cout<<"unable to allocate space"<<endl;
      exit(0);
    }
    first_execution=false;

  }
  if(!buff_controller_running){
      pthread_create(&buffer_controller_threadid,NULL,&buffer_controller,&socket_fd);
      buff_controller_running=true;
  }

  int z=0;
  //********************************* BUSY WAIT
  int recv_buff_content=0;

  while(recv_buff_content<=0){
    //cout<<"recv_buff_content: "<<recv_buff_content<<endl;

    z=(last_ack_byte)%RECV_BUFFER_SIZE-local_recv_buff_st;
    if(z>0)
      cout<<"z: "<<z<<endl;
    recv_buff_content=((last_ack_byte)%RECV_BUFFER_SIZE>=local_recv_buff_st)?(z):(RECV_BUFFER_SIZE-z+1);
    if(last_ack_byte<0){
      recv_buff_content=0;
    }
  }
  cout<<"in appRecv recv_buff_content,local_recv_buff_st,last_ack_byte "<<recv_buff_content<<" "<<local_recv_buff_st<<" "<<last_ack_byte<<endl;
  bzero(data,BUFSIZE);
  for(int i=0;i<recv_buff_content;i++){
    if(last_ack_byte>appRecv_bytes_read){
      //cout<<"last_ack_byte, appRecv_bytes_read: "<<last_ack_byte<<" "<<appRecv_bytes_read<<endl;
      if(i>=len){
          cout<<"i= "<<i<<endl;
         break;
       }
      data[i]=recv_data_buffer[(local_recv_buff_st+i)%RECV_BUFFER_SIZE];
      appRecv_bytes_read++;
      
    } 
  }
  
//******************************************************************

      //increasing the start pointer
      for(int i=local_recv_buff_st;i!=(appRecv_bytes_read)%RECV_BUFFER_SIZE;i=(i+1)%RECV_BUFFER_SIZE){//recv_buff_st+recv_buff_content
        byte_filled[i]=0;
      }
      local_recv_buff_st=(appRecv_bytes_read)%RECV_BUFFER_SIZE;
      recv_buff_st=local_recv_buff_st;


//******************************************************************
  
  cout<<"in appRecv: "<<data<<endl;
  return;
}


int ftp_receive_handshake(int sockfd, char*filename, long int *filesize){
    int n_recv,n_send;
    char *hostaddrp; /* dotted decimal host addr string */
    char buf[BUFSIZE];
    bzero(buf, BUFSIZE);
    n_recv = recvfrom(sockfd, buf, BUFSIZE,0,(struct sockaddr*)&clientaddr,&clientlen);
    if (n_recv < 0) 
      error("ERROR reading from socket");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    sscanf(buf,"%s %ld",filename,filesize);  /*get file name and size from first message*/
    if(filesize<=0){
      cout<<"<< Connection Rejected >> \n";//<<"msg received: "<< buf <<endl;
      return -1;
    }
    printf("server received datagram from %s\n", hostaddrp);
    fflush(stdout);
    
    
    printf("Receiving file: %s of size:%ld\n",filename,*filesize);
   n_send = sendto(sockfd, buf, strlen(buf),0,(struct sockaddr *) &clientaddr, clientlen); 
    if (n_send < 0) 
    error("ERROR writing to socket");
    return 1;
}


int establish_server(int portno){
  int sockfd; /* socket file descriptor - an ID to uniquely identify a socket by the application program */
  struct sockaddr_in serveraddr; /* server's addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE],buf_2[BUFSIZE]; /* message buf */
  int optval; /* flag value for setsockopt */
  long int filesize=-1; /*file size*/
  char filename[200];//filename
  int crsize;
  /* 
   * check command line arguments 
   */
  
  /* 
   * socket: create the socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
       (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
     sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  return sockfd;
}

int ftp_receive_md5_checksum(int sockfd,char * filename){
  int n_send;
  char buf[BUFSIZE],buf_2[BUFSIZE];
  char md5_command[500];
    char md5_file[200];
    sprintf(md5_command,"md5sum %s > MD5_%s",filename,filename);
    fflush(stdout);
    sprintf(md5_file,"MD5_%s",filename);
    system(md5_command);
    FILE * checksum=fopen(md5_file,"r");
    bzero(buf,BUFSIZE);
    bzero(buf_2,BUFSIZE);

    fscanf(checksum,"%s",buf);
    sprintf(buf_2,"CHECKSUM:%s",buf);
    cout<<buf_2<<endl;
    n_send = sendto(sockfd, buf_2, strlen(buf_2),0,(struct sockaddr *) &clientaddr, clientlen);
      if (n_send < 0) 
      error("ERROR writing to socket");

    fclose(checksum);
    sprintf(md5_command,"rm MD5_%s",filename);
    system(md5_command);           //deleting the created file of md5 sum

    return 1;
}

