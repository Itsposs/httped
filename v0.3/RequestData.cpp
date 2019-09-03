
#include <sys/time.h> // gettimeofday()
#include <deque>
#include "Util.h"
#include <string.h>  // strlen()
#include <sys/mman.h> // mmap()

// errno
#include <errno.h>

// signal
//#include <signal.h>

// stoi
#include <string>

// epoll
#include "Epoll.h"
#include <sys/epoll.h>

// open
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// priority_queue
#include <queue>

#include <stdio.h>
#include <unistd.h>  // close()
#include <algorithm>
#include <pthread.h>
#include "RequestData.h"

// opencv
//#include <opencv/cv.h>
//#include <opencv2/opencv.hpp>


// RequestData
const int STATE_PARSE_URI = 1;
const int STATE_PARSE_HEADERS = 2;
const int STATE_RECV_BODY = 3;
const int STATE_ANALYSIS = 4;
const int STATE_FINISH = 5;

// analysis
const int ANALYSIS_ERROR = -2;
const int ANALYSIS_SUCCESS = 0;


// buff
const int MAX_BUFF = 4096;

// 有请求出现但是读不到数据,可能是Request Aborted,
// 或者来自网络的数据没有到达等原因,对这样的请求尝
// 试超过一定次数就抛弃
const int AGAIN_MAX_TIMES = 200;


// uri
const int PARSE_URI_AGAIN = -1;
const int PARSE_URI_ERROR = -2;
const int PARSE_URI_SUCCESS = 0;

// method
const int METHOD_POST = 1;
const int METHOD_GET  = 2;

// http
const int HTTP_10 = 1;
const int HTTP_11 = 2;

// header
const int PARSE_HEADER_AGAIN = -1;
const int PARSE_HEADER_ERROR = -1;
const int PARSE_HEADER_SUCCESSS = 0;



// timeout
const int EPOLL_WAIT_TIME = 500;




// MimeType
pthread_mutex_t MimeType::lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t MutexLockGuard::lock = PTHREAD_MUTEX_INITIALIZER;
std::unordered_map<std::string, std::string> MimeType::mime;


std::string MimeType::getMime(const std::string &suffix)
{
	if(mime.size() == 0)
	{
		::pthread_mutex_lock(&lock);
		if(mime.size() == 0)
		{
			mime[".html"] = "text/html";
			mime[".avi"]  = "video/x-msvideo"; 
			mime[".bmp"]  = "image/bmp";
			mime[".c"]    = "text/plain";
			mime[".doc"]  = "application/msword";
			mime[".gif"]  = "image/gif";
			mime[".gz"]   = "application/x-gzip";
			mime[".htm"]  = "text/html";
			mime[".ico"]  = "application/x-ico";
			mime[".jpg"]  = "image/jpeg";
			mime[".png"]  = "image/png";
			mime[".txt"]  = "text/plain";
			mime[".mp3"]  = "audio/mp3";
			mime["default"] = "text/html";
		}
		::pthread_mutex_unlock(&lock);
	}
	if(mime.find(suffix) == mime.end())
		return mime["default"];
	else
		return mime[suffix];
}

// test
#include <iostream>


// 小根堆
std::priority_queue<std::shared_ptr<mytimer>, std::deque<std::shared_ptr<mytimer>>, timerCmp> myTimerQueue;  

RequestData::RequestData() :
	againTimes(0),
	now_read_pos(0),
	state(STATE_PARSE_URI),
	h_state(h_start),
	keep_alive(false)
{
	std::cout << "RequestData()" << std::endl;
}

RequestData::RequestData(std::string _path, int _fd, int _epollfd) :
	againTimes(0),
	path(_path),
	fd(_fd),
	epollfd(_epollfd),
	now_read_pos(0),
	state(STATE_PARSE_URI),
	h_state(h_state),
	keep_alive(false)
{
	std::cout << "RequestData(_epollfd, _fd, _path)" << std::endl;
}

