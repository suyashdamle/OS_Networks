#include<bits/stdc++.h>
#include<unistd.h>
#include <mutex>

//use g++ myfs.h -std=gnu++11 to compile

using namespace std;

#define MAX_BLOCKS_PER_FILE (8+64+64*64)
#define BLOCK_SIZE 256         // in bytes
#define TIME_BYTES sizeof(time_t)

mutex block_list_lock;
mutex inode_list_lock;

bool debugging=false;
int SB_ST_IDX=0;            	// start idx of the super block
int SB_END_IDX=0;           	// LAST idx of super block
int SB_BLOCK_LIST_BEGIN=0;		// start idx(block) of the bitmap for empty blocks
int SB_BLOCK_LIST_END=0;		// LAST idx -----------------------------
int SB_INODE_LIST_BEGIN=0;		// start idx(block) for bitmap of free inodes
int SB_INODE_LIST_END=0;		// LAST idx -----------------------------
int IL_ST_IDX=0;				// start idx of the inode list
int IL_END_IDX=0;
int INODE_SIZE=0;				// size of individual idx node in bytes
int DATA_BLOCK_ST_IDX=0;
int BYTES_FILLED=0;
int NUMBER_BLOCKS=0; 			// tot number of blocks
int MAX_NUMBER_INODES=0; 		// max number of inodes
int **MRFS;
int **FILE_DESCRIPTOR;			// the table of file descriptors of open files

int CWD=0;						// the Current Working Directory

typedef struct _inode{
	bool is_directory;         // type 
	bool access_perms[3][3];
	time_t time_last_read;
	time_t time_last_modified;
	int file_size;
	int direct_block[8];
	int ind_block;
	int double_ind_block;
}inode;



// function protorypes

int  write_char_into_intArray(int *destination,int len,int start_byte_idx, char* source_string);
int write_bits_into_intArray(int *destination,int len,int start_bit_idx, vector<bool>* source_bits);
char * read_char_from_intArray(int *source,int no_byte,int start_byte_idx,int *end_idx=NULL);
vector<bool> read_bits_from_intArray(int *source,int no_bits,int start_bit_idx,int *end_idx=NULL);
int buffer_write(int** buffer,char* str, int* block_idx, int start_byte_idx);
int buffer_write(int** buffer,vector<bool> source_bits, int* block_idx, int start_byte_idx);
int create_file(const char name[30],bool is_directory,int inode_idx=-1);

inode get_inode(int idx);
void update_inode(inode node, int inode_idx);
int create_inode(inode node,int start_byte_idx,int mode=-1);

void get_free_block(int*block_idx){
	block_list_lock.lock();
	for(int i=DATA_BLOCK_ST_IDX;i<NUMBER_BLOCKS;i++){
		vector<bool> vec1,vec2;
		vec1=read_bits_from_intArray(MRFS[SB_BLOCK_LIST_BEGIN+i/NUMBER_BLOCKS],1,i%NUMBER_BLOCKS);
		if(vec1[0]==false){
			*block_idx=i;
			vec2.push_back(true);
			write_bits_into_intArray(MRFS[SB_BLOCK_LIST_BEGIN+i/NUMBER_BLOCKS],BLOCK_SIZE/sizeof(int),i%NUMBER_BLOCKS,&vec2);
			break;
		}
	}

	block_list_lock.unlock();

}


// ADDS one block idx to the file's inode and returns the INDEX of the block
int add_block_to_file(int file_inode){
	//cout<<"in ADD_BLOCK_TO_FILE:"<<endl;
	//cout<<"working on inode no: "<<file_inode<<endl;
	inode node=get_inode(file_inode);
	int size=node.file_size;
	int block_no=size/BLOCK_SIZE;   				// total number of blocks 
	int new_block_idx;
	//cout<<"block number: "<<block_no<<endl;
	get_free_block(&new_block_idx);
	if(block_no<8){                                // if it is already in the last block, new would NOT be here
		//cout<<"add_block_to_file: inode, size "<<file_inode<<" "<<size<<endl;
		node.direct_block[block_no]=new_block_idx;
		//block_idx=node.direct_block[block_no];
		if(size<0){
			node.direct_block[0]=new_block_idx;
			node.file_size=0;
		}
	}
	else{
		if(block_no==8){
			int new_ind_block;
			get_free_block(&new_ind_block);
			node.ind_block=new_ind_block;
		}
		if(block_no<8+64){// available in single indirect node
			//put an indirect block first
			// check this one later
			int loc_byte_no=block_no-8;
			//cout<<node.ind_block<<" "<<loc_byte_no<<endl<<flush;
			memcpy(&MRFS[node.ind_block][loc_byte_no],&new_block_idx,sizeof(int));
		}
		else if(block_no<8+64+64*64){              // go to double indirect node
			if(block_no==8+64){
				int new_ind_block;
				get_free_block(&new_ind_block);
				node.double_ind_block=new_ind_block;
			}
			block_no-=(8+64);
			int level_2_idx=block_no/(64);
			// putting a level_1 block here
			int level_1_block_idx;
			get_free_block(&level_1_block_idx);
			memcpy(&MRFS[node.double_ind_block][level_2_idx],&level_1_block_idx,sizeof(int));
			block_no=block_no%64;
			memcpy(&MRFS[level_1_block_idx][block_no],&new_block_idx,sizeof(int));
		}
		else{
			return -1;
		}

	}
	//cout<<"allotting block idx: "<<new_block_idx<<endl;
	update_inode(node,file_inode);
	return new_block_idx;
}


// returns the list of indices of ALL the blocks being used by the inode at present
vector<int> get_file_block_list(int inode_idx){
	vector<int> blocks_list;
	inode node=get_inode(inode_idx);
	int size=node.file_size;
	//int block_no=size/BLOCK_SIZE;   				// total number of blocks 
	int present_block=0; 										// number of blocks already discovered
	int present_block_copy=0;
	while(size>0){
		int block_idx;

		if(present_block<8){
			block_idx=node.direct_block[present_block];
		}
		else{
			if(present_block<8+64){// available in single indirect node
				// check this one later
				int loc_byte_no=present_block-8;
				memcpy(&block_idx,&MRFS[node.ind_block][loc_byte_no],sizeof(int));
			}
			else{              // go to double indirect node
				present_block-=(8+64);
				int level_2_idx=present_block/(64);
				int level_1_block_idx;
				memcpy(&level_1_block_idx,&MRFS[node.double_ind_block][level_2_idx],sizeof(int));
				present_block=present_block%64;
				memcpy(&block_idx,&MRFS[level_1_block_idx][present_block],sizeof(int));
			}

		}
		if(block_idx>0)
			blocks_list.push_back(block_idx);
		present_block_copy++;
		present_block=present_block_copy;
		size-=BLOCK_SIZE;
	}
	return blocks_list;
}	


