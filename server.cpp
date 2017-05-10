  #ifndef SERVER_CPP
  #define SERVER_CPP

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




  //////////GLOBAL VAR//////////
  int result;
  mapBoard *mbs;
  unsigned char* my_copy;
  int serverSockFD;
  int sockfd;
  int s_rows = 0;
  int s_cols = 0;
  int status;
  unsigned char SockPlayer = G_SOCKPLR;
  unsigned char* tempMap;
  ///////////////

  void HUPhandler(int)
  {
    SockPlayer=G_SOCKPLR;
    if(mbs->players[0]!=0)
      SockPlayer|=G_PLR0;
    if(mbs->players[1]!=0)
      SockPlayer|=G_PLR1;
    if(mbs->players[2]!=0)
      SockPlayer|=G_PLR2;
    if(mbs->players[3]!=0)
      SockPlayer|=G_PLR3;
    if(mbs->players[4]!=0)
      SockPlayer|=G_PLR4;
    WRITE(serverSockFD,&SockPlayer,sizeof(unsigned char));
  }

  void USR1handler(int)
  {

    vector< pair<short,unsigned char> > tempVector;
    for(short i=0; i<s_rows*s_cols; ++i)
    {
      if(tempMap[i] != my_copy[i])
      {
        pair<short,unsigned char> myPair;
        myPair.first=i;
        myPair.second=tempMap[i];
        tempVector.push_back(myPair);
        my_copy[i]=tempMap[i];
      }
    }

    unsigned char byt = 0;
    if(tempVector.size()>0){

      write(serverSockFD,&byt,1);
      short sizeOfVector=tempVector.size();
      write(serverSockFD,&sizeOfVector,sizeof(short));

      for(short i=0; i<tempVector.size(); ++i)
      {

        cerr << "offset=" << tempVector[i].first;
        cerr << ", new value=" << tempVector[i].second << endl;

        write(serverSockFD,&tempVector[i].first,sizeof(short));
        write(serverSockFD,&tempVector[i].second, sizeof(unsigned char));

      }
    }

  }


  void runServerDeamon()
  {


    int i, shmFD;

    if(fork() > 0)
    {
      sleep(1);
      return;
    }

    if(fork() > 0)
    exit(0);
    if(setsid() == -1)
    exit(1);

    for(i = 0; i < sysconf(_SC_OPEN_MAX); i++)
    close(i);

    open("/dev/null", O_RDWR); //fd 0
    open("/dev/null", O_RDWR); //fd 1
    open("/dev/null", O_RDWR);
    umask(0);
    chdir("/");

    result = open("/home/aditya/611/in_process/gitData/GoldChase/myPipe", O_RDWR);

    struct sigaction usr1Action;
    usr1Action.sa_handler=USR1handler;
    sigemptyset(&usr1Action.sa_mask);
    usr1Action.sa_flags=0;
    usr1Action.sa_restorer=NULL ;
    sigaction(SIGUSR1, &usr1Action, NULL);

    struct sigaction hupAction;
    hupAction.sa_handler=HUPhandler;
    sigemptyset(&hupAction.sa_mask);
    hupAction.sa_flags=0;
    hupAction.sa_restorer=NULL ;
    sigaction(SIGHUP, &hupAction, NULL);

    shmFD = shm_open("AMJ_mymap", O_RDWR, S_IRUSR|S_IWUSR);
    if(shmFD == -1)
    {
      cerr << "Fault in server deamon shmFD\n";
    }
    read(shmFD, &s_rows, sizeof(int));
    read(shmFD, &s_cols, sizeof(int));

    mbs = (mapBoard*)mmap(NULL,s_rows*s_cols+sizeof(mapBoard), PROT_READ|PROT_WRITE, MAP_SHARED,shmFD,0);
    mbs->deamonID=getpid();
    tempMap = mbs->map;
    my_copy = new unsigned char[s_rows*s_cols];

    for(i = 0; i < s_rows*s_cols; i++)
    my_copy[i] = tempMap[i];

    //Server started



    //change this # between 2000-65k before using
    const char* portno="42427";
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints)); //zero out everything in structure
    hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
    hints.ai_socktype=SOCK_STREAM; // TCP stream sockets
    hints.ai_flags=AI_PASSIVE; //file in the IP of the server for me

    struct addrinfo *servinfo;

    if((status=getaddrinfo(NULL, portno, &hints, &servinfo))==-1){
      fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
      exit(1);
    }


    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    /*avoid "Address already in use" error*/
    int yes=1;

    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1){
      perror("setsockopt");
      exit(1);
    }

    //We need to "bind" the socket to the port number so that the kernel
    //can match an incoming packet on a port to the proper process
    if((status=bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1){
      perror("bind");
      exit(1);
    }


    //when done, release dynamically allocated memory
    freeaddrinfo(servinfo);

    if(listen(sockfd,1)==-1){
      perror("listen");
      exit(1);
    }


    printf("Blocking, waiting for client to connect\n");

    struct sockaddr_in client_addr;
    socklen_t clientSize = sizeof(client_addr);
    do
    {
      serverSockFD=accept(sockfd, (struct sockaddr*) &client_addr, &clientSize);
    } while(serverSockFD==-1 && errno==EINTR);

    if(serverSockFD==-1 && errno!=EINTR)
    {
      perror("accept");
      exit(1);
    }
    //read & write to the socket

    WRITE(serverSockFD,&s_rows,sizeof(int));
    WRITE(serverSockFD,&s_cols,sizeof(int));

    //Send the local copy to client
    WRITE(serverSockFD, my_copy, s_rows*s_cols);
    /*for(int j=0;j<s_rows*s_cols;j++)
    {
    WRITE(serverSockFD,&my_copy[j],sizeof(unsigned char));
  }*/

  unsigned char tempVar = 0;
  if(mbs->players[0] != 0)
  tempVar|=G_PLR0;
  if(mbs->players[1] != 0)
  tempVar|=G_PLR1;
  if(mbs->players[2] != 0)
  tempVar|=G_PLR2;
  if(mbs->players[3] != 0)
  tempVar|=G_PLR3;
  if(mbs->players[4] != 0)
  tempVar|=G_PLR4;

  tempVar|=SockPlayer;
  WRITE(serverSockFD, &tempVar,sizeof(unsigned char));


  while(1)
  {
    sem_t *my_sem_ptr;
    unsigned char byte;
    READ(serverSockFD, &byte, 1);

    if(byte==0)
    {
      short vectorSize, offset;
      unsigned char newLoc;
      READ(serverSockFD, &vectorSize, sizeof(short));
      for(short i=0; i<vectorSize; ++i)
      {
        READ(serverSockFD,&offset,sizeof(short));
        READ(serverSockFD,&newLoc, sizeof(unsigned char));
        mbs->map[offset]=newLoc;
        my_copy[offset]=newLoc;

      }
      for(int m=0;m<5;m++)
      {
        if(mbs->players[m]!=0)
        {
          kill(mbs->players[m],SIGUSR1);
        }
      }
    }


    if(byte & G_SOCKPLR)
    {
      unsigned char arr[5] = {G_PLR0, G_PLR1, G_PLR2, G_PLR3, G_PLR4};
      for(int i =0; i < 5; i++)
      {
        if(byte&arr[i] && mbs->players[i] == 0)
        {
          mbs->players[i] = mbs->deamonID;
        }
        else if(!byte&arr[i] && mbs->players[i] != 0)
        {
          mbs->players[i] = 0;
        }
      }
      for(int i = 0; i < 5; i++)
      {
        write(result, &i, sizeof(int));
        if(mbs->players[i] != 0 && mbs->players[i] != mbs->deamonID)
        {
          write(result, "in if:", 7);
          write(result, &i, sizeof(int));
          kill(mbs->players[i], SIGUSR1);
        }
      }
    }
  }
}

  #endif
