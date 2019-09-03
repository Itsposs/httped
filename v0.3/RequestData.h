
#ifndef _REQUESTDATA_H_
#define _REQUESTDATA_H_

#include <memory>
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

struct RequestData : public std::enable_shared_from_this<RequestData>
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
		std::weak_ptr<mytimer> timer;

	private:
		int parse_URI();
		int parse_Headers();
		int analysisRequest();
	
	public:
		RequestData();
		RequestData(std::string path, int _fd, int _epollfd);
		~RequestData();
		void addTimer(std::shared_ptr<mytimer> mtimer);
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
	std::shared_ptr<RequestData> request_data;

	mytimer(std::shared_ptr<RequestData> _request_data, int timeout);
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
	bool operator()(std::shared_ptr<mytimer> &lhs, std::shared_ptr<mytimer> &rhs) const;
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
