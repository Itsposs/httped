#ifndef _REQUESTDATA_H
#define _REQUESTDATA_H

#include <string>
#include <unordered_map>

const int STATE_PARSE_URI = 1;
const int STATE_PARSE_HEADERS = 2;
const int STATE_RECV_BODY = 3;
const int STATE_ANALYSIS = 4;
const int STATE_FINISH = 5;

const int MAX_BUFF = 4096;

// 有请求出现但是读不到数据,可能是Request Aborted,
// 或者来自网络的数据没有达到等原因,
// 对这样的请求尝试超过一定的次数就抛弃
const int AGAIN_MAX_TIMES = 200;

const int PARSE_URI_AGAIN = -1;
const int PARSE_URI_ERROR = -2;
const int PARSE_URI_SUCCESS = 0;

const int PARSE_HEADER_AGAIN = -1;
const int PARSE_HEADER_ERROR = -2;
const int PARSE_HEADER_SUCCESS = 0;

const int ANALYSIS_ERROR = -2;
const int ANALYSIS_SUCCESS = 0;

const int METHOD_POST = 1;
const int METHOD_GET = 2;
const int HTTP_10 = 1;
const int HTTP_11 = 2;

const int EPOLL_WAIT_TIME = 500;

class MimeType
{
	public:
		static std::string getMime(const std::string &suffix);
	private:
		MimeType();
		~MimeType();
		MimeType(const MimeType &rhs);
		MimeType & operator=(const MimeType &rhs);
		static std::unordered_map<std::string, std::string> mime;
};


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

struct mytimer;

// 命名不规范
// 注释较少
class requestData
{
	public:
		requestData();
		requestData(int _epollfd, int _fd, std::string _path);
		~requestData();
		void addTimer(mytimer *mtimer);
		void reset();
		void seperateTimer();
		int getFd() const;
		void setFd(int _fd);
		void handleRequest();
		void handleError(int fd, int err_num, std::string short_msg);
	
	private:
		int againTimes;
		std::string path;
		int fd;
		int epollfd;
		// content的内容用完就清
		std::string content;
		int method;
		int HTTPversion;
		std::string file_name;
		int now_read_pos;
		int state;
		int h_state;
		bool isfinish;
		bool keep_alive;
		std::unordered_map<std::string, std::string> headers;
		mytimer *timer;

		int parse_URI();
		int parse_Headers();
		int analysisRequest();

};


struct mytimer
{
	public:
		mytimer(requestData *_request_data, int timeout);
		~mytimer();
		bool isvalid();
		void clearReq();
		void setDeleted();
		bool isDeleted() const;
		void update(int timeout);
		size_t getExpTime() const;

		bool deleted;
		size_t expired_time;
		requestData *request_data;
};


struct timerCmp
{
    bool operator()(const mytimer *a, const mytimer *b) const;
};


#endif //_REQUESTDATA_H

