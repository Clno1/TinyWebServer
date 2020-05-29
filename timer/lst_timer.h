#ifndef LST_TIMER
#define LST_TIMER

#include<time.h>
#include<netinet/in.h>

class util_timer;
struct client_data
{
	sockaddr_in address;
	int sockfd;
	util_timer* timer;		//�ͻ���Ӧ�Ķ�ʱ������25���໥
};

//�����㣬�����¼��Ϳͻ�����
class util_timer
{
public:
	util_timer() : prev(NULL),next(NULL) {}

public:
	time_t expire;		//��¼ʱ��
	//!!!��ʱ����ִ�к�������ʱ��͵������
	void (*cb_func)(client_data*);	
	client_data* user_data;		//�ͻ����ݣ���12���໥
	util_timer* prev;		//˫������
	util_timer* next;		//˫������
};


//�������¼���������
class sort_timer_lst
{
public:
	//����Ĺ�������������
	sort_timer_lst() : head(NULL), tail(NULL) {};
	~sort_timer_lst() {
		util_timer* tmp = head;
		while (tmp) {
			head = tmp->next;
			delete tmp;
			tmp = head;
		}
	}
	//������
	void add_timer(util_timer* timer) {
		if (!timer) return;
		if (!head) {
			head = tail = timer;
			return;
		}
		//����µĶ�ʱ����ʱʱ��С�ڵ�ǰͷ�����
		//ֱ�ӽ���ǰ��ʱ�������Ϊͷ�����
		if (timer->expire < head->expire) {
			timer->next = head;
			head->prev = timer;
			head = timer;
			return;
		}

		//���ٲ��ǲ��뵽ͷ�����ú�����������
		add_timer(timer, head);
	}
	//������ʱ�����������仯ʱ��������ʱ���������е�λ��
	void adjust_timer(util_timer* timer) {
		if (!timer) return;
		util_timer* tmp = timer->next;

		//��Ϊֻ�����ӣ�������������϶��������
		if (!tmp || (timer->expire < tmp->expire)) return;

		//�����������ͷ/��ͷ��˼·������ɾ�����ٵ��ò��뺯�����²���
		if (timer == head) {
			head = head->next;
			head->prev = NULL;
			timer->next = NULL;
			add_timer(timer, head);
		}
		else {
			timer->prev->next = timer->next;
			timer->next->prev = timer->prev;
			add_timer(timer, timer->next);
		}
	}
	//ɾ����ʱ��
	void del_timer(util_timer* timer) {
		if (!timer) return;
		//�����������ʣ��һ����㣬ֱ��ɾ��
		if (timer == head && timer == tail) {
			delete timer;
			head = NULL;
			tail = NULL;
			return;
		}
		//��ɾ���Ķ�ʱ��Ϊͷ���
		if (timer == head) {
			head = head->next;
			head->prev = NULL;
			delete timer;
			return;
		}
		//��ɾ������β���
		if (timer == tail) {
			tail = tail->prev;
			tail->next = NULL;
			delete timer;
			return;
		}
		//����ͷβ����ͨɾ��
		timer->prev->next = timer->next;
		timer->next->prev = timer->prev;
		delete timer;
		return;
	}
	//��ʱ��������
	void tick() {
		if (!head) return;
		time_t cur = time(NULL);		//��ȡ��ǰʱ��
		util_timer* tmp = head;
		while (tmp)
		{
			if (cur < tmp->expire) break;		//�͵������ˣ������ִ��ʱ�䶼��û��
			tmp->cb_func(tmp->user_data);
			//ִ����֮��ɾ������ͷ���ƶ�ͷ
			head = tmp->next;
			if (head)
				head->prev = NULL;
			delete tmp;
			tmp = head;
		}
	}

private:
	//��timer���뵽�����У���������ļ�⵽�������ٲ��ǲ��뵽ͷ
	void add_timer(util_timer* timer, util_timer* lst_head) {
		util_timer* prev = lst_head;
		util_timer* tmp = prev->next;
		//������ǰ���֮����������ճ�ʱʱ���ҵ�Ŀ�궨ʱ����Ӧ��λ�ã�����˫������������
		while (tmp)
		{
			//���뵽prev��tmp֮ǰ
			if (timer->expire < tmp->expire) {
				prev->next = timer;
				timer->next = tmp;
				tmp->prev = timer;
				timer->prev = prev;
				break;
			}
			prev = tmp;
			tmp = tmp->next;
		}
		//����û�в���ɹ���֤��Ҫ���뵽�����
		if (!tmp) {
			prev->next = timer;
			timer->prev = prev;
			timer->next = NULL;
			tail = timer;
		}
	}

private:
	util_timer* head;
	util_timer* tail;
};


#endif