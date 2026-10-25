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

#include "cmd_args/expression/classified_expr.h"
#include "cmd_args/expression/token.h"
#include "cmd_args/context/context_fwd.h"
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

    // ===== Expression 识别规则（编译前端接口） =====

    /// @brief 识别当前迭代器指向的 token 是否匹配选项规则
    auto recognize(Tokens_T::iterator _It, Tokens_T const &_Tokens) const -> std::optional<ClassifiedExpr>
    {
        if (_It == _Tokens.end()) return std::nullopt;
        std::string const &tok = *_It;

        if (tok == "--") return std::nullopt;

        // --name[=value] 长选项
        if (util::starts_with(tok, "--") && tok.size() > 2) {
            auto body = tok.substr(2);
            auto eq   = body.find('=');
            ClassifiedExpr expr;
            expr.kind      = ExpressionKind_E::Option;
            expr.bExplicit = true;
            expr.name      = (eq == std::string::npos) ? body : body.substr(0, eq);
            if (eq != std::string::npos) { expr.value = body.substr(eq + 1); }
            return expr;
        }

        // -x / -x=value / -xvalue / -abc / -a?b:c / -a->b
        if (util::starts_with(tok, "-") && tok.size() >= 2) {
            if (__isRelationToken(tok)) {
                ClassifiedExpr expr;
                expr.bExplicit = true;
                auto qPos = tok.find('?');
                auto dPos = tok.find("->");
                if (qPos != std::string::npos) {
                    expr.kind         = ExpressionKind_E::Conditional;
                    expr.strCondition = tok.substr(1, qPos - 1);
                    auto colonPos     = tok.find(':', qPos);
                    if (colonPos != std::string::npos) {
                        expr.strThenBranch = tok.substr(qPos + 1, colonPos - qPos - 1);
                        expr.strElseBranch = tok.substr(colonPos + 1);
                    } else {
                        expr.strThenBranch = tok.substr(qPos + 1);
                        expr.strElseBranch = "_";
                    }
                } else if (dPos != std::string::npos) {
                    expr.kind         = ExpressionKind_E::Dependency;
                    expr.strDepSource = tok.substr(1, dPos - 1);
                    expr.strDepTarget = tok.substr(dPos + 2);
                }
                return expr;
            }

            // 短选项：-x / -abc（bool 聚合）/ -x=value / -xvalue
            ClassifiedExpr expr;
            expr.kind       = ExpressionKind_E::Option;
            expr.cShortName = tok[1];
            expr.name       = std::string(1, tok[1]);
            expr.bExplicit  = true;
            expr.bIsBool    = true;  // 默认为 bool，apply 根据注册类型决定
            return expr;
        }

        return std::nullopt;
    }

    /// @brief 将识别结果应用到 OptionData（赋值 + 标记 + 回调触发）
    void apply(ClassifiedExpr const &_Expr, Tokens_T::iterator &_It, Tokens_T &_Tokens)
    {
        // 关系语法
        if (_Expr.kind == ExpressionKind_E::Conditional || _Expr.kind == ExpressionKind_E::Dependency) {
            if (pGraph__) __parseRelation(*_It);
            _It = _Tokens.erase(_It);
            return;
        }

        std::string const &tok = *_It;

        // 长选项：检查是否注册
        if (util::starts_with(tok, "--") && tok.size() > 2) {
            // 短回路检测
            if (__fireOnLong(_Expr.name, _It, _Tokens)) return;

            auto *pOpt = pData__->mgrValues.mutFind(_Expr.name);
            if (!pOpt) {
                ++_It;  // 未注册，留给上层（可能是命名参数）
                return;
            }

            std::string value = _Expr.value;
            if (value.empty() && pOpt->strType != "bool") {
                auto nx = std::next(_It);
                if (nx == _Tokens.end() || util::starts_with(*nx, "-")) {
                    throw ErrorParse(ParseError_E::NEED_VALUE,
                        "option --" + _Expr.name + " need a value");
                }
                value = *nx;
                _Tokens.erase(nx);
            }
            __assign(pOpt, value, pOpt->strType == "bool");
            _It = _Tokens.erase(_It);
            return;
        }

        // 短选项
        if (util::starts_with(tok, "-") && tok.size() >= 2) {
            // 短回路检测
            if (__fireOnShort(tok, _It, _Tokens)) return;
            if (__fireOnShortChar(_Expr.cShortName, _It, _Tokens)) return;

            // 聚合短选项：-abc（多个 bool 标志）
            if (tok.size() > 2 && _Expr.value.empty()) {
                for (size_t i = 1; i < tok.size(); ++i) {
                    char key = tok[i];
                    if (__fireOnShortChar(key, _It, _Tokens)) return;

                    auto *pOpt = pData__->mgrValues.mutFind(key);
                    if (!pOpt) {
                        throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                            "unrecognized short option '" + std::string(1, key) + "' in " + tok);
                    }
                    if (pOpt->strType == "bool") {
                        __assign(pOpt, "", true);
                    }
                    else {
                        std::string val = tok.substr(i + 1);
                        if (!val.empty() && val.front() == '=') { val = val.substr(1); }
                        if (val.empty()) {
                            auto nx = std::next(_It);
                            if (nx == _Tokens.end()) {
                                throw ErrorParse(ParseError_E::NEED_VALUE,
                                    "option -" + std::string(1, key) + " need a value");
                            }
                            val = *nx;
                            _Tokens.erase(nx);
                        }
                        __assign(pOpt, val, false);
                        _It = _Tokens.erase(_It);
                        return;
                    }
                }
                _It = _Tokens.erase(_It);
                return;
            }

            // 单个短选项或带值短选项
            auto *pOpt = pData__->mgrValues.mutFind(_Expr.cShortName);
            if (!pOpt) {
                ++_It;  // 未注册，留给上层报错
                return;
            }

            // 从 token 文本提取值（-t5 → "5", -t=5 → "5"）
            std::string value;
            if (tok.size() > 2) {
                value = tok.substr(2);
                if (!value.empty() && value.front() == '=') { value = value.substr(1); }
            }

            if (value.empty() && pOpt->strType != "bool") {
                auto nx = std::next(_It);
                if (nx == _Tokens.end()) {
                    throw ErrorParse(ParseError_E::NEED_VALUE,
                        "option -" + std::string(1, _Expr.cShortName) + " need a value");
                }
                value = *nx;
                _Tokens.erase(nx);
            }
            __assign(pOpt, value, pOpt->strType == "bool");
            _It = _Tokens.erase(_It);
            return;
        }
    }

    /// @brief 解析 token 流中的选项 token（通过 recognize + apply 规则驱动）
    /// @param _Tokens 待解析 token 流（已消费项会被移除）
    /// @return 短回路是否已命中（命中即停止本次解析）
    bool parse(Tokens_T &_Tokens)
    {
        bFired__  = false;
        bOptEnd__ = false;
        if (pData__) { pData__->clearParseOrder(); }

        for (auto it = _Tokens.begin(); it != _Tokens.end();) {
            // -- 终止选项区
            if (*it == "--") {
                _Tokens.erase(it);
                bOptEnd__ = true;
                break;
            }

            // 识别：调用内置规则
            auto expr = recognize(it, _Tokens);
            if (!expr) {
                ++it;  // 不匹配选项规则，留给参数解析
                continue;
            }

            // 应用：写入 OptionData
            apply(*expr, it, _Tokens);
            if (bFired__) return true;
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
    bool hasOption(Name_T const &_Name) const noexcept
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
    auto getOption(Name_T const &_Name) const -> T
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
            if constexpr (std::is_same_v<T, bool>) { return util::parse_bool(p->getValue()); }
            throw ErrorParse(ParseError_E::UNKNOWN_OPTION, "option type mismatch: " + key);
        }
        if (p->strType != util::type_string<T>()) {
            throw ErrorParse(
                ParseError_E::UNKNOWN_OPTION,
                "option type mismatch: set '" + p->strType + "' but try get with '"
                    + util::type_string<T>() + "'");
        }

        return util::parse_value<T>(p->getValue());
    }

    // 一些 getOption 的类型便捷入口
    /// @brief 便捷判断 bool 型选项是否命中
    bool hasOptionBool(Name_T const &_Name) const { return getOption<bool>(_Name); }

    /// @brief 便捷读取 bool 型选项
    bool getOptionBool(Name_T const &_Name) const { return getOption<bool>(_Name); }

    /// @brief 便捷读取 int 型选项
    auto getOptionInt(Name_T const &_Name) const -> int { return getOption<int>(_Name); }

    /// @brief 便捷读取 int64_t 型选项
    auto getOptionInt64(Name_T const &_Name) const -> int64_t
    {
        return getOption<int64_t>(_Name);
    }

    /// @brief 便捷读取 double 型选项
    auto getOptionDouble(Name_T const &_Name) const -> double
    {
        return getOption<double>(_Name);
    }

    /// @brief 便捷读取 string 型选项
    auto getOptionString(Name_T const &_Name) const -> OptionValue::Value_T
    {
        return getOption<OptionValue::Value_T>(_Name);
    }

    /// @brief 生成选项使用行片段（委托给 OptionData）
    auto usageString() const -> OptionValue::Value_T { return pData__->usageString(); }

    /// @brief 追加帮助文本到输出流
    void appendHelp(std::ostringstream &_Out) const
    {
        if (pData__->mgrActions.doEmpty() && pData__->mgrValues.doEmpty()) return;
        _Out << "\nOptions:\n";
        auto max = pData__->calcDescMaxLength();
        pData__->appendHelp(_Out, max);
    }

    // ===== Token 规则接口（编译前端） =====

    /// @brief 单 Token 语法分类（只读查注册表，不赋值）
    auto recognizeToken(Token const &_Tok) const -> std::optional<ClassifiedExpr>
    {
        using TT = Token::Type_E;
        switch (_Tok.getAttr()) {
        case TT::LongName: {
            ClassifiedExpr e;
            e.kind      = ExpressionKind_E::Option;
            e.name      = _Tok.getName();
            e.bExplicit = true;
            e.strRaw    = _Tok.toText();
            return e;
        }
        case TT::ShortName: {
            ClassifiedExpr e;
            e.kind       = ExpressionKind_E::Option;
            e.cShortName = _Tok.getName().empty() ? '\0' : _Tok.getName()[0];
            e.name       = _Tok.getName();
            e.bIsBool    = true;
            e.bExplicit  = true;
            e.strRaw     = _Tok.toText();
            return e;
        }
        case TT::ContinualName: {
            ClassifiedExpr e;
            e.kind       = ExpressionKind_E::Option;
            e.cShortName = _Tok.getName().empty() ? '\0' : _Tok.getName()[0];
            e.name       = _Tok.getName().empty() ? "" : std::string(1, _Tok.getName()[0]);
            e.bIsBool    = true;
            e.bExplicit  = true;
            e.strRaw     = _Tok.toText();
            return e;
        }
        default: return std::nullopt;
        }
    }

    /// @brief 查询长名选项是否已注册（含短回路 action）
    auto knowsLong(std::string const &_Name) const noexcept -> bool
    {
        return (pData__ && pData__->mgrValues.find(_Name))
            || (pData__ && pData__->mgrActions.doGet(_Name));
    }

    /// @brief 查询短名选项是否已注册
    auto knowsShort(char _C) const noexcept -> bool
    {
        return pData__ && pData__->mgrValues.mutFind(_C);
    }

    /// @brief 查询长名选项是否为 bool 类型
    auto isBoolLong(std::string const &_Name) const noexcept -> bool
    {
        if (!pData__) return false;
        auto const *p = pData__->mgrValues.find(_Name);
        return p && p->strType == "bool";
    }

    /// @brief 查询短名选项是否为 bool 类型
    auto isBoolShort(char _C) const noexcept -> bool
    {
        if (!pData__) return false;
        auto const *p = pData__->mgrValues.mutFind(_C);
        return p && p->strType == "bool";
    }

    /// @brief 聚合串探测结果
    enum class ProbeAgg_E : char
    {
        AllBool,    ///< 所有字符均为已注册 bool 标志
        NeedsValue, ///< 最后一个字符是已注册非 bool 选项（需要值）
        UnknownChar ///< 包含未注册的字符
    };

    /// @brief 聚合串探测：返回聚合串解析结果
    auto probeAggregate(std::string const &_Raw) const -> ProbeAgg_E
    {
        if (_Raw.size() <= 1) return ProbeAgg_E::UnknownChar;
        bool lastIsNonBool = false;
        for (size_t i = 1; i < _Raw.size(); ++i) {
            char key = _Raw[i];
            auto const *pAct = pData__ ? pData__->mgrActions.doGet(key) : nullptr;
            if (pAct) continue;
            auto const *pOpt = pData__ ? pData__->mgrValues.mutFind(key) : nullptr;
            if (!pOpt) return ProbeAgg_E::UnknownChar;
            if (pOpt->strType != "bool") {
                if (i == _Raw.size() - 1) { lastIsNonBool = true; }
                else { return ProbeAgg_E::UnknownChar; }
            }
        }
        return lastIsNonBool ? ProbeAgg_E::NeedsValue : ProbeAgg_E::AllBool;
    }

    /// @brief 触发短回路 action（by long name）
    auto fireAction(std::string const &_Name) -> bool
    {
        if (!pData__) return false;
        auto const *pSc = pData__->mgrActions.doGet(_Name);
        if (!pSc) return false;
        bFired__ = true;
        static_cast<OptionAction const *>(pSc)->fnCallback();
        return true;
    }

    /// @brief 触发短回路 action（by short char）
    auto fireActionShort(char _C) -> bool
    {
        if (!pData__) return false;
        auto const *pSc = pData__->mgrActions.doGet(_C);
        if (!pSc) return false;
        bFired__ = true;
        static_cast<OptionAction const *>(pSc)->fnCallback();
        return true;
    }

    /// @brief Type-safe 应用：将 ClassifiedExpr 写入 OptionData
    /// @return true = 已应用（registered option）；false = 未注册
    auto applyExpr(ClassifiedExpr const &_E) -> bool
    {
        if (_E.bIsAction) return true;

        // 长选项
        if (!_E.strRaw.empty() && _E.strRaw[0] == '-' && _E.strRaw.size() >= 2 && _E.strRaw[1] == '-') {
            auto *pOpt = pData__->mgrValues.mutFind(_E.name);
            if (!pOpt) return false;

            if (_E.value.empty() && pOpt->strType != "bool") {
                throw ErrorParse(ParseError_E::NEED_VALUE,
                    "option --" + _E.name + " need a value");
            }
            __assign(pOpt, _E.value, pOpt->strType == "bool");
            return true;
        }

        // 短选项 / 聚合
        if (!_E.strRaw.empty() && _E.strRaw.size() >= 2 && _E.strRaw[0] == '-') {
            __consumeShortAggregate(_E.strRaw, _E.value);
            return true;
        }

        return false;
    }

    /// @brief 结构化关系入图
    void applyRelation(ClassifiedExpr const &_E)
    {
        if (!pGraph__) return;
        if (_E.kind == ExpressionKind_E::Conditional) {
            pGraph__->addEdge(_E.strCondition, _E.strThenBranch,
                OptionGraph::EdgeKind_E::Conditional, _E.strElseBranch);
        }
        else if (_E.kind == ExpressionKind_E::Dependency) {
            pGraph__->addEdge(_E.strDepSource, _E.strDepTarget, OptionGraph::EdgeKind_E::Dependency);
        }
    }

