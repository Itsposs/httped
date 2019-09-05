

#ifndef _LOGFILE_H_
#define _LOGFILE_H_

#include "FileUtil.h"
#include "MutexLock.h"
#include "noncopyable.h"
#include <memory>
#include <string>



class LogFile : noncopyable
{
	public:
		LogFile(const std::string &basename, int flushEveryN_ = 1024);
		~LogFile();

		void flush();
		bool rollFile();
		void append(const char *logline, int len);
	private:
		int count_;
		const int flushEveryN_;
		const std::string basename_;
		std::unique_ptr<MutexLock> mutex_;
		std::unique_ptr<AppendFile> file_;
		void append_unlocked(const char *logline, int len);
};


#endif
