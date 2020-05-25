#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<cstdio>
#include<pthread.h>
#include<exception>
#include<list>

#include "../lock/locker.h"
#include"../CGImysql/sql_connection_pool.h"

template<typename T>
class threadpool
{
public:
	/*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
	threadpool(connection_pool* connPool, int thread_number = 8, int max_request = 10000);
	~threadpool();
	bool append(T* request);

private:
	/*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
	static void* worker(void* arg);
	void run();

private:
	int m_thread_number;		//线程池线程数
	int m_max_requests;			//请求队列的最大请求数

	pthread_t* m_threads;		//线程池  数组
	std::list<T*> m_workqueue;		//请求	队列

	locker m_queuelocker;				//请求队列的互斥锁
	sem m_queuestat;					//请求队列的信号量(可以看出要处理的任务数)

	bool m_stop;			//线程池结束标志
	connection_pool* m_connPool;			//数据库连接池
};

//线程池构造函数
template<typename T>
threadpool<T>::threadpool(connection_pool* connPool, int thread_number, int max_request) :
	m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL), m_connPool(connPool) {
	if (thread_number <= 0 || max_request <= 0)	//不合理的线程数量和请求队列数量
		throw std::exception();
	m_threads = new pthread_t[m_thread_number];
	if (!m_threads)
		throw std::exception();
	//创造thread_number个线程并且存储起来
	for (int i = 0; i < thread_number; i++) {
		if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
			delete[] m_threads;		//失败
			throw std::exception();
		}
		if (pthread_detach(m_threads[i])) {
			delete[] m_threads;		//失败
			throw std::exception();
		}
	}
}

//线程池析构函数
template<typename T>
threadpool<T>::~threadpool() {
	delete[] m_threads;
	m_stop = true;
}

//将“待办工作”加入到请求队列
template<typename T>
bool threadpool<T>::append(T *request) {
	m_queuelocker.lock();
	if (m_workqueue.size() > m_max_requests) {
		m_queuelocker.unlock();
		return false;
	}
	m_workqueue.push_back(request);
	m_queuelocker.unlock();
	m_queuestat.post();
	return true;
}

//线程回调函数/工作函数，arg其实是this
template<typename T>
void* threadpool<T>::worker(void *arg) {
	threadpool* pool = (threadpool*)arg;
	pool->run();
	return pool;
}

//回调函数会调用这个函数工作
//
template<typename T>
void threadpool<T>::run() {
	while (!m_stop) {
		//请求队列长度--，互斥锁锁住
		m_queuestat.wait();
		m_queuelocker.lock();
		
		if (m_workqueue.empty()) {
			m_queuelocker.unlock();
			continue;
		}

		T* request = m_workqueue.front();
		m_workqueue.pop_front();
		
		m_queuelocker.unlock();

		if (!request) continue;

		//
		connectionRAII mysqlcon(&request->mysql, m_connPool);

		request->process();
	}
}


#endif // THREADPOOL_H