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

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

#include "cmd_args/option/option.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace option_detail {

/// @brief 选项管理器统一接口（短名/长名查询），供 OptionParser 多态使用
struct OptMgrItf
{
    virtual Option const *doGet(char _Key) const noexcept        = 0;
    virtual Option const *doGet(Name_T _Key) const noexcept = 0;
};

/**
 * @brief 值选项管理器：持有全部 OptionValue（含 builtin 交互选项）。
 *        提供查询（const / mutable）、遍历（forEachValue）、帮助排版（appendHelp）。
 */
class ValueOptMgr : OptMgrItf
{
public:
    /// @brief 按短名查询（只读，未找到返回 nullptr）
    Option const *doGet(char _Key) const noexcept override
    {
        auto it = std::find_if(
            vecValues__.cbegin(),
            vecValues__.cend(),
            [_Key](decltype(vecValues__)::const_reference _V) { return _V.cShortName == _Key; });
        if (it == vecValues__.cend()) return nullptr;

        return std::addressof(*it);
    }

    /// @brief 按长名查询（只读，未找到返回 nullptr）
    Option const *doGet(Name_T _Key) const noexcept override
    {
        auto it = std::find_if(
            vecValues__.cbegin(),
            vecValues__.cend(),
            [_Key](decltype(vecValues__)::const_reference _V) { return _V.strLongName == _Key; });
        if (it == vecValues__.cend()) return nullptr;

        return std::addressof(*it);
    }

    /// @brief 追加值选项（含短名）
    /// @param _SName 短名（'\0' 表示无短名）
    /// @param _LName 长名
    /// @param _Help 帮助文本
    /// @param _Type 类型标识
    /// @param _Value 默认值
    void push(Option::NameShort_T _SName, Option::NameLong_T _LName, Option::Desc_T _Help,
              OptionValue::Type_T _Type, OptionValue::Value_T _Value)
    {
        vecValues__.emplace_back(
            _SName, std::move(_LName), std::move(_Help), std::move(_Type), std::move(_Value));
    }

    /// @brief 追加值选项（仅长名）
    /// @param _LName 长名
    /// @param _Help 帮助文本
    /// @param _Type 类型标识
    /// @param _Value 默认值
    void push(Option::NameLong_T _LName, Option::Desc_T _Help,
              OptionValue::Type_T _Type, OptionValue::Value_T _Value)
    {
        vecValues__.emplace_back(std::move(_LName), std::move(_Help), std::move(_Type), std::move(_Value));
    }

    /// @brief 按短名查找（只读，未找到返回 nullptr）
    auto find(char _Key) const -> OptionValue const *
    {
        for (auto const &v : vecValues__) {
            if (v.cShortName == '\0') continue;
            if (v.cShortName == _Key) return std::addressof(v);
        }

        return nullptr;
    }

    /// @brief 按长名查找（只读，未找到返回 nullptr）
    auto find(Name_T _Key) const -> OptionValue const *
    {
        for (auto const &v : vecValues__) {
            if (v.strLongName == _Key) return std::addressof(v);
        }

        return nullptr;
    }

    /// @brief 按短名查找（可变：解析期写入 strValue / 标记 bExplicit__ / 触发 fnOnSet__）
    auto mutFind(char _Key) -> OptionValue *
    {
        for (auto &v : vecValues__) {
            if (v.cShortName == '\0') continue;
            if (v.cShortName == _Key) return std::addressof(v);
        }

        return nullptr;
    }

    /// @brief 按长名查找（可变：解析期写入 strValue / 标记 bExplicit__ / 触发 fnOnSet__）
    auto mutFind(Name_T _Key) -> OptionValue *
    {
        for (auto &v : vecValues__) {
            if (v.strLongName == _Key) return std::addressof(v);
        }

        return nullptr;
    }

    using FnValueVisitor_T    = std::function<void(OptionValue &)>;
    using FnValueVisitorC_T   = std::function<void(OptionValue const &)>;
    using ShortIndexMap_T     = std::map<char, Option::NameLong_T>;

    /// @brief 遍历值选项（可变）
    void forEachValue(FnValueVisitor_T const &_Fn)
    {
        for (auto &v : vecValues__) { _Fn(v); }
    }

    /// @brief 遍历值选项（只读）
    void forEachValue(FnValueVisitorC_T const &_Fn) const
    {
        for (auto const &v : vecValues__) { _Fn(v); }
    }

    /// @brief 是否为空容器
    bool doEmpty() const noexcept { return vecValues__.empty(); }

    /// @brief 计算帮助排版的最长名称列宽度
    auto calcDescMaxLength() const noexcept -> std::size_t
    {
        std::size_t max_name_length = 0;
        for (auto const &opt : vecValues__) {
            std::size_t length = opt.strLongName.length();
            if (opt.cShortName == '\0') { length += 4; }
            max_name_length = std::max(max_name_length, length);
        }

        return max_name_length;
    }

