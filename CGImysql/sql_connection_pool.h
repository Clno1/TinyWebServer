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


//数据库连接池类
class connection_pool
{
public:
	MYSQL* GetConnection();		//请求获取一个连接
	bool ReleaseConnection(MYSQL* conn);		//释放当前连接
	int GetFreeConn();				//获得FreeConn，空闲连接数
	void DestroyPool();			//销毁数据库连接池子

	//单例模式,获取连接池
	static connection_pool* GetInstance();
	//初始化函数，主要是创建MaxConn个数据库连接
	void init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int MaxConn);
	
	//构造析构函数
	connection_pool();
	~connection_pool();

private:
	unsigned int MaxConn;			//最大连接数
	unsigned int CurConn;			//当前已使用的连接数
	unsigned int FreeConn;			//当前空闲的连接数

private:
	locker lock;							//整个连接池的锁
	list<MYSQL*> connList;		//存储连接池的list
	sem reserve;							//信号量，管理连接数

private:
	string url;								//主机地址
	string Port;							//数据库端口号
	string User;							//登陆数据库用户名
	string PassWord;					//登陆数据库密码
	string DatabaseName;			//使用数据库名
};


//连接池的RAII资源管理
class connectionRAII {
public:
	connectionRAII(MYSQL** SQL, connection_pool* connPool);
	~connectionRAII();

private:
	MYSQL* conRAII;
	connection_pool* poolRAII;
};

#endif