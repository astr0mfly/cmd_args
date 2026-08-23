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
 * 异常兜底工具（可选引入，非强制）：
 * - exception::Exception   带文件/行号信息的运行时异常
 * - exception::Box         rethrow / Backtrace / handleException / print_nested_exception
 * - ENSURE / EXCEPTION_THROW / PRINT_HANDLE_EXCEPTION 等快捷宏
 * - exception_init         注册纯虚调用 / 系统故障 / 线程终止处理（Windows 生成 minidump）
 *
 * 依赖 vendored airbag（崩溃兜底），示例 target 在 Windows 需 /EHa。
 */

// clang-format off
#include <functional>
#include <iostream>
#include <sstream>


#include "cmd_args/extern/airbag/process_error.hpp"
#include "cmd_args/extern/airbag/thread_error.hpp"
#include "cmd_args/extern/airbag/system_failure.hpp"
#include "cmd_args/extern/airbag/minidump.hpp"

// clang-format on

namespace exception {

namespace detail {
    // Custom exception class to be used for more practical throwing
    class Exception : public std::runtime_error
    {
    public:
        Exception(std::string const &_Message, char const *_File, size_t _Line)
            : std::runtime_error(_Message)
            , m_strMessage__(std::string(_File) + ":" + std::to_string(_Line) + " : " + _Message)
        {}

        ~Exception() = default;

        char const *what() const noexcept override { return m_strMessage__.c_str(); }

    private:
        std::string m_strMessage__;
    };

    class EnsureException : public std::exception
    {
    public:
        EnsureException(char const *_Exp, char const *_File, int _Line)
        {
            std::ostringstream oss;
            oss << "ensure failed : " << _Exp << '\n';
            oss << _File << '(' << _Line << ')' << '\n';
            oss << "context variables:\n";
            m_what = oss.str();
        }

        ~EnsureException() noexcept {}

        template <typename T>
        EnsureException &operator<<(std::pair<const char *, T> const &_Pair)
        {
            std::ostringstream oss;
            oss << '\t' << _Pair.first << " : " << _Pair.second << '\n';
            m_what += oss.str();

            return *this;
        }
        EnsureException &operator<<(int) { return *this; }

        char const *what() const throw() { return m_what.c_str(); }

    private:
        mutable std::string m_what;
    };

    struct Box
    {
        // Rethrow (creates a std::nested_exception) an exception, using the Exception class
        // which contains file and line info. The original exception is preserved...
        static void rethrow(std::string const &_Msg, char const *_File, unsigned int _Line)
        try {
            auto eptr = std::current_exception();
            if (eptr) { std::rethrow_exception(eptr); }
        }
        catch (...) {
            std::throw_with_nested(Exception(_Msg, _File, _Line));
        }

        // Backtrace an exception by recursively unwrapping the nested exceptions
        static void Backtrace(std::exception const &_Ex)
        try {
            std::cerr << _Ex.what() << std::endl;
            rethrow_if_nested(_Ex);
        }
        catch (std::exception const &_ExNested) {
            Backtrace(_ExNested);
        }

        // General Exception handler
        static void handleException(std::exception const &_Ex, std::string const &_Namefunc)
        try {
            if (_Namefunc.size()) {
                std::cerr << "Exception caught in function \'" << _Namefunc << "\'" << std::endl;
            }
            std::cerr << "Backtrace:" << std::endl;
            Backtrace(_Ex);
        }
        catch (...) {
            std::cerr << "Something went super-wrong! TERMINATING!" << std::endl;
            throw;
        }

        static void print_nested_exception(
            std::exception_ptr const &_Eptr = std::current_exception(), size_t _Level = 0)
        try {
            static auto s_fnGetNested = [](auto &_E) -> std ::exception_ptr {
                try {
                    return dynamic_cast<std::nested_exception const &>(_E).nested_ptr();
                }
                catch (const std::bad_cast &) {
                    return nullptr;
                }
            };

            try {
                if (_Eptr) std::rethrow_exception(_Eptr);
            }
            catch (std::exception const &_Ex) {
                std::cerr << std::string(_Level, ' ') << "exception: " << _Ex.what() << '\n';
                // rewind all nested exception
                print_nested_exception(s_fnGetNested(_Ex), ++_Level);
            }
        }
        catch (...) {
            std::cerr << std::string(_Level, ' ') << "TERMINATING!" << std::endl;
            std::exit(EXIT_FAILURE);
        }
    };

}  // namespace detail

using detail::Box;
using detail::Exception;

}  // namespace exception

// Shorthand for throwing an Exception with file and line info using macros
#define EXCEPTION_THROW(_Msg) throw ::exception::Exception(_Msg, __FILE__, __LINE__);

// Shorthand for rethrowing and Exception with file and line info using macros
#define EXCEPTION_RETHROW(_Msg) ::exception::Box::rethrow(_Msg, __FILE__, __LINE__);

// Shorthand for handling an exception, including a backtrace
#define PRINT_HANDLE_EXCEPTION(_Ex)     ::exception::Box::handleException(_Ex, __FUNCTION__);

#define PRINT_TRACE_EXCEPTION()         ::exception::Box::print_nested_exception()

#define EXCEPTION_RETHROW_UNSPECIFIED() std::rethrow_exception(std::current_exception())

#define PUSH_ARG(_Arg, _N)              std::make_pair(#_Arg, _Arg) << _N
#define EXPAND_A(a)                     PUSH_ARG(a, EXPAND_B)
#define EXPAND_B(a)                     PUSH_ARG(a, EXPAND_A)
#define ENSURE(_EXP)                                                                               \
    if (!(_EXP)) throw ::exception::EnsureException(#_EXP, __FILE__, __LINE__) << EXPAND_A

#undef PUSH_ARG
#undef EXPAND_A
#undef EXPAND_B

/*
    exception_init：注册一套崩溃兜底钩子（仅 Windows/MSVC 生效）：
    - 纯虚调用（on_pure_call）
    - 系统故障（pre_system_failure，生成 minidump）
    - 线程终止（on_terminate，打印 backtrace）
    在 main 里定义静态实例即可（见 example/once/main.cpp）。
*/
struct exception_init
{
#if defined(_WIN32) && !defined(__MINGW32__) && !defined(__MINGW64__)
    exception_init()
    {
        process_error.on_pure_call([] { std::cerr << "Oops: pure virtual function call\n"; });

        process_error.pre_system_failure([this](airbag::system_failure const &f) {
            std::fprintf(
                stderr,
                "Fatal that %s at %s, generating minidump at '%s'\n",
                f.title(),
                f.module_name(),
                minidump.directory().string().data());
            if (!minidump.generate(f))
                std::fprintf(
                    stderr,
                    "Unable to generate minidump: %s\n",
                    minidump.last_error().message().data());
        });
        thread_error.on_terminate([](char const *message) {
            std::cerr << "Failed to process thread that " << message << "\nBacktrace:\n";
            PRINT_TRACE_EXCEPTION();
        });
    }

    airbag::process_error process_error;
    airbag::thread_error  thread_error;
    airbag::minidump      minidump;
#endif
};

// static exception_init stExceptionInit;