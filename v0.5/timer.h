

#ifndef _TIMER_H_
#define _TIMER_H_

#include <queue>
#include <memory>
#include "MutexLock.h"
#include "requestdata.h"

class RequestData;

class TimerNode
{
	public:
		typedef std::shared_ptr<RequestData> SP_ReqData;
		TimerNode(SP_ReqData _request_data, int timeout);
		~TimerNode();
		void update(int timeout);
		bool isvalid();
		void clearReq();
		void setDeleted();
		bool isDeleted() const;
		size_t getExpTime() const;
	private:
		bool deleted;
		size_t expired_time;
		SP_ReqData request_data;

};


struct timerCmp
{
	bool operator()(std::shared_ptr<TimerNode> &lhs, 
		std::shared_ptr<TimerNode> &rhs)
	{
		return lhs -> getExpTime() > rhs -> getExpTime();
	}
};


class TimerManager
{
	public:
		typedef std::shared_ptr<RequestData> SP_ReqData;
		typedef std::shared_ptr<TimerNode> SP_TimerNode;
		TimerManager() { }
		~TimerManager(){ }
		void handle_expired_event();
		void addTimer(SP_TimerNode timer_node);
		void addTimer(SP_ReqData request_data, int timeout);

	private:
		MutexLock lock;
		std::priority_queue<SP_TimerNode, std::deque<SP_TimerNode>, timerCmp> TimerNodeQueue;

};

#endif // _TIMER_H_
