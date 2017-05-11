/////////////////////////////////////////////////////////////////
// Name: Aditya M. Joshi
// Student id: 007374927
// GoldChase
// Phase I
/////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////
// Header File Declaration
/////////////////////////////////////////////////////////////////
#include <iostream>
#include "mapboard.h"
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
using namespace std;

/////////////////////////////////////////////////////////////////
//Function Prototypes
/////////////////////////////////////////////////////////////////
int readBoard();
int initialize();
void createFirstMap();
void setPlayer();
int generateRandom();
void exitFunction();
void winningFunction();
void processInput(int);

/////////////////////////////////////////////////////////////////
//Global Variables
/////////////////////////////////////////////////////////////////
char *board;
unsigned char thisPlayer;
int noOfRows, noOfCols, noOfGold, randomNum;
int shm_fd, playerPosition;
mqd_t readqueue_fd;
string mq_name = "/none";
int currentPlayer, result_m;
sem_t* sem;
Map *ptr = NULL;
mapBoard  *mb;

void mapRefresh(int)
{
  write(result_m, "mapRef called", sizeof("mapRef called"));
  if(ptr != NULL)
    ptr->drawMap();
}

void read_message(int)
{
  //set up message queue to receive signal whenever
  //message comes in. This is being done inside of
  //the handler function because it has to be "reset"
  //each time it is used.
  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(readqueue_fd, &mq_notification_event);

  //read a message
  int err;
  char msg[121];
  memset(msg, 0, 121);//set all characters to '\0'
  while((err=mq_receive(readqueue_fd, msg, 120, NULL))!=-1)
  {
    //cout << "Message received: " << msg << endl;
    ptr->postNotice(msg);
    memset(msg, 0, 121);//set all characters to '\0'
  }
  //we exit while-loop when mq_receive returns -1
  //if errno==EAGAIN that is normal: there is no message waiting
  if(errno!=EAGAIN)
  {
    perror("mq_receive");
    exit(1);
  }
}

int read_queue()
{
  struct sigaction act;
  act.sa_handler = read_message;
  sigemptyset(&act.sa_mask);
  act.sa_flags=0;
  sigaction(SIGUSR2, &act, NULL);

 struct mq_attr queuevalues;
 queuevalues.mq_flags =0;
 queuevalues.mq_maxmsg=10;
 queuevalues.mq_msgsize=120;

 readqueue_fd=mq_open(mq_name.c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
      S_IRUSR|S_IWUSR, &queuevalues);
  if(readqueue_fd==-1)
  {
    if(errno==EEXIST)
    {
      cerr << "Message queue already exists! Removing....\n";
      mq_unlink("/myqueue");
    }
    exit(1);
  }
  struct sigevent mq_notification_event; //create a structure object
  mq_notification_event.sigev_notify=SIGEV_SIGNAL; //signal notification
  mq_notification_event.sigev_signo=SIGUSR2; //using the SIGUSR2 signal
  mq_notify(readqueue_fd, &mq_notification_event); //register it!
}

void clean_up(int)
{
  cerr << "Cleaning up message queue" << endl;
  mq_close(readqueue_fd);
  mq_unlink(mq_name.c_str());
  exit(1);
}

int write_queue(string str)
{
  if(mq_name == "/none")
  {
    cerr << "Queue not set" << endl;
    exit(1);
  }


  const char *strq = str.c_str();
  mqd_t writequeue_fd;

  if((writequeue_fd=mq_open(mq_name.c_str(), O_WRONLY|O_NONBLOCK))==-1)
  {
    perror("mq_open failed");
    exit(1);
  }

  char message_text[121];
  memset(message_text, 0, 121);
  strncpy(message_text, strq, 120);

  if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
  {
    perror("bad send");
    exit(1);
  }

  mq_close(writequeue_fd);
//  mq_unlink("/papercup");
}

void clean_it_up(int)
{
  exitFunction();
  exit(1);
}

