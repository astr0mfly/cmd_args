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

#include <functional>
#include <iostream>
#include <sstream>

#include "cmd_args/extern/process_error.h"
#include "cmd_args/extern/system_failure.h"
#include "cmd_args/extern/thread_error.h"

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
        void rethrow(const std::string &message, const char *file, unsigned int line)
        try {
            auto eptr = std::current_exception();
            if (eptr) { std::rethrow_exception(eptr); }
        }
        catch (...) {
            std::throw_with_nested(Exception(message, file, line));
        }

        // Backtrace an exception by recursively unwrapping the nested exceptions
        void Backtrace(std::exception const &ex)
        try {
            std::cerr << ex.what() << std::endl;
            rethrow_if_nested(ex);
        }
        catch (std::exception const &nested_ex) {
            Backtrace(nested_ex);
        }

        // General Exception handler
        void handleException(std::exception const &ex, std::string const &function)
        try {
            if (function != "")
                std::cerr << "Exception caught in function \'" << function << "\'" << std::endl;
            std::cerr << "Backtrace:" << std::endl;
            Backtrace(ex);
        }
        catch (...) {
            std::cerr << "Something went super-wrong! TERMINATING!" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        void print_nested_exception(std::exception_ptr const &eptr, size_t level)
        try {
            static auto get_nested = [](auto &e) -> std ::exception_ptr {
                try {
                    return dynamic_cast<std::nested_exception const &>(e).nested_ptr();
                }
                catch (const std::bad_cast &) {
                    return nullptr;
                }
            };

            try {
                if (eptr) std::rethrow_exception(eptr);
            }
            catch (const std::exception &e) {
                std::cerr << std::string(level, ' ') << "exception: " << e.what() << '\n';
                print_nested_exception(get_nested(e), ++level);  // rewind all nested exception
            }
        }
        catch (...) {
            std::cerr << std::string(level, ' ') << "TERMINATING!" << std::endl;
            throw;
        }
    };

}  // namespace detail

}  // namespace exception

// Shorthand for throwing an Exception with file and line info using macros
#define EXCEPTION_THROW(_MSG) throw ::exception::Exception(_MSG, __FILE__, __LINE__);
// Shorthand for rethrowing and Exception with file and line info using macros
#define EXCEPTION_RETHROW(_MSG) ::exception::rethrow(_MSG, __FILE__, __LINE__);
// Shorthand for handling an exception, including a backtrace
#define PRINT_HANDLE_EXCEPTION(_Ex)     ::exception::handleException(_Ex, __FUNCTION__);
#define PRINT_TRACE_EXCEPTION()         ::exception::print_nested_exception()
#define EXCEPTION_RETHROW_UNSPECIFIED() std::rethrow_exception(std::current_exception())

#define PUSH_ARG(_Arg, _N)              std::make_pair(#_Arg, _Arg) << _N
#define EXPAND_A(a)                     PUSH_ARG(a, EXPAND_B)
#define EXPAND_B(a)                     PUSH_ARG(a, EXPAND_A)
#define ENSURE(_EXP)                                                                               \
    if (!(_EXP)) throw ::exception::EnsureException(#_EXP, __FILE__, __LINE__) << EXPAND_A

#undef PUSH_ARG
#undef EXPAND_A
#undef EXPAND_B

struct exception_init
{
    exception_init()
    {
        process_error.on_pure_call([] { std::cout << "Failed that pure virtual function call\n"; });

        process_error.pre_system_failure([](airbag::system_failure const &f) {
            std::cerr << "Fatal that " << f.title() << " at " << f.module_name()
                      << ", generating minidump at '" << minidump.directory().string().data()
                      << "'\n";

            if (!minidump.generate(f))
                std::cerr << "Unable to generate minidump: "
                          << minidump.last_error().message().data() << std::endl;
        });

        thread_error.on_terminate([](char const *message) {
            std::cerr << "Failed to process thread that " << message << "\nBacktrace:\n";
            PRINT_TRACE_EXCEPTION();
        });
    }

    airbag::process_error process_error;
    airbag::thread_error  thread_error;
    airbag::minidump      minidump;
};

// static exception_init stExceptionInit;