#include<bits/stdc++.h>
#include <unistd.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>

using namespace std;


#define PACKETSIZE	64
#define MAX_NO_PINGS 100

struct packet
{
	char ip_header[20];
	char icmp_header[12];
	char msg[PACKETSIZE-(32)];
};
typedef struct icmpheader
{
  u_int8_t type;		/* message type */
  u_int8_t code;		/* type sub-code */
  unsigned short checksum;
  unsigned short id;
  unsigned short sequence;
}icmpheader;

//GLOBAL VARS

int x;
int pid=-1;
struct protoent *proto=NULL;
bool show_packet=true;

long int time_sent[MAX_NO_PINGS];
float all_rtt[MAX_NO_PINGS];

unsigned short checksum(void *b, int len)
{	unsigned short *buf = (unsigned short *)b;
	unsigned int sum=0;
	unsigned short result;

	for ( sum = 0; len > 1; len -= 2 )
		sum += *buf++;
	if ( len == 1 )
		sum += *(unsigned char*)buf;
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	result = ~sum;
	return result;
}


void display(void *buf, int bytes){
	if(!show_packet)
		return;

	int i;

	// simply casting the buffer into the struct forms make it easy to access data

	struct iphdr *ip = (struct iphdr *)buf;
	//struct icmpheader *icmp =(struct icmpheader *) buf+ip->ihl*4;

	printf("\n-------------------------------------");
	for ( i = 0; i < bytes; i++ )
	{
		if ( !(i & 15) ) printf("\n%d:  ", i);
		printf("%02x ", ((unsigned char*)buf)[i]);
	}
	printf("\n");
	printf("IPv%d: hdr-size=%d pkt-size=%d protocol=%d TTL=%d src=%s ",
		ip->version, ip->ihl*4, ntohs(ip->tot_len), ip->protocol,
		ip->ttl, inet_ntoa(*(in_addr*)&ip->saddr));
	printf("dst=%s\n", inet_ntoa(*(in_addr*)&ip->daddr));

	printf("========================================\n");
	// collecting ICMP data:
	struct icmpheader icmp;
	int icmp_hdr_idx=20;
	memcpy(&icmp.type,(char*)buf+icmp_hdr_idx,1);
	icmp_hdr_idx+=1;

	memcpy(&icmp.code,(char*)buf+icmp_hdr_idx,1);
	icmp_hdr_idx+=1;

	memcpy(&icmp.checksum,(char*)buf+icmp_hdr_idx,2);
	icmp_hdr_idx+=2;

	memcpy(&icmp.id,(char*)buf+icmp_hdr_idx,2);
	icmp_hdr_idx+=2;
	icmp.id=ntohs(icmp.id);	

	memcpy(&icmp.sequence,(char*)buf+icmp_hdr_idx,2);
	icmp_hdr_idx+=2;
	icmp.sequence=ntohs(icmp.sequence);	

	if ( icmp.id == getpid())
	{
		printf("ICMP: type[%d/%d] checksum[%d] id[%d] seq[%d]\n",
			icmp.type, icmp.code, ntohs(icmp.checksum),
			icmp.id, icmp.sequence);
	}
}

void sig_handler(int signo){
	/*if(x!=0){
		kill(getpid(),SIGKILL);
		return;
	}*/
	// displaying stats:
	cout<<"\n\n---------------- STATS --------------------------"<<endl;
	float avg=0,max=FLT_MIN,min=FLT_MAX;
	int count=0;
	for(int i=0;i<MAX_NO_PINGS;i++){
		if(all_rtt[i]==-1){
			continue;
		}
		avg+=all_rtt[i];
		if(max<all_rtt[i])
			max=all_rtt[i];
		if(min>all_rtt[i])
			min=all_rtt[i];
		count++;
	}
	if(count>0)
		avg=avg/count;
	cout<<"\tAverage RTT: "<<avg<<" ms"<<endl;
	cout<<"\tMax RTT: "<<max<<" ms"<<endl;
	cout<<"\tMin RTT: "<<min<<" ms"	<<endl;
	cout<<"----------------------------------------------------"<<endl;

	//kill(listener_thread,SIGKILL);//x
	exit(0);
}