/////////////////////////////////////////////////////////////////
//main function
/////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
  result_m = open("/home/aditya/611/in_process/gitData/GoldChase/myPipe", O_RDWR);
  if(argc > 1)
  {
    runClientDeamon(argv[1]);
    sleep(3);
  }
  struct sigaction mapRef;
  mapRef.sa_handler = mapRefresh;
  sigemptyset(&mapRef.sa_mask);
  mapRef.sa_flags = 0;
  mapRef.sa_restorer = NULL;
  sigaction(SIGUSR1, &mapRef, NULL);
  sigaction(SIGHUP, &mapRef, NULL);


  struct sigaction cleanup;
  cleanup.sa_handler = clean_it_up;
  sigemptyset(&cleanup.sa_mask);
  cleanup.sa_flags = 0;
  cleanup.sa_restorer = NULL;
  sigaction(SIGINT, &cleanup, NULL);
  sigaction(SIGTERM, &cleanup, NULL);




  int shm_fd;
  int input;
  srand(time(NULL));
//  close(2);
//  int pipe_fd = open("mypipe", O_WRONLY);
  sem=sem_open("/mySEM",
             O_CREAT|O_EXCL,
             S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP| S_IROTH| S_IWOTH,
             1);


  if(sem==SEM_FAILED && errno!=EEXIST)
  {
    perror("Semaphore was not created ERROR!!!\n");
    exit(1);
  }

  if(sem!=SEM_FAILED) //First Player
  {
    shm_fd = initialize();
    createFirstMap();
    for(int i = 0; i < 5; i++)
      mb->players[i] = 0;

    sem_wait(sem);
    write(99, "placing player",getpid());
    setPlayer();
    sem_post(sem);

    if(mb->deamonID == 0)
      runServerDeamon();
    kill(mb->deamonID, SIGHUP);

  }

  else //Second Player
  {
    sem = sem_open("/mySEM", O_RDWR);
    shm_fd = shm_open("/AMJ_mymap",O_CREAT|O_RDWR,
    S_IROTH|S_IWOTH|S_IRGRP|S_IWGRP|S_IRUSR|S_IWUSR);
    mb=(mapBoard*)mmap(NULL,(noOfRows*noOfCols)+sizeof(mapBoard), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
  sem_wait(sem);
  write(99, "placing player",getpid());
  setPlayer();
  sem_post(sem);
  if(mb->deamonID != 0)
  {
    kill(mb->deamonID, SIGHUP);
  }
  for(int i = 0; i < 5; i++)
  {
    if(mb->players[i] != 0 && mb->players[i] != getpid())
    {
      kill(mb->players[i], SIGUSR1);
    }
  }
  }

  Map goldMine((const unsigned char*)mb->map, mb->rows, mb->cols);
  ptr = &goldMine;
  sleep(2);
  goldMine.postNotice("Welcome to GoldChase!");

  while(true)
  {
    kill(mb->deamonID, SIGUSR1);
    input = goldMine.getKey();
    if(input == 'Q')
    {
      exitFunction();
      return 0;
    }
    sem_wait(sem);
    processInput(input);
    kill(mb->deamonID, SIGUSR1);
    goldMine.drawMap();
    sem_post(sem);
    if(mb->map[playerPosition]&G_GOLD && !mb->goldFound)
    {
      bool reached = false;
      goldMine.postNotice("Real Gold!");
      while(!reached)
      {
        input = goldMine.getKey();
        processInput(input);
        goldMine.drawMap();
        if(playerPosition/mb->cols == 0 || playerPosition/mb->cols == mb->rows-1)
          reached = true;
        if(playerPosition%mb->cols == 0 || playerPosition%mb->cols == mb->cols-1)
          reached = true;
        if(reached)
        {
          std::stringstream ss;
          string mess;
          ss << "Player " << currentPlayer+1 << " has won!";
          mess = ss.str();
          if(mb->players[0] != 0 && currentPlayer!=0)
          {
            mq_name = "/aditya0";
            write_queue(mess);
          }
          if(mb->players[1] != 0 && currentPlayer!=1)
          {
            mq_name = "/aditya1";
            write_queue(mess);
          }
          if(mb->players[2] != 0 && currentPlayer!=2)
          {
            mq_name = "/aditya2";
            write_queue(mess);
          }
          if(mb->players[3] != 0 && currentPlayer!=3)
          {
            mq_name = "/aditya3";
            write_queue(mess);
          }
          if(mb->players[4] != 0 && currentPlayer !=4)
          {
            mq_name = "/aditya4";
            write_queue(mess);
          }
        }

        else if(input == 'Q')
        {
          exitFunction();
          return 0;
        }
      }
      goldMine.postNotice("You Won!!!");
      mb->goldFound = true;
      exitFunction();
      return 0;
    }
    else if(mb->map[playerPosition]&G_FOOL)
    {
      goldMine.postNotice("Fool's Gold here!!!...");
    }
  }

  return 0;
}

