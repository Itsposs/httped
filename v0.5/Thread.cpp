

#include <memory>
#include <errno.h>
#include "Thread.h"
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/syscall.h>


namespace CurrentThread
{
	__thread int  t_cacheTid;
	__thread char t_tidString[32];
	__thread int  t_tidStringLength;
	__thread const char *t_threadName;


	void cacheTid();
	inline int tid()
	{
		if(__builtin_expect(t_cacheTid == 0, 0))
			cacheTid();
		return t_cacheTid;
	}

	inline const char * tidString() { return t_tidString; }

	inline int tidStringLength() { return t_tidStringLength; }

	inline const char * name() { return t_threadName; }
}

pid_t gettid()
{
	return static_cast<pid_t>(::syscall(SYS_gettid));
}

struct ThreadData
{
	typedef Thread::ThreadFunc ThreadFunc;
	ThreadFunc func_;

	std::string name_;
	pid_t *tid_;
	CountDownLatch *latch_;
	
	ThreadData(const ThreadFunc &func, const std::string &name,
		pid_t *tid, CountDownLatch *latch)
		: func_(func),
		  name_(name),
		  tid_(tid),
		  latch_(latch) { }

	void runInThread()
	{
		*tid_ = CurrentThread::tid();
		 tid_ = NULL;
		 latch_ -> countDown();
		 latch_ = NULL;

		CurrentThread::t_threadName = name_.empty() ? "Thread" : name_.c_str();
		prctl(PR_SET_NAME, CurrentThread::t_threadName);

		func_();
		CurrentThread::t_threadName = "finished";
	}
};

void * startThread(void *obj)
{
	ThreadData *data = static_cast<ThreadData *>(obj);
	data -> runInThread();
	delete data;
	return NULL;
}

void CurrentThread::cacheTid()
{
	if(t_cacheTid == 0)
	{
		t_cacheTid = gettid();
		t_tidStringLength = snprintf(t_tidString, sizeof(t_tidString), "%5d", t_cacheTid);
	}
}

Thread::Thread(const ThreadFunc &func, const std::string &name)
	: started_(false),
	  joined_(false),
	  pthreadId_(0),
	  tid_(0),
	  func_(func),
	  name_(name),
	  latch_(1)  { setDefaultName(); }

Thread::~Thread()
{
	if(started_ && !joined_)
		pthread_detach(pthreadId_);
}

void Thread::setDefaultName()
{
	if(name_.empty())
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "Thread");
		name_ = buf;
	}
}

void Thread::start()
{
	assert(!started_);
	started_ = true;

	ThreadData *data = new ThreadData(func_,name_, &tid_, &latch_);
	if(pthread_create(&pthreadId_, NULL, &startThread, data))
	{
		started_ = false;
		delete data;
	}
	else
	{
		latch_.wait();
		assert(tid_ > 0);
	}
}

int Thread::join()
{
	assert(started_);
	assert(!joined_);
	joined_ = true;
	return pthread_join(pthreadId_, NULL);
}


