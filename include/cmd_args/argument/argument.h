/*
 * Copyright[2023][ValenciaFly]
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

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>

#include "cmd_args/base/util.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    Argument 基类：一条"参数"的载体（命名参数 / 方位参数共用）。
    - 名字 + 帮助 —— 构筑期填写，一经确定不再变化
    - 类型 + 默认值 —— type<T>() 定类型（空 = 未定型），构造时缓存默认值
    - 值（strValue__）—— 解析期被 CLI 赋值，或经 env() 绑定环境变量兜底，
      或经 setExternal() 被配置文件（YAML）外部赋值
    - 标记：bExternal__（值来自配置，env 兜底跳过）；
      hasValue() 即"当前已持有非空值"
    - reset() 还原默认值并清外部标记（loop 每行调用前执行，等价一次 once 输入）
*/
namespace argument_detail {

class Argument
{
public:
    // ===== 类型别名 =====
    using ptr     = std::unique_ptr<Argument>;   ///< 智能指针类型
    using Self_T = Argument;                     ///< 自身类型（CRTP基类用）
    using Name_T  = std::string;                 ///< 参数名称类型
    using Type_T  = std::string;                 ///< 参数类型标识符
    using Value_T = std::string;                 ///< 参数值类型
    using Desc_T  = std::string;                 ///< 帮助描述类型

    /// @brief 默认构造函数（参数名和帮助为空）
    Argument() = default;

    /// @brief 基础构造函数
    /// @param _Name 参数名称
    /// @param _Help 帮助文本
    Argument(Name_T _Name, Desc_T _Help)
        : strName__(std::move(_Name))
        , strHelp__(std::move(_Help))
    {}

    /// @brief 完整构造函数
    /// @param _Name 参数名称
    /// @param _Help 帮助文本
    /// @param _Type 类型标识符（空字符串表示未定型）
    /// @param _Default 默认值（同时作为初始值）
    Argument(Name_T _Name, Desc_T _Help, Type_T _Type, Value_T _Default = "")
        : strName__(std::move(_Name))
        , strHelp__(std::move(_Help))
        , strType__(std::move(_Type))
        , strDefault__(_Default)
        , strValue__(std::move(_Default))
    {}

    /// @brief 移动构造函数
    /// @param _Other 被移动的对象（资源所有权转移）
    explicit
    Argument(Argument &&_Other) noexcept
        : strName__(std::move(_Other.strName__))
        , strHelp__(std::move(_Other.strHelp__))
        , strType__(std::move(_Other.strType__))
        , strDefault__(std::move(_Other.strDefault__))
        , strValue__(std::move(_Other.strValue__))
        , strEnvName__(std::move(_Other.strEnvName__))
        , bExternal__(_Other.bExternal__)
    {}

    /// @brief 拷贝构造函数
    /// @param _Other 被拷贝的对象（深拷贝）
    Argument(Argument const &_Other) noexcept
        : strName__(_Other.strName__)
        , strHelp__(_Other.strHelp__)
        , strType__(_Other.strType__)
        , strDefault__(_Other.strDefault__)
        , strValue__(_Other.strValue__)
        , strEnvName__(_Other.strEnvName__)
        , bExternal__(_Other.bExternal__)
    {}

    auto operator=(Argument &&_Rhs) noexcept -> Self_T &
    {
        if (this == &_Rhs) return *this;

        strName__    = std::move(_Rhs.strName__);
        strHelp__    = std::move(_Rhs.strHelp__);
        strType__    = std::move(_Rhs.strType__);
        strDefault__ = std::move(_Rhs.strDefault__);
        strValue__   = std::move(_Rhs.strValue__);
        strEnvName__ = std::move(_Rhs.strEnvName__);
        bExternal__    = _Rhs.bExternal__;

        return *this;
    }

    auto operator=(Argument const &_Rhs) noexcept -> Self_T &
    {
        if (this == &_Rhs) return *this;

        strName__    = _Rhs.strName__;
        strHelp__    = _Rhs.strHelp__;
        strType__    = _Rhs.strType__;
        strDefault__ = _Rhs.strDefault__;
        strValue__   = _Rhs.strValue__;
        strEnvName__ = _Rhs.strEnvName__;
        bExternal__    = _Rhs.bExternal__;

        return *this;
    }

