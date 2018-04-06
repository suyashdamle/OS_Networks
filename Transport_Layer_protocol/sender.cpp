# include "protocol_stack.h"

int file_scanner(FILE* input,int size, char* buffer){
    char c;
    bzero(buffer,BUFSIZE);
    if((c=getc(input))==EOF)
        return 0;
    int index=0;
    size-=1;
    buffer[index++]=c;
    while(size--){
        c=getc(input);
        if(c==EOF){
            break;
        }
        buffer[index++]=c;
    }
    if(c!=EOF){   
    }
    buffer[index]='\0';
    //printf("buffer here: %s\n",buffer);
    return index;
}

int main(int argc, char **argv) {
    
    
    //char **data_buffer=(char**)malloc(MAX_WINDOW_SIZE*sizeof(char*));
    /*for(int i=0;i<MAX_WINDOW_SIZE;i++)
        data_buffer[i]=(char*)malloc(BUFSIZE*sizeof(char));
    */ if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    char * hostname = argv[1];
    int portno = atoi(argv[2]);


    deque <pair <int,int> > ack_buffer;
        char buf[BUFSIZE],buf_2[BUFSIZE];

    
    
    
    int err=establish_connection(hostname,portno);
    if(err<0){
        cout<<"error in establishing connection"<<endl;
        exit(0);
    }
    // creating the timeout for the time-out of the receive_from function
    

// -------------------------------------------------------------------------------------------------------------------------
     printf("Please enter complete file location: ");
    bzero(buf, BUFSIZE);
    char  file_loc[200];
    scanf(" %s",file_loc);             // assuming file would be in the original folder for now
    //fgets(buf, BUFSIZE, stdin);

    FILE *file_to_send=fopen(file_loc,"rb");
    if(file_to_send==NULL)
        printf("NULL file pointer\n");
    struct stat st; 
    long int file_size;
    if (stat(file_loc, &st) == 0)
        file_size= st.st_size;

    printf("file size being sent = %ld B\n",file_size);
    fflush(stdout);
    err=0;
    err=ftp_send_handshake(file_loc,file_size);
    if(err!=1){
        exit(-1);
    }

    //sending the file name and size agian and again as long as an ACK is not received
    int n_send,n_recv=-1;
    int err_count=0;

   

    bzero(buf, BUFSIZE);


    // sending the original file in chunks, affixed with a packet count

    int packet_count=(int)ceil((double)file_size/1013);
    int present_count=1;
    printf("NUMBER of packets: %d\n",packet_count);
    fflush(stdout);
    int new_present_count;
    
    // initializing the 2 pointers to 0
    // read_size keeps track of the number of characters read in each file-read operation
    int read_size=1;    
    //int buf_start,buf_end;
    read_size=file_scanner(file_to_send,1013,buf_2); 
    cout<<"this read size: "<<read_size<<endl;
    while(read_size>0){
        appSend(sender_socket_fd,buf_2);
        read_size=file_scanner(file_to_send,1013,buf_2); 
        cout<<"this read size: "<<read_size<<endl<<flush;
    }


    err=ftp_send_md5_checksum(file_loc);
    if(err<0){
        cout<<"NOT MATCH"<<endl;
    }

    return 0;
   

}
