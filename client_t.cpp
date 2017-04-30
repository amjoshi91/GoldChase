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


void runClientDeamon(char *addr)
{

  sem_t *clientSemaphore;
	int c_rows=0;
	int c_cols=0;
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

	result_c=open("/home/aditya/611/in_process/project_5/myPipe", O_RDWR);

	//===================Client Startup==============================
//	int sockfdClient; //file descriptor for the socket

	write(result_c, "Client starting\n", sizeof("Client starting\n"));

	//change this # between 2000-65k before using
	const char* portno="42424";

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

	for(int i=0;i<areaOfMap;i++)
	{
		READ(sockfdClient,&copy,sizeof(unsigned char));
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

 unsigned char* tempMap;
 tempMap=mbc->map;


  return;
}
#endif
