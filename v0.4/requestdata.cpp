
#include <sys/time.h> // gettimeofday()
#include <deque>
#include "util.h"
#include <string.h>  // strlen()
#include <sys/mman.h> // mmap()

// errno
#include <errno.h>

// signal
//#include <signal.h>

// stoi
#include <string>

// epoll
#include "epoll.h"
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
#include "requestdata.h"



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
pthread_once_t MimeType::once_control = PTHREAD_ONCE_INIT;
std::unordered_map<std::string, std::string> MimeType::mime;

void MimeType::init()
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

std::string MimeType::getMime(const std::string &suffix)
{
	::pthread_once(&once_control, MimeType::init);
	if(mime.find(suffix) == mime.end())
		return mime["default"];
	else
		return mime[suffix];
}

// test
#include <iostream>



RequestData::RequestData() :
	now_read_pos(0),
	state(STATE_PARSE_URI),
	h_state(h_start),
	keep_alive(false),
	isAbleRead(true),
	isAbleWrite(false),
	events(0),
	error(false)
{
	std::cout << "RequestData()" << std::endl;
}

RequestData::RequestData(int _epollfd, int _fd, std::string _path) :
	now_read_pos(0),
	state(STATE_PARSE_URI),
	h_state(h_start),
	keep_alive(false),
	path(_path),
	fd(_fd),
	epollfd(_epollfd),
	isAbleRead(true),
	isAbleWrite(false),
	events(0),
	error(false)
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
	::close(fd);
}

void RequestData::linkTimer(std::shared_ptr<TimerNode> mtimer)
{
	// shared_ptr重载了bool,但weak_ptr没有
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
	inBuffer.clear();
	file_name.clear();
	path.clear();
	now_read_pos = 0;
	state = STATE_PARSE_URI;
	h_state = h_start;
	headers.clear();
	//keep_alive = false;

	if(timer.lock())
	{
		 std::shared_ptr<TimerNode> my_timer(timer.lock());
		 my_timer -> clearReq();
		 timer.reset();
	}
}

void RequestData::seperateTimer()
{
	if(timer.lock())
	{
		 std::shared_ptr<TimerNode> my_timer(timer.lock());
		 my_timer -> clearReq();
		 timer.reset();
	}
}


void RequestData::handleRead()
{
	do
	{
		int read_num = ::readn(fd, inBuffer);
		if(read_num < 0)
		{
			error = true;
			handleError(fd, 400, "Bad Request");
			break;
		}
		else if(read_num == 0)
		{
			// 有请求出现但是读不到数据,可能是Request Aborted,
			// 或者来自网络的数据没有到达等原因
			error = true;
			break;
		}

		if(state == STATE_PARSE_URI)
		{
			int flag = this -> parse_URI();
			if(flag == PARSE_URI_AGAIN)
				break;
			else if(flag == PARSE_URI_ERROR)
			{
				error = true;
				handleError(fd, 400, "Bad Request");
				break;
			}
			else
				state = STATE_PARSE_HEADERS;
		}

		if(state == STATE_PARSE_HEADERS)
		{
			int flag = this -> parse_Headers();
			if(flag == PARSE_HEADER_AGAIN)
				break;
			else if(flag == PARSE_HEADER_ERROR)
			{
				error = true;
				handleError(fd, 400, "Bad Request");
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
				error = true;
				handleError(fd, 400, "Bad Request: Lack of argument(Content-length)");
				break;
			}
			if(inBuffer.size() < content_length)
				break;
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
				error = true;
				break;
			}
		}
	}while(false);

	if(!error)
	{
		if(outBuffer.size() > 0)
			events |= EPOLLOUT;
		// 加入epoll继续
		if(state == STATE_FINISH)
		{
			if(!keep_alive)
				return;
			else
			{
				this -> reset();
				events |= EPOLLIN;
			}
		}
		else
			events |= EPOLLIN;
	}
}

void RequestData::handleWrite()
{
	if(!error)
	{
		if(::writen(fd, outBuffer) < 0)
		{
			events = 0;
			error = true;
		}
		else if(outBuffer.size() > 0)
			events |= EPOLLOUT;
	}
}

