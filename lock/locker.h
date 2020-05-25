#ifndef LOCKER_H
#define LOCKER_H

#include<exception>
#include<pthread.h>
#include<semaphore.h>

//一个简单的信号量类
class sem
{
public:
	sem() {
		if (sem_init(&m_sem, 0, 0) != 0)
			throw std::exception;
	}
	sem(int num) {
		if (sum_init(&m_sem, 0, num) != 0)
			throw std::exception;
	}
	~sem() {
		sem_destroy(&m_sem);
	}

	//加锁与解锁
	bool wait() {
		return sem_wait(&m_sem) == 0;
	}
	bool post() {
		return sem_post(&m_sem) == 0;
	}

private:
	sem_t m_sem;
};


//简单的互斥锁
class locker
{
public:
	locker() {
		if (pthread_mutex_init(&m_mutex,NULL) != 0)
			throw std::exception;
	}
	~locker() {
		pthread_mutex_destroy(&m_mutex);
	}

	bool lock() {
		return pthread_mutex_lock(&m_mutex) == 0;
	}
	bool unlock() {
		return pthread_mutex_unlock(&m_mutex) == 0;
	}
	pthread_mutex_t* get() {
		return &m_mutex;
	}

private:
	pthread_mutex_t m_mutex;
};


//简单的条件变量
class cond
{
public:
	cond() {
		if (pthread_cond_init(&m_cond, NULL) != 0)
			throw std::exception;
	}
	~cond() {
		pthread_cond_destroy(&m_cond);
	}

	//设置条件变量
	bool wait(pthread_mutex_t* m_mutex) {
		return pthread_cond_wait(&m_cond, m_mutex);
	}
	bool timewait(pthread_mutex_t* m_mutex, struct timespec t) {
		return pthread_cond_timedwait(&m_cond, m_mutex, &t)==0;
	}
	//条件变量满足，唤醒阻塞在m_mutex互斥量的线程
	bool signal() {
		return pthread_cond_signal(&m_cond) == 0;
	}
	bool broadcast() {
		return pthread_cond_broadcast(&m_cond) == 0;
	}

private:
	pthread_cond_t m_cond;
};

#endif // !LOCKER_H