void get_file_pointer(int inode_idx,int *block_idx,int *byte_idx){
	// trying to reach till the last block of this file
	inode node=get_inode(inode_idx);
	int size=node.file_size;
	if(size<0){   // no allotment yet
		//cout<<"calling add_block_to_file: 1"<<endl;
		*block_idx=add_block_to_file(inode_idx);
		*byte_idx=0;
		return;
	}
	int block_no=size/BLOCK_SIZE;
	*byte_idx=size%BLOCK_SIZE;                    // byte index determined and fixed
	//cout<<"BLOCK NUMBER , byte_idx: "<<block_no<<" , "<<*byte_idx<<endl;
	if(*byte_idx<BLOCK_SIZE-1 && *byte_idx!=0){					// could be returned as it is 
		vector<int> block_list=get_file_block_list(inode_idx);
		*block_idx=block_list[block_list.size()-1];
	}
	else{
		//cout<<"calling add_block_to_file: 2: size: "<<size<<endl;
		*byte_idx=0;
		*block_idx=add_block_to_file(inode_idx);
	}
	//cout<<"returning: "<<*block_idx<<" "<<*byte_idx<<endl<<flush;
	return;
}

void get_free_inode(int *inode_idx){
	inode_list_lock.lock();

	for(int i=0;i<MAX_NUMBER_INODES;i++){
		vector<bool> vec1,vec2;
		int block_idx=SB_INODE_LIST_BEGIN+i/MAX_NUMBER_INODES;
		int bit_idx=i%MAX_NUMBER_INODES;
		vec1=read_bits_from_intArray(MRFS[block_idx],1,bit_idx);
		if(vec1[0]==false){
			*inode_idx=i;
			vec2.push_back(true);
			write_bits_into_intArray(MRFS[block_idx],BLOCK_SIZE/sizeof(int),bit_idx,&vec2);
			break;
		}
	}	
	inode_list_lock.unlock();
}


int find_file_by_name(const char * source){
	bool match_found=false;
	char present_name[30];
	int present_inode;
	vector<int>block_list=get_file_block_list(CWD);
	for(int i=0;i<block_list.size();i++){
		int byte_idx=0;
		while(!match_found){
			present_inode=0;
			bzero(present_name,30);
			bzero(&present_inode,2);
			memcpy(present_name,(char*)(void*)&MRFS[block_list[i]][0]+byte_idx,30);
			byte_idx+=30;
			memcpy(&present_inode,(char*)(void*)&MRFS[block_list[i]][0]+byte_idx,2);
			byte_idx+=2;
			if(strcmp(present_name,source)==0)
				match_found=true;
			if(byte_idx==BLOCK_SIZE)
				break;
		}
		if(match_found)
			break;
	}
	if(match_found)
		return present_inode;
	else 
		return -1;
}



void update_inode(inode source,int idx){
	inode_list_lock.lock();
	int inodes_per_block=BLOCK_SIZE/INODE_SIZE;
	int inode_block=IL_ST_IDX+idx/inodes_per_block;
	int inode_byte_idx=((idx%inodes_per_block))*INODE_SIZE;
	int start_idx=create_inode(source,inode_byte_idx,inode_block);
	inode_list_lock.unlock();
}

inode get_inode(int idx){
	inode dest;
	int inodes_per_block=BLOCK_SIZE/INODE_SIZE;
	int inode_block=IL_ST_IDX+idx/inodes_per_block;
	int inode_byte_idx=((idx%inodes_per_block))*INODE_SIZE;

	int start_byte_idx=inode_byte_idx;
	vector<bool> vec1=read_bits_from_intArray(MRFS[inode_block],10,inode_byte_idx*8);
	dest.is_directory=vec1[0];
	for(int i=0;i<3;i++){
		for(int j=0;j<3;j++){
			dest.access_perms[i][j]=vec1[3*i+j];
		}
	}
	start_byte_idx+=2;

	//Writing times
	memcpy(&dest.time_last_read,&MRFS[inode_block][start_byte_idx],TIME_BYTES);
	start_byte_idx+=TIME_BYTES;
	memcpy(&dest.time_last_modified,&MRFS[inode_block][start_byte_idx],TIME_BYTES);
	start_byte_idx+=TIME_BYTES;

	// writing file size
	memcpy(&dest.file_size,&MRFS[inode_block][start_byte_idx],sizeof(int));
	start_byte_idx+=sizeof(int);

	//direct and indirect pointers
	for(int i=0;i<8;i++){
		memcpy(&dest.direct_block[i],&MRFS[inode_block][start_byte_idx],sizeof(int));
		start_byte_idx+=sizeof(int);
	}
	memcpy(&dest.ind_block,&MRFS[inode_block][start_byte_idx],sizeof(int));
	start_byte_idx+=sizeof(int);
	memcpy(&dest.double_ind_block,&MRFS[inode_block][start_byte_idx],sizeof(int));
	start_byte_idx+=sizeof(int);

	return dest;

}


// returns the idx of the byte after the last one written. mode=-1 for initialization phase. Else, pass block idx here
int create_inode(inode node,int start_byte_idx,int mode){
	int block_idx=IL_END_IDX;
	if(mode>=0){
		block_idx=mode;
	}
	//cout<<"inside create_inode: "<<start_byte_idx<<" "<<block_idx<<endl;
	if(start_byte_idx+INODE_SIZE>=BLOCK_SIZE)
		return -1;
	//writing type and access perms
	vector<bool> init_vec;
	init_vec.push_back(node.is_directory);
	for(int i=0;i<3;i++){
		for(int j=0;j<3;j++){
			init_vec.push_back(node.access_perms[i][j]);
		}
	}
	write_bits_into_intArray(MRFS[block_idx],BLOCK_SIZE/sizeof(int),start_byte_idx*8,&init_vec);
	start_byte_idx+=2;

	//Writing times
	memcpy(&MRFS[block_idx][start_byte_idx],&node.time_last_read,TIME_BYTES);
	start_byte_idx+=TIME_BYTES;
	memcpy(&MRFS[block_idx][start_byte_idx],&node.time_last_modified,TIME_BYTES);
	start_byte_idx+=TIME_BYTES;

	// writing file size
	memcpy(&MRFS[block_idx][start_byte_idx],&node.file_size,sizeof(int));
	start_byte_idx+=sizeof(int);

	//direct and indirect pointers
	for(int i=0;i<8;i++){
		memcpy(&MRFS[block_idx][start_byte_idx],&node.direct_block[i],sizeof(int));
		start_byte_idx+=sizeof(int);
	}
	memcpy(&MRFS[block_idx][start_byte_idx],&node.ind_block,sizeof(int));
	start_byte_idx+=sizeof(int);
	memcpy(&MRFS[block_idx][start_byte_idx],&node.double_ind_block,sizeof(int));
	start_byte_idx+=sizeof(int);

	return start_byte_idx;
}



