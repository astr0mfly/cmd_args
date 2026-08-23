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
    std::string type;   // bool, int, double, std::string
    mutable std::string value;  // the defaut value

    // 是否被命令行显式赋值过（区别于默认值/环境变量兜底）
    mutable bool bExplicit__ = false;

    // 绑定的环境变量名：命令行未显式给出时，用该变量的值兜底
    std::string strEnvName__;

    // 值来自外部配置（YAML）而非命令行/环境变量：env 兜底跳过
    bool bExternal__ = false;

    // 懒注册的内置交互选项（--env-set 等）：不参与参数格式导出/配置序列化
    bool bBuiltin__ = false;

    // 构造时的内置默认值（loop reset 还原用）
    std::string strDefault__;

    // 自定义 setter 钩子：命令行赋值成功后回调（用于 --env-set 等内置交互选项）
    mutable std::function<void(std::string const &)> fnOnSet__;

    option_value(
        char sname, std::string lname, std::string help, std::string type, std::string value)
        : option(sname, std::move(lname), std::move(help))
        , type(std::move(type))
        , value(value)
        , strDefault__(std::move(value))
    {}
    option_value(std::string lname, std::string help, std::string type, std::string value)
        : option(0, std::move(lname), (std::move(help)))
        , type(std::move(type))
        , value(value)
        , strDefault__(std::move(value))
    {}

    /* 绑定环境变量：命令行显式值 > 环境变量 > 内置默认值 */
    option_value &env(std::string _EnvName)
    {
        strEnvName__ = std::move(_EnvName);
        return *this;
    }
    option_value &on_set(std::function<void(std::string const &)> _Fn)
    {
        fnOnSet__ = std::move(_Fn);
        return *this;
    }

    /* 外部赋值（来自配置文件 YAML）：写入值并标记 bExternal__ */
    option_value &set_external(std::string const &_V)
    {
        value      = _V;
        bExternal__ = true;
        return *this;
    }
    option_value &set_external(std::string &&_V)
    {
        value       = std::move(_V);
        bExternal__ = true;
        return *this;
    }

    /* 还原为内置默认、清除显式/外部标记（loop 每次调用前调用） */
    option_value &reset() noexcept
    {
        value       = strDefault__;
        bExplicit__ = false;
        bExternal__ = false;
        return *this;
    }
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