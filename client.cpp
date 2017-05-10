#ifndef CLIENT_CPP
#define CLIENT_CPP
#include <iostream>
#include "mapboard.h"
#include <unistd.h>
#include <vector>
#include <utility>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <stdlib.h>
#include <fstream>
#include <mqueue.h>
#include "goldchase.h"
#include "Map.h"
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <signal.h>
#include <sstream>
#include <time.h>
#include "fancyrw.h"
using namespace std;

//Global Variables///////////////////////////
int result_c;
mapBoard* mbc;
unsigned char* clientCopy;
int sockfdClient;
int c_rows=0;
int c_cols=0;
unsigned char* tempMap1;
unsigned char SockPlayer_c;


void HUPhandler_c(int)
{
	SockPlayer_c=G_SOCKPLR;
	if(mbc->players[0]!=0)
		SockPlayer_c|=G_PLR0;
	if(mbc->players[1]!=0)
		SockPlayer_c|=G_PLR1;
	if(mbc->players[2]!=0)
		SockPlayer_c|=G_PLR2;
	if(mbc->players[3]!=0)
		SockPlayer_c|=G_PLR3;
	if(mbc->players[4]!=0)
		SockPlayer_c|=G_PLR4;
	WRITE(sockfdClient,&SockPlayer_c,sizeof(unsigned char));
}

void USR1handler_c(int){


	vector< pair<short,unsigned char> > tempVector;
	  for(short i=0; i<c_rows*c_cols; ++i)
	  {
	    if(tempMap1[i]!= clientCopy[i])
	    {
	      pair<short,unsigned char> myPair;
	      myPair.first=i;
	      myPair.second=tempMap1[i];
	      tempVector.push_back(myPair);
	      clientCopy[i]=tempMap1[i];
	    }
	  }

	  //here iterate through pvec, writing out to socket
	  //testing we will print it:
	  unsigned char byt = 0;
	  if(tempVector.size()>0){

	      WRITE(sockfdClient,&byt,1);
	      short sizeOfVector=tempVector.size();
	      WRITE(sockfdClient,&sizeOfVector,sizeof(short));

	      for(short i=0; i<tempVector.size(); ++i)
	      {

	        cerr << "offset=" << tempVector[i].first;
	        cerr << ", new value=" << tempVector[i].second << endl;

	        WRITE(sockfdClient,&tempVector[i].first,sizeof(short));
	        WRITE(sockfdClient,&tempVector[i].second, sizeof(unsigned char));
	      }
	  }

}



