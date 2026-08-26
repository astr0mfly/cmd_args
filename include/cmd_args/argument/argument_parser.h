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

#include <sstream>
#include <vector>

#include "cmd_args/argument/argument.h"
#include "cmd_args/argument/argument_data.h"
#include "cmd_args/expression/classified_expr.h"
#include "cmd_args/context/context_fwd.h"
#include "cmd_args/error/parser_error.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    参数解析器：
    - 命名参数  : --name=value / --name value
    - 方位参数  : 剩余的非选项 token，按声明顺序依次赋值
    - 数量校验  : 方位参数数量不一致抛 UNKNWON_ARGS（携带明细）
*/
class ArgumentParser
{
public:
    /// @brief 构造参数解析器
    /// @param _Data 底层参数数据仓库（非空）
    ArgumentParser(ArgumentData *_Data)
        : pData__(_Data)
    {}

    // ===== Expression 识别规则（编译前端接口） =====

    /// @brief 识别当前迭代器指向的 token 是否匹配参数规则
    /// @param _It 当前 token 迭代器
    /// @param _Tokens 完整 token 流（用于消费 value token）
    /// @return 识别成功返回 ClassifiedExpr，否则 nullopt
    auto recognize(Tokens_T::iterator _It, Tokens_T const &_Tokens) const -> std::optional<ClassifiedExpr>
    {
        if (_It == _Tokens.end()) return std::nullopt;
        std::string const &tok = *_It;

        // --name=value / --name value 命名参数
        if (util::starts_with(tok, "--") && tok.size() > 2) {
            auto body = tok.substr(2);
            auto eq   = body.find('=');
            std::string name = (eq == std::string::npos) ? body : body.substr(0, eq);
            // 检查是否为已注册的命名参数
            if (pData__->mgrName.doGet(name)) {
                ClassifiedExpr expr;
                expr.kind      = ExpressionKind_E::Argument;
                expr.name      = name;
                expr.bExplicit = true;
                if (eq != std::string::npos) { expr.value = body.substr(eq + 1); }
                return expr;
            }
        }

        // 普通 token（非 - 开头）→ 方位参数
        if (!util::starts_with(tok, "-")) {
            ClassifiedExpr expr;
            expr.kind      = ExpressionKind_E::Argument;
            expr.name      = tok;
            expr.value     = tok;
            expr.bExplicit = true;
            return expr;
        }

        return std::nullopt;  // 不匹配参数规则（可能是选项）
    }

    /// @brief 将识别结果应用到 ArgumentData
    /// @param _Expr recognize() 返回的识别结果
    /// @param _It 当前 token 迭代器（apply 后会消费已处理的 token）
    /// @param _Tokens 完整 token 流（用于消费 value token）
    void apply(ClassifiedExpr const &_Expr, Tokens_T::iterator &_It, Tokens_T &_Tokens)
    {
        if (_Expr.kind != ExpressionKind_E::Argument) return;

        // 命名参数
        if (util::starts_with(*_It, "--") && _It->size() > 2) {
            auto *p = pData__->mgrName.mutGet(_Expr.name);
            if (!p) {
                ++_It;  // 未注册，留给上层
                return;
            }

            std::string value = _Expr.value;
            if (value.empty()) {
                auto nx = std::next(_It);
                if (nx == _Tokens.end()) {
                    throw ErrorParse(ParseError_E::NEED_VALUE,
                        "named argument '--" + _Expr.name + "' need a value");
                }
                value = *nx;
                _Tokens.erase(nx);
            }

            p->set(std::move(value));
            _It = _Tokens.erase(_It);
            return;
        }

        // 方位参数：由 parse() 统一处理（需要数量校验）
        // 此处不消费 token，留给 parse() 处理
    }

