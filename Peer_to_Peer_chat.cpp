/* 
 * tcpserver.c - A simple TCP echo server 
 * usage: tcpserver <port>

  SERVERS TO CHECK THE CODE: 10.5.18.74  AND  10.5.18.101
  
  **** CODE Runs on  server port# 8111

 http://code.activestate.com/recipes/578591-primitive-peer-to-peer-chat/

 */

#include <bits/stdc++.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

using namespace std;

//GLOBAL VARIABLES
int n_peers;
int *client_fds;

#define BUFSIZE 1024
#define PEER_TIMEOUT_DURATION 120       // in seconds


#define KMAG  "\x1B[1;33m"
#define CYAN "\x1B[1;36m"
#define RESET "\x1B[0m"



#if 0
/* 
 * Structs exported from in.h
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};


typedef struct fd_set {
  u_int  fd_count;
  SOCKET fd_array[FD_SETSIZE];
} fd_set;



/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  int     h_addrtype;     /* host address type */
  int     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses */
}
#endif

/*
 * error - wrapper for perror
 */
void error(const char *msg) {
  perror(msg);
  exit(1);
}

int get_max_fd(vector<int> all_fds){
  int max_val=INT_MIN;
  for (vector<int>::iterator itr=all_fds.begin();itr!=all_fds.end();itr++){
    if(*itr>max_val){
      max_val=*itr;
    }
  }
  return max_val;
}

// overloaded functions for searching the index of the peer based on 'to_search' parameter
int search_peer(int n_peers,char **peer_details,char *to_search){
  for (int i=0;i<n_peers;i++){
    if(strcmp(peer_details[i],to_search)==0)
      return i;
  }
  return -1;
}


void sig_handler(int signo){
  for(int i=0;i<n_peers;i++){
    close(client_fds[i]);
  }
  exit(0);
}


