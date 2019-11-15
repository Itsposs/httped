
#include <queue>
#include "util.h"
#include "epoll.h"
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include "requestdata.h"
#include <unordered_map>


pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t MimeType::lock = PTHREAD_MUTEX_INITIALIZER;
std::unordered_map<std::string, std::string> MimeType::mime;

std::string MimeType::getMime(const std::string &suffix) {
	if (mime.size() == 0) {
		pthread_mutex_lock(&lock);
    if (mime.size() == 0) {
			mime[".html"] = "text/html";
			mime[".avi"] = "video/x-msvideo";
			mime[".bmp"] = "image/bmp";
			mime[".c"] = "text/plain";
			mime[".doc"] = "application/msword";
			mime[".gif"] = "image/gif";
			mime[".gz"] = "application/x-gzip";
			mime[".htm"] = "text/html";
			mime[".ico"] = "application/x-ico";
			mime[".jpg"] = "image/jpeg";
			mime[".png"] = "image/png";
			mime[".txt"] = "text/plain";
			mime[".mp3"] = "audio/mp3";
			mime["default"] = "text/html";
		}
		pthread_mutex_unlock(&lock);
	}
  if (mime.find(suffix) == mime.end())
		return mime["default"];
	else
		return mime[suffix];
}


std::priority_queue<mytimer*, std::deque<mytimer*>, timerCmp> myTimerQueue;

requestData::requestData(): againTimes(0), now_read_pos(0), 
	state(STATE_PARSE_URI), h_state(h_start), isfinish(false), 
	keep_alive(false),timer(NULL) {}

requestData::requestData(int _epollfd, int _fd, std::string _path):
    againTimes(0), path(_path), fd(_fd), epollfd(_epollfd), 
	now_read_pos(0), state(STATE_PARSE_URI), h_state(h_start), 
	isfinish(false), keep_alive(false), timer(NULL)
{ std::cout << "requestData(*)" << std::endl; }

requestData::~requestData()
{
    std::cout << "~requestData()" << std::endl;
    struct epoll_event ev;
    // 超时的一定都是读请求，没有"被动"写
	// 先删除再清理时间，关闭套接字
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.ptr = (void*)this;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
    if (timer != NULL)
    {
        timer -> clearReq();
        timer = NULL;
    }
    close(fd);
}

void requestData::addTimer(mytimer *mtimer)
{
	std::cout << "addTimer" << std::endl;
    if (timer == NULL)
        timer = mtimer;
}

int requestData::getFd() const
{
    return fd;
}
void requestData::setFd(int _fd)
{
    fd = _fd;
}

void requestData::reset()
{
	std::cout << "reset()" << std::endl;
    againTimes = 0;
    content.clear();
    file_name.clear();
    path.clear();
    now_read_pos = 0;
    state = STATE_PARSE_URI;
    h_state = h_start;
    headers.clear();
    keep_alive = false;
}

void requestData::seperateTimer()
{
	std::cout << "seperateTimer()" << std::endl;
    if (timer)
    {
        timer -> clearReq();
        timer = NULL;
    }
}

