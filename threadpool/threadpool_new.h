#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<list>
#include<cstdio>
#include<exception>
#include<pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
    threadpool(connection_pool * connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T *request);
private:
    static void * worker(void *arg);
    void run();

    int m_thread_number; // /线程池中的线程数
    int m_max_request; // 请求队列中允许的最大请求数
    pthread_t *m_threads; // 描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; // 用list(底层是双向链表)做请求队列
    locker m_queuelocker; // 保护请求队列的互斥锁
    sem m_queuestat; // 是否有任务需要处理
    bool m_stop; // 是否结束线程
    connection_pool* m_connPool; // 数据库
};
template<typename T>
threadpool<T>::threadpool(connection_pool* connPool, int thread_number, int max_requests):m_thread_number(thread_number),m_max_requests(max_requests), m_stop(false), m_threads(nullptr), m_connPool(connPool){
    if(thread_number <=0 || max_requests <= 0)
        throw std::exception();// 如果线程数量非法 抛出异常
    if(!m_threads)
        throw std::exception();// 如果指针为空
    for(int i = 0; i < thread_number; ++i){
        cout<<"create the "<<i<<"th thread"<<endl;
        if(pthread_create(m_threads + i, NULL, worker, this) != 0){
            // 如果创建失败，删除数组
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach()){
            // 将线程设置为脱离态，如果设置失败直接返回
            deletep[] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool(){
    delete[] m_threads;
    m_stop = true;
}

template <typename T>
bool threadpool<T>::append(T *request){
    // 在请求队列中，每一次需要判断当前请求队列的大小于最大请求书的关系。
    m_queuelocker.lock();
    if(m_workkque.size() > m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();// 需要处理的任务数量+1，信号量+1；
    return true;
}

template <typename T>
void* threadpool<T>::worker(void *arg){
    threadpool* pool = (threadpool*) arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run(){
    while(!m_stop){
        // 需要处理的任务数量-1，信号量-1
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty()){
            // 如果说此时队列已经为空了直接解锁
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request) continue;

        // 数据库链接操作
        connectionRAII mysqlcon(&request->mysql, m_connPool);
        request->process();
    }
}


#endif