void RequestData::handleConn()
{
	if(!error)
	{
		if(events != 0)
		{
			// 一定要先加时间信息,否则可能会出现刚加进去,下一个读请求触发,然后
			// 分离失败后,又加入队列,最后超时被删除,然后正在线程中进行任务出错,
			// 二次释放错误.
			int timeout = 2000;
			if(keep_alive)
				timeout = 5 * 60 * 1000;
			isAbleRead = false;
			isAbleWrite = false;
			Epoll::add_timer(shared_from_this(), timeout);
			if((events & EPOLLIN) && (events & EPOLLOUT))
			{
				events = __uint32_t(0);
				events |= EPOLLOUT;
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
			
			events |= (EPOLLET | EPOLLONESHOT);
			__uint32_t _events = events;
			events = 0;
			if(Epoll::epoll_mod(fd, shared_from_this(), _events) < 0)
				perror("Epoll::epoll_mod error");
		}
	}
	else if(keep_alive)
	{
		events |= (EPOLLIN | EPOLLET | EPOLLONESHOT);
		int timeout = 5 * 60 * 1000;
		isAbleRead = false;
		isAbleWrite = false;
		Epoll::add_timer(shared_from_this(), timeout);
		__uint32_t _events = events;
		events = 0;
		if(Epoll::epoll_mod(fd, shared_from_this(), _events) < 0)
			perror("Epoll::epoll_mod error");
	}
}

int RequestData::parse_URI()
{
	std::string &str = inBuffer;
	// 读到完整的请求行再开始解析请求
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
	return PARSE_URI_SUCCESS;
}

int RequestData::parse_Headers()
{
	std::string &str = inBuffer;
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
		// get inBuffer
		std::string header;
		header += std::string("HTTP/1.1 200 OK\r\n");
		if(headers.find("Connection") != headers.end() && headers["Connection"] == "keep-alive")
		{
			keep_alive = true;
			header += std::string("Connection: keep-alive\r\n");
			header += std::string("Keep-Alive: timeout=");
			header += std::to_string(5 * 60 * 1000) + "\r\n";
		}
		
		int length = stoi(headers["Content-length"]);
		std::vector<char> data(inBuffer.begin(), inBuffer.begin() + length);
		cv::Mat src = cv::imdecode(data, CV_LOAD_IMAGE_ANYDEPTH | CV_LOAD_IMAGE_ANYCOLOR);
		cv::imwrite("receive.bmp", src);
		cv::Mat res = stitch(src);
		std::vector<uchar> data_encode;
		cv::imencode(".png", res, data_encode);

		header += std::string("Content-length:") + std::to_string(data_encode.size()) + "\r\n\r\n";
		outBuffer += header + std::string(data_encode.begin(), data_encode.end());
		inBuffer = inBuffer.substr(length);
		return ANALYSIS_SUCCESS;
	}
	else if(method == METHOD_GET)
	{
		std::string header;
		header +="HTTP/1.1 200 OK\r\n";
		if(headers.find("Contention") != headers.end() && headers["Contention"] == "keep-alive")
		{
			keep_alive = true;
			header += std::string("Connection: keep-alive\r\n");
			header += std::string("Keep-Alive: timeout=");
			header += std::to_string(5 * 60 * 1000) + "\r\n";
		}

		// filetype
		int dot_pos = file_name.find('.');
		std::string filetype;
		if(dot_pos < 0)
			filetype = MimeType::getMime("default");
		else
			filetype = MimeType::getMime(file_name.substr(dot_pos));
		struct stat sbuf;
		if(::stat(file_name.c_str(), &sbuf) < 0)
		{
			header.clear();
			handleError(fd, 404, "Not Found!");
			return ANALYSIS_ERROR;
		}

		header += "Content-type: " + filetype + "\r\n";
		// 通过Content-length返回文件大小
		header += "Content-length: " + std::to_string(sbuf.st_size) + "\r\n";
		// 头部结束
		header += "\r\n";
		outBuffer += header;

		int src_fd = ::open(file_name.c_str(), O_RDONLY, 0);
		char *src_addr = static_cast<char *>(::mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0));
		::close(src_fd);

		// send file
		outBuffer += src_addr;
		//send_len = ::writen(fd, src_addr, sbuf.st_size);
		//if((unsigned int)send_len != sbuf.st_size)
		//	return ANALYSIS_ERROR;
		//::munmap(src_addr, sbuf.st_size);
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

	body_buff += "<html><title>哎～出错了!</title>";
	body_buff += "<body bgcolor=\"ffffff\">";
	body_buff += std::to_string(err_num) + short_msg;
	body_buff += "<hr><em>Hvate's Web Server</em>\n</body></html>";

	header_buff += "HTTP/1.1 " + std::to_string(err_num) + short_msg + "\r\n";
	header_buff += "Content-type: text/html\r\n";
	header_buff += "Connection: close\r\n";
	header_buff += "Content-length: " + std::to_string(body_buff.size()) + "\r\n";
	header_buff += "\r\n";
	
	// 错误处理不考虑writen不完的情况
	::sprintf(send_buff, "%s", header_buff.c_str());
	::writen(fd, send_buff, ::strlen(send_buff));
	::sprintf(send_buff, "%s", body_buff.c_str());
	::writen(fd, send_buff, ::strlen(send_buff));
}

void RequestData::disableReadAndWrite()
{
	isAbleRead = false;
	isAbleWrite = false;
}

void RequestData::enableRead()
{
	isAbleRead = true;
}

void RequestData::enableWrite()
{
	isAbleWrite = true;
}

bool RequestData::canRead()
{
	return isAbleRead;
}

bool RequestData::canWrite()
{
	return isAbleWrite;
}


