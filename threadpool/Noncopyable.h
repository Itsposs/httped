

#ifndef _NONCOPYABLE_H_
#define _NONCOPYABLE_H_


inline namespace tp
{
	// inline namespace util
	namespace util
	{
		class Noncopyable
		{
			public:
				Noncopyable(const Noncopyable &rhs) = delete;
				Noncopyable& operator=(const Noncopyable &rhs) = delete;
			private:
			protected:
				Noncopyable() = default;
				~Noncopyable() = default;

		};

	} // namespace util

} // namespace threadpool



#endif // _NONCOPYABLE_H_
