

#ifndef _CONDITION_H_
#define _CONDITION_H_

#include "mutexlock.h"
#include "noncopyable.h"

class Condition : public noncopyable
{
	public:
		explicit Condition(MutexLock &_mutex) : 
			mutex(_mutex) { ::pthread_cond_init(cond, NULL); }
		~Condition() { ::pthread_cond_destroy(cond); }
		void wait() { ::pthread_cond_wait(&cond, mutex.get()); }
		void notify() { ::pthread_cond_signal(cond); }
		void notifyAll() { ::pthread_cond_broadcast(&cond); }
	private:
		MutexLock &mutex;
		pthread_cond_t cond;
};



#endif //_CONDITION_H_
