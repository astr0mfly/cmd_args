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

#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <functional>
#include <initializer_list>
#include <regex>
#include <utility>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"
#include "cmd_args/error/builder_error.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace validator_detail {

/* 辅助：全部为 ASCII 数字 */
inline bool allDigits(std::string const &_S) noexcept
{
    if (_S.empty()) return false;
    for (char c : _S) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

/// @brief 端口：1..65535 十进制整数
inline bool is_port(std::string const &_S) noexcept
{
    if (_S.empty() || _S.size() > 5 || !allDigits(_S)) return false;
    unsigned long v = std::strtoul(_S.c_str(), nullptr, 10);
    return v >= 1 && v <= 65535;
}

/// @brief IPv4：四段十进制 0..255（允许前导零）
inline bool is_ipv4(std::string const &_S) noexcept
{
    if (_S.empty()) return false;
    auto parts = util::split(_S, '.');
    if (parts.size() != 4) return false;
    for (auto const &p : parts) {
        if (p.empty() || p.size() > 3 || !allDigits(p)) return false;
        if (std::strtoul(p.c_str(), nullptr, 10) > 255) return false;
    }
    return true;
}

/// @brief IPv6：8 组 hex 或 '::' 压缩（至多一次），允许内嵌 IPv4 尾段
inline bool is_ipv6(std::string const &_S) noexcept
{
    if (_S.empty()) return false;
    std::size_t i = 0, n = _S.size();
    std::size_t groups  = 0;
    bool        compressed = false;
    bool        ipv4Tail   = false;

    while (i < n) {
        if (_S[i] == ':') {
            if (i + 1 < n && _S[i + 1] == ':') {
                if (compressed) return false;   // 只允许一次压缩
                compressed = true;
                i += 2;                          // 尾部 "::" 合法（如 "1::"）
            }
            else {
                ++i;
                if (i == n) return false;        // 单冒号结尾非法
            }
            continue;
        }
        if (ipv4Tail) return false;              // IPv4 之后不再允许组
        std::size_t j = i;
        while (j < n && _S[j] != ':') ++j;
        std::string seg = _S.substr(i, j - i);
        if (seg.find('.') != std::string::npos) {
            if (j != n || !is_ipv4(seg)) return false;  // IPv4 段必须位于末尾
            ipv4Tail = true;
            groups += 2;
        }
        else {
            if (seg.empty() || seg.size() > 4) return false;
            for (char c : seg) {
                if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
            }
            ++groups;
        }
        i = j;
    }

    if (compressed) return groups <= 7;   // 压缩减至 >=1 组
    return groups == 8;
}

/// @brief float：完整解析且有限（拒绝溢出/NaN/Inf/尾随字符）
inline bool is_float(std::string const &_S) noexcept
{
    std::string t = util::trim(_S);
    if (t.empty()) return false;
    char *end = nullptr;
    errno = 0;
    float v = std::strtof(t.c_str(), &end);
    return end != t.c_str() && *end == '\0' && errno != ERANGE && std::isfinite(v);
}

/// @brief double：完整解析且有限
inline bool is_double(std::string const &_S) noexcept
{
    std::string t = util::trim(_S);
    if (t.empty()) return false;
    char *end = nullptr;
    errno = 0;
    double v = std::strtod(t.c_str(), &end);
    return end != t.c_str() && *end == '\0' && errno != ERANGE && std::isfinite(v);
}

/// @brief 日期：YYYY-MM-DD 且为真实日历（含闰年）
inline bool is_date(std::string const &_S) noexcept
{
    if (_S.size() != 10 || _S[4] != '-' || _S[7] != '-') return false;
    for (std::size_t i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(_S[i]))) return false;
    }
    int y = std::atoi(_S.substr(0, 4).c_str());
    int m = std::atoi(_S.substr(5, 2).c_str());
    int d = std::atoi(_S.substr(8, 2).c_str());
    if (m < 1 || m > 12) return false;
    static const int days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int md = days[m - 1];
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) md = 29;
    return d >= 1 && d <= md;
}

