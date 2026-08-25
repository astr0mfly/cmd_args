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

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <type_traits>

#include "cmd_args/context/parse_fwd.h"
#include "cmd_args/error/parser_error.h"
#include "cmd_args/option/option.h"
#include "cmd_args/option/option_data.h"
#include "cmd_args/option/option_graph.h"

CMD_ARGS_NAMESPACE_BEGIN

/**
 * 选项解析器：消费 Tokens 中形如
 *     --long=value / --long value / -x value / -xvalue / -abc(聚合 bool)
 * 的选项 token。
 *
 * - 短回路选项优先级最高：命中其一立即回调并停止解析
 * - 内置交互选项（bBuiltin__ 且有 fnOnSet__）在解析期即时生效；用户 action 由 Executor 延迟执行
 * - 未注册的长选项 token 原样保留（可能是命名参数，由上层决定）
 * - 未知的短选项在聚合串里直接报 UNKNOWN_OPTION
 * - `--` 之后的 token 一律不再作为选项处理
 */
class OptionParser
{
public:
    /// @brief 构造选项解析器
    /// @param _Data 底层选项数据仓库（非空）
    OptionParser(OptionData *_Data)
        : pData__(_Data)
    {}

    virtual ~OptionParser() = default;

    /// @brief 绑定关系有向图（可选，为 nullptr 时关系语法被忽略）
    void setGraph(OptionGraph *_Graph) { pGraph__ = _Graph; }

    /// @brief 解析 token 流中的选项 token（赋值 + 标记；内置选项回调内联触发，用户 action 由 Executor 延迟执行）
    /// @param _Tokens 待解析 token 流（已消费项会被移除）
    /// @return 短回路是否已命中（命中即停止本次解析）
    bool parse(Tokens_T &_Tokens)
    {
        bFired__  = false;
        bOptEnd__ = false;

        // 每次解析前清空 actionOrder：上一次失败解析残留的顺序不得在本次/下次
        // 解析中经 Executor 触发（Model.hasAction() 也据此在解析前为 false）
        if (pData__) { pData__->clearParseOrder(); }

        for (auto it = _Tokens.begin(); it != _Tokens.end();) {
            std::string const &tok = *it;

            if (tok == "--") {
                // 结束选项区：其后的 token 全部留给位置参数
                _Tokens.erase(it);
                bOptEnd__ = true;
                break;
            }

            if (util::starts_with(tok, "--") && tok.size() > 2) {
                auto          body = tok.substr(2);
                auto          eq   = body.find('=');
                std::string   name = (eq == std::string::npos) ? body : body.substr(0, eq);
                std::string   value;
                if (eq != std::string::npos) { value = body.substr(eq + 1); }

                if (__fireOnLong(name, it, _Tokens)) return true;

                auto *pOpt = pData__->mgrValues.mutFind(name);
                if (pOpt) {
                    // --long=value 与 --long value 两种形式都支持；
                    // 未显式给值且非 bool 选项时，消费下一个 token 作为值
                    if (value.empty() && pOpt->strType != "bool") {
                        auto nx = std::next(it);
                        if (nx == _Tokens.end() || util::starts_with(*nx, "-")) {
                            throw ErrorParse(
                                ParseError_E::NEED_VALUE,
                                "option --" + name + " need a value");
                        }
                        value = *nx;
                        _Tokens.erase(nx);
                    }
                    __assign(pOpt, value, pOpt->strType == "bool");
                    it = _Tokens.erase(it);
                    continue;
                }

                // 不是选项：可能是命名参数（--name=value），留给上层
                ++it;
                continue;
            }

            if (util::starts_with(tok, "-") && tok.size() >= 2) {
                // 新增：检测关系语法 token（-a?b:c / -a->b）
                if (pGraph__ && __isRelationToken(tok)) {
                    __parseRelation(tok);
                    it = _Tokens.erase(it);
                    continue;
                }

                if (__fireOnShort(tok, it, _Tokens)) return true;

                if (tok.size() == 2) {
                    auto *pOpt = pData__->mgrValues.mutFind(tok[1]);
                    if (!pOpt) {
                        ++it;  // 可能是未识别短选项，交给上层统一报错
                        continue;
                    }
                    if (pOpt->strType == "bool") {
                        __assign(pOpt, "", true);
                    }
                    else {
                        auto nx = std::next(it);
                        if (nx == _Tokens.end()) {
                            throw ErrorParse(
                                ParseError_E::NEED_VALUE,
                                "option -" + std::string(1, tok[1]) + " need a value");
                        }
                        __assign(pOpt, *nx, false);
                        _Tokens.erase(nx);
                    }
                    it = _Tokens.erase(it);
                    continue;
                }

                // 聚合短选项：-abc 或 -tvalue
                bool consumed = false;
                for (size_t i = 1; i < tok.size() && !consumed; ++i) {
                    char key = tok[i];
                    if (__fireOnShortChar(key, it, _Tokens)) return true;

                    auto *pOpt = pData__->mgrValues.mutFind(key);
                    if (!pOpt) {
                        throw ErrorParse(
                            ParseError_E::UNKNOWN_OPTION,
                            "unrecognized short option '" + std::string(1, key) + "' in " + tok);
                    }
                    if (pOpt->strType == "bool") {
                        __assign(pOpt, "", true);
                        continue;
                    }

                    // 值选项出现在聚合串中：余下部分就是它的值（-tvalue）
                    std::string val = tok.substr(i + 1);
                    if (!val.empty() && val.front() == '=') { val = val.substr(1); }
                    if (val.empty()) {
                        auto nx = std::next(it);
                        if (nx == _Tokens.end()) {
                            throw ErrorParse(
                                ParseError_E::NEED_VALUE,
                                "option -" + std::string(1, key) + " need a value");
                        }
                        val = *nx;
                        _Tokens.erase(nx);
                    }
                    __assign(pOpt, val, false);
                    consumed = true;
                }

                it = _Tokens.erase(it);
                continue;
            }

            // 普通 token：留给参数解析
            ++it;
            continue;
        }

        return bFired__;
    }