RequestData::~RequestData()
{
	std::cout << "~RequestData()" << std::endl;
	//struct epoll_event ev{};
	// 超时的一定是读请求,没有"被动"写
	//ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
	//ev.data.ptr = (void *)this;
	//::epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
	//if(timer != NULL)
	//{
	//	timer -> clearReq();
	//	timer = NULL;
	//}
	close(fd);
}

void RequestData::addTimer(std::shared_ptr<mytimer>mtimer)
{
	// shared_ptr重载了bool,但是weak_ptr没有
	//if(timer == NULL)
		timer = mtimer;
}

void RequestData::setFd(int _fd)
{
	fd = _fd;
}

int RequestData::getFd()
{
	return fd;
}

void RequestData::reset()
{
	againTimes = 0;
	content.clear();
	file_name.clear();
	path.clear();
	now_read_pos = 0;
	state = STATE_PARSE_URI;
	h_state = h_start;
	headers.clear();
	keep_alive = false;

	if(timer.lock())
	{
		 std::shared_ptr<mytimer> my_timer(timer.lock());
		 my_timer -> clearReq();
		 timer.reset();
	}
}

void RequestData::seperateTimer()
{
	if(timer.lock())
	{
		 std::shared_ptr<mytimer> my_timer(timer.lock());
		 my_timer -> clearReq();
		 timer.reset();
	}
}


void RequestData::handleRequest()
{
	char buff[MAX_BUFF];
	bool isError = false;

	while(true)
	{
		int read_num = ::readn(fd, buff, MAX_BUFF);
		if(read_num < 0)
		{
			isError = true;
			break;
		}
		else if(read_num == 0)
		{
			// 有请求出现但是读不到数据,可能是Request Aborted,
			// 或者来自网络的数据没有到达等原因
			if(errno == EAGAIN)
			{
				if(againTimes > AGAIN_MAX_TIMES)
					isError = true;
				else
					++againTimes;
			}
			else if(errno != 0)
				isError = true;
			break;
		}
		std::string now_read(buff, buff + read_num);
		content += now_read;

		if(state == STATE_PARSE_URI)
		{
			int flag = this -> parse_URI();
			if(flag == PARSE_URI_AGAIN)
				break;
			else if(flag == PARSE_URI_ERROR)
			{
				isError = true;
				return;
			}
		}

		if(state == STATE_PARSE_HEADERS)
		{
			int flag = this -> parse_Headers();
			if(flag == PARSE_HEADER_AGAIN)
				break;
			else if(flag == PARSE_HEADER_ERROR)
			{
				isError = true;
				break;
			}
			if(method == METHOD_POST)
				state = STATE_RECV_BODY;
			else
				state = STATE_ANALYSIS;

		}


		if(state == STATE_RECV_BODY)
		{
			unsigned int content_length = -1;
			if(headers.find("Content-lenngth") != headers.end())
				content_length = std::stoi(headers["Content-length"]);
			else
			{
				isError = true;
				break;
			}
			if(content.size() < content_length)
				continue;
			state = STATE_ANALYSIS;
		}

		if(state == STATE_ANALYSIS)
		{
			int flag = this -> analysisRequest();
			if(flag == ANALYSIS_SUCCESS)
			{
				state = STATE_FINISH;
				break;
			}
			else
			{
				isError = true;
				break;
			}
		}
	}

	if(isError)
	{
		//delete this;
		return;
	}

	// 加入epoll继续
	if(state == STATE_FINISH)
	{
		if(!keep_alive)
		{
			//delete this;
			return;
		}
		else
			this -> reset();
	}

	// 一定要先加时间信息,否则可能会出现刚加进去,下一个读请求触发,然后
	// 分离失败后,又加入队列,最后超时被删除,然后正在线程中进行任务出错,
	// 二次释放错误.
	std::shared_ptr<mytimer> mtimer(new mytimer(shared_from_this(), 500));
	this -> addTimer(mtimer);
	{
		MutexLockGuard lock;
		myTimerQueue.push(mtimer);
	}

	// LT 只要存在事件就会不断地触发,直到处理完成
	// ET 只触发一次相同事件或者说只在从非触发到触发两个状态转换的时候才触发
	// LT模式下:
	// 如果在多线程\进程情况下,一个socket事件到来,数据开始解析,这个时候这个socket
	// 又来了同样一个这样的事件,而你的数据解析尚未完成,那么程序自动调度另外一
	// 个线程或者进程处理新的事件,这造成一个很严重的问题.即使在ET模式下也有可能出
	// 现这种情况!!!
	// 方法1:单独的线程\进程解析数据
	// 方法2:在epoll注册EPOLLONESHOT事件,处理当前socket事件后不再重新注册相关事件,
	// 那么这个事件就不再响应了或者触发了,要想重新注册事件则需要调用epoll_ctl()重置
	// 文件描述符上的事件,这样socket就不会出现竟态了,可以保证同一个socket只能被一个
	// 线程处理,不会跨越多个线程.
	
	__uint32_t _epo_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
	int ret = Epoll::epoll_mod(fd, shared_from_this(), _epo_event);
	if(ret < 0)
	{
		//delete this;
		return;
	}
}

