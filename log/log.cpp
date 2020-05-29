

#include<stdio.h>
#include<cstring>
#include<string>
#include <stdarg.h>
#include <pthread.h>

#include"log.h"
#include"block_queue.h"


Log::Log() {
	m_count = 0;
	m_is_async = false;
}
Log::~Log() {
	if (m_fp != NULL) fclose(m_fp);
}

//
void* Log::async_write_log() {
	std::string single_log;
	//从阻塞队列中取出一个日志string，写入文件
/*	while (m_log_queue->pop(single_log)) {
		m_mutex.lock();
		fputs(single_log.c_str(), m_fp);
		m_mutex.unlock();
	}
	*/
}

//异步需要设置阻塞队列的长度，同步不需要设置
bool Log::init(const char* file_name, int log_buf_size, int split_lines, int max_queue_size) {
	//如果设置了max_queue_size,则设置为异步（以max_queue_size判断是否异步）
	if (max_queue_size >= 1) {
		m_is_async = true;
/*		m_log_queue = new block_queue<std::string>(max_queue_size);		*/
		pthread_t tid;
		//flush_log_thread为回调函数,这里表示创建线程异步写日志
		pthread_create(&tid, NULL, flush_log_thread, NULL);
	}

	//这里是无论异步同步都要初始化的
	m_log_buf_size = log_buf_size;
	m_buf = new char[m_log_buf_size];
	memset(m_buf, '\0', m_log_buf_size);
	m_split_lines = split_lines;

	time_t nowt = time(NULL);
	struct tm* sys_tm = localtime(&nowt);
	struct tm my_tm = *sys_tm;

	const char* p = strchr(file_name, '/');
	char log_full_name[256] = { 0 };


	//
	if (p == NULL)
	{
		snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
	}
	else
	{
		strcpy(log_name, p + 1);
		strncpy(dir_name, file_name, p - file_name + 1);
		snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
	}

	m_today = my_tm.tm_mday;

	m_fp = fopen(log_full_name, "a");
	if (m_fp == NULL)
	{
		return false;
	}

	return true;
}

//
void Log::write_log(int level, const char* format, ...) {

}

//
void Log::flush(void) {
	m_mutex.lock();
	fflush(m_fp);
	m_mutex.unlock();
}