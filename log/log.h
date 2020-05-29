#ifndef LOG_H
#define LOG_H


#include<stdio.h>
#include<iostream>
#include<pthread.h>
#include<cstring>
#include<string>

#include"block_queue.h"

//���ĸ��궨���������ļ���ʹ�ã���Ҫ���ڲ�ͬ���͵���־���
//__VA_ARGS__��һ���ɱ�����ĺ꣬����ʱ�궨���в����б�����һ������Ϊʡ�Ժ�
#define LOG_DEBUG(format, ...) Log::get_instance()->write_log(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) Log::get_instance()->write_log(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) Log::get_instance()->write_log(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::get_instance()->write_log(3, format, ##__VA_ARGS__)

class Log
{
public:
	//C++11�Ժ�,ʹ�þֲ������������ü���
	static Log* get_instance() {
		static Log instance;
		return &instance;
	}
	//�첽д��־���з���������˽�з���async_write_log
	static void* flush_log_thread(void* args) {
		Log::get_instance()->async_write_log();
	}
	//��ѡ��Ĳ�������־�ļ�����־��������С����������Լ����־������
	bool init(const char* file_name, int log_buf_size = 8192, int splite_lines = 5000000, int max_queue_size = 0);
	//��������ݰ��ձ�׼��ʽ����
	void write_log(int level, const char* format, ...);
	//ǿ��ˢ�»�����
	void flush(void);

private:
	Log();
	virtual ~Log();
	void* async_write_log();

private:
	char dir_name[128];		//·����
	char log_name[128];		//�ļ���
	
	int m_split_lines;			//��־�������
	int m_log_buf_size;		//��־��������С
	long long m_count;		//��־������¼
	int m_today;					//��¼��ǰ����һ�죿

	FILE* m_fp;					//log�ļ�ָ��
	char* m_buf;					//
	bool m_is_async;			//�Ƿ�ͬ����־
	
	locker m_mutex;			//���еĻ�����
	block_queue<std::string>* m_log_queue;		//��־��������

};

#endif // !LOG_H
