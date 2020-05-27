#ifndef BLOCK_QUEUE
#define BLOCK_QUEUE

#include<iostream>
#include<stdlib.h>

#include"../lock/locker.h"

template<class T>
class block_queue
{
public:
	block_queue(int max_size = 1000);
	~block_queue();
	void clear();
	bool full();				//判断队列是否满了
	bool empty();		//判断队列是否为空

	bool front(T& value);		//返回队首元素
	bool back(T& value);			//返回队尾元素

	int size();
	int max_size();

	bool push(const T& item);	//往队列添加元素
	bool pop(T& item);				//弹出元素
	bool pop(T& item, int ms_timeout);	//增加了超时处理

private:
	locker m_mutex;		//互斥锁
	cond m_cond;			//条件锁

	T* m_array;			//队列
	
	int m_size;			//队列大小
	int m_max_size;	//队列最大长度
	int m_front;			//队头
	int m_back;			//队尾
};


#endif // BLOCK_QUEUE
