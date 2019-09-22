


#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <string>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "LogStream.h"


class AsyncLogging;

class Logger
{
	public:
		~Logger();
		Logger(const char *fileName, int line);
		LogStream& stream() { return impl_.stream_; }

	private:
		class Impl
		{
			public:
				Impl(const char *fileName, int line);
				void formatTime();

				LogStream stream_;
				int line_;
				std::string basename_;
		};
		Impl impl_;
};

#define LOG Logger(__FILE__, __LINE__).stream();

#endif