void listener(){
	int sd;
	struct sockaddr_in addr;
	unsigned char buf[1024];
	sd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if ( sd < 0 )
	{
		perror("socket");
		exit(0);
	}
	bool received=false;
	while(!received){
		int bytes, len=sizeof(addr);

		bzero(buf, sizeof(buf));
		bytes = recvfrom(sd, buf, sizeof(buf), 0, (struct sockaddr*)&addr,(socklen_t *) &len);
		if ( bytes > 0 ){
			// received something
			// simply casting the buffer into the struct forms make it easy to access data
			unsigned short count=0;unsigned short id=0;
			memcpy(&id,buf+20+4,2);
			if(ntohs(id)!=getpid()){
				continue;
			}
			memcpy(&count,buf+20+6,2);
			count=ntohs(count);
			long int time1=0;
			timeval tv;
			gettimeofday(&tv,NULL);
			long int time2=tv.tv_usec;
			memcpy(&time1,buf+20+8,4);
			long int time_1_org=time_sent[count];
			if(time_1_org!=time1)
				cout<<"Timestamp data NOT FOUND in packet"<<endl;
			float time_taken=(float)(time2-time1);
			cout<<"\n"<<bytes<<" Bytes of data received from "<<inet_ntoa(addr.sin_addr)<<"\t seq no: "<<count<<"\t RTT= "<<time_taken/1000<<" ms"<<endl;
			all_rtt[count]=time_taken/1000;
			if(show_packet)
				display(buf, bytes);
			received=true;
			
		}
		else
			perror("recvfrom");
	}
}