private:
    /// @brief 纯解析：赋值 + bExplicit__ 标记 + 记录解析顺序；子类可 override onOptionAssigned 实现内联 action
    void __assign(OptionValue *_Opt, std::string const &_Value, bool _IsBool)
    {
        if (_IsBool) {
            _Opt->setValue(_Value.empty() ? "1" : (util::parse_bool(_Value) ? "1" : "0"));
        }
        else {
            _Opt->setValue(_Value);
        }
        _Opt->bExplicit__ = true;

        if (_Opt->bBuiltin__ && _Opt->fnOnSet__) {
            // 内置交互选项（--load-config/--env-set/--env-unset/--save-config）
            // 语义为「解析期立即生效」：内联触发回调，使其影响后续解析
            // （如 --load-config 需先导入配置值，方位参数数量校验才能通过）。
            // 不记入 actionOrder——Executor 不会重复触发。
            _Opt->fnOnSet__(_Opt->getValue());
        }
        else if (pData__ && (_Opt->fnOnSet__ || !_Opt->strActionTemplate__.empty())) {
            // 记录含 action 的用户选项的解析顺序（Executor 按此顺序统一执行）
            pData__->recordParseOrder(_Opt->strLongName);
        }

        // 虚拟钩子：子类（Interpreter）可 override 实现逐 Token 内联 action 执行
        onOptionAssigned(_Opt, _Opt->getValue());
    }

    /// @brief 选项赋值后的虚拟钩子（纯解析 = 空操作；子类 override 实现 action 执行）
    virtual void onOptionAssigned(OptionValue *, OptionValue::Value_T const &) {}

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

    /// @brief 聚合短选项消费（from strRaw + value）
    void __consumeShortAggregate(std::string const &_Tok, std::string const &_Value)
    {
        if (_Tok.size() == 2) {
            char key = _Tok[1];
            if (fireActionShort(key)) return;
            auto *pOpt = pData__->mgrValues.mutFind(key);
            if (!pOpt) {
                throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                    "unrecognized short option '" + std::string(1, key) + "' in " + _Tok);
            }
            __assign(pOpt, _Value, pOpt->strType == "bool");
            return;
        }
        for (size_t i = 1; i < _Tok.size(); ++i) {
            char key = _Tok[i];
            if (fireActionShort(key)) return;
            auto *pOpt = pData__->mgrValues.mutFind(key);
            if (!pOpt) {
                throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                    "unrecognized short option '" + std::string(1, key) + "' in " + _Tok);
            }
            if (pOpt->strType == "bool") {
                __assign(pOpt, "", true);
            } else {
                std::string val = _Tok.substr(i + 1);
                if (!val.empty() && val.front() == '=') { val = val.substr(1); }
                if (val.empty() && !_Value.empty()) { val = _Value; }
                if (val.empty()) {
                    throw ErrorParse(ParseError_E::NEED_VALUE,
                        "option -" + std::string(1, key) + " need a value");
                }
                __assign(pOpt, val, false);
                return;
            }
        }
    }

private:
    OptionData   *pData__ = nullptr;  ///< 底层选项数据仓库指针
    OptionGraph  *pGraph__ = nullptr; ///< 关系有向图（可选，为 nullptr 时关系语法被忽略）
    bool          bFired__ = false;   ///< 短回路是否已命中（命中即停止本次解析）
    bool          bOptEnd__ = false;  ///< 是否已结束选项区（`--` 之后）
};

CMD_ARGS_NAMESPACE_END