int create_myfs(int size,int **MRFS2=NULL){
	NUMBER_BLOCKS=ceil((size*1024*1024)/BLOCK_SIZE);             // size in MB
	MAX_NUMBER_INODES=100;//ceil((float)NUMBER_BLOCKS/MAX_BLOCKS_PER_FILE);
	// creating space in heap for MRFS
	MRFS=(int**)malloc(NUMBER_BLOCKS*sizeof(int*));
	if(MRFS==NULL)
		return -1;
	for(int i=0;i<NUMBER_BLOCKS;i++){
		MRFS[i]=(int*)malloc(sizeof(int)*(BLOCK_SIZE/sizeof(int)));   // MRFS implemented as buffer of ints
		if(MRFS[i]==NULL)
			return -1;	
	}
	if(MRFS2!=NULL)
		MRFS=MRFS2;

	// FILE_DESCRIPTOR is used to keep track of open files:
	//		block idx 	at 	idx 0
	//		byte offset	at 	idx 1
	//		mode 		at 	idx 2
	FILE_DESCRIPTOR=(int**)malloc(MAX_NUMBER_INODES*sizeof(int*));
	for(int i=0;i<MAX_NUMBER_INODES;i++){
		FILE_DESCRIPTOR[i]=(int*)malloc(3*sizeof(int));
		FILE_DESCRIPTOR[i][0]=-1;
	}



	int write_idx=0;
	cout<<"time_bytes: "<<TIME_BYTES<<endl;
	cout<<"max no. of blocks: "<<NUMBER_BLOCKS<<endl;
	cout<<"max blocks per file: "<<MAX_BLOCKS_PER_FILE<<endl;
	cout<<"max number of inodes: "<<MAX_NUMBER_INODES<<endl;
	//creating the super block

	memcpy(&MRFS[SB_END_IDX][write_idx],&size,sizeof(int));
	write_idx+=sizeof(int);           			     				// size of MRFS.....................idx=0
	memcpy(&MRFS[SB_END_IDX][write_idx],&NUMBER_BLOCKS,sizeof(int));
	write_idx+=sizeof(int);										  	// max. number of  data blocks......idx=1
	memcpy(&MRFS[SB_END_IDX][write_idx],&NUMBER_BLOCKS,sizeof(int));
	write_idx+=sizeof(int);   										// number of free data blocks.......idx=2
	memcpy(&MRFS[SB_END_IDX][write_idx],&MAX_NUMBER_INODES,sizeof(int));
	write_idx+=sizeof(int);											// max. number inodes...............idx=3
	memcpy(&MRFS[SB_END_IDX][write_idx],&MAX_NUMBER_INODES,sizeof(int));
	write_idx+=sizeof(int);											// number of free inodes............idx=4

	//for convenience, the bit map would begin in the following block and the inode in the one next to it - even if they overflow, the...
	// ... remaining space in the block is left - out
	
	// the list of free blocks - 0 denotes free
	vector<bool> initialization_vec(NUMBER_BLOCKS,0);
	initialization_vec[0]=1;
	SB_END_IDX++;
	write_idx=0;
	SB_BLOCK_LIST_BEGIN=SB_END_IDX;cout<<"SB block list begin: "<<SB_BLOCK_LIST_BEGIN<<endl;
	SB_BLOCK_LIST_END=SB_END_IDX;
	write_idx=buffer_write(MRFS,initialization_vec,&SB_BLOCK_LIST_END,write_idx); 
	SB_END_IDX=SB_BLOCK_LIST_END;cout<<"SB block list end: "<<SB_BLOCK_LIST_END<<endl;

	// the list of free inodes
	SB_INODE_LIST_BEGIN=SB_END_IDX+1;cout<<"SB inode list begin: "<<SB_INODE_LIST_BEGIN<<endl;
	SB_INODE_LIST_END=SB_END_IDX+1;
	write_idx=0;
	write_idx=buffer_write(MRFS,initialization_vec,&SB_INODE_LIST_END,write_idx); 
	SB_END_IDX=SB_INODE_LIST_END;cout<<"SB inode list end: "<<SB_INODE_LIST_END<<endl;
	cout<<"SB end: "<<SB_END_IDX<<endl;

	// creating the INODE LIST
	IL_ST_IDX=SB_END_IDX+1;cout<<"inode  list begin: "<<IL_ST_IDX<<endl;
	INODE_SIZE=2+2*TIME_BYTES+4+40;                  // in bytes 
	cout<<"Inode size: "<<INODE_SIZE<<endl;
	int inodes_per_block=ceil((float)BLOCK_SIZE/(float)INODE_SIZE);


	inode initialization_inode;
	initialization_inode.is_directory=false;
	for(int i=0;i<3;i++)
		for(int j=0;j<2;j++)
			initialization_inode.access_perms[i][j]=false;
	initialization_inode.time_last_read=time(0);
	initialization_inode.time_last_modified=time(0);
	initialization_inode.file_size=-1;
	for(int i=0;i<8;i++)
		initialization_inode.direct_block[i]=-1;
	initialization_inode.ind_block=-1;
	initialization_inode.double_ind_block=-1;

	IL_END_IDX=IL_ST_IDX;
	int start_byte_idx=0;
	for(int i=0;i<MAX_NUMBER_INODES;i++){
		//cout<<"inode #"<<i<<" at: "<<IL_END_IDX<<" : "<<start_byte_idx<<endl<<flush;
		start_byte_idx=create_inode(initialization_inode,start_byte_idx);
		if(start_byte_idx<0){
			IL_END_IDX++;
			start_byte_idx=0;
			start_byte_idx=create_inode(initialization_inode,start_byte_idx);
			if(start_byte_idx<0){
				cout<<"ERROR: inode too big for one block"<<endl;
				exit(-1);
			}
		}
	}
	DATA_BLOCK_ST_IDX=IL_END_IDX+1;
	cout<<"inode list end: "<<IL_END_IDX<<endl;
	cout<<"DATA_BLOCK_ST_IDX: "<<DATA_BLOCK_ST_IDX<<endl;

	// setting up the root directory
	CWD=0;
	char str[30]="..";
	create_file(str,true,0);
	BYTES_FILLED+=(IL_END_IDX-SB_ST_IDX)*BLOCK_SIZE;

	return 1;
}