/// @brief 时长：数字[.小数] + 单位 s/m/h/d；裸数字 = 秒；非负
inline bool is_duration(std::string const &_S) noexcept
{
    std::string t = util::trim(_S);
    if (t.empty()) return false;
    std::size_t i = 0;
    bool        dot = false, hasDigit = false;
    while (i < t.size()) {
        char c = t[i];
        if (std::isdigit(static_cast<unsigned char>(c))) { hasDigit = true; ++i; continue; }
        if (c == '.' && !dot) { dot = true; ++i; continue; }
        break;
    }
    if (!hasDigit) return false;
    std::string unit = t.substr(i);
    if (unit.empty()) return true;                       // 裸数字 → 秒
    return unit == "s" || unit == "m" || unit == "h" || unit == "d";
}

/// @brief 主机名：RFC 1123 规范（1-63 字符，字母数字+连字符，首尾非连字符），总长 <= 253
inline bool is_hostname(std::string const &_S) noexcept
{
    if (_S.empty() || _S.size() > 253) return false;
    auto labels = util::split(_S, '.');
    for (auto const &l : labels) {
        if (l.empty() || l.size() > 63) return false;
        if (!std::isalnum(static_cast<unsigned char>(l.front()))
            || !std::isalnum(static_cast<unsigned char>(l.back()))) {
            return false;
        }
        for (char c : l) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-') return false;
        }
    }
    return true;
}

/// @brief UUID：8-4-4-4-12 十六进制
inline bool is_uuid(std::string const &_S) noexcept
{
    if (_S.size() != 36) return false;
    for (std::size_t i = 0; i < 36; ++i) {
        char c = _S[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') return false;
            continue;
        }
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

/// @brief 路径（轻语法：非空、不含 NUL；不校验存在性）
inline bool is_path(std::string const &_S) noexcept
{
    return !_S.empty() && _S.find('\0') == std::string::npos;
}

/// @brief 存在且为常规文件
inline bool is_existing_file(std::string const &_S) noexcept
{
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::path(_S), ec);
}

/// @brief 存在且为目录
inline bool is_existing_dir(std::string const &_S) noexcept
{
    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::path(_S), ec);
}

/// @brief 校验器条目：名称 + 校验函数 + 失败消息模板
/// @note fnCheck 为空表示恒通过（占位校验器）
struct Validator
{
    using FnCheck_T = std::function<bool(std::string const &)>;

    std::string strName;     ///< 校验器名称（schema 展示 / 语义去重用）
    FnCheck_T   fnCheck;     ///< 校验函数（空 = 恒通过）
    std::string strFailMsg;  ///< 失败消息模板（%value% 占位符）

    Validator() = default;
    Validator(std::string _Name, FnCheck_T _Fn, std::string _FailMsg = {})
        : strName(std::move(_Name))
        , fnCheck(std::move(_Fn))
        , strFailMsg(std::move(_FailMsg))
    {}

    /// @brief 校验值：通过返回空串，失败返回替换后的消息
    auto validate(std::string const &_Value) const -> std::string
    {
        if (!fnCheck) return {};
        if (fnCheck(_Value)) return {};
        if (!strFailMsg.empty()) return util::replace(strFailMsg, "%value%", _Value);
        std::string msg = "invalid value '" + _Value + "'";
        if (!strName.empty()) msg += " (" + strName + ")";
        return msg;
    }
};

/// @brief 数值区间校验（可配开闭；闭区间默认）；
///        先经 is_double 语义，再比较区间
inline Validator between(double _Min, double _Max,
                         bool _MinInclusive = true, bool _MaxInclusive = true)
{
    std::string name = "range (" + std::to_string(_Min)
        + (_MinInclusive ? " <= " : " < ") + "x"
        + (_MaxInclusive ? " <= " : " < ") + std::to_string(_Max) + ")";
    return Validator(
        name,
        [_Min, _Max, _MinInclusive, _MaxInclusive](std::string const &_S) {
            if (!is_double(_S)) return false;
            double v = std::strtod(_S.c_str(), nullptr);
            if (_MinInclusive ? v < _Min : v <= _Min) return false;
            if (_MaxInclusive ? v > _Max : v >= _Max) return false;
            return true;
        },
        "%value% out of range");
}

/// @brief 枚举集合校验：值必须命为 _Values 之一
inline Validator one_of(std::initializer_list<std::string> _Values)
{
    std::vector<std::string> vec(_Values);
    std::string name = "one of {";
    for (auto const &v : vec) name += v + " ";
    name += "}";
    return Validator(
        name,
        [vec](std::string const &_S) {
            for (auto const &v : vec) { if (v == _S) return true; }
            return false;
        },
        "%value% not allowed");
}