    /// @brief 短回路是否已命中（命中即停止本次解析）
    bool isFired() const noexcept { return bFired__; }

    /// @brief 是否已进入 `--` 之后的纯位置参数模式
    bool isOptionsEnded() const noexcept { return bOptEnd__; }

    /// @brief 按短名查询是否被显式赋值
    /// @param _Name 短名（'\0' 必为 false）
    bool isParsed(NameShort_T _Name) const noexcept
    {
        if (_Name == '\0') return false;
        auto const *p = pData__->mgrValues.find(_Name);
        return p && p->bExplicit__;
    }

    /// @brief 按长名查询是否被显式赋值
    /// @param _Name 长名
    bool isParsed(NameLong_T const &_Name) const noexcept
    {
        auto const *p = pData__->mgrValues.find(_Name);
        return p && p->bExplicit__;
    }

    /// @brief 查询选项是否已注册（长名可带 -- 前缀，短名亦可用 char 查询）
    /// @param _Name 选项名
    bool hasOption(std::string const &_Name) const noexcept
    {
        auto key = _Name;
        if (util::starts_with(key, "--")) { key = key.substr(2); }

        if (auto const *p = pData__->mgrValues.find(key)) { (void)p; return true; }
        if (key.size() == 1 && pData__->mgrValues.find(key[0])) { return true; }
        return false;
    }