    operator bool() const noexcept { return !strValue__.empty(); }  ///< 隐式转换：有非空值时为 true
    
    virtual ~Argument() = default;

    bool hasValue() const noexcept { return operator bool(); }  ///< 检查是否已持有非空值
    
    /// @brief 设置参数名称（左值版本）
    auto name(Name_T &&_Name) & -> Self_T &
    {
        strName__ = _Name;
        return *this;
    }

    /// @brief 设置参数名称（右值版本，支持移动语义）
    auto name(Name_T &&_Name) && -> Self_T 
    {
        strName__ = _Name;
        return std::move(*this);
    }

    /// @brief 设置帮助文本（左值版本）
    auto help(Desc_T &&_V) & noexcept -> Self_T &
    {
        strHelp__ = std::move(_V);
        return *this;
    }

    /// @brief 设置帮助文本（右值版本，支持移动语义）
    auto help(Desc_T &&_V) && noexcept -> Self_T
    {
        strHelp__ = std::move(_V);
        return std::move(*this);
    }

    /// @brief 设置参数类型（通过类型推导）
    template <typename T>
    auto type() -> Self_T &
    {
        strType__ = util::type_string<T>();
        return *this;
    }

    /// @brief 设置参数类型（通过字符串标识）
    auto type(Type_T &&_Type) -> Self_T &
    {
        strType__ = std::move(_Type);
        return *this;
    }

    /// @brief 设置参数值（右值版本，移动语义）
    auto set(Value_T &&_V) noexcept -> Self_T &
    {
        strValue__ = std::move(_V);
        return *this;
    }

    /// @brief 设置参数值（左值版本）
    auto set(Value_T const &_V) -> Self_T &
    {
        strValue__ = _V;
        return *this;
    }

    /**
     * @brief 设置外部配置值（来自配置文件 YAML）
     * @param _V 配置值（会被移动）
     * @return Self_T& 支持链式调用
     * @note 优先级：CLI显式值 > 配置文件 > 环境变量 > 默认值
     * @note 标记 bExternal__ 为 true，使 env 兜底跳过本参数
     */
    auto setExternal(Value_T &&_V) noexcept -> Self_T &
    {
        strValue__  = std::move(_V);
        bExternal__   = true;
        return *this;
    }

    /**
     * @brief 设置外部配置值（左值版本）
     * @param _V 配置值
     * @return Self_T& 支持链式调用
     */
    auto setExternal(Value_T const &_V) -> Self_T &
    {
        strValue__ = _V;
        bExternal__  = true;
        return *this;
    }

    bool isExternal() const noexcept { return bExternal__; }  ///< 值是否来自外部配置

    /**
     * @brief 还原为内置默认值
     * @return Self_T& 支持链式调用
     * @note 还原 strValue__ 为 strDefault__，清除 bExternal__ 标记
     * @note 名称/说明/类型/env 绑定保持不变
     * @note 每次循环调用前执行，等价一次 once 输入
     */
    auto reset() noexcept -> Self_T &
    {
        strValue__ = strDefault__;
        bExternal__  = false;
        return *this;
    }

    /**
     * @brief 绑定环境变量（左值版本）
     * @param _EnvName 环境变量名
     * @return Self_T& 支持链式调用
     * @note 优先级：CLI显式值 > 环境变量 > 未设置
     */
    auto env(Name_T &&_EnvName) & -> Self_T &
    {
        strEnvName__ = std::move(_EnvName);
        return *this;
    }

    /**
     * @brief 绑定环境变量（右值版本）
     * @param _EnvName 环境变量名（会被移动）
     * @return Self_T 支持链式调用
     */
    auto env(Name_T &&_EnvName) && -> Self_T
    {
        strEnvName__ = std::move(_EnvName);
        return std::move(*this);
    }

