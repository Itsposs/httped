
#ifndef _REQUESTDATA_H_
#define _REQUESTDATA_H_

#include <memory>
#include <string>
#include "timer.h"
#include <unordered_map>

// opencv
//#include <opencv/cv.h>
#include <opencv2/opencv.hpp>

class MimeType
{
	public:
		static std::string getMime(const std::string &suffix);

	private:
		MimeType();
		MimeType(const MimeType &rhs);
		static void init();
		static pthread_once_t once_control;
		static std::unordered_map<std::string, std::string> mime;
};

// 
enum HeadersState
{
	h_start = 0,
	h_key,
	h_colon,
	h_spaces_after_colon,
	h_value,
	h_CR,
	h_LF,
	h_end_CR,
	h_end_LF
};


//  
class TimerNode;

struct RequestData : public std::enable_shared_from_this<RequestData>
{
	private:
		std::string path;
		int fd;
		int epollfd;

		std::string inBuffer;
		std::string outBuffer;
		__uint32_t events;
		bool error;
		int method;
		int HTTPversion;
		std::string file_name;
		int now_read_pos;
		int state;
		int h_state;
		bool isfinish;
		bool keep_alive;
		std::unordered_map<std::string, std::string> headers;
		std::weak_ptr<TimerNode> timer;
		
		bool isAbleRead;
		bool isAbleWrite;

	private:
		int parse_URI();
		int parse_Headers();
		int analysisRequest();
	
		cv::Mat stitch(cv::Mat &src) { return src; }
	public:
		RequestData();
		RequestData(int _epollfd, int _fd, std::string _path);
		~RequestData();
		void linkTimer(std::shared_ptr<TimerNode> mtimer);
		void reset();
		void seperateTimer();
		int getFd();
		void setFd(int _fd);
		void handleRead();
		void handleWrite();
		void handleError(int fd, int err_num, std::string short_msg);
		void handleConn();

		void disableReadAndWrite();
		void enableRead();
		void enableWrite();
		bool canRead();
		bool canWrite();
};


#endif
