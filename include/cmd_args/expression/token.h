/*
 * Copyright[2026][ValenciaFly]
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

#include <cstddef>
#include <string>
#include <vector>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

/**
 * @brief 公共词法单元：Scanner 产出的最小语义原子。
 *
 * 词法分析器将字节流切分为 Token 序列后，
 * ExpressionRecognizer 以 Token 为最小单元进行模式匹配，组装完整 Expression。
 * 每个 Token 携带类型（Type_E）、名称/内容、行位置以及行内等号标记（hasInlineEq）。
 */
class Token
{
public:
    using Name_T = std::string;

    /// @brief 关键字种类（Scanner 关键字匹配用）
    enum class KeyWord_E : char
    {
        Env_,     ///< 读取环境变量
        File_,    ///< 读取文件
        Plugin_,  ///< 读取脚本
    };

    /// @brief 词法单元种类
    enum class Type_E : char
    {
        Unknown,        ///< 未知类型
        LongName,       ///< --name（长名词，不含 "--" 前缀）
        ShortName,      ///< -x（单个短名，不含 "-" 前缀）
        ContinualName,  ///< -xyz / -abc（聚合短名串，不含 "-" 前缀）
        Variable,       ///< [a-zA-Z_][\w]*（普通变量/值）
        Integer,        ///< [0-9]+（整数）
        Real,           ///< 实数（含科学计数法）
        String,         ///< 带引号的字符串 / Env 值
        EnvName,        ///< 环境变量名（兼容保留）
        Delimiter,      ///< 单字符分隔符（? : = > < & | 等）
        ScopeSep,       ///< 环境空间级联分隔符（CMD_ARGS_ENV_SCOPE_SEP，默认 "::"）
        Keyword         ///< 关键字（env / file / plugin）
    };

    /// @brief 默认构造（Unknown 类型，operator bool 为 false）
    Token() = default;

    /// @brief 按类型、名称、行号构造
    Token(Type_E _T, Name_T _Content, std::size_t _Line)
        : enAttr__(_T)
        , strName__(std::move(_Content))
        , locLine__(_Line)
    {}

    /// @brief 是否有效（非 Unknown）
    operator bool() const noexcept { return enAttr__ != Type_E::Unknown; }

    /// @brief 获取词法单元种类
    auto getAttr() const noexcept -> Type_E { return enAttr__; }

    /// @brief 获取名称/内容（不含前缀符号）
    auto getName() const noexcept -> Name_T const & { return strName__; }

    /// @brief 获取名称末尾字符（用于双字符分隔符检测）
    auto back() const noexcept -> char { return strName__.empty() ? '\0' : strName__.back(); }

    /// @brief 设置行号
    void setLoc(std::size_t _Line) { locLine__ = _Line; }

    /// @brief 设置词法单元种类
    void setAttr(Type_E _A) { enAttr__ = _A; }

    /// @brief 吸收一个字符到名称末尾
    void pick(char _C) { strName__ += _C; }

    /// @brief 清空名称缓冲
    void resetName() { strName__.clear(); }

    /// @brief 设置行内等号标记（--name=value 中 '=' 直接结束长名词）
    void setInlineEq(bool _V) { bInlineEq__ = _V; }

    /// @brief 是否由 '=' 内联分隔
    bool hasInlineEq() const noexcept { return bInlineEq__; }

    /// @brief 复位为 Unknown 状态
    void clear()
    {
        enAttr__    = Type_E::Unknown;
        strName__.clear();
        locLine__   = 0;
        bInlineEq__ = false;
    }

    /// @brief 获取行位置
    auto getLoc() const noexcept -> std::size_t { return locLine__; }

    /**
     * @brief 归一化为命令行文本
     * @note LongName → "--name"；ShortName/ContinualName → "-x"；ScopeSep → 编译期分隔符（默认 "::"）；其余保持原样
     */
    auto toText() const -> std::string
    {
        switch (enAttr__) {
        case Type_E::LongName:     return "--" + strName__;
        case Type_E::ShortName:
        case Type_E::ContinualName: return "-" + strName__;
        case Type_E::ScopeSep:     return ENV_SCOPE_SEP;
        default:                   return strName__;
        }
    }

private:
    Type_E      enAttr__    = Type_E::Unknown;
    Name_T      strName__;
    std::size_t locLine__   = 0;
    bool        bInlineEq__ = false;
};

/// @brief 类型化 Token 流（Scanner 输出 / ExpressionRecognizer 输入）
using TokenStream_T = std::vector<Token>;

CMD_ARGS_NAMESPACE_END
