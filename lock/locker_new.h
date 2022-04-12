#ifdef LOCKER_H
#define LOCKER_H

#include<exception>
#include<pthread.h>
#include<semaphore.h>
/*用RAII思想封装信号量*/
class sem{
public:
    // 创建并初始化信号量
    sem(){
        if(sem_init(&m_sem, 0, 0) != 0){
            // 如果构造函数没有返回值
            throw std::exception();
        }
    }
    sem(int num){
        if(sem_init(&m_sem, 0, num) != 0){
            // 如果构造函数没有返回值
            throw std::exception();
        }
    }
    ~sem(){
        sem_destroy(&m_sem);
    }
    
    bool wait(){
        //信号-1，为0阻塞
        return sem_wait(&m_sem) == 0;
    }

    bool post(){
        // 大于0唤醒调用线程
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};
/*互斥锁，互斥锁*/
class locker{
public:
    locker(){
        if(pthread_mutex_init(&m_mutex,NULL) != 0){
            throw std::exception();
        }
    }
    ~locker(){
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock(){
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    bool unlock(){
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    pthread_mutex_t *get(){
        return &m_mutex;
    }
private:
    pthread_mutex_t m_mutex;
}

// 条件变量 用于同一个线程之间的
class cond{
public:
    cond(){
        if(pthread_cond_init(&m_cond,NULL) != 0){
            throw std::exception();
        }
    }
    ~cond(){
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t *m_mutex){
        return pthread_cond_wait(&m_cond, m_mutex) == 0;
    }
    //??? 用来干么的
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t){
        return pthread_cond_timedwait(&m_cond, m_mutex, &t) == 0; 
    }
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool broadcast(){
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    pthread_cond_t m_cond;
};
#endif