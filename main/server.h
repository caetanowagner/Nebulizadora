#ifndef _SERVER_H
#define _SERVER_H

typedef struct {
  uint32_t TotalOperHours;
  uint32_t PartialOperHours;
  uint32_t NumStartsPerHour;
  uint32_t DelayTime;
  uint32_t MaxStartsPerHour;
} NVSParams;

NVSParams histData;

void RegisterEndPoints(void);
void getStorageData();
void setStorageData();
void initHistData(void);

#endif


/*

 //main.h

typedef struct
{
    double v;
    int i;
 }foo;


//extern.h

extern foo fo;


//main.c

#include "main.h"
#include "extern.h"
//use fo.v here


//second.c

#include "second.h"
#include "main.h"
#include "extern.h"
foo fo;
//use fo.v here

Just include

#include "main.h"
#include "extern.h"

in all .c files you want to use fo.
Notice that foo fo is only in second.c,
nowhere else

 */