void runClientDeamon(char *addr)
{

  sem_t *clientSemaphore;

  int status;


	if(fork()>0)//I'm the parent, leave the function
	{
    return;
	}

	if(fork()>0)
		exit(0);
	if(setsid()==-1)
		exit(1);

	for(int i=0; i< sysconf(_SC_OPEN_MAX); ++i) close(i);

	open("/dev/null", O_RDWR); //fd 0
	open("/dev/null", O_RDWR); //fd 1
	open("/dev/null", O_RDWR);
	umask(0);
	chdir("/");

	result_c=open("/home/aditya/611/gitData/GoldChase/myPipe", O_RDWR);

//CLient starting


	struct sigaction hupAction_c;
	hupAction_c.sa_handler=HUPhandler_c;
	sigemptyset(&hupAction_c.sa_mask);
	hupAction_c.sa_flags=0;
	hupAction_c.sa_restorer=NULL ;
	sigaction(SIGHUP, &hupAction_c, NULL);

  struct sigaction usr1Action_c;
  usr1Action_c.sa_handler=USR1handler_c;
  sigemptyset(&usr1Action_c.sa_mask);
  usr1Action_c.sa_flags=0;
  usr1Action_c.sa_restorer=NULL ;
  sigaction(SIGUSR1, &usr1Action_c, NULL);

	//change this # between 2000-65k before using
	const char* portno="42427";

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints)); //zero out everything in structure
	hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
	hints.ai_socktype=SOCK_STREAM; // TCP stream sockets


	struct addrinfo *servinfo;
	//instead of "localhost", it could by any domain name
	if((status=getaddrinfo(addr, portno, &hints, &servinfo))==-1){
			fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
	  	exit(1);
		}


	sockfdClient=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

	if((status=connect(sockfdClient, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
	{
		perror("connect");
	//	exit(1);
	}


	freeaddrinfo(servinfo);

	int err=READ(sockfdClient,&c_rows,sizeof(int));
	READ(sockfdClient,&c_cols,sizeof(int));
	if(err==-1){
	}






	clientSemaphore = sem_open(
			"/mySEM",
			O_CREAT|O_EXCL,
			S_IROTH| S_IWOTH| S_IRGRP| S_IWGRP| S_IRUSR| S_IWUSR,
			1
			);
//sem_wait here. Do sem_post just before client enters infinite loop reading a single byte

	if(clientSemaphore==SEM_FAILED)
	{
		if(errno==EEXIST)
		{perror("semphone present inside first palyer ");}
	}

	int ret=shm_open("AMJ_mymap", O_CREAT|O_RDWR,  S_IRUSR | S_IWUSR);

	if (ret==-1){perror("the shared memory open client daemon problem:");}
	else{perror("Shared memory created if 1 client daemon player:");}


	if(ftruncate(ret,c_rows*c_cols)==-1)
	{perror("ftruncate called:");
		exit(1);
	}

	mbc=(mapBoard*) mmap (NULL,c_rows*c_cols+sizeof(mapBoard),PROT_READ|PROT_WRITE,MAP_SHARED,ret,0);

	mbc->deamonID=getpid();
 	int areaOfMap=c_rows*c_cols;

	clientCopy = new unsigned char [areaOfMap];


	mbc->rows=c_rows;
	mbc->cols=c_cols;
	unsigned char copy;

//        READ(sockfdClient, clientCopy, c_rows*c_cols);
//        memcpy(mbc->map, clientCopy, c_rows*c_cols);
	for(int i=0;i<areaOfMap;i++)
	{
		READ(sockfdClient, &copy,sizeof(unsigned char));
		mbc->map[i]=copy;
		clientCopy[i]=copy;
	}


  unsigned char players_c;
  READ(sockfdClient, &players_c, sizeof(unsigned char));

  if(players_c&G_PLR0)
    mbc->players[0] = 1;
  if(players_c&G_PLR1)
    mbc->players[1] = 1;
  if(players_c&G_PLR2)
    mbc->players[2] = 1;
  if(players_c&G_PLR3)
    mbc->players[3] = 1;
  if(players_c&G_PLR4)
    mbc->players[4] = 1;

 tempMap1=mbc->map;

 while(1)
 {
   sem_t *my_sem_ptr;
   unsigned char byte;
   READ(sockfdClient, &byte, 1);

   if(byte==0)
   {
     short vectorSize, offset;
     unsigned char newLoc;
     READ(sockfdClient, &vectorSize, sizeof(short));
     for(short i=0; i<vectorSize; ++i)
     {
       READ(sockfdClient,&offset,sizeof(short));
       READ(sockfdClient,&newLoc, sizeof(unsigned char));
       mbc->map[offset]=newLoc;
       clientCopy[offset]=newLoc;

     }
     for(int m=0;m<5;m++)
     {
       if(mbc->players[m]!=0)
       {
         kill(mbc->players[m],SIGUSR1);
       }
     }
   }

	 if(byte & G_SOCKPLR)
	 {
		 unsigned char arr[5] = {G_PLR0, G_PLR1, G_PLR2, G_PLR3, G_PLR4};
		 for(int i =0; i < 5; i++)
		 {
			 if(byte&arr[i] && mbc->players[i] == 0)
			 {
				 mbc->players[i] = mbc->deamonID;
			 }
			 else if(!byte&arr[i] && mbc->players[i] != 0)
			 	{
				 mbc->players[i] = 0;
			 	}
		 	}
	 	}
	}
}

#endif