int RequestData::parse_URI()
{
	std::string &str = content;
	// 读到完整的请求行再开始解析请求
	std::cout << "now_read_pos:" << now_read_pos << std::endl;
	unsigned int pos = str.find('\r', now_read_pos);
	if(pos < 0)
		return PARSE_URI_AGAIN;
	// 去掉请求行所占的空间,节省空间
	std::string resquest_line = str.substr(0, pos);
	if(str.size() > pos + 1)
		str = str.substr(pos + 1);
	else
		str.clear();

	// method
	pos = resquest_line.find("GET");
	if(pos < 0)
	{
		pos = resquest_line.find("POST");
		if(pos < 0)
			return PARSE_URI_ERROR;
		else
			method = METHOD_POST;
	}
	else
		method = METHOD_GET;

	// filename
	pos = resquest_line.find("/", pos);
	if(pos < 0)
		return PARSE_URI_ERROR;
	else
	{
		int _pos = resquest_line.find(' ', pos);
		if(_pos < 0)
			return PARSE_URI_ERROR;
		else
		{
			if(_pos - pos > 1)
			{
				file_name = resquest_line.substr(pos + 1, _pos - pos - 1);
				int __pos = file_name.find('?');
				if(__pos >= 0)
					file_name = file_name.substr(0, __pos); //???
			}
			else
				file_name = "index.html";
		}
		pos = _pos;
	}

	// http version
	pos = resquest_line.find("/", pos);
	if(pos < 0)
		return PARSE_URI_ERROR;
	else
	{
		if(resquest_line.size() -pos <= 3)
			return PARSE_URI_ERROR;
		else
		{
			std::string version = resquest_line.substr(pos + 1, 3);
			if(version == "1.0")
				HTTPversion = HTTP_10;
			else if(version == "1.1")
				HTTPversion = HTTP_11;
			else
				return PARSE_URI_ERROR;
		}
	}
	// header
	state = STATE_PARSE_HEADERS;
	return PARSE_URI_SUCCESS;
}

