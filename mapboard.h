#ifndef MAPBOARD_H
#define MAPBOARD_H

struct mapBoard
{
  int rows;
  int cols;
  bool goldFound;
  pid_t players[5];
  int deamonID = 0;
  //unsigned char players;
  unsigned char map[0];
};

void runServerDeamon();
void runClientDeamon(char *ipAddr);

#endif