int buffer_write(int** buffer,char* str, int* block_idx, int start_byte_idx){
	if(*block_idx>=BLOCK_SIZE)
		(*block_idx)++;
	int remaining=strlen(str);
	int str_start=0;
	while(remaining){
		int block_rem_space=(BLOCK_SIZE-start_byte_idx);  // remaining space in this block
		char substr[min(block_rem_space,remaining)];
		memcpy(substr,&str[str_start],min(block_rem_space,remaining));
		start_byte_idx=write_char_into_intArray(buffer[*block_idx],BLOCK_SIZE/sizeof(int),start_byte_idx,substr);
		//start_byte_idx+=block_rem_space;
		if(remaining>block_rem_space){
			(*block_idx)++;
			remaining-=block_rem_space;
			start_byte_idx=0;	
		}
		else
			remaining=0;
		
	}
	return start_byte_idx;
}

int buffer_write(int** buffer,vector<bool> source_bits, int* block_idx, int start_bit_idx){
	if(*block_idx>=BLOCK_SIZE)
		(*block_idx)++;
	int start_bit=0;
	int remaining=source_bits.size();
	while(remaining){
		int block_rem_space=(BLOCK_SIZE*8-start_bit_idx);  // remaining space in this block in bits
		vector<bool> new_vector;
		for(int i=0;i<min(block_rem_space,remaining);i++){
			new_vector.push_back(source_bits[i]);
		}
		start_bit_idx=write_bits_into_intArray(buffer[*block_idx],BLOCK_SIZE/sizeof(int),start_bit_idx,&new_vector);
		//start_bit_idx+=block_rem_space;
		if(remaining>block_rem_space){
			(*block_idx)++;
			remaining-=block_rem_space;
			start_bit_idx=0;
		}
		else
			remaining=0;

	}
	return start_bit_idx;
}



// returns : a pair indicating the END idx - RIGHT AFTER THE LAST FILLED ONE. The first is the idx of int ...
// ... and the second one is the idx of BYTE in the int that is to be filled next. The byte till the previous...
// ...  one would be filled.  RETURN VALUE {-1,-1} implies that the int array was TOO SHORT for the char string
int  write_char_into_intArray(int *destination,int len, int start_byte_idx, char* source_string){
	pair <int,int> start_idx(start_byte_idx/sizeof(int),start_byte_idx%sizeof(int));
	pair <int,int> end_idx;
	int *destination_org=destination;

	// to start writing from here...
	int int_idx=start_idx.first;
	int byte_idx=start_idx.second;
	unsigned char *bytes = reinterpret_cast<unsigned char *>(&destination[int_idx]);

	for (int char_idx=0;char_idx<strlen(source_string);char_idx++){
		if(int_idx>=len){          // unable to write into the array
			// restoring the original int array
			destination=destination_org;
			// returning error indicator, using -ve values as end_idx
			end_idx.first=end_idx.second=-1;
			return end_idx.first*4+end_idx.second;;
		}
		if(byte_idx==0)
			bytes = reinterpret_cast<unsigned char *>(&destination[int_idx]);
		bytes[byte_idx]=source_string[char_idx];
		byte_idx=(byte_idx+1)%(sizeof(int));
		if(byte_idx==0){
			int_idx++;
		}
	}
	end_idx.first=int_idx;
	end_idx.second=byte_idx;
	return end_idx.first*4+end_idx.second;
}


// NOTE : here, the start_idx and the returned end_idx are in terms of bit_indices
// inserts bits from LEFT TO RIGHT 
int write_bits_into_intArray(int *destination,int len,int start_bit_idx, vector<bool>* source_bits){
	pair<int,int> start_idx(start_bit_idx/(8*sizeof(int)),start_bit_idx%(8*sizeof(int)));
	pair <int,int> end_idx;
	int *destination_org=destination;

	// to start writing from here...
	int int_idx=start_idx.first;
	int bit_idx=start_idx.second;
	for (int vect_idx=0;vect_idx<(*source_bits).size();vect_idx++){
		if(int_idx>=len){          // unable to write into the array
			// restoring the original int array
			destination=destination_org;
			// returning error indicator, using -ve values as end_idx
			end_idx.first=end_idx.second=-1;
			return end_idx.first*(8*sizeof(int))+end_idx.second;
		}
		int shifted_bits=(1<<(8*sizeof(int)-vect_idx-1-start_idx.second));//-bit_idx
		if((*source_bits)[vect_idx]==true){
			destination[int_idx]|=shifted_bits;
			//cout<<"BITS in write function first: "<<bitset<32>(destination[int_idx])<<endl;
		}
		else{
			destination[int_idx]&=(~shifted_bits);
			//cout<<"BITSET(false): "<<bitset<32>(~(1<<(8*sizeof(int)-vect_idx-1)))<<endl;
		}
		bit_idx=(bit_idx+1)%(8*sizeof(int));
		if(bit_idx==0){
			int_idx++;
		}
	}
	end_idx.first=int_idx;
	end_idx.second=bit_idx;
	//cout<<"exit here"<<endl<<flush;
	//cout<<"BITS in write function "<<bitset<32>(destination[int_idx])<<endl;
	return end_idx.first*(8*sizeof(int))+end_idx.second;
}


// returns the read string, TERMINATED WITH A '\0' CHARACTER. The end_idx (the one after the last one read) could be...
//... optionally obtained  using the last parameter -  passing a pointer to the end_idx pair
// char array returned is of size 1 MORE THAN the specified - to accomodate the trailing NULL CHARACTER

