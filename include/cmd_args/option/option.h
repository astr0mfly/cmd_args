/*
 *Copyright [2023] [ValenciaFly]
 * Email: pengwanring@live.com
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

#include <algorithm>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace detail {
struct option
{
    char        short_name;  // "-t"
    std::string long_name;   // "--test"
    std::string help;        // "it's a test"
    option(char sname, std::string lname, std::string help)
        : short_name(std::move(sname))
        , long_name(std::move(lname))
        , help(std::move(help))
    {}
    virtual ~option() = default;
};
/*
option:
FIXME 对于 bool选项，不需要默认值，有对应的命令行参数就是TRUE
其他类型的添加需要默认值
*/
struct option_value : option
{
    std::string         type;   // bool, int, double, std::string
    mutable std::string value;  // the defaut value

    option_value(
        char sname, std::string lname, std::string help, std::string type, std::string value)
        : option(sname, std::move(lname), std::move(help))
        , type(std::move(type))
        , value(std::move(value))
    {}
    option_value(std::string lname, std::string help, std::string type, std::string value)
        : option(0, std::move(lname), (std::move(help)))
        , type(std::move(type))
        , value(std::move(value))
    {}
};

/*
short_circuit_option:
优先级比option高，先得到响应，同时需要一个回调函数，代表行动，之后立即退出程序
*/
struct option_action : option
{
    std::function<void(void)> callback;

    option_action(
        char sname, std::string lname, std::string help, std::function<void(void)> callback)
        : option(sname, std::move(lname), (std::move(help)))
        , callback(std::move(callback))
    {}
    option_action(std::string lname, std::string help, std::function<void(void)> callback)
        : option(0, std::move(lname), (std::move(help)))
        , callback(std::move(callback))
    {}
};

}  // namespace detail

using detail::option;
using detail::option_action;
using detail::option_value;

CMD_ARGS_NAMESPACE_END