#ifndef LST_TIMER
#define LST_TIMER

#include<time.h>
#include<netinet/in.h>

class util_timer;
struct client_data
{
	sockaddr_in address;
	int sockfd;
	util_timer* timer;		//客户对应的定时器，和25行相互
};

//链表结点，包含事件和客户数据
class util_timer
{
public:
	util_timer() : prev(NULL),next(NULL) {}

public:
	time_t expire;		//记录时间
	//!!!定时器的执行函数，到时间就调用这个
	void (*cb_func)(client_data*);	
	client_data* user_data;		//客户数据，和12行相互
	util_timer* prev;		//双向链表
	util_timer* next;		//双向链表
};


//链表，按事件升序排序
class sort_timer_lst
{
public:
	//链表的构造与析构函数
	sort_timer_lst() : head(NULL), tail(NULL) {};
	~sort_timer_lst() {
		util_timer* tmp = head;
		while (tmp) {
			head = tmp->next;
			delete tmp;
			tmp = head;
		}
	}
	//插入结点
	void add_timer(util_timer* timer) {
		if (!timer) return;
		if (!head) {
			head = tail = timer;
			return;
		}
		//如果新的定时器超时时间小于当前头部结点
		//直接将当前定时器结点作为头部结点
		if (timer->expire < head->expire) {
			timer->next = head;
			head->prev = timer;
			head = timer;
			return;
		}

		//至少不是插入到头，调用函数继续插入
		add_timer(timer, head);
	}
	//调整定时器，任务发生变化时，调整定时器在链表中的位置
	void adjust_timer(util_timer* timer) {
		if (!timer) return;
		util_timer* tmp = timer->next;

		//因为只会增加，所以如果在最后肯定无需调整
		if (!tmp || (timer->expire < tmp->expire)) return;

		//分两种情况：头/非头。思路都是先删除，再调用插入函数重新插入
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
	//删除定时器
	void del_timer(util_timer* timer) {
		if (!timer) return;
		//即整个链表就剩下一个结点，直接删除
		if (timer == head && timer == tail) {
			delete timer;
			head = NULL;
			tail = NULL;
			return;
		}
		//被删除的定时器为头结点
		if (timer == head) {
			head = head->next;
			head->prev = NULL;
			delete timer;
			return;
		}
		//被删除的是尾结点
		if (timer == tail) {
			tail = tail->prev;
			tail->next = NULL;
			delete timer;
			return;
		}
		//不是头尾，普通删除
		timer->prev->next = timer->next;
		timer->next->prev = timer->prev;
		delete timer;
		return;
	}
	//定时任务处理函数
	void tick() {
		if (!head) return;
		time_t cur = time(NULL);		//获取当前时间
		util_timer* tmp = head;
		while (tmp)
		{
			if (cur < tmp->expire) break;		//就到这里了，后面的执行时间都还没到
			tmp->cb_func(tmp->user_data);
			//执行完之后，删除链表头并移动头
			head = tmp->next;
			if (head)
				head->prev = NULL;
			delete tmp;
			tmp = head;
		}
	}

private:
	//把timer插入到链表中，经过上面的检测到这里至少不是插入到头
	void add_timer(util_timer* timer, util_timer* lst_head) {
		util_timer* prev = lst_head;
		util_timer* tmp = prev->next;
		//遍历当前结点之后的链表，按照超时时间找到目标定时器对应的位置，常规双向链表插入操作
		while (tmp)
		{
			//插入到prev后，tmp之前
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
		//上面没有插入成功，证明要插入到最后面
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