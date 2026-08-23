/*
 * Copyright [2023] [ValenciaFly] <pengwanring@live.com>
 *
 *     Licensed under the Apache License,
 *     Version 2.0(the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

/*
    RAII defer 工具（类似 Go 的 defer / 各类 scope_exit）：
    - Defer      构造时接收 lambda，析构时执行（不可取消、不可拷贝）
    - UniqueDefer 多一个 dismiss()：可取消析构时的执行
    - DEFER(e)/UNIQUE_DEFER(e)    宏：当前作用域结束执行 e
    - DEFER_/UNIQUE_DEFER_        宏：lambda 体写法，见下方用例
*/
namespace detail {

template <typename F>
class Defer
{
public:
    Defer(F &&_Func)
        : m_fnRun__(std::forward<F>(_Func))
    {}
    ~Defer()
    {
        if (m_fnRun__) m_fnRun__();
    }

private:
    using Func_T                    = typename std::remove_reference<F>::type;
    Defer(Defer const &)            = delete;
    Defer &operator=(Defer const &) = delete;

    Func_T m_fnRun__;
};

template <typename F>
class UniqueDefer
{
public:
    UniqueDefer(F &&_Func)
        : m_fnRun__(std::forward<F>(_Func))
    {}
    ~UniqueDefer()
    {
        if (!m_bCancel__ && m_fnRun__) m_fnRun__();
    }
    /* 取消执行（之前设置的 lambda 将不再运行） */
    void dismiss() { m_bCancel__ = true; }

private:
    using Func_T = typename std::remove_reference<F>::type;

    UniqueDefer(UniqueDefer const &)            = delete;
    UniqueDefer &operator=(UniqueDefer const &) = delete;

    bool   m_bCancel__ = false;
    Func_T m_fnRun__;
};

/* 供 DEFER_ 宏把 lambda 与 Defer 组合起来的空标记类型 */
enum class DeferHelper
{
};

template <typename F>
inline Defer<F> operator+(DeferHelper, F &&_Fn)
{
    return Defer<F>(std::forward<F>(_Fn));
}

/* 供 UNIQUE_DEFER_ 宏使用的空标记类型 */
enum class UniqueDeferHelper
{
};

template <typename F>
inline UniqueDefer<F> operator+(UniqueDeferHelper, F &&_Fn)
{
    return UniqueDefer<F>(std::forward<F>(_Fn));
}

}  // namespace detail

/* 编程式创建：auto d = make_defer([&]{ ... }); */
template <typename F>
inline detail::Defer<F> make_defer(F &&_Func)
{
    return detail::Defer<F>(std::forward<F>(_Func));
}

/* 编程式创建（可取消版） */
template <typename F>
inline detail::UniqueDefer<F> make_unique_defer(F &&f)
{
    return detail::UniqueDefer<F>(std::forward<F>(f));
}

#define _defer_name_cat(x, n)  x##n
#define _defer_make_name(x, n) _defer_name_cat(x, n)
#define _defer_name            _defer_make_name(_defer_, __LINE__)

/* 用法：DEFER(close_file(f)); —— 作用域结束执行 close_file(f) */
#define DEFER(e)               auto _defer_name = make_defer([&]() { e; })
/* 用法：UNIQUE_DEFER(close_file(f)); —— 可调 dismiss() 取消 */
#define UNIQUE_DEFER(e)        auto _defer_name = make_unique_defer([&]() { e; })

/* 用法：DEFER_ { ... }; —— 多语句 lambda 体写法（语句以 } 结束，无需分号歧义） */
#define DEFER_                 auto _defer_name = detail::DeferHelper() + [&]()
#define UNIQUE_DEFER_          auto _defer_name = detail::UniqueDeferHelper() + [&]()