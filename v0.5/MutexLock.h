
#ifndef _MUTEXLOCK_H_
#define _MUTEXLOCK_H_

#include <pthread.h>
#include "noncopyable.h"


class MutexLock : public noncopyable
{
		// 由于友元类不受访问权限的影响,放在那里都可以
		friend class Condition;
	public:
		MutexLock() { ::pthread_mutex_init(&mutex, NULL); }
		~MutexLock() 
		{
			::pthread_mutex_lock(&mutex);
			::pthread_mutex_destroy(&mutex);
		}
		void lock() { ::pthread_mutex_lock(&mutex); }
		void unlock() { ::pthread_mutex_unlock(&mutex); }
		pthread_mutex_t * get() { return &mutex; }
	private:
		pthread_mutex_t mutex;
};


class MutexLockGuard : public noncopyable
{
	public:
		explicit MutexLockGuard(MutexLock &_mutex) :
			mutex(_mutex) { mutex.lock(); }
		~MutexLockGuard() { mutex.unlock(); }
	private:
		MutexLock &mutex;
};





#endif // _MUTEXLOCK_H_