    /**
     * @brief 解析 token 流：先消化命名参数，再分配方位参数并做数量校验
     * @param _Tokens 待解析 token 流（已消费项会被移除）
     * @note 数量校验：剩余 token 须落在 [未外部赋值数, 总数] 区间
     */
    void parse(Tokens_T &_Tokens)
    {
        parseNamedOnly(_Tokens);

        // 2. 方位参数：剩余的非选项 token
        std::vector<Tokens_T::value_type> vecPos;
        for (auto const &t : _Tokens) {
            if (!util::starts_with(t, "-")) { vecPos.emplace_back(t); }
        }

        // 数量校验：剩余 token 须不少于"未由配置外部赋值"的位置参数数，
        // 且不多于位置参数总数（配置已提供的位，CLI 可省略、也可覆盖）
        size_t total = pData__->mgrPos.doSize();
        size_t need  = 0;
        pData__->mgrPos.doConVisit([&](Argument const &a) {
            if (!a.isExternal()) ++need;
        });
        if (vecPos.size() < need || vecPos.size() > total) {
            std::ostringstream oss;
            oss << "give " << vecPos.size() << ", but need " << need << ".." << total << '\n';
            oss << "uncaptured command line arguments:\n";
            for (auto const &tok : _Tokens) { oss << tok << '\n'; }
            throw ErrorParse(ParseError_E::UNKNWON_ARGS, oss.str());
        }

        // 恰好满足未赋值者时，跳过配置已填的位；多余 token 覆盖配置值（CLI > 配置）
        bool exact = (vecPos.size() == need);
        size_t vi  = 0;
        for (size_t seq = 0; seq < total && vi < vecPos.size(); ++seq) {
            auto *p = pData__->mgrPos.mutGetBySeq(seq);
            if (!p) continue;
            if (exact && p->isExternal()) continue;
            p->set(vecPos[vi++]);
        }

        // 已消费的非选项 token 全部移出
        for (auto pos = _Tokens.begin(); pos != _Tokens.end();) {
            if (!util::starts_with(*pos, "-")) { pos = _Tokens.erase(pos); }
            else { ++pos; }
        }
    }

    /**
     * @brief 只消化命名参数（--name=value / --name value），不动方位参数
     * @param _Tokens 待解析 token 流（已消费命名参数项会被移除）
     * @note 用于 `--` 结束选项区之前先消费命名参数的场景
     */
    void parseNamedOnly(Tokens_T &_Tokens)
    {
        for (auto pos = _Tokens.begin(); pos != _Tokens.end();) {
            Tokens_T::value_type const &tok = *pos;
            if (!util::starts_with(tok, "--")) {
                ++pos;
                continue;
            }

            auto        body = tok.substr(2);
            auto        eq   = body.find('=');
            std::string name = (eq == std::string::npos) ? body : body.substr(0, eq);
            std::string value;
            if (eq != std::string::npos) { value = body.substr(eq + 1); }

            auto *p = pData__->mgrName.mutGet(name);
            if (!p) {
                ++pos;  // 未知长选项/命名参数，留给上层报错
                continue;
            }

            if (value.empty()) {
                auto nx = std::next(pos);
                if (nx == _Tokens.end()) {
                    throw ErrorParse(
                        ParseError_E::NEED_VALUE,
                        "named argument '--" + name + "' need a value");
                }
                value = *nx;
                _Tokens.erase(nx);
            }

            p->set(std::move(value));
            pos = _Tokens.erase(pos);
        }
    }