char * read_char_from_intArray(int *source,int no_byte,int start_byte_idx,int *end_idx){
	pair<int,int> start_idx(start_byte_idx/sizeof(int),start_byte_idx%sizeof(int));
	int int_idx=start_idx.first;
	int byte_idx=start_idx.second;
	char *destination=(char*)malloc((no_byte+1)*sizeof(char));
	unsigned char *bytes = reinterpret_cast<unsigned char *>(&source[int_idx]);	
	for(int i=0;i<no_byte;i++){
		if(byte_idx==0)
			bytes = reinterpret_cast<unsigned char *>(&source[int_idx]);
		destination[i]=bytes[byte_idx];
		byte_idx=(byte_idx+1)%(sizeof(int));
		if(byte_idx==0){
			int_idx++;
		}	
	}
	destination[no_byte]='\0';
	if(end_idx!=NULL){
		*end_idx=0;
		(*end_idx)+=int_idx*sizeof(int);
		(*end_idx)+=byte_idx;
	}
	return destination;
}

// returns bits from LEFT TO RIGHT
vector<bool> read_bits_from_intArray(int *source,int no_bits,int start_bit_idx,int* end_idx){
	pair<int,int> start_idx(start_bit_idx/(8*sizeof(int)),start_bit_idx%(8*sizeof(int)));
	int int_idx=start_idx.first;
	int bit_idx=start_idx.second;
	vector<bool> destination;

	bitset<32> bit_array(source[int_idx]);
	for(int i=0;i<no_bits;i++){
		if(bit_idx==0){
			bit_array=bitset<32>(source[int_idx]);
		}
		destination.push_back(bit_array[8*sizeof(int)-start_idx.second-1-i]);
		bit_idx+=1;
		bit_idx=(bit_idx)%(8*sizeof(int));
		if(bit_idx==0){
			int_idx++;
		}
	}
	if(end_idx!=NULL){
		*end_idx=0;
		(*end_idx)+=int_idx*(8*sizeof(int));
		(*end_idx)+=bit_idx;
	}
	return destination;

}	














// ---------------------- HIGHEST LEVEL FUNCTIONS -----------------------------//







// mode ='r' or 'w'
int open_myfs(const char *file_name,char mode){
	if(mode!='r' && mode!='w'){
		cout<<"ERROR incorrect mode"<<endl;
		return -1;
	}
	int file_inode_no=find_file_by_name(file_name);


	int block_idx,byte_idx;

	if(mode=='r'){
		if(file_inode_no==-1){
			file_inode_no=create_file(file_name,false);
		}
		inode file_inode=get_inode(file_inode_no);
		block_idx=file_inode.direct_block[0];
		byte_idx=0;
	}
	else{
		if(file_inode_no==-1){
			file_inode_no=create_file(file_name,false);
			block_idx=add_block_to_file(file_inode_no);
			byte_idx=0;
		}
		else
			get_file_pointer(file_inode_no,&block_idx, &byte_idx);
	}
	FILE_DESCRIPTOR[file_inode_no][0]=block_idx;
	FILE_DESCRIPTOR[file_inode_no][1]=byte_idx;
	FILE_DESCRIPTOR[file_inode_no][2]=(mode=='r')?0:1;
	return file_inode_no;
}

int close_myfs(int fd){
	int file_inode_no=fd;
	FILE_DESCRIPTOR[file_inode_no][0]=-1;
	FILE_DESCRIPTOR[file_inode_no][1]=-1;
	FILE_DESCRIPTOR[file_inode_no][2]=-1;
	return 1;
}

int read_myfs (int fd, int nbytes, char *buff){

	int file_inode_no=fd;
	if(FILE_DESCRIPTOR[file_inode_no][0]==-1 || FILE_DESCRIPTOR[file_inode_no][2]!=0){
		cout<<"file not open yet or not open for reading";
		return -1;
	}
	int last_block_idx,last_byte_idx;
	get_file_pointer(file_inode_no,&last_block_idx,&last_byte_idx);
	vector<int> file_blocks_list=get_file_block_list(file_inode_no);
	int present_block_idx=0;
	bzero(buff,nbytes+1);
	inode file_inode=get_inode(file_inode_no);
	char str[257];
	int bytes_to_read=1,bytes_read=0;
	int block_idx,byte_idx;
	while(bytes_read<nbytes && bytes_to_read>0){//
		block_idx=file_blocks_list[present_block_idx];
		byte_idx=FILE_DESCRIPTOR[file_inode_no][1];

		if((nbytes-bytes_read)+byte_idx<BLOCK_SIZE){
			if(block_idx!=last_block_idx)
				bytes_to_read=nbytes-bytes_read;
			else
				bytes_to_read=min(nbytes-bytes_read,last_byte_idx-byte_idx);
			FILE_DESCRIPTOR[file_inode_no][1]+=bytes_to_read;
		}
		else{
			if(block_idx!=last_block_idx){
				bytes_to_read=BLOCK_SIZE;
				FILE_DESCRIPTOR[file_inode_no][0]=file_blocks_list[++present_block_idx];
				FILE_DESCRIPTOR[file_inode_no][1]=0;
			}
			else{
				bytes_to_read=min(BLOCK_SIZE,last_byte_idx-byte_idx);
				if(bytes_to_read==BLOCK_SIZE){
					FILE_DESCRIPTOR[file_inode_no][0]=file_blocks_list[++present_block_idx];
					FILE_DESCRIPTOR[file_inode_no][1]=0;
				}
				else
					FILE_DESCRIPTOR[file_inode_no][1]+=bytes_to_read;
			}
		}
		bzero(str,257);
		memcpy(buff+bytes_read,(char*)(void*)(&MRFS[block_idx][0])+byte_idx,bytes_to_read);
		int n;

		//cout<<"reading: "<<bytes_to_read<<" , "<<block_idx<<" "<<byte_idx<<endl<<flush;
		bytes_read+=bytes_to_read;
		//strcat(buff,str);
	}

	buff[nbytes]='\0';
	return bytes_read;
}