/// @brief 正则匹配校验（std::regex 完整匹配；非法正则视为校验失败）
inline Validator matches(std::string const &_Pattern)
{
    return Validator(
        "regex " + _Pattern,
        [_Pattern](std::string const &_S) {
            try { return std::regex_match(_S, std::regex(_Pattern)); }
            catch (...) { return false; }
        },
        "%value% does not match regex");
}

/// @brief 自定义校验器
inline Validator custom(Validator::FnCheck_T _Fn, std::string _Name = "custom",
                        std::string _FailMsg = "%value% rejected")
{
    return Validator(std::move(_Name), std::move(_Fn), std::move(_FailMsg));
}

/// @brief 语义校验表：语义名 → 校验器（T1 纯函数的登记处）
using SemanticTable_T = std::vector<std::pair<std::string, Validator>>;

inline SemanticTable_T const &semanticTable()
{
    static SemanticTable_T const table = {
        { "ipv4",     Validator("ipv4", is_ipv4, "%value% is not a valid IPv4 address") },
        { "ipv6",     Validator("ipv6", is_ipv6, "%value% is not a valid IPv6 address") },
        { "port",     Validator("port", is_port, "%value% is not a valid port (1-65535)") },
        { "file",     Validator("file", is_existing_file, "%value% is not an existing file") },
        { "path",     Validator("path", is_path, "%value% is not a valid path") },
        { "date",     Validator("date", is_date, "%value% is not a valid date (YYYY-MM-DD)") },
        { "duration", Validator("duration", is_duration, "%value% is not a valid duration (30s / 5m / 1h)") },
        { "hostname", Validator("hostname", is_hostname, "%value% is not a valid hostname") },
        { "uuid",     Validator("uuid", is_uuid, "%value% is not a valid UUID") },
        { "float",    Validator("float", is_float, "%value% is not a valid float") },
        { "double",   Validator("double", is_double, "%value% is not a valid double") },
    };
    return table;
}

/// @brief 按语义名取校验器；未知语义名抛 ErrorArgument（构造期错误）
inline Validator semantic_validator(std::string const &_Name)
{
    for (auto const &kv : semanticTable()) {
        if (kv.first == _Name) return kv.second;
    }
    throw ErrorArgument(BuildError_E::UNSUPPORT_TYPE);
}

/// @brief 校验链 mixin：为 Argument / OptionValue / EnvSpec 注入统一的链式校验 API
/// @tparam Self_T CRTP 派生类型（必须可移动构造）
template <typename Self_T>
class ValidatorMixin
{
public:
    using SelfMixin_T = Self_T;
    using FnCheck_T   = std::function<bool(std::string const &)>;

    /** @brief 标记必选（左值链式）
     *  @return Self_T& 支持链式调用 */
    auto required() & -> Self_T &
    {
        bRequired__ = true;
        return static_cast<Self_T &>(*this);
    }

    /// @brief 标记必选（右值链式）
    auto required() && -> Self_T
    {
        bRequired__ = true;
        return std::move(static_cast<Self_T &>(*this));
    }

    /// @brief 动态设置必选与否（required(false) 解除必选）
    auto required(bool _On) & -> Self_T &
    {
        bRequired__ = _On;
        return static_cast<Self_T &>(*this);
    }

    /// @brief 动态设置必选与否（右值链式）
    auto required(bool _On) && -> Self_T
    {
        bRequired__ = _On;
        return std::move(static_cast<Self_T &>(*this));
    }

    /// @brief 追加校验器（左值链式）
    auto check(Validator const &_V) & -> Self_T &
    {
        vecValidators__.push_back(_V);
        return static_cast<Self_T &>(*this);
    }

    /// @brief 追加校验器（右值链式）
    auto check(Validator const &_V) && -> Self_T
    {
        vecValidators__.push_back(_V);
        return std::move(static_cast<Self_T &>(*this));
    }

    /// @brief 追加自定义校验器（函数式简写）
    auto check(FnCheck_T _Fn, std::string _Name = "custom") & -> Self_T &
    {
        vecValidators__.push_back(custom(std::move(_Fn), std::move(_Name)));
        return static_cast<Self_T &>(*this);
    }

