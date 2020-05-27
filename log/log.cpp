
#include<stdio.h>
#include<string.h>

#include"log.h"

using namespace std;

Log::Log() {
	m_count = 0;
	m_is_async = false;
}
Log::~Log() {
	if (m_fp != NULL) fclose(m_fp);
}

//
void* Log::async_write_log() {
	string single_log;
	//������������ȡ��һ����־string��д���ļ�
	while (m_log_queue->pop(single_log)) {
		m_mutex.lock();
		fputs(single_log.c_str(), m_fp);
		m_mutex.unlock();
	}
}

//�첽��Ҫ�����������еĳ��ȣ�ͬ������Ҫ����
bool Log::init(const char* file_name, int log_buf_size, int split_lines, int max_queue_size) {
	//���������max_queue_size,������Ϊ�첽����max_queue_size�ж��Ƿ��첽��
	if (max_queue_size >= 1) {
		m_is_async = true;
		m_log_queue = new block_queue<string>(max_queue_size);
		pthread_t tid;
		//flush_log_threadΪ�ص�����,�����ʾ�����߳��첽д��־
		pthread_create(&tid, NULL, flush_log_thread, NULL);
	}

	//�����������첽ͬ����Ҫ��ʼ����
	m_log_buf_size = log_buf_size;
	m_buf = new char[m_log_buf_size];
	memset(m_buf, '\0', m_log_buf_size);
	m_split_lines = split_lines;

	time_t nowt = time(NULL);
	struct tm* sys_tm = localtime(&nowt);
	struct tm my_tm = *sys_tm;

	const char* p = strchr(file_name, '/');
	char log_full_name[256] = { 0 };

	if (p == NULL) {

	}
	else {

	}
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