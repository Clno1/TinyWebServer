#include<stdio.h>
#include<mysql/mysql.h>
#include<string.h>
#include<list>
#include<pthread.h>
#include<iostream>

#include"sql_connection_pool.h"

using namespace std;

//构造函数
connection_pool::connection_pool() {
	this->CurConn = 0;
	this->FreeConn = 0;
	this->MaxConn = 0;
}

//单例模式，静态
connection_pool* connection_pool::GetInstance() {
	static connection_pool connPool;
	return &connPool;
}

//真正的初始化函数
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, unsigned int MaxConn) {
	this->url = url;
	this->Port = Port;
	this->User = User;
	this->PassWord = PassWord;
	this->DatabaseName = DBName;

	//先互斥锁锁住池子，创造MaxConn数据库链接
	lock.lock();
	for (int i = 0; i < MaxConn; i++) {
		//新创建一个连接资源
		MYSQL* con = NULL;
		con = mysql_init(con);

		if (con == NULL) {
			cout << "mysqlinit  Error：" << mysql_error(con)<<endl;
			exit(1);
		}

		con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);
		if (con == NULL) {
			cout << "mysql connect Error：" << mysql_error(con) << endl;
			exit(1);
		}

		//把这个资源放入链表
		connList.push_back(con);		
		++FreeConn;
	}
	
	//初始化信号量和池子数量
	reserve = sem(FreeConn);
	this->MaxConn = FreeConn;

	lock.unlock();
}

//请求获取一个连接资源
MYSQL* connection_pool::GetConnection() {
	MYSQL* con = NULL;

	if (connList.size() == 0)	return NULL;

	//请求一个资源，互斥锁/信号量 准备
	reserve.wait();
	lock.lock();

	con = connList.front();		//从链表头取得一个资源
	connList.pop_front();

	--FreeConn;
	++CurConn;

	lock.unlock();
	return con;
}

//获取空闲连接数
int connection_pool::GetFreeConn() {
	return this->FreeConn;
}

//释放当前连接资源con
bool connection_pool::ReleaseConnection(MYSQL* con) {
	if (con == NULL)		return false;
	lock.lock();

	connList.push_back(con);
	++FreeConn;
	--CurConn;

	reserve.post();
	lock.unlock();
}

//析构函数
connection_pool::~connection_pool() {
	DestroyPool();
}

//销毁整个数据库连接池
void connection_pool::DestroyPool() {
	lock.lock();
	if (connList.size() > 0) {
		list<MYSQL*>::iterator it;
		for (it = connList.begin(); it != connList.end(); it++) {
			MYSQL* con = *it;
			mysql_close(con);		//获得每一个连接资源close掉
		}
		CurConn = 0;
		FreeConn = 0;
		connList.clear();
	}
	lock.unlock();
}


//连接池RAII
connectionRAII::connectionRAII(MYSQL** SQL, connection_pool* connPool) {
	*SQL = connPool->GetConnection();
	conRAII = *SQL;
	poolRAII = connPool;
}
connectionRAII::~connectionRAII() {
	poolRAII->ReleaseConnection(conRAII);
}