int RequestData::parse_Headers()
{
	std::string &str = content;
	int key_start = -1, key_end = -1, value_start = -1, value_end = -1;
	int now_read_line_begin = 0;
	bool notFinish = true;

	for(unsigned int i = 0; i < str.size() && notFinish; ++i)
	{
		switch(h_state)
		{
			case h_start:
			{
				if(str[i] == '\n' || str[i] == '\r')
					break;
				h_state = h_key;
				key_start = i;
				now_read_line_begin = i;
				break;
			}
			case h_key:
			{
				if(str[i] == ':')
				{
					key_end = i;
					if(key_end - key_start <= 0)
						return PARSE_HEADER_ERROR;
					h_state = h_colon;
				}
				else if(str[i] == '\n' || str[i] == '\r')
					return PARSE_HEADER_ERROR;
				break;
			}
			case h_colon:
			{
				if(str[i] == ' ')
					h_state = h_spaces_after_colon;
				else
					return PARSE_HEADER_ERROR;
				break;
			}
			case h_spaces_after_colon:
			{
				h_state = h_value;
				value_start = i;
				break;
			}
			case h_value:
			{
				if(str[i] == '\r')
				{
					h_state = h_CR;
					value_end = i;
					if(value_end - value_start <= 0)
						return PARSE_HEADER_ERROR;
				}
				else if(i - value_start > 255)
					return PARSE_HEADER_ERROR;
				break;
			}
			case h_CR:
			{
				if(str[i] == '\n')
				{
					h_state = h_LF;
					std::string key(str.begin() + key_start, str.begin()  + key_end);
					std::string value(str.begin() + value_start, str.begin() + value_end);
					headers[key] = value;
					now_read_line_begin = i;
				}
				else
					return PARSE_HEADER_ERROR;
				break;
			}
			case h_LF:
			{
				if(str[i] == '\r')
					h_state = h_end_CR;
				else
				{
					key_start = i;
					h_state = h_key;
				}
				break;
			}
			case h_end_CR:
			{
				if(str[i] == '\n')
					h_state = h_end_LF;
				else
					return PARSE_HEADER_ERROR;
				break;
			}
			case h_end_LF:
			{
				notFinish = false;
				key_start = i;
				now_read_line_begin = i;
				break;
			}
		}
	}
	if(h_state == h_end_LF)
	{
		str = str.substr(now_read_line_begin);
		return PARSE_HEADER_SUCCESSS;
	}
	str = str.substr(now_read_line_begin);
	return PARSE_HEADER_AGAIN;
}

int RequestData::analysisRequest()
{
	if(method == METHOD_POST)
	{
		char header[MAX_BUFF];
		::sprintf(header, "HTTP/1.1 %d %s\r\n", 200, "OK");
		if(headers.find("Connection") != headers.end() && headers["Connection"] == "keep-alive")
		{
			keep_alive = true;
			::sprintf(header, "%sConnection: keep-alive\r\n", header);
			::sprintf(header, "%sKeep-Alive: timeout=%d\r\n", header, EPOLL_WAIT_TIME);
		}

		char *send_content = "I have receiced this.";

		::sprintf(header, "%sContent-length: %zu\r\n", header, ::strlen(send_content));
		::sprintf(header, "%s\r\n", header);

		size_t sent_len = (size_t)::writen(fd, header, ::strlen(header));
		if(sent_len != ::strlen(header))
			return ANALYSIS_ERROR;

		sent_len = (size_t)::writen(fd, send_content, ::strlen(send_content));
		if(sent_len != ::strlen(send_content))
			return ANALYSIS_ERROR;

		// image
		std::vector<char> data(content.begin(), content.end());
		//cv::Mat test = cv::imdecode(data, CV_LOAD_IMAGE_ANYDEPTH | CV_LOAD_IMAGE_ANYCOLOR);
		//cv::imwrite("receive.bmp", test);
		return ANALYSIS_SUCCESS;

	}
	else if(method == METHOD_GET)
	{
		char header[MAX_BUFF];
		::sprintf(header, "HTTP/1.1 %d %s\r\n", 200, "OK");
		if(headers.find("Contention") != headers.end() && headers["Contention"] == "keep-alive")
		{
			keep_alive = true;
			::sprintf(header, "%sConnection: keep-alive\r\n", header);
			::sprintf(header, "%sKeep-Alive: timeout=%d\r\n", header, EPOLL_WAIT_TIME);
		}

		// filetype
		int dot_pos = file_name.find('.');
		const char *filetype = NULL;
		if(dot_pos < 0)
			filetype = MimeType::getMime("default").c_str();
		else
			filetype = MimeType::getMime(file_name.substr(dot_pos)).c_str();
		struct stat sbuf;
		if(::stat(file_name.c_str(), &sbuf) < 0)
		{
			handleError(fd, 404, "Not Found!");
			return ANALYSIS_ERROR;
		}

		::sprintf(header, "%sContent-type: %s\r\n", header, filetype);
		// 通过Content-length返回文件大小
		::sprintf(header, "%sContent-length: %ld\r\n", header, sbuf.st_size);
		::sprintf(header, "%s\r\n", header);

		size_t send_len = (size_t)::writen(fd, header, ::strlen(header));
		if(send_len != ::strlen(header))
			return ANALYSIS_ERROR;
		
		int src_fd = ::open(file_name.c_str(), O_RDONLY, 0);
		char *src_addr = static_cast<char *>(::mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0));
		::close(src_fd);

		// send file
		send_len = ::writen(fd, src_addr, sbuf.st_size);
		if((unsigned int)send_len != sbuf.st_size)
			return ANALYSIS_ERROR;
		::munmap(src_addr, sbuf.st_size);
		return ANALYSIS_SUCCESS;
	}
	else
		return ANALYSIS_ERROR;
}

