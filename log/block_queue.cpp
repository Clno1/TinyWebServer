
#include<iostream>
#include<stdlib.h>

#include"block_queue.h"

//构造析构函数
template<class T>
block_queue<T>::block_queue(int max_size=1000) {
	if (max_size <= 0) exit(-1);
	m_max_size = max_size;
	m_array = new T[max_size];
	m_size = 0;
	m_front = -1;
	m_back = -1;
}
template<class T>
block_queue<T>::~block_queue() {
	m_mutex.lock();
	if (m_array != NULL)
		delete[] m_array;
	m_mutex.unlock();
}

template<class T>
void block_queue<T>::clear() {
	m_mutex.lock();
	m_size = 0;
	m_front = -1;
	m_back = -1;
	m_mutex.unlock();
}

template<class T>
bool block_queue<T>::full() {
	m_mutex.lock();
	if (m_size >= m_max_size) {
		m_mutex.unlock();
		return true;
	}
	m_mutex.unlock();
	return false;
}

template<class T>
bool block_queue<T>::empty() {
	m_mutex.lock();
	if (m_size == 0) {
		m_mutex.unlock();
		return true;
	}
	m_mutex.unlock();
	return false;
}

template<class T>
bool block_queue<T>::front(T& value) {
	m_mutex.lock();
	if (m_size == 0) {
		m_mutex.unlock();
		return false;
	}
	value = m_array[m_front];
	m_mutex.unlock();
	return true;
}
template<class T>
bool block_queue<T>::back(T &value) {
	m_mutex.lock();
	if (m_size == 0) {
		m_mutex.unlock();
		return false;
	}
	value = m_array[m_back];
	m_mutex.unlock();
	return true;
}

template<class T>
int block_queue<T>::size() {
	int tmp = 0;
	m_mutex.lock();
	tmp = m_size;
	m_mutex.unlock();
	return tmp;
}
template<class T>
int block_queue<T>::max_size() {
	int tmp = 0;
	m_mutex.lock();
	tmp = m_max_size;
	m_mutex.unlock();
	return tmp;
}



template<class T>
bool block_queue<T>::push(const T& item) {

}

template<class T>
bool block_queue<T>::pop(T &item) {

}

template<class T>		//这是带时间的pop
bool block_queue<T>::pop(T& item, int ms_timeout) {

}