#ifndef SQL_CONNECTION_POOL
#define SQL_CONNECTION_POOL

#include<stdio.h>
#include<list>
#include<string.h>
#include<errno.h>
#include<list>
#include<mysql/mysql.h>
#include<string>

#include"../lock/locker.h"

using namespace std;


//���ݿ����ӳ���
class connection_pool
{
public:
	MYSQL* GetConnection();		//�����ȡһ������
	bool ReleaseConnection(MYSQL* conn);		//�ͷŵ�ǰ����
	int GetFreeConn();				//���FreeConn������������
	void DestroyPool();			//�������ݿ����ӳ���

	//����ģʽ,��ȡ���ӳ�
	static connection_pool* GetInstance();
	//��ʼ����������Ҫ�Ǵ���MaxConn�����ݿ�����
	void init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int MaxConn);
	
	//������������
	connection_pool();
	~connection_pool();

private:
	unsigned int MaxConn;			//���������
	unsigned int CurConn;			//��ǰ��ʹ�õ�������
	unsigned int FreeConn;			//��ǰ���е�������

private:
	locker lock;							//�������ӳص���
	list<MYSQL*> connList;		//�洢���ӳص�list
	sem reserve;							//�ź���������������

private:
	string url;								//������ַ
	string Port;							//���ݿ�˿ں�
	string User;							//��½���ݿ��û���
	string PassWord;					//��½���ݿ�����
	string DatabaseName;			//ʹ�����ݿ���
};


//���ӳص�RAII��Դ����
class connectionRAII {
public:
	connectionRAII(MYSQL** SQL, connection_pool* connPool);
	~connectionRAII();

private:
	MYSQL* conRAII;
	connection_pool* poolRAII;
};

#endif