    /// @brief 带类型校验地读取选项值
    /// @tparam T 读取目标类型
    /// @param _Name 选项名（可带 -- 前缀）
    /// @return 解析后的 T 值
    /// @note 类型必须与注册时一致，否则抛 UNKNOWN_OPTION
    template <typename T>
    auto getOption(std::string const &_Name) const -> T
    {
        using namespace util;
        auto key = _Name;
        if (util::starts_with(key, "--")) { key = key.substr(2); }

        auto const *p = pData__->mgrValues.find(key);
        if (!p && key.size() == 1) { p = pData__->mgrValues.find(key[0]); }
        if (!p) {
            throw ErrorParse(ParseError_E::UNKNOWN_OPTION, "option not found: " + key);
        }

        if (p->strType == "bool") {
            if constexpr (std::is_same_v<T, bool>) { return util::parse_bool(p->strValue); }
            throw ErrorParse(ParseError_E::UNKNOWN_OPTION, "option type mismatch: " + key);
        }
        if (p->strType != util::type_string<T>()) {
            throw ErrorParse(
                ParseError_E::UNKNOWN_OPTION,
                "option type mismatch: set '" + p->strType + "' but try get with '"
                    + util::type_string<T>() + "'");
        }

        return util::parse_value<T>(p->strValue);
    }

    // 一些 getOption 的类型便捷入口
    /// @brief 便捷判断 bool 型选项是否命中
    bool hasOptionBool(std::string const &_Name) const { return getOption<bool>(_Name); }

    /// @brief 便捷读取 bool 型选项
    bool getOptionBool(std::string const &_Name) const { return getOption<bool>(_Name); }

    /// @brief 便捷读取 int 型选项
    auto getOptionInt(std::string const &_Name) const -> int { return getOption<int>(_Name); }

    /// @brief 便捷读取 int64_t 型选项
    auto getOptionInt64(std::string const &_Name) const -> int64_t
    {
        return getOption<int64_t>(_Name);
    }

    /// @brief 便捷读取 double 型选项
    auto getOptionDouble(std::string const &_Name) const -> double
    {
        return getOption<double>(_Name);
    }

    /// @brief 便捷读取 string 型选项
    auto getOptionString(std::string const &_Name) const -> std::string
    {
        return getOption<std::string>(_Name);
    }

    /// @brief 生成选项使用行片段（委托给 OptionData）
    auto usageString() const -> std::string { return pData__->usageString(); }

    /// @brief 追加帮助文本到输出流
    void appendHelp(std::ostringstream &_Out) const
    {
        if (pData__->mgrActions.doEmpty() && pData__->mgrValues.doEmpty()) return;
        _Out << "\nOptions:\n";
        auto max = pData__->calcDescMaxLength();
        pData__->appendHelp(_Out, max);
    }

private:
    /// @brief 纯解析：赋值 + bExplicit__ 标记 + 记录解析顺序；子类可 override onOptionAssigned 实现内联 action
    void __assign(OptionValue *_Opt, std::string const &_Value, bool _IsBool)
    {
        if (_IsBool) {
            _Opt->strValue = _Value.empty() ? "1" : (util::parse_bool(_Value) ? "1" : "0");
        }
        else {
            _Opt->strValue = _Value;
        }
        _Opt->bExplicit__ = true;

        if (_Opt->bBuiltin__ && _Opt->fnOnSet__) {
            // 内置交互选项（--load-config/--env-set/--env-unset/--save-config）
            // 语义为「解析期立即生效」：内联触发回调，使其影响后续解析
            // （如 --load-config 需先导入配置值，方位参数数量校验才能通过）。
            // 不记入 actionOrder——Executor 不会重复触发。
            _Opt->fnOnSet__(_Opt->strValue);
        }
        else if (pData__ && (_Opt->fnOnSet__ || !_Opt->strActionTemplate__.empty())) {
            // 记录含 action 的用户选项的解析顺序（Executor 按此顺序统一执行）
            pData__->recordParseOrder(_Opt->strLongName);
        }

        // 虚拟钩子：子类（Interpreter）可 override 实现逐 Token 内联 action 执行
        onOptionAssigned(_Opt, _Opt->strValue);
    }

