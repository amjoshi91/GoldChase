/*
 * write template functions that are guaranteed to read and write the
 * number of bytes desired
 */

#ifndef FANCYRW_H
#define FANCYRW_H
#include<unistd.h>
#include <errno.h>
#include <iostream>
using std::cout;
using std::endl;

template<typename T>
int READ(int fd, T* obj_ptr, int count)
{
  char* addr=(char*)obj_ptr;
  int err = 0;
  int tempCount = count;
  while(count > 0)
  {
    err = read(fd, addr, count);
    if(err == -1 && errno==EINTR)
    {
      count = tempCount;
      continue;
    }
    if(err == -1)
    {
      return err;
    }
    addr+=err;
    count = count - err;
  }
  return tempCount;
  //loop. Read repeatedly until count bytes are read in
}

template<typename T>
int WRITE(int fd, T* obj_ptr, int count)
{
  char* addr=(char*)obj_ptr;
  int tempCount = count;
  int err = 0;
  while(count > 0)
  {
    err = write(fd, addr, count);
    if(err == -1 && errno==EINTR)
    {
      count = tempCount;
      continue;
    }
    if(err == -1)
    {
      return err;
    }
    addr+=err;
    count = count - err;
  }
  return tempCount;
  //loop. Write repeatedly until count bytes are written out
}
#endif

//Example of a method for testing your functions shown here:
//opened/connected/etc to a socket with fd=7
//
/*
int count=write(7, "Todd Gibson", 11);
cout << count << endl;//will display a number between 1 and 11
int count=write(7, "d Gibson", 8);
//
//How to test your READ template. Read will be: READ(7, ptr, 11);
write(7, "To", 2);
write(7, "DD ", 3);
write(7, "Gibso", 5);
write(7, "n", 1);
*/
//
//