/////////////////////////////////////////////////////////////////
//function to read in board from mymap.txt. FUnction returns number of gold
//readboard()
/////////////////////////////////////////////////////////////////
int readBoard()
{
  int Gold;
  noOfRows = 0;
  string tempboard;
  string buffer;
  ifstream boardFile;
  boardFile.open("mymap.txt");
  if(!boardFile)
  {
    cout << "Could not open autoToprog.txt" << endl;
    return -1;
  }
  getline(boardFile, buffer);
  Gold = atoi(buffer.c_str());
  getline(boardFile, buffer);
  tempboard.append(buffer);
  noOfRows++;
  noOfCols = buffer.length();
  while(getline(boardFile, buffer))
  {
    tempboard.append(buffer);
    noOfRows++;
  }
  boardFile.close();
  board = new char[(noOfCols*noOfRows)];
  for(int i = 0; i<(noOfRows*noOfCols); ++i)
  {
    if(tempboard[i]==' ')
      board[i]=0;
    else if(tempboard[i]=='*')
      board[i]=G_WALL; //A wall
    else if(tempboard[i]=='1')
      board[i]=G_PLR0; //The first player
    else if(tempboard[i]=='2')
      board[i]=G_PLR1; //The second player
    else if(tempboard[i]=='G')
      board[i]=G_GOLD; //Real gold!
    else if(tempboard[i]=='F')
      board[i]=G_FOOL; //Fool's gold
  }
  return Gold;
}

/////////////////////////////////////////////////////////////////
//initialize()
//This function opens the shared memory for players
/////////////////////////////////////////////////////////////////
int initialize()
{
  shm_fd=shm_open("/AMJ_mymap",O_CREAT|O_RDWR,
  S_IRUSR|S_IWUSR); //read in file to determine # rows & cols
  return shm_fd;
}

/////////////////////////////////////////////////////////////////
//int generateRandom()
//This function returns a random number which is between 0 and rows*cols
/////////////////////////////////////////////////////////////////
int generateRandom()
{
  return rand()%(mb->rows*mb->cols);
}