int write_myfs(int fd,int nbytes, char *buff){
	int file_inode_no=fd;
	if(FILE_DESCRIPTOR[file_inode_no][2]!=1 || FILE_DESCRIPTOR[file_inode_no][0]==-1){
		cout<<"not open for writing or not open at all"<<endl;
		return -1;
	}
	int block_idx,byte_idx;
	get_file_pointer(file_inode_no,&block_idx,&byte_idx);
	int bytes_written=0;
	int bytes_to_write=0;
	//cout<<"nbytes here: "<<nbytes<<endl;
	char str[257];
	int bytes_remaining=nbytes;
	while(bytes_written<nbytes){
		bzero(str,257);
		bytes_to_write=min(bytes_remaining,BLOCK_SIZE-byte_idx);
		inode file_inode=get_inode(file_inode_no);
		//cout<<"bytes_to_write, block_idx "<<bytes_to_write<<" "<<block_idx<<endl;


		for(int i=0;i<bytes_to_write;i++){
			str[i]=buff[bytes_written+i];
		}
		memcpy((char*)(void*)&MRFS[block_idx][0]+byte_idx,str,bytes_to_write);
		//memcpy(str,(char*)(void*)&MRFS[block_idx][0]+byte_idx,bytes_to_write);
		bytes_written+=bytes_to_write;
		byte_idx+=bytes_to_write;
		bytes_remaining=nbytes-bytes_written;
		file_inode.file_size+=bytes_to_write;
		update_inode(file_inode,file_inode_no);
		if(byte_idx==BLOCK_SIZE){
			block_idx=add_block_to_file(file_inode_no);

			byte_idx=0;
			file_inode=get_inode(file_inode_no);
		}

	}
	BYTES_FILLED+=(bytes_written);
	return bytes_written;
}

int ls_myfs(){
	cout<<"LISTING FILES HERE "<<endl;
	cout<<"-------------------------------------------------------------------------------"<<endl;
	char present_name[30];
	int present_inode;
	vector<int>block_list=get_file_block_list(CWD);
	inode cwd_inode=get_inode(CWD);
	for(int i=0;i<block_list.size();i++){
		int byte_idx=0;
		while(byte_idx<BLOCK_SIZE && i*BLOCK_SIZE+byte_idx<cwd_inode.file_size){
			present_inode=0;
			bzero(present_name,30);
			bzero(&present_inode,2);
			memcpy(present_name,(char*)(void*)&MRFS[block_list[i]][0]+byte_idx,30);
			byte_idx+=30;
			memcpy(&present_inode,(char*)(void*)&MRFS[block_list[i]][0]+byte_idx,2);
			byte_idx+=2;
			if(present_inode<0 || present_inode==65535)
				continue;
			cout<<present_name<<"\t\t: type: ";
			inode file_inode=get_inode(present_inode);
			if(file_inode.is_directory)
				cout<<"directory ;\t";
			else
				cout<<"file ;\t";
			cout<<file_inode.file_size<<" B;\tlast modified: ";
			cout<<ctime(&file_inode.time_last_modified);
			
		}
	}
	cout<<"-------------------------------------------------------------------------------"<<endl;
}

// creates a file in the CWD and returns the inode idx of the created file
int create_file(const char name[30],bool is_directory,int inode_idx){
	inode initialization_inode;
	if(inode_idx<0){
		// getting a free inode for this file
		get_free_inode(&inode_idx);

		//updating the node in the list
		
		for(int i=0;i<3;i++)
			for(int j=0;j<2;j++)
				initialization_inode.access_perms[i][j]=true;
		initialization_inode.time_last_read=time(0);
		initialization_inode.time_last_modified=time(0);
		initialization_inode.file_size=-1;
		for(int i=0;i<8;i++)
			initialization_inode.direct_block[i]=-1;
		initialization_inode.ind_block=-1;
		initialization_inode.double_ind_block=-1;

	}
	else{
		initialization_inode=get_inode(inode_idx);
	}
	if(is_directory)
		initialization_inode.is_directory=true;
	else
		initialization_inode.is_directory=false;

	// get space for the file
	int block_idx,byte_idx;

	// putting the record of this file in CWD
	// establishing an inode in this free space for this file - NO SPACE ALLOTTED TO THE FILE'S CONTENT YET
	get_file_pointer(CWD,&block_idx,&byte_idx);
	memcpy((char*)(void*)&MRFS[block_idx][0]+byte_idx,name,30);
	memcpy((char*)(void*)&MRFS[block_idx][0]+byte_idx+30,&inode_idx,2);
	inode cwd_inode=get_inode(CWD);
	cwd_inode.file_size+=32;
	update_inode(initialization_inode,inode_idx);
	update_inode(cwd_inode,CWD);
	BYTES_FILLED+=32;
	return inode_idx;	

}

int mkdir_myfs(char name[30]){
	int initial_CWD=CWD;
	// create a file and put 2 more files - for parent inode and the directory inode in this file
	int inode_dir_no=create_file(name,true);
	inode dir_inode=get_inode(inode_dir_no);
	dir_inode.is_directory=true;
	update_inode(dir_inode,inode_dir_no);
	// change directory to this directory
	CWD=inode_dir_no;
	char str[30]="..";   // pointer to parent directory
	inode_dir_no= create_file(str,true,initial_CWD);
	CWD=initial_CWD;
}



int copy_myfs2pc(const char*source, const char*dest){
	// finding the location of the file inode
	int present_inode=find_file_by_name(source);
	// inode of the file found
	inode file_inode=get_inode(present_inode);
	int file_size=file_inode.file_size;
	vector<int> block_list=get_file_block_list(present_inode);
	FILE *output=fopen(dest,"w");
	char str[257];
	for(int i=0;i<block_list.size();i++){
		int bytes_to_read=min(file_size,BLOCK_SIZE);
		memcpy(str,&MRFS[block_list[i]][0],256);
		str[256]='\0';
		fprintf(output,"%s",str);
		file_size-=BLOCK_SIZE;
	}
	fclose(output);
	return file_size;

}


