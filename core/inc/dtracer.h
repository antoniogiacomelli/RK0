#ifndef DTRACER_H
#define DTRACKER_H
#include <kservices.h>



typedef struct dtracer
{
    RK_TICK time;
    CHAR *const type;
    RK_TASK_HANDLE running;


};