/////////////////////////////////////////////////////////////////
//createFirstMap()
//This function is called only by the first player to store data
// shared memory
/////////////////////////////////////////////////////////////////
void createFirstMap()
{
  sem_wait(sem);
  noOfGold = readBoard();
  ftruncate(shm_fd, (noOfRows*noOfCols)+sizeof(mapBoard));
  mb=(mapBoard*)mmap(NULL,(noOfRows*noOfCols)+sizeof(mapBoard), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
  mb->rows = noOfRows;
  mb->cols = noOfCols;
  for(int i = 0; i < (noOfCols*noOfRows); i++)
    mb->map[i] = board[i];
if(noOfGold >=1)
{
  for(int i = 0; i < noOfGold-1; i++)
  {
    int randomNum = generateRandom();
    while(mb->map[randomNum] !=0)
      randomNum = generateRandom();
    mb->map[randomNum]= mb->map[randomNum]|G_FOOL;
  }
  while(mb->map[randomNum] !=0)
    randomNum = generateRandom();
  mb->map[randomNum]= mb->map[randomNum]|G_GOLD;
  mb->goldFound = false;

}
  sem_post(sem);
}

/////////////////////////////////////////////////////////////////
//setplayer()
//This function sets the bit in the players variable in the shared memory
// structure
/////////////////////////////////////////////////////////////////
void setPlayer()
{
  currentPlayer = -1;
  int position = generateRandom();
  while(mb->map[position] != 0)
    position = generateRandom();
  playerPosition = position;
  bool checker = false;
  for(int i = 0; i < 5; i++)
  {
    if(mb->players[i] == 0)
    {
      currentPlayer = i;
      checker = true;
      mb->players[i] = getpid();
      break;
    }
  }
  if(currentPlayer == 0)
  {
    thisPlayer = G_PLR0;
    mb->map[position]|=G_PLR0;
    for(int i = 0; i < 5; i++)
    {
      if(mb->players[i] != 0)
        kill(mb->players[i], SIGUSR1);
        kill(mb->players[i], SIGHUP);
    }
    mq_name = "/aditya0";
    read_queue();
    return;
  }
  else if(currentPlayer == 1)
  {
    thisPlayer = G_PLR1;
    mb->map[position]|=G_PLR1;
    for(int i = 0; i < 5; i++)
    {
      if(mb->players[i] != 0)
        kill(mb->players[i], SIGUSR1);
        kill(mb->players[i], SIGHUP);

    }
    mq_name = "/aditya1";
    read_queue();
    return;
  }
  else if(currentPlayer == 2)
  {
    thisPlayer = G_PLR2;
    mb->map[position]|=G_PLR2;
    for(int i = 0; i < 5; i++)
    {
      if(mb->players[i] != 0)
        kill(mb->players[i], SIGUSR1);
        kill(mb->players[i], SIGHUP);

    }
    mq_name = "/aditya2";
    read_queue();
    return;
  }
  else if(currentPlayer == 3)
  {
    thisPlayer = G_PLR3;
    mb->map[position]|=G_PLR3;
    for(int i = 0; i < 5; i++)
    {
      if(mb->players[i] != 0)
        kill(mb->players[i], SIGUSR1);
        kill(mb->players[i], SIGHUP);

    }
    mq_name = "/aditya3";
    read_queue();
    return;
  }
  else if(currentPlayer == 4)
  {
    thisPlayer = G_PLR4;
    mb->map[position]|=G_PLR4;
    for(int i = 0; i < 5; i++)
    {
      if(mb->players[i] != 0)
        kill(mb->players[i], SIGUSR1);
        kill(mb->players[i], SIGHUP);

    }
    mq_name = "/aditya4";
  //  read_queue();
    return;
  }

  if(checker == false)
  {
    cout << "Too many players\n";
    exit(1);
  }
  /*if(!(mb->players&G_PLR0))
  {
    thisPlayer=G_PLR0;
    mb->players|=G_PLR0;
    mb->map[position]|=G_PLR0;
    return;
  }
  else if(!(mb->players&G_PLR1))
  {
    thisPlayer=G_PLR1;
    mb->players|=G_PLR1;
    mb->map[position]|=G_PLR1;
    return;
  }
  else if(!(mb->players&G_PLR2))
  {
    thisPlayer=G_PLR2;
    mb->players|=G_PLR2;
    mb->map[position]|=G_PLR2;
    return;
  }
  else if(!(mb->players&G_PLR3))
  {
    thisPlayer=G_PLR3;
    mb->players|=G_PLR3;
    mb->map[position]|=G_PLR3;
    return;
  }
  else if(!(mb->players&G_PLR4))
  {
    thisPlayer=G_PLR4;
    mb->players|=G_PLR4;
    mb->map[position]|=G_PLR4;
    return;
  }*/
  //cout << "Too many players. Please try again later!...";
}

/////////////////////////////////////////////////////////////////
//processInput()
//This function is used to process input and move the player accordingly
/////////////////////////////////////////////////////////////////
void processInput(int input)
{
  switch(input)
  {
    case 'h':
      if((playerPosition%mb->cols) > 0 && (mb->map[playerPosition-1] != G_WALL))
      {
        mb->map[playerPosition]&=(~thisPlayer);
        mb->map[playerPosition-1]|=thisPlayer;
        playerPosition--;
        for(int i = 0; i < 5; i++)
        {
          if(mb->players[i] != 0)
            kill(mb->players[i], SIGUSR1);
        }
      }
      break;
    case 'l':
      if((playerPosition%mb->cols) != mb->cols-1 && mb->map[playerPosition+1] != G_WALL)
      {
        mb->map[playerPosition]&=(~thisPlayer);
        mb->map[playerPosition+1]|=thisPlayer;
        playerPosition++;
        for(int i = 0; i < 5; i++)
        {
          if(mb->players[i] != 0)
            kill(mb->players[i], SIGUSR1);
        }
      }
      break;
    case 'j':
      if(playerPosition < (mb->rows-1)*mb->cols && mb->map[playerPosition+mb->cols] != G_WALL)
      {
        mb->map[playerPosition]&=(~thisPlayer);
        mb->map[playerPosition+mb->cols]|=thisPlayer;
        playerPosition+=mb->cols;
        for(int i = 0; i < 5; i++)
        {
          if(mb->players[i] != 0)
            kill(mb->players[i], SIGUSR1);
        }
      }
      break;
    case 'k':
      if(playerPosition > mb->cols && mb->map[playerPosition-mb->cols] != G_WALL)
      {
        mb->map[playerPosition]&=(~thisPlayer);
        mb->map[playerPosition-mb->cols]|=thisPlayer;
        playerPosition-=mb->cols;
        for(int i = 0; i < 5; i++)
        {
          if(mb->players[i] != 0)
            kill(mb->players[i], SIGUSR1);
        }
      }
      break;
    case 'm':
      {unsigned int temp = 0;
      if(mb->players[0] != 0)
        temp|=G_PLR0;
      if(mb->players[1] != 0)
        temp|=G_PLR1;
      if(mb->players[2] != 0)
        temp|=G_PLR2;
      if(mb->players[3] != 0)
        temp|=G_PLR3;
      if(mb->players[4] != 0)
        temp|=G_PLR4;

      temp&=~thisPlayer;
      unsigned int inp = ptr->getPlayer(temp);

      if(inp == G_PLR0)
        mq_name = "/aditya0";
      else if(inp == G_PLR1)
        mq_name = "/aditya1";
      else if(inp == G_PLR2)
        mq_name = "/aditya2";
      else if(inp == G_PLR3)
        mq_name = "/aditya3";
      else if(inp == G_PLR4)
        mq_name = "/aditya4";
      std::stringstream ss;
      string mess = ptr->getMessage();
      ss << "Player " << currentPlayer+1 << " says: " << mess;
      mess = ss.str();
      write_queue(mess);
      break;}
    case 'b':
      {std::stringstream ss;
      string mess = ptr->getMessage();
      ss << "Player " << currentPlayer+1 << " says: " << mess;
      mess = ss.str();
      if(mb->players[0] != 0 && currentPlayer!=0)
      {
        mq_name = "/aditya0";
        write_queue(mess);
      }
      if(mb->players[1] != 0 && currentPlayer!=1)
      {
        mq_name = "/aditya1";
        write_queue(mess);
      }
      if(mb->players[2] != 0 && currentPlayer!=2)
      {
        mq_name = "/aditya2";
        write_queue(mess);
      }
      if(mb->players[3] != 0 && currentPlayer!=3)
      {
        mq_name = "/aditya3";
        write_queue(mess);
      }
      if(mb->players[4] != 0 && currentPlayer !=4)
      {
        mq_name = "/aditya4";
        write_queue(mess);
      }
      break;}
  }
}

/////////////////////////////////////////////////////////////////
//exitFunction()
//This function is used when the last player exits the game to destroy and
//unlink semaphore and shared memory
/////////////////////////////////////////////////////////////////
void exitFunction()
{
  sem_wait(sem);
  bool checker = true;
  mb->players[currentPlayer] = 0;
  for(int i = 0; i < 5; i++)
  {
    if(mb->players[i] != 0)
      checker = false;
  }

  mb->map[playerPosition]&=(~thisPlayer);
  /*mb->players&=(~thisPlayer);
  write(99, &mb->players, getpid());*/
  for(int i = 0; i < 5; i++)
  {
    if(mb->players[i] != 0)
      kill(mb->players[i], SIGUSR1);
  }
  mq_close(readqueue_fd);
  if(currentPlayer == 0)
    mq_unlink("/aditya0");
  if(currentPlayer == 1)
    mq_unlink("/aditya1");
  if(currentPlayer == 2)
    mq_unlink("/aditya2");
  if(currentPlayer == 3)
    mq_unlink("/aditya3");
  if(currentPlayer == 4)
    mq_unlink("/aditya4");
  //kill(mb->deamonID, SIGHUP);
  sem_post(sem);
  //if(mb->players == 0)
  if(checker)
  {
    sem_close(sem);
    sem_unlink("/mySEM");
    shm_unlink("/AMJ_mymap");
  }
}
// end of file
