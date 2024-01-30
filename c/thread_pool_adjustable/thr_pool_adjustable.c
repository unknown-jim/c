#include "thr_pool_adjustable.h"

//工作线程数量管理者线程执行的函数
void *thr_manager(void *arg)
{
    thread_pool *thr_pool = (thread_pool *)arg;

    while (thr_pool->survive)
    {
        sleep(ADJUST_TIME);//每隔一段时间调整一次线程数量，并且一次性增加或减少固定数额的数量

        pthread_mutex_lock(&thr_pool->lock);
        int live_thr_num = thr_pool->live_thr_num;
        int task_num = thr_pool->task_num;
        pthread_mutex_unlock(&thr_pool->lock);

        pthread_mutex_lock(&thr_pool->thr_busyNum_lock);
        int busy_thr_num = thr_pool->busy_thr_num;
        pthread_mutex_unlock(&thr_pool->thr_busyNum_lock);

        // 如果线程数量还没到最大值，并且任务队列中的任务达到了应增加线程的数量就会增加线程
        if (task_num > thr_pool->wait_task_num && live_thr_num < thr_pool->max_thr_num)
        {
            pthread_mutex_lock(&thr_pool->lock);
            int add = 0;

            //在线程池的线程数组中寻找空闲线程位置创建线程
            for (int i = 0; i < thr_pool->max_thr_num && add < THREAD_VARY && thr_pool->live_thr_num < thr_pool->max_thr_num; i += 1)
            {
                // 如果线程号已经初始化过，并且还没有退出，就不可以用来创建新的线程
                // pthread_kill（pthread_t, 0）不可以用来检测资源已经被回收的线程
                if (thr_pool->threads[i] != 0 && pthread_kill(thr_pool->threads[i], 0) == ESRCH)
                    continue;

                pthread_create(thr_pool->threads + i, NULL, thr_worker, arg);
                add += 1;
                thr_pool->live_thr_num += 1;
                printf("thread %ld created\n", thr_pool->threads[i]);
            }

            pthread_mutex_unlock(&thr_pool->lock);
        }

        //如果线程数量还没到最小值，并且超过半数的线程都没有工作就会减少线程
        else if (2 * busy_thr_num < live_thr_num && live_thr_num > thr_pool->min_thr_num)
        {
            pthread_mutex_lock(&thr_pool->lock);
            thr_pool->wait_del_thr_num = THREAD_VARY;
            pthread_mutex_unlock(&thr_pool->lock);

            for (int i = 0; i < THREAD_VARY; i += 1)
            {
                pthread_cond_signal(&thr_pool->not_empty);
            }
        }
    }

    pthread_detach(pthread_self());
    pthread_exit(NULL);
}

//工作者线程执行的函数
void *thr_worker(void *arg)
{
    thread_pool *thr_pool = (thread_pool *)arg;
    task_t task;

    while (1)
    {
        pthread_mutex_lock(&thr_pool->lock);

        //没有任务并且线程池存活时阻塞
        while (thr_pool->task_num < 1 && thr_pool->survive == 1)
        {
            pthread_cond_wait(&thr_pool->not_empty, &thr_pool->lock);

            //如果管理者线程要减少线程数量，就退出
            if (thr_pool->wait_del_thr_num > 0)
            {
                thr_pool->wait_del_thr_num -= 1;

                if (thr_pool->live_thr_num > thr_pool->min_thr_num)
                {
                    printf("thread %ld waited too long to be deleted\n", pthread_self());
                    thr_pool->live_thr_num -= 1;
                    pthread_mutex_unlock(&thr_pool->lock);
                    pthread_exit(NULL);
                }
            }
        }

        //如果线程池不再存活，就退出
        if (!thr_pool->survive)
        {
            thr_pool->live_thr_num -= 1;
            pthread_mutex_unlock(&thr_pool->lock);
            printf("thread %ld is exiting\n", pthread_self());
            pthread_exit(NULL);
        }

        //将线程池中的任务取出并复制
        int task_pos = thr_pool->task_pop;
        task.taskFunc = thr_pool->tasks[task_pos].taskFunc;
        task.arg = thr_pool->tasks[task_pos].arg;

        thr_pool->task_pop = (task_pos + 1) % thr_pool->max_task_num;
        thr_pool->task_num -= 1;
        
        //任务数量减少后就可以解锁
        pthread_mutex_unlock(&thr_pool->lock);
        pthread_cond_signal(&thr_pool->not_full);

        //工作中的线程数量增加
        pthread_mutex_lock(&thr_pool->thr_busyNum_lock);
        thr_pool->busy_thr_num += 1;
        pthread_mutex_unlock(&thr_pool->thr_busyNum_lock);

        task.taskFunc(task.arg);

        //工作中的线程数量减少
        printf("thread %ld completed task %d\n", pthread_self(), *(int *)task.arg);
        pthread_mutex_lock(&thr_pool->thr_busyNum_lock);
        thr_pool->busy_thr_num -= 1;
        pthread_mutex_unlock(&thr_pool->thr_busyNum_lock);
    }
}

