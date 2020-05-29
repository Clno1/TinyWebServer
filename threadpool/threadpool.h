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
	/*thread_number���̳߳����̵߳�������max_requests������������������ġ��ȴ���������������*/
	threadpool(connection_pool* connPool, int thread_number = 8, int max_request = 10000);
	~threadpool();
	bool append(T* request);

private:
	/*�����߳����еĺ����������ϴӹ���������ȡ������ִ��֮*/
	static void* worker(void* arg);
	void run();

private:
	int m_thread_number;		//�̳߳��߳���
	int m_max_requests;			//������е����������

	pthread_t* m_threads;		//�̳߳�  ����
	std::list<T*> m_workqueue;		//����	����

	locker m_queuelocker;				//������еĻ�����
	sem m_queuestat;					//������е��ź���(���Կ���Ҫ�����������)

	bool m_stop;			//�̳߳ؽ�����־
	connection_pool* m_connPool;			//���ݿ����ӳ�
};

//�̳߳ع��캯��
template<typename T>
threadpool<T>::threadpool(connection_pool* connPool, int thread_number, int max_request) :
	m_thread_number(thread_number), m_max_requests(max_request), m_stop(false), m_threads(NULL), m_connPool(connPool) {
	if (thread_number <= 0 || max_request <= 0)	//��������߳������������������
		throw std::exception();
	m_threads = new pthread_t[m_thread_number];
	if (!m_threads)
		throw std::exception();
	//����thread_number���̲߳��Ҵ洢����
	for (int i = 0; i < thread_number; i++) {
		if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
			delete[] m_threads;		//ʧ��
			throw std::exception();
		}
		if (pthread_detach(m_threads[i])) {
			delete[] m_threads;		//ʧ��
			throw std::exception();
		}
	}
}

//�̳߳���������
template<typename T>
threadpool<T>::~threadpool() {
	delete[] m_threads;
	m_stop = true;
}

//�������칤�������뵽�������
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

//�̻߳ص�����/����������arg��ʵ��this
template<typename T>
void* threadpool<T>::worker(void *arg) {
	threadpool* pool = (threadpool*)arg;
	pool->run();
	return pool;
}

//�ص���������������������
//
template<typename T>
void threadpool<T>::run() {
	while (!m_stop) {
		//������г���--����������ס
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