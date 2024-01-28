#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>

typedef struct
{
    int taskid;
    void *arg;
    void (*task_func)(void *arg);
} poolTask;

typedef struct
{
    int survive;

    int max_task_num;
    int task_num;

    poolTask *tasks;
    int task_push;
    int task_pop;

    int thr_num;
    pthread_t *threads;

    pthread_mutex_t pool_lock;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} threadPool;

threadPool *create_threadPool(int thr_num, int max_task_num);
void del_threadPool(threadPool *pool);
void addTask(threadPool *pool);
void taskRun(void *arg);
void *thrRun(void *arg);

#endif