void requestData::handleRequest()
{
	std::cout << "handleRequest()" << std::endl;
    char buff[MAX_BUFF];
    bool isError = false;

	while(true)
    {
        int read_num = readn(fd, buff, MAX_BUFF);
		std::cout << "read_num:" << read_num << std::endl;
		std::cout << "buff:" << buff << std::endl;

		if(read_num < 0)
        {
            isError = true;
            break;
        }
        else if(read_num == 0)
        {
            // 有请求出现但是读不到数据，可能是Request Aborted，或者来自网络的数据没有达到等原因
            perror("read_num == 0");
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
		//std::cout << "content:" << content << std::endl;

        if(state == STATE_PARSE_URI)
        {
            int flag = this -> parse_URI();
            if (flag == PARSE_URI_AGAIN)
            {
                break;
            }
            else if(flag == PARSE_URI_ERROR)
            {
                perror("2");
                isError = true;
                break;
            }
        }
        if(state == STATE_PARSE_HEADERS)
        {
            int flag = this -> parse_Headers();
            if(flag == PARSE_HEADER_AGAIN)
            {  
                break;
            }
            else if(flag == PARSE_HEADER_ERROR)
            {
                perror("3");
                isError = true;
                break;
            }
            if(method == METHOD_POST)
            {
                state = STATE_RECV_BODY;
            }
            else 
            {
                state = STATE_ANALYSIS;
            }
        }
        if (state == STATE_RECV_BODY)
        {
            int content_length = -1;
            if(headers.find("Content-length") != headers.end())
            {
                content_length = stoi(headers["Content-length"]);
            }
            else
            {
                isError = true;
                break;
            }
            if(content.size() < (unsigned int)content_length)
                continue;
            state = STATE_ANALYSIS;
        }
        if(state == STATE_ANALYSIS)
        {
            int flag = this -> analysisRequest();
            if(flag < 0)
            {
                isError = true;
                break;
            }
            else if(flag == ANALYSIS_SUCCESS)
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
        delete this;
        return;
    }
    // 加入epoll继续
    if(state == STATE_FINISH)
    {
        if(keep_alive)
        {
            printf("ok\n");
            this -> reset();
        }
        else
        {
            delete this;
            return;
        }
    }
    // 一定要先加时间信息，否则可能会出现刚加进去，下个in触发来了，然后分离失败后，又加入队列，最后超时被删，
	// 然后正在线程中进行的任务出错，double free错误。
    
	// 新增时间信息
    pthread_mutex_lock(&qlock);
    mytimer *mtimer = new mytimer(this, 500);
    timer = mtimer;
    myTimerQueue.push(mtimer);
    pthread_mutex_unlock(&qlock);

    __uint32_t _epo_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
    int ret = epoll_mod(epollfd, fd, static_cast<void*>(this), _epo_event);
    if(ret < 0)
    {
        // 返回错误处理
        delete this;
        return;
    }
}

int requestData::parse_URI()
{
	std::cout << "parse_URI" << std::endl;
	std::string &str = content;
    // 读到完整的请求行再开始解析请求
    int pos = str.find('\r', now_read_pos);
	std::cout << "pos:" << pos << std::endl;
	for(auto i = 0; i < pos; i++)
		std::cout << str[i];
	std::cout << std::endl;
	std::cout << "now_read_pos:" << now_read_pos << std::endl;
    
	if (pos < 0)
    {
        return PARSE_URI_AGAIN;
    }

    // 去掉请求行所占的空间，节省空间
	std::string request_line = str.substr(0, pos);
	std::cout << "request_line:" << request_line << std::endl;
    if(str.size() > (unsigned int)(pos + 1))
        str = str.substr(pos + 1);
    else 
        str.clear();
	//std::cout << "str:" << str << std::endl;
    // Method
    pos = request_line.find("GET");
    if(pos < 0)
    {
        pos = request_line.find("POST");
        if(pos < 0)
        {
            return PARSE_URI_ERROR;
        }
        else
        {
            method = METHOD_POST;
        }
    }
    else
    {
        method = METHOD_GET;
    }
    printf("method = %d\n", method);
    // filename
    pos = request_line.find("/", pos);
    if(pos < 0)
    {
        return PARSE_URI_ERROR;
    }
    else
    {
		std::cout << "pos:" << pos << std::endl;
        int _pos = request_line.find(' ', pos);
        if(_pos < 0)
            return PARSE_URI_ERROR;
        else
        {
			std::cout << "request_line:" << request_line << std::endl;
			std::cout << "_pos:" << _pos << std::endl;
            if(_pos - pos > 1)
            {
                file_name = request_line.substr(pos + 1, _pos - pos - 1);
				std::cout << "file_name:" << file_name << std::endl;
                
				int __pos = file_name.find('?');
                if(__pos >= 0)
                {
                    file_name = file_name.substr(0, __pos);
                }
            }
                
            else
                file_name = "index.html";
        }
        pos = _pos;
    }
    //cout << "file_name: " << file_name << endl;
    // HTTP 版本号
    pos = request_line.find("/", pos);
    if(pos < 0)
    {
        return PARSE_URI_ERROR;
    }
    else
    {
		std::cout << "pos:" << pos << std::endl;
        if(request_line.size() - pos <= 3)
        {
            return PARSE_URI_ERROR;
        }
        else
        {
					std::string ver = request_line.substr(pos + 1, 3);
			std::cout << "ver:" << ver << std::endl;
            if(ver == "1.0")
                HTTPversion = HTTP_10;
            else if (ver == "1.1")
                HTTPversion = HTTP_11;
            else
                return PARSE_URI_ERROR;
        }
    }
    state = STATE_PARSE_HEADERS;
    return PARSE_URI_SUCCESS;
}

int requestData::parse_Headers()
{
	std::cout << "parse_Headers" << std::endl;
	std::string &str = content;
	std::cout << "content1:" << str << std::endl;
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
                    if (key_end - key_start <= 0)
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
                {
                    h_state = h_spaces_after_colon;
                }
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
										std::string key(str.begin() + key_start, str.begin() + key_end);
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
                {
                    h_state = h_end_CR;
                }
                else
                {
                    key_start = i;
                    h_state = h_key;
                }
                break;
            }
            case h_end_CR:
            {
                if (str[i] == '\n')
                {
                    h_state = h_end_LF;
                }
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
    if (h_state == h_end_LF)
    {
        str = str.substr(now_read_line_begin);
        return PARSE_HEADER_SUCCESS;
    }
    str = str.substr(now_read_line_begin);
    return PARSE_HEADER_AGAIN;
}

int requestData::analysisRequest()
{
	std::cout << "analysisRequest" << std::endl;
    if (method == METHOD_POST)
    {
        //get content
        char header[MAX_BUFF];
        sprintf(header, "HTTP/1.1 %d %s\r\n", 200, "OK");
        if(headers.find("Connection") != headers.end() && headers["Connection"] == "keep-alive")
        {
            keep_alive = true;
            sprintf(header, "%sConnection: keep-alive\r\n", header);
            sprintf(header, "%sKeep-Alive: timeout=%d\r\n", header, EPOLL_WAIT_TIME);
        }
        //cout << "content=" << content << endl;
        // test char*
        char send_content[] = "I have receiced this.";

        sprintf(header, "%sContent-length: %zu\r\n", header, strlen(send_content));
        sprintf(header, "%s\r\n", header);

        size_t send_len = (size_t)writen(fd, header, strlen(header));
        if(send_len != strlen(header))
        {
            perror("Send header failed");
            return ANALYSIS_ERROR;
        }
        
        send_len = (size_t)writen(fd, send_content, strlen(send_content));
        if(send_len != strlen(send_content))
        {
            perror("Send content failed");
            return ANALYSIS_ERROR;
        }
        std::cout << "content size ==" << content.size() << std::endl;
				std::vector<char> data(content.begin(), content.end());
		// ???
        //Mat test = imdecode(data, CV_LOAD_IMAGE_ANYDEPTH|CV_LOAD_IMAGE_ANYCOLOR);
        //imwrite("receive.bmp", test);
        return ANALYSIS_SUCCESS;
    }
    else if (method == METHOD_GET)
    {
        char header[MAX_BUFF];
        sprintf(header, "HTTP/1.1 %d %s\r\n", 200, "OK");
        if(headers.find("Connection") != headers.end() && headers["Connection"] == "keep-alive")
        {
            keep_alive = true;
            sprintf(header, "%sConnection: keep-alive\r\n", header);
            sprintf(header, "%sKeep-Alive: timeout=%d\r\n", header, EPOLL_WAIT_TIME);
        }
        int dot_pos = file_name.find('.');
        const char* filetype;
        if (dot_pos < 0) 
            filetype = MimeType::getMime("default").c_str();
        else
            filetype = MimeType::getMime(file_name.substr(dot_pos)).c_str();
        struct stat sbuf;
        if (stat(file_name.c_str(), &sbuf) < 0)
        {
            handleError(fd, 404, "Not Found!");
            return ANALYSIS_ERROR;
        }

        sprintf(header, "%sContent-type: %s\r\n", header, filetype);
        // 通过Content-length返回文件大小
        sprintf(header, "%sContent-length: %ld\r\n", header, sbuf.st_size);

        sprintf(header, "%s\r\n", header);
        size_t send_len = (size_t)writen(fd, header, strlen(header));
        if(send_len != strlen(header))
        {
            perror("Send header failed");
            return ANALYSIS_ERROR;
        }
        int src_fd = open(file_name.c_str(), O_RDONLY, 0);
		// ???
        char *src_addr = static_cast<char*>(mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0));
        close(src_fd);
    
        // 发送文件并校验完整性
        send_len = writen(fd, src_addr, sbuf.st_size);
        if(send_len != (size_t)sbuf.st_size)
        {
            perror("Send file failed");
            return ANALYSIS_ERROR;
        }
		// ???
        munmap(src_addr, sbuf.st_size);  
        return ANALYSIS_SUCCESS;
    }
    else
        return ANALYSIS_ERROR;
}

void requestData::handleError(int fd, int err_num, std::string short_msg)
{
	std::cout << "handleError" << std::endl;
    char send_buff[MAX_BUFF];
		std::string body_buff, header_buff;

    short_msg = " " + short_msg;
    body_buff += "<html><title>TKeed Error</title>";
    body_buff += "<body bgcolor=\"ffffff\">";
    body_buff += std::to_string(err_num) + short_msg;
    body_buff += "<hr><em> LinYa's Web Server</em>\n</body></html>";

    header_buff += "HTTP/1.1 " + std::to_string(err_num) + short_msg + "\r\n";
    header_buff += "Content-type: text/html\r\n";
    header_buff += "Connection: close\r\n";
    header_buff += "Content-length: " + std::to_string(body_buff.size()) + "\r\n";
    header_buff += "\r\n";
    sprintf(send_buff, "%s", header_buff.c_str());
    writen(fd, send_buff, strlen(send_buff));
    sprintf(send_buff, "%s", body_buff.c_str());
    writen(fd, send_buff, strlen(send_buff));
}

// 改用C++11
mytimer::mytimer(requestData *_request_data, int timeout): deleted(false), request_data(_request_data)
{
    std::cout << "mytimer()" << std::endl;
    struct timeval now;
    gettimeofday(&now, NULL);
    // 以毫秒计
    expired_time = ((now.tv_sec * 1000) + (now.tv_usec / 1000)) + timeout;
}

mytimer::~mytimer()
{
    std::cout << "~mytimer()" << std::endl;
    if (request_data != NULL)
    {
        std::cout << "request_data=" << request_data << std::endl;
        delete request_data;
        request_data = NULL;
    }
}

void mytimer::update(int timeout)
{
	std::cout << "update()" << std::endl;
    struct timeval now;
    gettimeofday(&now, NULL);
    expired_time = ((now.tv_sec * 1000) + (now.tv_usec / 1000)) + timeout;
}

bool mytimer::isvalid()
{
	std::cout << "isvalid()" << std::endl;
    struct timeval now;
    gettimeofday(&now, NULL);
    size_t temp = ((now.tv_sec * 1000) + (now.tv_usec / 1000));
    if (temp < expired_time)
    {
        return true;
    }
    else
    {
        this -> setDeleted();
        return false;
    }
}

void mytimer::clearReq()
{
	std::cout << "clearReq()" << std::endl;
    request_data = NULL;
    this -> setDeleted();
}

void mytimer::setDeleted()
{
	std::cout << "setDeleted()" << std::endl;
    deleted = true;
}

bool mytimer::isDeleted() const
{
	std::cout << "isDeleted()" << std::endl;
    return deleted;
}

size_t mytimer::getExpTime() const
{
	std::cout << "getExpTime()" << std::endl;
    return expired_time;
}

bool timerCmp::operator()(const mytimer *a, const mytimer *b) const
{
    return a -> getExpTime() > b -> getExpTime();
}
