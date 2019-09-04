

#ifndef _FILEUTIL_H_
#define _FILEUTIL_H_

#include <string>
#include "noncopyable.h"

class AppendFile : noncopyable
{
	public:
		explicit AppendFile(std::string filename);
		~AppendFile();
		void flush();
		void append(const char *logline, const size_t len);
	private:
		FILE fp_;
		char buffer_[64 * 1024];
		size_t write(const char *logline, size_t len);
};





#endif