// this function creates the right ping message and sends it - with the IP and ICMP header manually created  
void ping_and_receive(struct sockaddr_in *addr){

	int i, sd, cnt=1;
	int hdrincl=1;									// FOR DISABLING IP HEADER INCLUSION
	struct sockaddr_in r_addr;

	sd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if ( sd < 0 )
	{
		perror("socket");
		return;
	}


	if ( setsockopt(sd, IPPROTO_IP,IP_HDRINCL, &hdrincl, sizeof(hdrincl)) != 0) 		// FOR DISABLING AUTOMATIC IP HEADER INCLUSION
		perror("Unable to Set HDRINCL option");

	/*if ( fcntl(sd, F_SETFL, O_NONBLOCK) != 0 )
		perror("Unable to Request nonblocking I/O");*/


	// IP header constants
	int version=4;				// IPv4 first 4 bits
	int header_len=20/4;		// header length next - number of 32 bit words

	int dscp=0;					// Differentiated Services Code Field - default =0 - only BEST EFFORT SERVICE REQUESTED
	int ecn=0;					// External Congestion Notification - not requesting end-to-end congestion notification - allowing packet drop
	unsigned short int total_length=(20);// initializing at 20 - GETS UPDATED AUTOMATICALLY
	
	unsigned short identification=(23512);//randomly started - 2 bytes long
	int flag_val=2;						// 3 bits (010) - Reserved bit=0; dont fragment=1; more fragments=0
	int frag_offset=0;					// no fragment offset - each packet is independent

	int ttl=255;						// unrestricted by time-out
	int proto_code=1;					// for ICMP
	unsigned short int hdr_chksum=0;	// to be calculated for each packet

	unsigned long destination=addr->sin_addr.s_addr;

	unsigned long source=inet_addr("0.0.0.0");


	// data from ICMP header

	int icmp_type=ICMP_ECHO;  						//=8
	int icmp_code=0;								// for subtype
	unsigned short icmp_chk=0;						
	unsigned short int icmp_identifier=pid;			// could be used to distinguish between ping requests of different messages
	unsigned short int icmp_seq_no=cnt;



	
	int number_of_pings=MAX_NO_PINGS;
	while((number_of_pings--)>0){

		struct packet pckt;
		int len=sizeof(r_addr);
		bzero(&pckt, sizeof(pckt));



		//***********************************************************************************************************//
		//**************** creating and attaching IP header to send in the ping

		int ip_header_idx=0;	//the byte index upto which data has been written
		int chksum_idx=10;		// the byte index of the chk_sum field

		int version_ihl=(version<<4)+header_len;
		memcpy(pckt.ip_header+ip_header_idx,&version_ihl,1);
		ip_header_idx+=1;

		int dscp_ecn=(dscp<<2)+ecn;
		memcpy(pckt.ip_header+ip_header_idx,&dscp_ecn,1);
		ip_header_idx+=1;

		int total_length_=htons(total_length);						// converting to network byte order
		memcpy(pckt.ip_header+ip_header_idx,&total_length_,2);
		ip_header_idx+=2;

		int id=htons(identification);								// converting to network byte order
		memcpy(pckt.ip_header+ip_header_idx,&id,2);
		ip_header_idx+=2;
		identification+=1;

		int flag_frag=htons((flag_val<<13)+frag_offset);			// converting to network byte order
		memcpy(pckt.ip_header+ip_header_idx,&flag_frag,2);
		ip_header_idx+=2;		

		memcpy(pckt.ip_header+ip_header_idx,&ttl,1);
		ip_header_idx+=1;		

		memcpy(pckt.ip_header+ip_header_idx,&proto_code,1);
		ip_header_idx+=1;


		int hdr_chksum_=htons(hdr_chksum);							// converting to network byte order
		memcpy(pckt.ip_header+ip_header_idx,&hdr_chksum_,2);
		ip_header_idx+=2;		

		memcpy(pckt.ip_header+ip_header_idx,&source,4);
		ip_header_idx+=4;		

		memcpy(pckt.ip_header+ip_header_idx,&destination,4);
		ip_header_idx+=4;
		
		//***********************************************************************************************************//
	

		//***********************************************************************************************************//
		// *********************** Filling and attaching the ICMP packet header

		int icmp_hdr_idx=0;
		memcpy(pckt.icmp_header+icmp_hdr_idx,&icmp_type,1);
		icmp_hdr_idx+=1;

		memcpy(pckt.icmp_header+icmp_hdr_idx,&icmp_code,1);
		icmp_hdr_idx+=1;

		int icmp_chk_idx=icmp_hdr_idx;
		//deferring the filling of checksum
		icmp_hdr_idx+=2;

		int icmp_identifier_=htons(icmp_identifier);
		memcpy(pckt.icmp_header+icmp_hdr_idx,&icmp_identifier_,2);
		icmp_hdr_idx+=2;

		icmp_seq_no=cnt++;
		int icmp_seq_no_=htons(icmp_seq_no);
		memcpy(pckt.icmp_header+icmp_hdr_idx,&icmp_seq_no_,2);
		icmp_hdr_idx+=2;

		timeval tv;
		gettimeofday(&tv,NULL);
		long int time_now=tv.tv_usec;

		memcpy(pckt.icmp_header+icmp_hdr_idx,&time_now,4);		// time of origin

		//creating some messsage
		for ( i = 0; i < sizeof(pckt.msg)-1; i++ )
			pckt.msg[i] = i+'0';
		pckt.msg[i] = 0;




		icmp_chk=checksum(pckt.icmp_header, sizeof(pckt)-20);
		//unsigned short int icmp_chk_=htons(icmp_chk);
		memcpy(pckt.icmp_header+icmp_chk_idx,&icmp_chk,2);


		if ( sendto(sd, &pckt, sizeof(pckt), 0, (struct sockaddr*)addr, sizeof(*addr)) <= 0 )
			perror("Unable to send");
		time_sent[cnt-1]=time_now;
		listener();
		sleep(1);
		//exit(0);
	}
}

int main(int count, char *strings[]){
	signal(SIGINT,sig_handler);

	struct hostent *hname;
	struct sockaddr_in addr;
	if ( count != 2 )
	{
		printf("usage: %s <addr>\n", strings[0]);
		exit(0);
	}

	for(int i=0;i<MAX_NO_PINGS;i++){
		all_rtt[i]=-1;
	}
	pid = getpid();
	proto = getprotobyname("ICMP");
	hname = gethostbyname(strings[1]);
	bzero(&addr, sizeof(addr));
	addr.sin_family = hname->h_addrtype;
	addr.sin_port = 0;
	addr.sin_addr.s_addr = *(long*)hname->h_addr;
	ping_and_receive(&addr);
	wait(0);

	return 0;
}