    /// @brief 选项赋值后的虚拟钩子（纯解析 = 空操作；子类 override 实现 action 执行）
    virtual void onOptionAssigned(OptionValue *, std::string const &) {}

    /// @brief 长名短回路触发：命中即回调并移除 token
    bool __fireOnLong(
        std::string const &_Name, Tokens_T::iterator &_It, Tokens_T &_Tokens)
    {
        auto const *pSc = pData__->mgrActions.doGet(_Name);
        if (!pSc) return false;

        auto const *pAct = static_cast<OptionAction const *>(pSc);
        bFired__ = true;
        _It      = _Tokens.erase(_It);
        pAct->fnCallback();
        return true;
    }

    /// @brief 短名短回路触发（-x 形式）
    bool __fireOnShort(std::string const &_Tok, Tokens_T::iterator &_It, Tokens_T &_Tokens)
    {
        if (_Tok.size() != 2) return false;

        auto const *pSc = pData__->mgrActions.doGet(_Tok[1]);
        if (!pSc) return false;

        auto const *pAct = static_cast<OptionAction const *>(pSc);
        bFired__ = true;
        _It      = _Tokens.erase(_It);
        pAct->fnCallback();
        return true;
    }

    /// @brief 聚合串内短名短回路触发（-abc 中命中 b）
    bool __fireOnShortChar(char _Key, Tokens_T::iterator &_It, Tokens_T &_Tokens)
    {
        auto const *pSc = pData__->mgrActions.doGet(_Key);
        if (!pSc) return false;

        auto const *pAct = static_cast<OptionAction const *>(pSc);
        bFired__ = true;
        _It      = _Tokens.erase(_It);
        pAct->fnCallback();
        return true;
    }

    /// @brief 判断 token 是否为关系语法（-a?b:c / -a?b:_ / -a->b）
    bool __isRelationToken(std::string const &_Tok) const
    {
        if (_Tok.size() < 4 || _Tok[0] != '-') return false;
        for (size_t i = 1; i < _Tok.size(); ++i) {
            if (_Tok[i] == '?') return true;
            if (_Tok[i] == '-' && i + 1 < _Tok.size() && _Tok[i + 1] == '>') return true;
        }
        return false;
    }

    /// @brief 解析关系语法 token 并注册到 OptionGraph
    void __parseRelation(std::string const &_Tok)
    {
        if (!pGraph__) return;

        auto qPos = _Tok.find('?');
        auto dPos = _Tok.find("->");

        if (qPos != std::string::npos) {
            // -a?b:c 或 -a?b:_ 或 -a?b
            auto condition = _Tok.substr(1, qPos - 1);
            auto colonPos  = _Tok.find(':', qPos);
            std::string thenBranch, elseBranch;
            if (colonPos != std::string::npos) {
                thenBranch = _Tok.substr(qPos + 1, colonPos - qPos - 1);
                elseBranch = _Tok.substr(colonPos + 1);
            }
            else {
                thenBranch = _Tok.substr(qPos + 1);
                elseBranch = "_";
            }
            pGraph__->addEdge(condition, thenBranch,
                OptionGraph::EdgeKind_E::Conditional, elseBranch);
        }
        else if (dPos != std::string::npos) {
            // -a->b
            auto source = _Tok.substr(1, dPos - 1);
            auto target = _Tok.substr(dPos + 2);
            pGraph__->addEdge(source, target, OptionGraph::EdgeKind_E::Dependency);
        }
    }

private:
    OptionData   *pData__ = nullptr;  ///< 底层选项数据仓库指针
    OptionGraph  *pGraph__ = nullptr; ///< 关系有向图（可选，为 nullptr 时关系语法被忽略）
    bool          bFired__ = false;   ///< 短回路是否已命中（命中即停止本次解析）
    bool          bOptEnd__ = false;  ///< 是否已结束选项区（`--` 之后）
};

CMD_ARGS_NAMESPACE_END
