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
	bool full();				//�ж϶����Ƿ�����
	bool empty();		//�ж϶����Ƿ�Ϊ��

	bool front(T& value);		//���ض���Ԫ��
	bool back(T& value);			//���ض�βԪ��

	int size();
	int max_size();

	bool push(const T& item);	//���������Ԫ��
	bool pop(T& item);				//����Ԫ��
	bool pop(T& item, int ms_timeout);	//�����˳�ʱ����

private:
	locker m_mutex;		//������
	cond m_cond;			//������

	T* m_array;			//����
	
	int m_size;			//���д�С
	int m_max_size;	//������󳤶�
	int m_front;			//��ͷ
	int m_back;			//��β
};


#endif // BLOCK_QUEUE