    /// @brief 追加帮助文本到输出流
    void appendHelp(std::ostringstream &_Out, std::size_t _Max) const
    {
        for (auto const &opt : vecValues__) {
            _Out << "  ";
            std::size_t printed_length = 0;
            if (opt.cShortName) {
                _Out << opt.cShortName << ", ";
                printed_length = 4;
            }
            _Out << opt.strLongName;
            printed_length += opt.strLongName.length();
            _Out << std::string(_Max - printed_length, ' ');
            if (opt.strType != "bool") { _Out << "(" << opt.strType << ") "; }
            _Out << util::replace(opt.strHelp, "\n", "\n" + std::string(_Max + 2, ' ')) << '\n';
        }
    }

    /// @brief 追加指定成员的帮助文本（分组渲染用）
    void appendHelpFiltered(std::ostringstream &_Out, std::size_t _Max,
                            std::vector<Name_T> const &_Members) const
    {
        for (auto const &opt : vecValues__) {
            bool found = false;
            for (auto const &m : _Members) {
                if (m == opt.strLongName) { found = true; break; }
            }
            if (!found) continue;

            _Out << "  ";
            std::size_t printed_length = 0;
            if (opt.cShortName) {
                _Out << opt.cShortName << ", ";
                printed_length = 4;
            }
            _Out << opt.strLongName;
            printed_length += opt.strLongName.length();
            _Out << std::string(_Max - printed_length, ' ');
            if (opt.strType != "bool") { _Out << "(" << opt.strType << ") "; }
            _Out << util::replace(opt.strHelp, "\n", "\n" + std::string(_Max + 2, ' ')) << '\n';
        }
    }

    /// @brief 追加不在排除集合中的选项帮助文本（未分组渲染用）
    void appendHelpUnfiltered(std::ostringstream &_Out, std::size_t _Max,
                              std::set<Name_T> const &_Excluded) const
    {
        for (auto const &opt : vecValues__) {
            if (_Excluded.count(opt.strLongName)) continue;

            _Out << "  ";
            std::size_t printed_length = 0;
            if (opt.cShortName) {
                _Out << opt.cShortName << ", ";
                printed_length = 4;
            }
            _Out << opt.strLongName;
            printed_length += opt.strLongName.length();
            _Out << std::string(_Max - printed_length, ' ');
            if (opt.strType != "bool") { _Out << "(" << opt.strType << ") "; }
            _Out << util::replace(opt.strHelp, "\n", "\n" + std::string(_Max + 2, ' ')) << '\n';
        }
    }

private:
    using ConValues_T = std::vector<OptionValue>;  ///< 值选项容器类型
    ConValues_T vecValues__;                       ///< 值选项容器
};

/**
 * @brief 短回路选项管理器：持有全部 OptionAction（--help / --env 等）。
 *        命中其一即回调并停止本次解析（isFired）。
 */
class ActionOptMgr : OptMgrItf
{
public:
    /// @brief 按短名查询（只读，未找到返回 nullptr）
    Option const *doGet(char _Key) const noexcept override
    {
        auto it = std::find_if(
            vecActions__.cbegin(),
            vecActions__.cend(),
            [_Key](decltype(vecActions__)::const_reference _V) { return _V.cShortName == _Key; });
        if (it == vecActions__.cend()) return nullptr;

        return std::addressof(*it);
    }

    /// @brief 按长名查询（只读，未找到返回 nullptr）
    Option const *doGet(Name_T _Key) const noexcept override
    {
        auto it = std::find_if(
            vecActions__.cbegin(),
            vecActions__.cend(),
            [_Key](decltype(vecActions__)::const_reference _V) { return _V.strLongName == _Key; });
        if (it == vecActions__.cend()) return nullptr;

        return std::addressof(*it);
    }

    using FnActionVisitorC_T = std::function<void(OptionAction const &)>;

    /// @brief 追加短回路选项（含短名）
    void push(Option::NameShort_T _SName, Option::NameLong_T _LName, Option::Desc_T _Help,
              OptionAction::FnCallback_T _Cb)
    {
        vecActions__.emplace_back(_SName, std::move(_LName), std::move(_Help), std::move(_Cb));
    }

    /// @brief 追加短回路选项（仅长名）
    void push(Option::NameLong_T _LName, Option::Desc_T _Help, OptionAction::FnCallback_T _Cb)
    {
        vecActions__.emplace_back(std::move(_LName), std::move(_Help), std::move(_Cb));
    }

    /// @brief 遍历短回路选项（只读）
    void forEachAction(FnActionVisitorC_T const &_Fn) const
    {
        for (auto const &v : vecActions__) { _Fn(v); }
    }

    /// @brief 是否为空容器
    bool doEmpty() const noexcept { return vecActions__.empty(); }