int main(int argc, char **argv) {
  int server_fd; /* parent socket */
  int portno; /* port to listen on */
  socklen_t clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buffer */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */
	long int filesize; /*file size*/
	char filename[200];//filename
  char  str[1000];
	FILE *filept; /*file pointer to record input*/  
  /*
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }*/
  //portno = atoi(argv[1]);
  portno=8111;

  /* 
   * socket: create the parent socket 
   */

  // first of all, creating the TCP SERVER 

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) 
    error("ERROR opening socket");

  // ensuring socket reuse for ease of debugging
  optval = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  //creating the datastruct for server
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  // binding to a well-known port
  if (bind(server_fd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  //putting it in LISTEN MODE

  // ********************* check this *************************
  
  printf("P2P Server Running at port: %d\n",portno);
  if (listen(server_fd, 5) < 0) /* allowing max of 5 peer requests */ 
    error("ERROR on listen");
  

  //accepting the user_info data structure and storing it.
  cout<<"Enter the details of the peers and respective IP addresses: "<<endl;
  cout<<"number of peers: ";
  int n_peers;
  cin>>n_peers;
  client_fds=(int*)malloc(n_peers*sizeof(int));
  float last_interaction[n_peers];             // the time of last interaction - used to set time-out
  struct sockaddr_in peeraddr[n_peers];
  char **peer_ips=(char**)malloc(n_peers*sizeof(char*));
  char **peer_names=(char**)malloc(n_peers*sizeof(char*));
  for(int j=0;j<n_peers;j++){
    peer_ips[j]=(char*)malloc(30*sizeof(char));
    peer_names[j]=(char*)malloc(30*sizeof(char));
  }
  unsigned short peer_ports[n_peers],x;
  // default server port
  x=8111;                        
  
  
  for(int i=0;i<n_peers;i++){
    client_fds[i]=last_interaction[i]=-1;
    char name[20],ip[30];
    cout<<"name of peer "<<i<<" : ";
    scanf(" %s",name);
    strcpy(peer_names[i],name);
    cout<<"ip of peer "<<i<<" : ";
    scanf(" %s",ip);
    strcpy(peer_ips[i],ip);
    //cout<<"port# of peer "<<i<<" : ";
    //scanf("%hu",&x);
    peer_ports[i]=htons(x);
  }

  signal(SIGINT,sig_handler);
  cout<<KMAG<<"Chat session set up complete..."<<RESET<<endl;

  //initializing the select() and adding the known file descriptors-stdin and server_fd to select()
  fd_set readfds;
  fd_set writefds;
  fd_set exceptfds;
  struct timeval timeout;
  int maxfd, result=-1;
  vector<int> all_fds;
  vector<int> input_fds;
  //vector<int> output_fds;
  input_fds.push_back(STDIN_FILENO);
  input_fds.push_back(server_fd);
  all_fds.push_back(STDIN_FILENO);
  all_fds.push_back(server_fd);

  clock_t session_begin_time=clock();       // setting reference time for all future clock-based operations
  while(1){                                 // running in loop till user exits
    result=-1;

    while(result==-1){
      // checking last usage times of each of the peers, to TIME OUT...
      //... those that have not interacted for more than time out duaration
      for(int j=0;j<n_peers;j++){
        float time_gap=(float)(clock()-last_interaction[j])/CLOCKS_PER_SEC;
        if(time_gap>=PEER_TIMEOUT_DURATION){
          last_interaction[j]=-1;
          client_fds[j]=-1;
        }
      }
      FD_ZERO(&readfds);
      FD_ZERO(&writefds);
      // at the time of initialization, there are no connections. The select should just keep track of stdin and server_fd...
      for(vector<int>::iterator it=input_fds.begin();it!=input_fds.end();it++){
        FD_SET(*it,&readfds);
      }
      timeout.tv_sec=120;                     // setting at 2 min for initial experimentation
      result=select(get_max_fd(all_fds)+1,&readfds,&writefds,&exceptfds,&timeout);
    }

    //got some connection request
    if(FD_ISSET(server_fd,&readfds)){        // if it is the server_fd
      //cout<<"reached in server_fd"<<endl;
      int childfd; /* child socket */
      struct sockaddr_in clientaddr; /* client addr */
      clientlen = sizeof(clientaddr);

      childfd = accept(server_fd, (struct sockaddr *) &clientaddr, &clientlen);
      hostaddrp = inet_ntoa(clientaddr.sin_addr);
      if (hostaddrp == NULL)
        error("ERROR on inet_ntoa\n");
      int client_idx= search_peer(n_peers,peer_ips,hostaddrp);//ntohs
      if(client_idx==-1){
        cout<<"Request received from unknown host"<<endl;
      }
      else{
        client_fds[client_idx]=childfd;
        last_interaction[client_idx]=(float)(clock()-session_begin_time)/CLOCKS_PER_SEC;
      }
      input_fds.push_back(childfd);
      all_fds.push_back(childfd);
    }


    if(FD_ISSET(STDIN_FILENO,&readfds)){
        //cout<<"reached in readfd"<<endl;
        // reading the message and separating the peer_name and message parts 
        bzero(str,1000);
        read(STDIN_FILENO,str,1000);
        char *friend_name=(char*)malloc(100*sizeof(char));
        char *msg=(char*)malloc(900*sizeof(char));
        friend_name=strtok(str,"/");
        msg=strtok(NULL,"/");
        int peer_idx=search_peer(n_peers,peer_names,friend_name);
        if(peer_idx==-1){
          cout<<"No such peer found in the list: "<<friend_name<<endl;
        }

        if(last_interaction[peer_idx]==-1){          // that is, the session with this peer has either timed-out or has not been established yet
          int newfd=socket(AF_INET, SOCK_STREAM, 0);
          if (newfd < 0) 
            error("ERROR opening socket");
          
          struct hostent *server=gethostbyname(peer_ips[peer_idx]);
          // build the peer's Internet address 
          bzero((char *) &peeraddr[peer_idx], sizeof(peeraddr[peer_idx]));
          peeraddr[peer_idx].sin_family = AF_INET;
          bcopy((char *)server->h_addr, 
          (char *)&peeraddr[peer_idx].sin_addr.s_addr, server->h_length);
          peeraddr[peer_idx].sin_port = (peer_ports[peer_idx]);//htons

          //connect: create a connection with the peer
          if (connect(newfd, (struct sockaddr*)&peeraddr[peer_idx], sizeof(peeraddr[peer_idx])) < 0) 
            error("ERROR connecting peer");

          client_fds[peer_idx]=newfd;
          input_fds.push_back(newfd);
          //output_fds.push_back(newfd);
          all_fds.push_back(newfd);
        }

        // the connection with this peer now exists
        // sending the message..........

        n = write(client_fds[peer_idx], msg, strlen(msg));
        if (n < 0) 
          error("ERROR sending message to peer");
        bzero(msg,strlen(msg));
        last_interaction[peer_idx]=(float)(clock()-session_begin_time)/CLOCKS_PER_SEC;
    }

    // if the select has returned the fd of a peer sending a message
    for(int j=0;j<n_peers;j++){
      if(FD_ISSET(client_fds[j],&readfds)){
        bzero(buf,BUFSIZE);
        n = read(client_fds[j], buf, BUFSIZE);
        last_interaction[j]=(float)(clock()-session_begin_time)/CLOCKS_PER_SEC;
        //printf("remanining: %d\n",packet_count);
        if (n < 0) 
          error("ERROR reading from peer");
        if(n==0)
          close(client_fds[j]);
        else{
          // printing the message...
          cout<<CYAN<<peer_names[j]<<" : "<<RESET<<buf<<endl;
        }

      }
    }

  }
}



