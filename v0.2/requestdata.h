
#ifndef _REQUESTDATA_H_
#define _REQUESTDATA_H_

#include <string>
#include <unordered_map>


class MimeType
{
	public:
		static std::string getMime(const std::string &suffix);

	private:
		MimeType();
		MimeType(const MimeType &rhs);
		static pthread_mutex_t lock;
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
struct mytimer;

struct requestData
{
	private:
		int againTimes;
		std::string path;
		int fd;
		int epollfd;

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

	private:
		int parse_URI();
		int parse_Headers();
		int analysisRequest();
	
	public:
		requestData();
		requestData(std::string path, int _fd, int _epollfd);
		~requestData();
		void addTimer(mytimer *mtimer);
		void reset();
		void seperateTimer();
		int getFd();
		void setFd(int _fd);
		void handleRequest();
		void handleError(int fd, int err_num, std::string short_msg);
};

struct mytimer
{
	bool deleted;
	size_t expired_time;
	requestData *request_data;

	mytimer(requestData *_request_data, int timeout);
	~mytimer();
	void update(int timeout);
	bool isvalid();
	void clearReq();
	void setDeleted();
	bool isDeleted() const;
	size_t getExpTime() const;
};


struct timerCmp
{
	bool operator() (const mytimer *lhs, const mytimer *rhs);
};

class MutexLockGuard
{
	public:
		explicit MutexLockGuard();
		~MutexLockGuard();
	
	private:
		static pthread_mutex_t lock;
		MutexLockGuard(const MutexLockGuard &rhs);
		MutexLockGuard& operator=(const MutexLockGuard &rhs);
};


#endif