int copy_pc2myfs (const char *source,const char *dest){
	//cout<<"copy function called"<<flush;
	int file_inode_no=create_file(dest,false);
	//cout<<"obtained inode no: "<<file_inode_no<<endl<<flush;
	inode file_inode=get_inode(file_inode_no);
	int present_block_number=0;
	int block_idx,byte_idx;
	FILE *input=fopen(source,"r");
	char str[256];
	file_inode.file_size=0;
	int n_read=fscanf(input,"%256[^\a]",str);
	while(n_read>0){
		int bytes_written=0;
		while(bytes_written<strlen(str)){
			get_file_pointer(file_inode_no,&block_idx,&byte_idx);
			//cout<<"in copy function: "<<block_idx<<" "<<byte_idx<<endl;
			file_inode=get_inode(file_inode_no);
			int bytes_to_write=min(BLOCK_SIZE-byte_idx,(int)strlen(str)-bytes_written);
			//cout<<"bytes to write: "<<bytes_to_write<<endl;
			(file_inode.file_size)+=bytes_to_write;
			memcpy((char*)(void*)&MRFS[block_idx][0]+byte_idx,&str[bytes_written],bytes_to_write);
			bytes_written+=bytes_to_write;
			//cout<<"\n\n\ncalling update_inode: "<<file_inode_no<<" "<<file_inode.is_directory<<endl;
			update_inode(file_inode,file_inode_no);
		}
		n_read=fscanf(input,"%256[^\a]",str);
		BYTES_FILLED+=bytes_written;
	}

	return file_inode_no;
}



void showfile_myfs(const char* file_name){
	int file_inode_no=find_file_by_name(file_name);
	cout<<"\n\nprinting file: "<<endl;
	inode file_inode=get_inode(file_inode_no);
	int file_size=file_inode.file_size;
	char str[257];
	vector<int> file_blocks=get_file_block_list(file_inode_no);
	for(int i=0;i<file_blocks.size();i++){
		int bytes_to_read=min(file_size,BLOCK_SIZE);
		memcpy(str,&MRFS[file_blocks[i]][0],256);
		str[256]='\0';
		cout<<str;
		file_size-=BLOCK_SIZE;
	}
	return;
}

int chdir_myfs(const char *dirname){
	int dir_inode_no=find_file_by_name(dirname);
	if(dir_inode_no<0){
		cout<<"no such directory";
		return -1;
	}
	inode dir_inode=get_inode(dir_inode_no);
	if(dir_inode.is_directory)
		CWD=dir_inode_no;
	else{
		cout<<"no such directory";
		return -1;
	}
	return 1;
}
int eof_myfs(int fd){
	int file_inode_no=fd;
	int block_idx=FILE_DESCRIPTOR[file_inode_no][0];
	int byte_idx=FILE_DESCRIPTOR[file_inode_no][1];
	inode file_inode=get_inode(file_inode_no);
	if((block_idx-1)*BLOCK_SIZE+byte_idx==file_inode.file_size-1)
		return 1;
	else 
		return -1;
}
int dump_myfs(const char *dumpfile){
	// The following global parameters are required to be saved

	/*int SB_ST_IDX=0;            	// start idx of the super block
int SB_END_IDX=0;           	// LAST idx of super block
int SB_BLOCK_LIST_BEGIN=0;		// start idx(block) of the bitmap for empty blocks
int SB_BLOCK_LIST_END=0;		// LAST idx -----------------------------
int SB_INODE_LIST_BEGIN=0;		// start idx(block) for bitmap of free inodes
int SB_INODE_LIST_END=0;		// LAST idx -----------------------------
int IL_ST_IDX=0;				// start idx of the inode list
int IL_END_IDX=0;
int INODE_SIZE=0;				// size of individual idx node in bytes
int DATA_BLOCK_ST_IDX=0;
int BYTES_FILLED=0;
int NUMBER_BLOCKS=0; 			// tot number of blocks
int MAX_NUMBER_INODES=0; 		// max number of inodes*/
	FILE * output=fopen(dumpfile,"w");
	// storing the global parameters:
	fprintf(output,"%d ",SB_ST_IDX);
	fprintf(output,"%d ",SB_END_IDX);
	fprintf(output,"%d ",SB_BLOCK_LIST_BEGIN);
	fprintf(output,"%d ",SB_BLOCK_LIST_END);
	fprintf(output,"%d ",SB_INODE_LIST_BEGIN);
	fprintf(output,"%d ",SB_INODE_LIST_END);
	fprintf(output,"%d ",IL_ST_IDX);
	fprintf(output,"%d ",IL_END_IDX);
	fprintf(output,"%d ",INODE_SIZE);
	fprintf(output,"%d ",DATA_BLOCK_ST_IDX);
	fprintf(output,"%d ",BYTES_FILLED);
	fprintf(output,"%d ",NUMBER_BLOCKS);
	fprintf(output,"%d ",MAX_NUMBER_INODES);
	fprintf(output,"%d ",CWD);

	for(int i=0;i<NUMBER_BLOCKS;i++){
		for(int j=0;j<BLOCK_SIZE/sizeof(int);j++)
			fprintf(output,"%d ",MRFS[i][j]);
	}
	fclose(output);
	return 1;
}
int restore_myfs(const char*sourcefile){
	FILE *input=fopen(sourcefile,"r");

	//restoring the global parameters
	fscanf(input,"%d ",&SB_ST_IDX);
	fscanf(input,"%d ",&SB_END_IDX);
	fscanf(input,"%d ",&SB_BLOCK_LIST_BEGIN);
	fscanf(input,"%d ",&SB_BLOCK_LIST_END);
	fscanf(input,"%d ",&SB_INODE_LIST_BEGIN);
	fscanf(input,"%d ",&SB_INODE_LIST_END);
	fscanf(input,"%d ",&IL_ST_IDX);
	fscanf(input,"%d ",&IL_END_IDX);
	fscanf(input,"%d ",&INODE_SIZE);
	fscanf(input,"%d ",&DATA_BLOCK_ST_IDX);
	fscanf(input,"%d ",&BYTES_FILLED);
	fscanf(input,"%d ",&NUMBER_BLOCKS);
	fscanf(input,"%d ",&MAX_NUMBER_INODES);
	fscanf(input,"%d ",&CWD);
	cout<<NUMBER_BLOCKS<<endl;
	MRFS=(int **)malloc(NUMBER_BLOCKS*sizeof(int*));
	for(int i=0;i<NUMBER_BLOCKS;i++)
		MRFS[i]=(int*)malloc(sizeof(int)*(BLOCK_SIZE/sizeof(int)));

	// restoring the file descriptor table
	FILE_DESCRIPTOR=(int**)malloc(MAX_NUMBER_INODES*sizeof(int*));
	for(int i=0;i<MAX_NUMBER_INODES;i++){
		FILE_DESCRIPTOR[i]=(int*)malloc(3*sizeof(int));
		FILE_DESCRIPTOR[i][0]=-1;
	}


	for(int i=0;i<NUMBER_BLOCKS;i++){
		for(int j=0;j<BLOCK_SIZE/sizeof(int);j++){
			fscanf(input,"%d ",&MRFS[i][j]);
		}
	}
	fclose(input);
}
int rm_myfs(const char*filename){
	int file_inode_no=find_file_by_name(filename);
	if(file_inode_no<0){
		cout<<"no such file to delete"<<endl;
		return -1;
	}
	vector<int> file_blocks_list=get_file_block_list(file_inode_no);
	// freeing all memory blocks:
	vector<bool>vec2;
	vec2.push_back(false);
	for(int i=0;i<file_blocks_list.size();i++){
		write_bits_into_intArray(MRFS[SB_BLOCK_LIST_BEGIN+i/NUMBER_BLOCKS],BLOCK_SIZE/sizeof(int),i%NUMBER_BLOCKS,&vec2);
	}
	//freeing the file inode:
	int block_idx=SB_INODE_LIST_BEGIN+file_inode_no/MAX_NUMBER_INODES;
	int bit_idx=file_inode_no%MAX_NUMBER_INODES;
	write_bits_into_intArray(MRFS[block_idx],BLOCK_SIZE/sizeof(int),bit_idx,&vec2);

	// making changes in CWD
	char present_name[30];int present_inode=-1;bool matched=false;
	inode cwd_inode=get_inode(CWD);
	vector<int> cwd_blocks=get_file_block_list(CWD);
	for(int i=0;i<cwd_blocks.size();i++){
	int byte_idx=0;
		while(byte_idx<BLOCK_SIZE && i*BLOCK_SIZE+byte_idx<cwd_inode.file_size){
			bzero(present_name,30);
			memcpy(present_name,(char*)(void*)&MRFS[cwd_blocks[i]][0]+byte_idx,30);
			byte_idx+=30;
			if(strcmp(present_name,filename)==0){
				memcpy((char*)(void*)&MRFS[cwd_blocks[i]][0]+byte_idx,&present_inode,2);		
				matched=true;
				break;
			}
			byte_idx+=2;
		}
		if(matched)
			break;
	}
	return 1;
}

