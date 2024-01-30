#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>

// 任务结构体
typedef struct
{
    void *arg;
    void *(*taskFunc)(void *);
} task_t;

// 线程池结构体
typedef struct
{
    int survive; // 线程池是否依然应该存活

    pthread_mutex_t lock;             // 管理整个线程池的锁
    pthread_mutex_t thr_busyNum_lock; // 管理忙碌线程数量的锁

    pthread_cond_t not_full;  // 工作线程通知主线程已接收一个任务的条件变量
    pthread_cond_t not_empty; // 主线程通知工作线程已发布一个新任务的条件变量

    int task_num;          // 当前任务数量
    int wait_task_num; // 应增加线程等待任务数
    int max_task_num;      // 最大任务数量
    int task_pop;          // 模拟循环队列的队头
    int task_push;         // 模拟循环队列的队尾
    task_t *tasks;         // 任务组成的数组

    int min_thr_num;      // 最小线程数量
    int max_thr_num;      // 最大线程数量
    int live_thr_num;     // 当前线程数量
    int busy_thr_num;     // 忙碌线程数量
    int wait_del_thr_num; // 等待注销的线程数量
    pthread_t *threads;   // 工作线程组成的数组
    pthread_t manager;    // 管理线程
} thread_pool;

const static int ADJUST_TIME = 10;
const static int THREAD_VARY = 10;

void *thr_manager(void *arg);
void *thr_worker(void *arg);
int thread_pool_free(thread_pool *thr_pool);
thread_pool *thread_pool_init(int min_thr_num, int max_thr_num, int wait_task_num, int max_task_num);
void task_add(thread_pool *thr_pool, void *(*task_func)(void *), void *arg);
void *task_job(void *arg);
int thread_pool_destroy(thread_pool *thr_pool);

#endif