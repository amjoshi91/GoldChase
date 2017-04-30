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
  int new_sockfd;
  int sockfd;
  int s_rows = 0;
  int s_cols = 0;
  int status;
  unsigned char SockPlayer = G_SOCKPLR;
  unsigned char* tempMap;
  ///////////////

  void USR1handler(int)
  {
    write(result, "GOT SIG USER 1!\n", sizeof("GOT SIG USER1!\n"));

    vector< pair<short,unsigned char> > tempVector;
    for(short i=0; i<s_rows*s_cols; ++i)
    {
      write(result, "inside map size!\n", 20);
      if(tempMap[i] != my_copy[i])
      {
        pair<short,unsigned char> myPair;
        myPair.first=i;
        myPair.second=tempMap[i];
        tempVector.push_back(myPair);
        my_copy[i]=tempMap[i];
        write(result, "changed square!\n", 20);
      }
    }

    //here iterate through pvec, writing out to socket
    //testing we will print it:
    unsigned char byt = 0;
    if(tempVector.size()>0){
      write(result, "vectore greater !\n", 15);

      write(new_sockfd,&byt,1);
      short sizeOfVector=tempVector.size();
      write(new_sockfd,&sizeOfVector,sizeof(short));

      for(short i=0; i<tempVector.size(); ++i)
      {
        write(result, "forloop start!\n", 15);

        cerr << "offset=" << tempVector[i].first;
        cerr << ", new value=" << tempVector[i].second << endl;

        write(new_sockfd,&tempVector[i].first,sizeof(short));
        write(new_sockfd,&tempVector[i].second, sizeof(unsigned char));
        write(result, "forloop end!\n", 15);

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
    write(result, "99", 2);

    struct sigaction usr1Action;
    usr1Action.sa_handler=USR1handler;
    sigemptyset(&usr1Action.sa_mask);
    usr1Action.sa_flags=0;
    usr1Action.sa_restorer=NULL ;
    sigaction(SIGUSR1, &usr1Action, NULL);

    shmFD = shm_open("AMJ_mymap", O_RDWR, S_IRUSR|S_IWUSR);
    if(shmFD == -1)
    {
      cerr << "Fault in server deamon shmFD\n";
    }
    write(result, "Shared mem created", 19);
    read(shmFD, &s_rows, sizeof(int));
    read(shmFD, &s_cols, sizeof(int));

    mbs = (mapBoard*)mmap(NULL,s_rows*s_cols+sizeof(mapBoard), PROT_READ|PROT_WRITE, MAP_SHARED,shmFD,0);
    mbs->deamonID=getpid();
    tempMap = mbs->map;
    my_copy = new unsigned char[s_rows*s_cols];

    for(i = 0; i < s_rows*s_cols; i++)
    my_copy[i] = tempMap[i];


    for(i = 0; i < s_rows*s_cols; i++)
    {
      if(my_copy[i]&G_WALL)
      write(result, "*", 1);
      if(my_copy[i]&G_PLR0)
      write(result, "1", 1);
      if(my_copy[i] == 0)
      write(result, " ", 1);
    }

    /*===========================server statrtup===========================*/


    write(result,"Server started (Initiating socket)\n",40);

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
      write(result, "ERROR setsockopt !\n", sizeof("ERROR setsockopt!\n"));
      exit(1);
    }

    //We need to "bind" the socket to the port number so that the kernel
    //can match an incoming packet on a port to the proper process
    if((status=bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1){
      perror("bind");
      write(result, "ERROR bind !\n", sizeof("ERROR bind!\n"));
      exit(1);
    }

    write(result,"server status bind done \n",sizeof("server is status bind done \n"));

    //when done, release dynamically allocated memory
    freeaddrinfo(servinfo);

    if(listen(sockfd,1)==-1){
      perror("listen");
      write(result,"server listen error\n",sizeof("server is lister error\n"));
      exit(1);
    }


    write(result,"server Blocking, waiting for client to connect\n",sizeof("server Blocking, waiting for client to connect\n"));
    printf("Blocking, waiting for client to connect\n");

    struct sockaddr_in client_addr;
    socklen_t clientSize = sizeof(client_addr);
    do
    {
      new_sockfd=accept(sockfd, (struct sockaddr*) &client_addr, &clientSize);
    } while(new_sockfd==-1 && errno==EINTR);

    if(new_sockfd==-1 && errno!=EINTR)
    {
      perror("accept");
      exit(1);
    }
    //read & write to the socket

    WRITE(new_sockfd,&s_rows,sizeof(int));
    WRITE(new_sockfd,&s_cols,sizeof(int));

    //Send the local copy to client
    WRITE(new_sockfd, my_copy, s_rows*s_cols);
    /*for(int j=0;j<s_rows*s_cols;j++)
    {
    WRITE(new_sockfd,&my_copy[j],sizeof(unsigned char));
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
  WRITE(new_sockfd, &tempVar,sizeof(unsigned char));

  write(result, "Connection estd", 15);

  while(1)
  {
    sem_t *my_sem_ptr;
    unsigned char byte;
    READ(new_sockfd, &byte, 1);

    if(byte==0)
    {
      short vectorSize, offset;
      write(result, "map chenge !\n",20);
      unsigned char newLoc;
      READ(new_sockfd, &vectorSize, sizeof(short));
      write(result, "map chenge after read !\n",25);
      for(short i=0; i<vectorSize; ++i)
      {
        write(result, "map chenge inside for!\n",25);
        READ(new_sockfd,&offset,sizeof(short));
        READ(new_sockfd,&newLoc, sizeof(unsigned char));
        mbs->map[offset]=newLoc;
        my_copy[offset]=newLoc;

      }
      write(result, "map chenge drawMap !\n",25);
      for(int m=0;m<5;m++)
      {
        if(mbs->players[m]!=0)
        {
          kill(mbs->players[m],SIGUSR1);
        }
      }
      }
    }
  }

  #endif
