

#ifndef _NONCOPYABLE_H_
#define _NONCOPYABLE_H_

namespace threadpool
{
	inline namespace util
	{
		class Noncopyable
		{
			public:
				Noncopyable(const Noncopyable &rhs) = delete;
				void operator=(const Noncopyable &rhs) = delete;
			private:
			protected:
				Noncopyable() = default;
				~Noncopyable() = default;

		};

	} // namespace util

} // namespace threadpool



#endif // _NONCOPYABLE_H_
