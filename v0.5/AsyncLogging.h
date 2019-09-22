

#ifndef _ASYNCLOGGING_H_
#define _ASYNCLOGGING_H_

#include <vector>
#include <string>
#include <functional>
#include "Thread.h"
#include "LogStream.h"
#include "MutexLock.h"
#include "noncopyable.h"
#include "CountDownLatch.h"


class AsyncLogging : noncopyable
{
	public:
		AsyncLogging(const std::string basename, int flushInterval = 2);
		~AsyncLogging()
		{
			if(running_)
				stop();
		}
		
		void append(const char *logline, int len);
		void start()
		{
			running_ = true;
			thread_.start();
			latch_.wait();
		}

		void stop()
		{
			running_ = false;
			cond_.notify();
			thread_.join();
		}
		
	private:
		AsyncLogging(const AsyncLogging&);
		void operator=(const AsyncLogging&);

		void threadFunc();

		typedef FixedBuffer<kLargeBuffer> Buffer;   // buffer
		typedef std::vector<std::shared_ptr<Buffer>> BufferVector; // buffers' pointer of vector
		typedef std::shared_ptr<Buffer> BufferPtr; // buffers' pointer

		const int flushInterval_;
		bool running_;
		std::string basename_;
		Thread thread_;
		MutexLock mutex_;
		Condition cond_;
		BufferPtr currentBuffer_;
		BufferPtr nextBuffer_;
		BufferVector buffers_;
		CountDownLatch latch_;
};


#endif
