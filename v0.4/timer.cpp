
#include "epoll.h"
#include "timer.h"
#include <sys/time.h>  // gettimeday()



TimerNode::TimerNode(SP_ReqData _request_data, int timeout) :
	deleted(false), request_data(_request_data)
{
	struct timeval now;
	::gettimeofday(&now, NULL);
	// 以毫秒计算
	expired_time = (now.tv_sec * 1000) + now.tv_usec / 1000 + timeout;
}


TimerNode::~TimerNode()
{
	if(request_data)
		Epoll::epoll_del(request_data -> getFd());
}

void TimerNode::update(int timeout)
{
	struct timeval now;
	::gettimeofday(&now, NULL);
	expired_time = (now.tv_sec * 1000) + now.tv_usec / 1000 + timeout;
	
}

bool TimerNode::isvalid()
{
	struct timeval now;
	::gettimeofday(&now, NULL);
	size_t temp = now.tv_sec * 1000 + now.tv_usec / 1000;
	if(temp >= expired_time)
	{
		this -> setDeleted();
		return false;
	}
	else
		return true;
}

void TimerNode::clearReq()
{
	request_data.reset();
	this -> setDeleted();
}

void TimerNode::setDeleted()
{
	deleted = true;
}

bool TimerNode::isDeleted() const
{
	return deleted;
}

size_t TimerNode::getExpTime() const
{
	return expired_time;
}

void TimerManager::addTimer(SP_ReqData request_data, int timeout)
{
	SP_TimerNode new_node(new TimerNode(request_data, timeout));
	{
		MutexLockGuard locker(lock);
		TimerNodeQueue.push(new_node);
	}
	request_data -> linkTimer(new_node);
}

void TimerManager::addTimer(SP_TimerNode timer_node)
{

}

// 处理逻辑:
// 优先级队列不支持随机访问
// 即使支持,随机删除某个节点后破坏了堆的结构,需要更新堆结构.
// 所以对于被置为deleted的节点,会延迟到它超时或它前面的节点都被删除了,它才会被删除.
// 一个节点被置为deleted,它最迟会在TIMER_TIME_OUT时间后被删除
// 好处:
// 不需要遍历队列,省时.
// 给超时时间一个容忍的时间,就是设定的超时时间是删除的下限(并不是一道超时时间就立即删除),
// 如果请求在超时后下一次请求中又一次出现了,就不需要重新申请requestData节点了,这样就可以
// 重新申请requestData节点了,这样就可以重复利用前面的requestData,减少delete和new的时间.


void TimerManager::handle_expired_event()
{
	MutexLockGuard locker(lock);
	while(!TimerNodeQueue.empty())
	{
		SP_TimerNode ptimer_now = TimerNodeQueue.top();
		if(ptimer_now -> isDeleted())
			TimerNodeQueue.pop();
		else if(ptimer_now -> isvalid() == false)
			TimerNodeQueue.pop();
		else
			break;
	}
}