int rmdir_myfs(const char *dirname){
	int dir_inode_no=find_file_by_name(dirname);
	if(dir_inode_no<0)
		return -1;
	inode dir_inode=get_inode(dir_inode_no);
	if(!dir_inode.is_directory)
		return -1;

	char present_name[30];
	vector<int> dir_blocks=get_file_block_list(dir_inode_no);
	for(int i=0;i<dir_blocks.size();i++){
		int byte_idx=0;
		while(byte_idx<BLOCK_SIZE && i*BLOCK_SIZE+byte_idx<dir_inode.file_size){
			bzero(present_name,30);
			memcpy(present_name,(char*)(void*)&MRFS[dir_blocks[i]][0]+byte_idx,30);
			byte_idx+=30;
			if(strcmp(present_name,"..")!=0){
				//cout<<"deleting "<<present_name<<endl;
				rm_myfs(present_name);
			}
			byte_idx+=2;
		}
	}
	int present_inode=-1;
	bool matched=false;
	// doing necessary changes in the CWD
	inode cwd_inode=get_inode(CWD);
	vector<int> cwd_blocks=get_file_block_list(CWD);
	for(int i=0;i<cwd_blocks.size();i++){
	int byte_idx=0;
		while(byte_idx<BLOCK_SIZE && i*BLOCK_SIZE+byte_idx<cwd_inode.file_size){
			bzero(present_name,30);
			memcpy(present_name,(char*)(void*)&MRFS[cwd_blocks[i]][0]+byte_idx,30);
			byte_idx+=30;
			if(strcmp(present_name,dirname)==0){
				memcpy((char*)(void*)&MRFS[cwd_blocks[i]][0]+byte_idx,&present_inode,2);		
				matched=true;
				break;
			}
			byte_idx+=2;
		}
		if(matched)
			break;
	}
	rm_myfs(dirname);	

	return 1;
}
int chmod_myfs(const char *name,int mode){
	int file_inode_no=find_file_by_name(name);
	inode file_inode=get_inode(file_inode_no);
	// figuring out the meaning of the mode variable. n3 is Most Significant digit
	int n1=mode%10;
	int n2=(mode/10)%10;
	int n3=(mode/100)%10;
	int numbers[3]={n3,n2,n1};
	for(int i=0;i<3;i++){
		if(numbers[i]>7)
			return -1;
		int b1=0,b2=0,b3=0;//again , b3 is most significant
		b1=numbers[i]%2;
		numbers[i]=numbers[i]/2;
		b2=numbers[i]%2;
		numbers[i]=numbers[i]/2;
		b3=numbers[i]%2;
		file_inode.access_perms[i][0]=b3;
		file_inode.access_perms[i][1]=b2;
		file_inode.access_perms[i][2]=b1;

	}
	
	update_inode(file_inode,file_inode_no);

}
int status_myfs(){
	int free_block_count=0;
	for(int i=DATA_BLOCK_ST_IDX;i<NUMBER_BLOCKS;i++){
		vector<bool> vec1;
		vec1=read_bits_from_intArray(MRFS[SB_BLOCK_LIST_BEGIN+i/NUMBER_BLOCKS],1,i%NUMBER_BLOCKS);
		if(vec1[0]==false){
			free_block_count++;
		}
	}
	int number_of_files=0;
	for(int i=0;i<MAX_NUMBER_INODES;i++){
		vector<bool> vec1;
		int block_idx=SB_INODE_LIST_BEGIN+i/MAX_NUMBER_INODES;
		int bit_idx=i%MAX_NUMBER_INODES;
		vec1=read_bits_from_intArray(MRFS[block_idx],1,bit_idx);
		if(vec1[0]==true){
			number_of_files++;
		}
	}
	cout<<"\tMRFS STATUS"<<endl;
	cout<<"--------------------------------------------"<<endl;
	cout<<"TOTAL SIZE:\t\t"<<NUMBER_BLOCKS*BLOCK_SIZE<<" B"<<endl;
	cout<<"FREE SPACE:\t\t"<<NUMBER_BLOCKS*BLOCK_SIZE-BYTES_FILLED<<" B"<<endl;
	cout<<"NUMBER OF FILES:\t"<<number_of_files<<endl;
	cout<<"FREE DATA BLOCKS:\t"<<free_block_count<<endl;
	cout<<"--------------------------------------------"<<endl;
	return 1;
}


