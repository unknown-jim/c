#include "thread_pool.h"

static int curID = 1000;

//往任务队列中增加任务
void addTask(threadPool *thr_pool)
{
    pthread_mutex_lock(&thr_pool->pool_lock);

    while (thr_pool->max_task_num <= thr_pool->task_num)
    {
        pthread_cond_wait(&thr_pool->not_full, &thr_pool->pool_lock);
    }

    thr_pool->tasks[thr_pool->task_push].taskid = curID++;
    thr_pool->tasks[thr_pool->task_push].task_func = taskRun;
    thr_pool->tasks[thr_pool->task_push].arg = NULL;
    thr_pool->task_push = (thr_pool->task_push + 1) % thr_pool->max_task_num;

    thr_pool->task_num += 1;

    pthread_mutex_unlock(&thr_pool->pool_lock);
    pthread_cond_signal(&thr_pool->not_empty);
}

//子线程的运行函数
void* thrRun(void *arg)
{
    threadPool *thr_pool = (threadPool *)arg;
    poolTask task;

    while(1)
    {
        pthread_mutex_lock(&thr_pool->pool_lock);

        while (thr_pool->task_num < 1 && thr_pool->survive)
        {
            pthread_cond_wait(&thr_pool->not_empty, &thr_pool->pool_lock);
        }

        if (thr_pool->task_num > 0)
        {
            int taskPos = thr_pool->task_pop;
            thr_pool->task_pop = (thr_pool->task_pop + 1) % thr_pool->max_task_num;

            memcpy(&task, thr_pool->tasks + taskPos, sizeof(task));
            thr_pool->task_num -= 1;

            pthread_mutex_unlock(&thr_pool->pool_lock);
            pthread_cond_signal(&thr_pool->not_full);

            printf("pthread %ld get task %d\n", pthread_self(), task.taskid);

            task.arg = &task;
            task.task_func(task.arg);
        }
        else
        {
            pthread_mutex_unlock(&thr_pool->pool_lock);
        }

        if (!thr_pool->survive)
        {
            pthread_exit(NULL);
        }
    }
}

//线程池初始化函数
threadPool *create_threadPool(int thr_num, int max_task_num)
{
    threadPool *thr_pool = (threadPool *)malloc(sizeof(threadPool));
    thr_pool->survive = 1;

    thr_pool->max_task_num = max_task_num;
    thr_pool->task_num = 0;

    thr_pool->tasks = (poolTask *)malloc(max_task_num * sizeof(poolTask));
    thr_pool->task_push = 0;
    thr_pool->task_pop = 0;

    thr_pool->thr_num = thr_num;
    thr_pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thr_num);

    //必须先初始化条件变量，否则子线程初始化后pthread_cond_wait()接收不到信号，子线程会直接永久阻塞
    pthread_mutex_init(&thr_pool->pool_lock, NULL);
    pthread_cond_init(&thr_pool->not_full, NULL);
    pthread_cond_init(&thr_pool->not_empty, NULL);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    //pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    for (int i = 0; i < thr_num; i += 1)
    {
        pthread_create(thr_pool->threads + i, &attr, thrRun, (void *)thr_pool);
    }

    return thr_pool;
}

//任务队列中任务的回调函数
void taskRun(void* arg)
{
    poolTask *task = (poolTask *)arg;
    sleep(1);
    printf("task %d completed\n", task->taskid);
}

//销毁线程池
void del_threadPool(threadPool* thr_pool)
{
    thr_pool->survive = 0;
    pthread_cond_broadcast(&thr_pool->not_empty);

    for (int i = 0; i < thr_pool->thr_num; i += 1)
    {
        pthread_join(thr_pool->threads[i], NULL);
    }

    pthread_mutex_destroy(&thr_pool->pool_lock);
    pthread_cond_destroy(&thr_pool->not_empty);
    pthread_cond_destroy(&thr_pool->not_full);

    free(thr_pool->tasks);
    free(thr_pool->threads);
    free(thr_pool);
}

int main(const int argc, const char **argv)
{
    threadPool *thr_pool = create_threadPool(3, 20);

    for (int i = 0; i < 50; i += 1)
    {
        addTask(thr_pool);
    }

    sleep(10);

    del_threadPool(thr_pool);

    return 0;
}