    /**
     * @brief 绑定环境变量（const 左值版本）
     * @param _EnvName 环境变量名
     * @return Self_T& 支持链式调用
     */
    auto env(Name_T const &_EnvName) & -> Self_T &
    {
        strEnvName__ = _EnvName;
        return *this;
    }

    /**
     * @brief 绑定环境变量（const 右值版本）
     * @param _EnvName 环境变量名
     * @return Self_T 支持链式调用
     */
    auto env(Name_T const &_EnvName) && -> Self_T
    {
        strEnvName__ = _EnvName;
        return std::move(*this);
    }

    /// @brief 深拷贝当前参数对象
    auto copy() const -> Self_T { return *this; }

    /// @brief 获取参数名称（只读）
    auto getName() const noexcept -> Name_T const & { return strName__; }
    
    /// @brief 获取帮助文本（只读）
    auto getHelp() const noexcept -> Desc_T const & { return strHelp__; }

    /// @brief 获取参数类型标识（只读）
    auto getType() const noexcept -> Type_T const & { return strType__; }

    /// @brief 获取绑定的环境变量名（只读）
    auto getEnvName() const noexcept -> Name_T const & { return strEnvName__; }

    /// @brief 获取当前参数值（只读）
    auto getValue() const noexcept -> Value_T const & { return strValue__; }

private:
    // ===== 基础属性（构造期确定，不可变） =====
    Name_T  strName__;   ///< 参数名（如 "--output", "file"）
    Desc_T  strHelp__;   ///< 帮助文本
    Type_T  strType__;   ///< 类型标识（空 = 未定型）

    // ===== 运行时状态 =====
    Value_T strDefault__;            ///< 构造时的默认值（reset 还原用）
    mutable Value_T strValue__;      ///< 当前值（mutable：允许 const 方法内部修改）
    Name_T  strEnvName__;            ///< 绑定的环境变量名

    // ===== 标志位 =====
    mutable bool bExternal__ = false; ///< 值来自外部配置（YAML），env 兜底跳过
};

/*
    ArgumentNamed：命名参数，按照参数名赋值，形如 "--name=value" / "--name value"。
*/
struct ArgumentNamed : public Argument
{
public:
    ArgumentNamed() = default;

    ArgumentNamed(std::string _Name, std::string _Help)
        : Argument(_Name, _Help)
    {}

    explicit ArgumentNamed(Argument &&_Other)
        : Argument(std::move(_Other))
    {}

    ArgumentNamed(Argument const &_Other)
        : Argument(_Other)
    {}

    using Argument::help;
    using Argument::name;
    using Argument::env;
};

/**
 * @struct ArgumentPosition
 * @brief 位置参数，按声明顺序依次赋值
 *
 * 命令行解析完毕后，剩余的 token 按 ArgumentPosition 的声明顺序
 * 依次赋值。iSeq 记录声明顺序（从 0 开始），用于：
 * - 数量校验（确保提供了足够/不多的位置参数）
 * - 排序（按原始声明顺序输出）
 */
struct ArgumentPosition : public Argument
{
    using Seq_T = size_t;

    Seq_T iSeq = 0;  ///< 声明顺序索引（0 起）

    /// @brief 可变参数模板构造函数
    template <typename... Args_T>
    ArgumentPosition(Seq_T _Seq, Args_T &&..._Args)
        : Argument(std::forward<Args_T>(_Args)...)
        , iSeq(_Seq)
    {}

    /// @brief 比较运算符（按声明顺序排序）
    bool operator<(ArgumentPosition const &_Rhs) const noexcept { return iSeq < _Rhs.iSeq; }
    
    /// @brief 设置声明顺序索引
    auto No(Seq_T _Idx) -> ArgumentPosition &
    {
        iSeq = _Idx;
        return *this;
    }
    
    /// @brief 获取声明顺序索引
    auto getSeq() const noexcept -> Seq_T { return iSeq; }
};

}  // namespace detail

using argument_detail::Argument;
using argument_detail::ArgumentNamed;
using argument_detail::ArgumentPosition;

CMD_ARGS_NAMESPACE_END