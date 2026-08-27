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
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

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

CMD_ARGS_NAMESPACE_END
