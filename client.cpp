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








void USR1handler_c(int){

	write(result_c, "GOT SIG USER 1!\n", sizeof("GOT SIG USER1!\n"));

	vector< pair<short,unsigned char> > tempVector;
	  for(short i=0; i<c_rows*c_cols; ++i)
	  {
	    write(result_c, "inside map size!\n", 20);
	    if(tempMap1[i]!= clientCopy[i])
	    {
	      pair<short,unsigned char> myPair;
	      myPair.first=i;
	      myPair.second=tempMap1[i];
	      tempVector.push_back(myPair);
	      clientCopy[i]=tempMap1[i];
	      write(result_c, "changed square!\n", 20);
	    }
	  }

	  //here iterate through pvec, writing out to socket
	  //testing we will print it:
	  unsigned char byt = 0;
	  if(tempVector.size()>0){
	    write(result_c, "vectore greater !\n", 15);

	      WRITE(sockfdClient,&byt,1);
	      short sizeOfVector=tempVector.size();
	      WRITE(sockfdClient,&sizeOfVector,sizeof(short));

	      for(short i=0; i<tempVector.size(); ++i)
	      {
	        write(result_c, "forloop start!\n", 15);

	        cerr << "offset=" << tempVector[i].first;
	        cerr << ", new value=" << tempVector[i].second << endl;

	        WRITE(sockfdClient,&tempVector[i].first,sizeof(short));
	        WRITE(sockfdClient,&tempVector[i].second, sizeof(unsigned char));
	        write(result_c, "forloop end!\n", 15);

	      }
	  }

}



void runClientDeamon(char *addr)
{

  sem_t *clientSemaphore;

  int status;


	if(fork()>0)//I'm the parent, leave the function
	{
    sleep(5);
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

	//===================Client Startup==============================
//	int sockfdClient; //file descriptor for the socket

	write(result_c, "Client starting\n", sizeof("Client starting\n"));

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

	write(result_c, "client addrinfo Done !\n", sizeof("Client addrinfo Done !\n"));

	struct addrinfo *servinfo;
	//instead of "localhost", it could by any domain name
	if((status=getaddrinfo(addr, portno, &hints, &servinfo))==-1){
			fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
			write(result_c, "ERROR getaddrinfo error: !\n", sizeof("ERROR getaddrinfo error:!\n"));
	  	exit(1);
		}

	write(result_c, "Client getaddrinfo Done!\n", sizeof("Client getaddrinfo Done !\n"));

	sockfdClient=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

	write(result_c, "Client next step !\n", sizeof("Client next step !\n"));

	if((status=connect(sockfdClient, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
	{
		perror("connect");
		write(result_c, "ERROR connect: !\n", sizeof("ERROR connect:!\n"));
	//	exit(1);
	}

	write(result_c, "Client after connect Done !\n", sizeof("Client after connect Done !\n"));

	//release the information allocated by getaddrinfo()
	freeaddrinfo(servinfo);
	write(result_c, "Client freeaddrinfo Done\n", sizeof("Client freeaddrinfo Done\n"));

	int err=READ(sockfdClient,&c_rows,sizeof(int));
	READ(sockfdClient,&c_cols,sizeof(int));
	if(err==-1){
		write(result_c, "ERROR read: !\n", sizeof("ERROR read:!\n"));
	}
	write(result_c,&c_rows, sizeof(c_rows));





	write(result_c, "Now semaphore will be created: !\n", sizeof("Now semaphore will be created:!\n"));

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

	write(result_c, "semaphore created in client daemon !\n", sizeof("semaphore created in client daemon!\n"));


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

  for(int i = 0; i < c_rows*c_cols; i++)
  {
    if(clientCopy[i]&G_WALL)
      write(result_c, "*", 1);
    if(clientCopy[i]&G_PLR0)
      write(result_c, "1", 1);
    if(clientCopy[i] == 0)
      write(result_c, " ", 1);
  }

  write(result_c, "All written!", 15);
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
     write(result_c, "map chenge !\n",20);
     unsigned char newLoc;
     READ(sockfdClient, &vectorSize, sizeof(short));
     write(result_c, "map chenge after read !\n",25);
     for(short i=0; i<vectorSize; ++i)
     {
       write(result_c, "map chenge inside for!\n",25);
       READ(sockfdClient,&offset,sizeof(short));
       READ(sockfdClient,&newLoc, sizeof(unsigned char));
       mbc->map[offset]=newLoc;
       clientCopy[offset]=newLoc;

     }
     write(result_c, "map chenge drawMap !\n",25);
     for(int m=0;m<5;m++)
     {
       if(mbc->players[m]!=0)
       {
         kill(mbc->players[m],SIGUSR1);
       }
     }
   }
}
}

#endif
