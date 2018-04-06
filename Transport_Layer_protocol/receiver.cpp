#include "protocol_stack.h"


int main(int argc, char **argv) {
  int sockfd; /* socket file descriptor - an ID to uniquely identify a socket by the application program */
  int portno; /* port to listen on */
  struct sockaddr_in serveraddr; /* server's addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE],buf_2[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n_send,n_recv; /* message byte size */
  long int filesize=-1; /*file size*/
  char filename[200];//filename
  int crsize;
  FILE *filept; /*file pointer to record input*/
  /* 
   * check command line arguments 
   */
  if (argc != 3) {
    fprintf(stderr, "usage: %s <port_for_server> <drop probability>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);
  drop_prob=atof(argv[2]);
  sockfd=establish_server(portno);

  int packet_count=0;
  while (1) {
      
      int err=ftp_receive_handshake(sockfd,filename,&filesize);
      if(err<0)
        continue;
      /*open file with given file name*/
      filept = fopen(filename, "wb");
      
      /* return same message*/
     
    
      while(filesize>1){
        bzero(buf,BUFSIZE);
        appRecv(buf,sockfd,1013);
        fprintf(filept,"%s",buf);
        filesize-=strlen(buf);
        cout<<"remaining : "<<filesize<<endl;
      }
      fclose(filept);

    
     
    /* creating and sending the md5 checksum through the API function*/
    err=ftp_receive_md5_checksum(sockfd,filename);  
    filesize=-1;
    exit(0);
  }
}