    /// @brief 计算帮助排版的最长名称列宽度
    auto calcDescMaxLength() const noexcept -> std::size_t
    {
        std::size_t max_name_length = 0;
        for (auto const &opt : vecActions__) {
            std::size_t length = opt.strLongName.length();
            if (opt.cShortName == '\0') { length += 4; }
            max_name_length = std::max(max_name_length, length);
        }

        return max_name_length;
    }

    /// @brief 追加帮助文本到输出流
    void appendHelp(std::ostringstream &_Out, std::size_t _Max) const
    {
        for (auto const &opt : vecActions__) {
            _Out << "  ";
            std::size_t printed_length = 0;
            if (opt.cShortName) {
                _Out << opt.cShortName << ", ";
                printed_length = 4;
            }
            _Out << opt.strLongName;
            printed_length += opt.strLongName.length();
            _Out << std::string(_Max - printed_length, ' ');
            _Out << util::replace(opt.strHelp, "\n", "\n" + std::string(_Max + 2, ' ')) << '\n';
        }
    }

private:
    using ConActions_T = std::vector<OptionAction>;  ///< 短回路选项容器类型
    ConActions_T vecActions__;                       ///< 短回路选项容器
};

/// @brief 选项数据仓库：短回路选项 + 值选项 + 短名索引
struct OptionData
{
    using ActionOrder_T = std::vector<Name_T>;  ///< 含 action 的选项解析顺序类型

    ActionOptMgr mgrActions;              ///< 短回路选项管理器（命中即停，如 -?/--help）
    ValueOptMgr  mgrValues;               ///< 值选项/标志管理器（--long value / -x）
    ValueOptMgr::ShortIndexMap_T mapShortNameIndex;  ///< 短名 → 长名 索引（预留）

    /// @brief 记录含 action 的选项解析顺序（供 Executor 按序执行）
    void recordParseOrder(Name_T const &_Key)
    {
        for (auto const &k : actionOrder__) { if (k == _Key) return; }
        actionOrder__.push_back(_Key);
    }

    /// @brief 获取解析顺序
    auto actionOrder() const -> ActionOrder_T const & { return actionOrder__; }

    /// @brief 清空解析顺序（每次解析前调用）
    void clearParseOrder() { actionOrder__.clear(); }

    /// @brief 是否为空（短回路与值选项都为空）
    bool empty() const noexcept
    {
        return mgrActions.doEmpty() && mgrValues.doEmpty();
    }

    /// @brief 选项部分的使用行片段：[-? | --help] [-x | --timeout <int>]
    auto usageString() const -> OptionValue::Value_T
    {
        std::ostringstream oss;
        mgrActions.forEachAction([&](OptionAction const &_A) {
            oss << " [";
            if (_A.cShortName) { oss << "-" << _A.cShortName << " | "; }
            oss << "--" << _A.strLongName << "]";
        });
        mgrValues.forEachValue([&](OptionValue const &_V) {
            oss << " [";
            if (_V.cShortName) { oss << "-" << _V.cShortName << " | "; }
            oss << "--" << _V.strLongName;
            if (_V.strType != "bool") { oss << " <" << _V.strType << ">"; }
            oss << "]";
        });
        return oss.str();
    }

    /// @brief 计算帮助排版的最长名称列宽度（两类管理器取大，且不小于 25）
    auto calcDescMaxLength() const noexcept -> std::size_t
    {
        std::size_t max_name_length = mgrActions.calcDescMaxLength();
        max_name_length = std::max(max_name_length, mgrValues.calcDescMaxLength());
        max_name_length = std::max(max_name_length, std::size_t(25));

        return max_name_length;
    }

    /// @brief 追加选项帮助文本到输出流
    void appendHelp(std::ostringstream &_Oss, size_t &_Max) const
    {
        mgrActions.appendHelp(_Oss, _Max);
        mgrValues.appendHelp(_Oss, _Max);
    }

    /// @brief 追加指定成员的选项帮助文本（分组渲染用）
    void appendHelpFiltered(std::ostringstream &_Oss, size_t _Max,
                            std::vector<Name_T> const &_Members) const
    {
        mgrValues.appendHelpFiltered(_Oss, _Max, _Members);
    }

    /// @brief 追加不在排除集合中的选项帮助文本（未分组渲染用）
    void appendHelpUnfiltered(std::ostringstream &_Oss, size_t _Max,
                              std::set<Name_T> const &_Excluded) const
    {
        mgrValues.appendHelpUnfiltered(_Oss, _Max, _Excluded);
    }

    /// @brief 遍历值选项（只读）
    void forEachValue(ValueOptMgr::FnValueVisitorC_T const &_Fn) const
    {
        mgrValues.forEachValue(_Fn);
    }

private:
    ActionOrder_T actionOrder__;  ///< 含 action 的选项解析顺序（Executor 按序执行）
};

}  // namespace option_detail

using option_detail::OptionData;

CMD_ARGS_NAMESPACE_END