    /// @brief 追加自定义校验器（右值链式）
    auto check(FnCheck_T _Fn, std::string _Name = "custom") && -> Self_T
    {
        vecValidators__.push_back(custom(std::move(_Fn), std::move(_Name)));
        return std::move(static_cast<Self_T &>(*this));
    }

    /// @brief 数值区间校验（闭区间默认，可配开闭）
    auto range(double _Min, double _Max,
               bool _MinInclusive = true, bool _MaxInclusive = true) & -> Self_T &
    {
        vecValidators__.push_back(between(_Min, _Max, _MinInclusive, _MaxInclusive));
        return static_cast<Self_T &>(*this);
    }

    /// @brief 数值区间校验（右值链式）
    auto range(double _Min, double _Max,
               bool _MinInclusive = true, bool _MaxInclusive = true) && -> Self_T
    {
        vecValidators__.push_back(between(_Min, _Max, _MinInclusive, _MaxInclusive));
        return std::move(static_cast<Self_T &>(*this));
    }

    /// @brief 枚举集合校验
    auto oneOf(std::initializer_list<std::string> _Values) & -> Self_T &
    {
        vecValidators__.push_back(one_of(_Values));
        return static_cast<Self_T &>(*this);
    }

    /// @brief 枚举集合校验（右值链式）
    auto oneOf(std::initializer_list<std::string> _Values) && -> Self_T
    {
        vecValidators__.push_back(one_of(_Values));
        return std::move(static_cast<Self_T &>(*this));
    }

    /// @brief 正则匹配校验
    auto match(std::string const &_Pattern) & -> Self_T &
    {
        vecValidators__.push_back(matches(_Pattern));
        return static_cast<Self_T &>(*this);
    }

    /// @brief 正则匹配校验（右值链式）
    auto match(std::string const &_Pattern) && -> Self_T
    {
        vecValidators__.push_back(matches(_Pattern));
        return std::move(static_cast<Self_T &>(*this));
    }

    /** @brief 语义类型校验（ipv4 / ipv6 / port / file / path / date / duration /
     *         hostname / uuid / float / double；同名替换、未知构建错误）
     *  @note 语义名表见 semanticTable() */
    auto semantic(std::string const &_Name) & -> Self_T &
    {
        __applySemantic(_Name);
        return static_cast<Self_T &>(*this);
    }

    /// @brief 语义类型校验（右值链式）
    auto semantic(std::string const &_Name) && -> Self_T
    {
        __applySemantic(_Name);
        return std::move(static_cast<Self_T &>(*this));
    }

    /// @brief 是否标记为必选
    bool isRequired() const noexcept { return bRequired__; }

    /// @brief 是否已配置校验器
    bool hasValidators() const noexcept { return !vecValidators__.empty(); }

    /// @brief 校验器列表（只读，schema 展示用）
    auto validators() const noexcept -> std::vector<Validator> const & { return vecValidators__; }

    /** @brief 依次运行全部校验器，返回第一条失败消息（空串 = 全部通过） */
    auto validate(std::string const &_Value) const -> std::string
    {
        for (auto const &v : vecValidators__) {
            auto msg = v.validate(_Value);
            if (!msg.empty()) return msg;
        }
        return {};
    }

private:
    /// @brief 语义校验器绑定：同名替换，否则追加
    void __applySemantic(std::string const &_Name)
    {
        auto v = semantic_validator(_Name);
        for (auto &cur : vecValidators__) {
            if (cur.strName == v.strName) { cur = v; return; }
        }
        vecValidators__.push_back(std::move(v));
    }

protected:
    /** 必选标记（通过 isRequired() 读取） */
    bool                     bRequired__ = false;
    /** 校验器链 */
    std::vector<Validator>   vecValidators__;
};

}  // namespace validator_detail

using validator_detail::is_port;
using validator_detail::is_ipv4;
using validator_detail::is_ipv6;
using validator_detail::is_float;
using validator_detail::is_double;
using validator_detail::is_date;
using validator_detail::is_duration;
using validator_detail::is_hostname;
using validator_detail::is_uuid;
using validator_detail::is_path;
using validator_detail::is_existing_file;
using validator_detail::is_existing_dir;

using validator_detail::Validator;
using validator_detail::ValidatorMixin;
using validator_detail::between;
using validator_detail::one_of;
using validator_detail::matches;
using validator_detail::custom;
using validator_detail::semantic_validator;

CMD_ARGS_NAMESPACE_END
