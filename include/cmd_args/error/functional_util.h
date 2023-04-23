#pragma once

#include<type_traits>
#include<iostream>
#include<string>
#include<map>

namespace func_util{

template<typename T>
class Optional
{
	using data_t = typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type;
public:
	Optional()
		: m_bHasInit__(false)
	{ }

	Optional(T const &_V)
		: Optional()
	{
		Create(_V);
	}
 
	Optional(T &&_V)
		: Optional()
	{
		Create(std::move(_V));
	}
 
	Optional(Optional &&_Other)
		: Optional()
	{
		if (_Other.IsInit()) {
			Assign(std::move(_Other));
			_Other.Destroy();
		}
	}
 
	Optional(Optional const &_Other)
		: Optional()
	{
		if (_Other.IsInit()) Assign(_Other);
	}

	Optional& operator=(Optional &&_Other)
	{
		Assign(std::move(_Other));

		return *this;
	}
 
	Optional& operator=(const Optional &_Other)
	{
		Assign(_Other);
		return *this;
	}
 	~Optional() { Destroy(); }

	template<class... Args>
	void emplace(Args&&... args)
	{
		Destroy();
		Create(std::forward<Args>(args)...);
	}
 
	bool IsInit() const { return m_bHasInit__; }
 
	operator bool() const { return IsInit(); }
 
	T &operator*()
	{
		if (IsInit()) return *((T*)(&m_Data__));
 
		throw std::logic_error("is not init");
	}
 
	T const &operator *() const
	{
		if (IsInit()) return *((T*)(&m_Data__));
 
		throw std::logic_error("is not init");
	}
 
	bool operator == (const Optional<T>& rhs) const
	{
		return (!bool(*this)) != (!rhs) ? false : (!bool(*this) ? true : (*(*this)) == (*rhs));
	}
 
	bool operator < (const Optional<T>& rhs) const
	{
		return !rhs ? false : (!bool(*this) ? true : (*(*this) < (*rhs)));
	}
 
	bool operator != (const Optional<T>& rhs)
	{
		return !(*this == (rhs));
	}

private:
	template<class... Args>
	void Create(Args&&... args)
	{
		new (&m_Data__) T(std::forward<Args>(args)...);
		m_bHasInit__ = true;
	}
 
	void Destroy()
	{
		if (m_bHasInit__) {
			m_bHasInit__ = false;
			((T*)(&m_Data__))->~T();
		}
	}
 
	void Assign(const Optional& other)
	{
		if (other.IsInit()) {
			Copy(other.m_Data__);
			m_bHasInit__ = true;
		}
		else {
			Destroy();
		}
	}
 
	void Assign(Optional&& other)
	{
		if (other.IsInit()) {
			Move(std::move(other.m_Data__));
			m_bHasInit__ = true;
			other.Destroy();
		}
		else {
			Destroy();
		}
	}
 
	void Move(data_t&& val)
	{
		Destroy();
		new (&m_Data__) T(std::move(*((T*)(&val))));
	}
 
	void Copy(const data_t& val)
	{
		Destroy();
		new (&m_Data__) T(*((T*)(&val)));
	}
 
private:
	bool m_bHasInit__;
	data_t m_Data____;
};


#include<memory>
#include<functional>
template<typename T>
struct Lazy
{
	Lazy()=default;
	//保存需要延迟执行的函数
	template<typename Func, typename ...Args>
	Lazy(Func& f, Args&&... args)
	{ //给出需要调用的函数和参数，封装起来。等待之后被调用
		m_func = [&f, &args...]{ return f(args...); };
	}
	//延迟执行，将结果放到optional中缓存起来，下次不用重新计算可以直接得到结果
	T& Value()
	{
		if (!m_value.IsInit()) {
			m_value = m_func();
		}

		return *m_value;
	}
 
	bool IsValueCreated() const { return m_value.IsInit(); }
private:
	std::function<T()> m_func;		//返回值类型为T的无参可调用对象 m_func
	Optional<T> m_value;	
};
 
//定义一个模板函数，返回值类型为 Lazy
template<class Func, typename... Args>
auto lazy(Func&& fun, Args&& ...args)->Lazy<typename std::result_of<Func(Args...)>::type>
{
	return Lazy<typename std::result_of<Func(Args...)>::type>(std::forward<Func>(fun), std::forward<Args>(args)...);
}

}