//释放线程池的资源
int thread_pool_free(thread_pool *thr_pool)
{
    //如果是空指针就直接退出
    if (!thr_pool)
    {
        return -1;
    }

    //如果任务队列已经初始化就释放任务队列
    if (thr_pool->tasks)
    {
        free(thr_pool->tasks);
    }

    // 如果线程数组已经初始化就释放线程数组
    if (thr_pool->threads)
    {
        //释放所有创建过的线程的资源
        for (int i = 0; i < thr_pool->max_thr_num && thr_pool->threads[i] != 0; i += 1)
        {
            pthread_join(thr_pool->threads[i], NULL);
        }
        free(thr_pool->threads);

        //释放锁和条件变量
        pthread_mutex_lock(&thr_pool->lock);
        pthread_mutex_destroy(&thr_pool->lock);
        pthread_mutex_lock(&thr_pool->thr_busyNum_lock);
        pthread_mutex_destroy(&thr_pool->thr_busyNum_lock);
        pthread_cond_destroy(&thr_pool->not_empty);
        pthread_cond_destroy(&thr_pool->not_full);
    }

    free(thr_pool);
    thr_pool = NULL;

    return 0;
}

//线程池初始化
thread_pool *thread_pool_init(int min_thr_num, int max_thr_num, int wait_task_num, int max_task_num)
{
    thread_pool *thr_pool = NULL;

    //如果初始化过程中任意步骤失败，就可以提前退出这个不会重复的循环
    do
    {
        thr_pool = (thread_pool *)malloc(sizeof(thread_pool));
        if (thr_pool == NULL)
        {
            perror("thread_pool malloc: ");
            break;
        }
        thr_pool->survive = 1;

        if (
            pthread_mutex_init(&thr_pool->lock, NULL) != 0 ||
            pthread_mutex_init(&thr_pool->thr_busyNum_lock, NULL) != 0 ||
            pthread_cond_init(&thr_pool->not_empty, NULL) != 0 ||
            pthread_cond_init(&thr_pool->not_full, NULL) != 0)
        {
            perror("mutex or cond init :");
            break;
        }

        thr_pool->wait_task_num = wait_task_num;
        thr_pool->max_task_num = max_task_num;
        thr_pool->task_num = 0;
        thr_pool->task_pop = 0;
        thr_pool->task_push = 0;
        thr_pool->tasks = (task_t *)malloc(max_task_num * sizeof(task_t));
        if (thr_pool->tasks == NULL)
        {
            perror("tasks malloc: ");
            break;
        }

        thr_pool->min_thr_num = min_thr_num;
        thr_pool->live_thr_num = min_thr_num;
        thr_pool->max_thr_num = max_thr_num;
        thr_pool->busy_thr_num = 0;
        thr_pool->wait_del_thr_num = 0;
        thr_pool->threads = (pthread_t *)calloc(max_thr_num, sizeof(pthread_t));
        if (thr_pool->threads == NULL)
        {
            perror("threads calloc: ");
            break;
        }

        for (int i = 0; i < min_thr_num; i += 1)
        {
            pthread_create(thr_pool->threads + i, NULL, thr_worker, (void *)thr_pool);
            printf("thread %ld created\n", thr_pool->threads[i]);
        }

        pthread_create(&thr_pool->manager, NULL, thr_manager, (void *)thr_pool);

        return thr_pool;
    } while (0);
    
    //如果初始化失败就直接释放已初始化的资源
    thread_pool_free(thr_pool);
    return NULL;
}

//主线程发布任务的函数
void task_add(thread_pool *thr_pool, void *(*task_func)(void *), void *arg)
{
    pthread_mutex_lock(&thr_pool->lock);

    //任务队列已满时阻塞
    while (thr_pool->task_num >= thr_pool->max_task_num)
    {
        pthread_cond_wait(&thr_pool->not_full, &thr_pool->lock);
    }

    thr_pool->tasks[thr_pool->task_push].taskFunc = task_func;
    thr_pool->tasks[thr_pool->task_push].arg = arg;
    thr_pool->task_push = (thr_pool->task_push + 1) % thr_pool->max_task_num;
    thr_pool->task_num += 1;

    pthread_mutex_unlock(&thr_pool->lock);
    pthread_cond_signal(&thr_pool->not_empty);
}

//每一个任务的回调函数
void *task_job(void *arg)
{
    printf("thread %ld conducting task %d\n", pthread_self(), *(int *)arg);
    sleep(1);

    return NULL;
}

//将线程池销毁的函数
int thread_pool_destroy(thread_pool *thr_pool)
{
    // 如果是空指针就直接退出
    if (thr_pool == NULL)
    {
        return -1;
    }

    //存活状态置0
    thr_pool->survive = 0;

    //通知每一个还没退出的工作线程退出
    int live_thr_num = thr_pool->live_thr_num;
    for (int i = 0; i < live_thr_num; i += 1)
    {
        pthread_cond_broadcast(&thr_pool->not_empty);
    }

    thread_pool_free(thr_pool);
    return 0;
}

int main()
{
    thread_pool *thr_pool = thread_pool_init(3, 100, 10, 100);
    if (thr_pool == NULL)
    {
        return -1;
    }

    //每隔一段时间，连续发布60个任务，总共10段
    for (int i = 0, num[300]; i < 10; i += 1)
    {
        for (int j = 0; j < 60; j += 1)
        {
            num[i * 10 + j] = i * 60 + j;
            printf("added task %d\n", i * 10 + j);
            task_add(thr_pool, task_job, (void *)(num + i * 10 + j));
        }
        sleep(20);
    }

    printf("remain %d thread\n", thr_pool->live_thr_num);
    thread_pool_destroy(thr_pool);
    sleep(10);

    return 0;
}