void RequestData::handleError(int fd, int err_num, std::string short_msg)
{
	short_msg = " " + short_msg;
	char send_buff[MAX_BUFF];
	std::string body_buff, header_buff;

	body_buff += "<html><title>TKeed Error</title>";
	body_buff += "<body bgcolor=\"ffffff\">";
	body_buff += std::to_string(err_num) + short_msg;
	body_buff += "<hr><em>Hvate's Web Server</em>\n</body></html>";

	header_buff += "HTTP/1.1 " + std::to_string(err_num) + short_msg + "\r\n";
	header_buff += "Content-type: text/html\r\n";
	header_buff += "Connection: close\r\n";
	header_buff += "Content-length: " + std::to_string(body_buff.size()) + "\r\n";
	header_buff += "\r\n";

	::sprintf(send_buff, "%s", header_buff.c_str());
	::writen(fd, send_buff, ::strlen(send_buff));
	::sprintf(send_buff, "%s", body_buff.c_str());
	::writen(fd, send_buff, ::strlen(send_buff));
}


void mytimer::update(int timeout)
{
	std::cout << "update()" << std::endl;
	struct timeval now{};
	::gettimeofday(&now, NULL);
	long ntime = now.tv_sec * 1000 + now.tv_usec / 1000;
	std::cout << "ntime" << ntime << std::endl;
	expired_time = (now.tv_sec * 1000) + (now.tv_usec / 1000) + timeout;
	std::cout << "expired_time:" << expired_time << std::endl;
}

mytimer::mytimer(std::shared_ptr<RequestData>_request_data, int timeout) 
	: deleted(false), request_data(_request_data)
{
	std::cout << "mytimer()" << std::endl;
	this -> update(timeout);
}

mytimer::~mytimer()
{
	std::cout << "~mytimer()" << std::endl;
	if(request_data)
		Epoll::epoll_del(request_data -> getFd(), EPOLLIN | EPOLLET | EPOLLONESHOT);
}

void mytimer::setDeleted()
{
	deleted = true;
}

bool mytimer::isDeleted() const
{
	return deleted;
}

size_t mytimer::getExpTime() const
{
	return expired_time;
}

bool mytimer::isvalid()
{
	struct timeval now{};
	::gettimeofday(&now, NULL);
	size_t temp = (now.tv_sec * 1000) + (now.tv_usec / 1000);
	if(temp >= expired_time)
	{
		this -> setDeleted();
		return false;
	}
	else
		return true;
}

void mytimer::clearReq()
{
	request_data.reset();
	this -> setDeleted();
}

bool timerCmp::operator()(std::shared_ptr<mytimer> &lhs, std::shared_ptr<mytimer> &rhs) const
{
	return lhs -> getExpTime() > rhs -> getExpTime();
}


MutexLockGuard::MutexLockGuard()
{
	pthread_mutex_lock(&lock);
}

MutexLockGuard::~MutexLockGuard()
{
	pthread_mutex_unlock(&lock);
}