    /**
     * @brief `--` 之后进入纯方位模式：剩余全部 token（含 - 开头）当作方位参数
     * @param _Tokens 待解析 token 流（全部清空）
     */
    void parsePositionalOnly(Tokens_T &_Tokens)
    {
        std::vector<Tokens_T::value_type> vecPos(_Tokens.begin(), _Tokens.end());

        size_t total = pData__->mgrPos.doSize();
        size_t need  = 0;
        pData__->mgrPos.doConVisit([&](Argument const &a) {
            if (!a.isExternal()) ++need;
        });
        if (vecPos.size() < need || vecPos.size() > total) {
            std::ostringstream oss;
            oss << "give " << vecPos.size() << ", but need " << need << ".." << total << '\n';
            throw ErrorParse(ParseError_E::UNKNWON_ARGS, oss.str());
        }

        bool   exact = (vecPos.size() == need);
        size_t vi    = 0;
        for (size_t seq = 0; seq < total && vi < vecPos.size(); ++seq) {
            auto *p = pData__->mgrPos.mutGetBySeq(seq);
            if (!p) continue;
            if (exact && p->isExternal()) continue;
            p->set(vecPos[vi++]);
        }
        _Tokens.clear();
    }

    /**
     * @brief "KEY=VALUE" 单行形态：直接给指定命名参数赋值（不经 token 流）
     * @param _Line 形如 "KEY=VALUE" 的输入行
     */
    void parse(std::string const &_Line)
    {
        auto [k, v] = util::parse_line(_Line);
        auto *p     = pData__->mgrName.mutGet(k);
        if (!p) { throw ErrorParse(ParseError_E::UNKNWON_ARGS, "unknown named argument: " + k); }
        p->set(std::move(v));
    }

    /**
     * @brief 带类型校验地读取参数值
     * @tparam T 读取目标类型
     * @param _Name 参数名
     * @return 解析后的 T 值
     * @note 类型未定型（空）时任何 T 均可；否则必须与 type_string<T> 一致
     */
    template <typename T>
    auto getArgument(Argument::Name_T const &_Name) const -> T
    {
        using namespace util;

        auto const *p = pData__->mgrName.doGet(_Name);
        if (!p) { p = pData__->mgrPos.doGet(_Name); }
        if (!p) {
            throw ErrorParse(ParseError_E::UNKNWON_ARGS, "argument not found: " + _Name);
        }

        auto const &type = p->getType();
        if (!type.empty() && type != util::type_string<T>()) {
            throw ErrorParse(
                ParseError_E::UNKNWON_ARGS,
                "argument type mismatch: set '" + type + "' but try get with '"
                    + util::type_string<T>() + "'");
        }

        return util::parse_value<T>(p->getValue());
    }

    /// @brief 查询指定命名参数是否已被解析出非空值
    bool isParsed(NameLong_T const &_Name) const noexcept
    {
        auto const *p = pData__->mgrName.doGet(_Name);
        return p && p->hasValue();
    }

    // 一些 getArgument 的类型别名便捷入口
    /// @brief 便捷读取 int 型参数
    auto getArgumentInt(Argument::Name_T const &_Name) const -> int { return getArgument<int>(_Name); }

    /// @brief 便捷读取 int64_t 型参数
    auto getArgumentInt64(Argument::Name_T const &_Name) const -> int64_t
    {
        return getArgument<int64_t>(_Name);
    }

    /// @brief 便捷读取 double 型参数
    auto getArgumentDouble(Argument::Name_T const &_Name) const -> double
    {
        return getArgument<double>(_Name);
    }

    /// @brief 便捷读取 string 型参数
    auto getArgumentString(Argument::Name_T const &_Name) const -> std::string
    {
        return getArgument<std::string>(_Name);
    }

    /// @brief 生成参数格式摘要（命名 + 方位）
    auto genUsageString() const -> Argument::Desc_T
    {
        Argument::Desc_T res;
        res += pData__->mgrName.doGetUsage();
        res += pData__->mgrPos.doGetUsage();

        return res;
    }

    /// @brief 追加命名/方位帮助文本到输出流
    void appendHelp(std::ostringstream &_Oss, size_t &_Max) const
    {
        pData__->mgrName.doGetHelp(_Oss, _Max);
        pData__->mgrPos.doGetHelp(_Oss, _Max);
    }

private:
    ArgumentData *pData__ = nullptr;  ///< 底层参数数据仓库指针
};

CMD_ARGS_NAMESPACE_END
