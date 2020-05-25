#include<stdio.h>
#include<mysql/mysql.h>
#include<string.h>
#include<list>
#include<pthread.h>
#include<iostream>

#include"sql_connection_pool.h"

using namespace std;

//���캯��
connection_pool::connection_pool() {
	this->CurConn = 0;
	this->FreeConn = 0;
	this->MaxConn = 0;
}

//����ģʽ����̬
connection_pool* connection_pool::GetInstance() {
	static connection_pool connPool;
	return &connPool;
}

//�����ĳ�ʼ������
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, unsigned int MaxConn) {
	this->url = url;
	this->Port = Port;
	this->User = User;
	this->PassWord = PassWord;
	this->DatabaseName = DBName;

	//�Ȼ�������ס���ӣ�����MaxConn���ݿ�����
	lock.lock();
	for (int i = 0; i < MaxConn; i++) {
		//�´���һ��������Դ
		MYSQL* con = NULL;
		con = mysql_init(con);

		if (con == NULL) {
			cout << "mysqlinit  Error��" << mysql_error(con)<<endl;
			exit(1);
		}

		con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);
		if (con == NULL) {
			cout << "mysql connect Error��" << mysql_error(con) << endl;
			exit(1);
		}

		//�������Դ��������
		connList.push_back(con);		
		++FreeConn;
	}
	
	//��ʼ���ź����ͳ�������
	reserve = sem(FreeConn);
	this->MaxConn = FreeConn;

	lock.unlock();
}

//�����ȡһ��������Դ
MYSQL* connection_pool::GetConnection() {
	MYSQL* con = NULL;

	if (connList.size() == 0)	return NULL;

	//����һ����Դ��������/�ź��� ׼��
	reserve.wait();
	lock.lock();

	con = connList.front();		//������ͷȡ��һ����Դ
	connList.pop_front();

	--FreeConn;
	++CurConn;

	lock.unlock();
	return con;
}

//��ȡ����������
int connection_pool::GetFreeConn() {
	return this->FreeConn;
}

//�ͷŵ�ǰ������Դcon
bool connection_pool::ReleaseConnection(MYSQL* con) {
	if (con == NULL)		return false;
	lock.lock();

	connList.push_back(con);
	++FreeConn;
	--CurConn;

	reserve.post();
	lock.unlock();
}

//��������
connection_pool::~connection_pool() {
	DestroyPool();
}

//�����������ݿ����ӳ�
void connection_pool::DestroyPool() {
	lock.lock();
	if (connList.size() > 0) {
		list<MYSQL*>::iterator it;
		for (it = connList.begin(); it != connList.end(); it++) {
			MYSQL* con = *it;
			mysql_close(con);		//���ÿһ��������Դclose��
		}
		CurConn = 0;
		FreeConn = 0;
		connList.clear();
	}
	lock.unlock();
}


//���ӳ�RAII
connectionRAII::connectionRAII(MYSQL** SQL, connection_pool* connPool) {
	*SQL = connPool->GetConnection();
	conRAII = *SQL;
	poolRAII = connPool;
}
connectionRAII::~connectionRAII() {
	poolRAII->ReleaseConnection(conRAII);
}
