
#ifndef _NONCOPYABLE_H_
#define _NONCOPYABLE_H_

class noncopyable
{
	private:
		noncopyable(const noncopyable&);
		const noncopyable& operator=(const noncopyable&);
	protected:
		noncopyable() { }
		~noncopyable() { }
};






#endif // _NONCOPYABLE_H_
