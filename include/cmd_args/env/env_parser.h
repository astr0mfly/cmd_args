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

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/expression/classified_expr.h"
#include "cmd_args/expression/token.h"
#include "cmd_args/context/context_fwd.h"

#include "cmd_args/env/env.h"
#include "cmd_args/env/env_data.h"

CMD_ARGS_NAMESPACE_BEGIN

/**
 * 环境变量解析/服务：装载进程环境（envp）、读取并标注“被引用”、
 * ${VAR}/%VAR%/$VAR 展开、环境展示、并把会话变更回写操作系统。
 */
class EnvParser
{
public:
    /// @brief 构造环境变量解析/服务对象
    /// @param _Data 底层环境数据仓库（非空）
    EnvParser(EnvData *_Data) noexcept
        : pData__(_Data)
    {}

    // ===== Expression 识别规则（编译前端接口） =====

    /// @brief 识别环境变量串是否匹配 Env 规则
    /// @param _EnvStr 环境变量串（"KEY=VALUE" 格式）
    /// @return 识别成功返回 ClassifiedExpr，否则 nullopt
    auto recognize(std::string const &_EnvStr) const -> std::optional<ClassifiedExpr>
    {
        auto eq = _EnvStr.find('=');
        if (eq == std::string::npos) return std::nullopt;

        ClassifiedExpr expr;
        expr.kind      = ExpressionKind_E::Env;
        expr.name      = _EnvStr.substr(0, eq);
        expr.value     = _EnvStr.substr(eq + 1);
        expr.bExplicit = true;
        return expr;
    }

    /// @brief 将识别结果应用到 EnvData
    /// @param _Expr recognize() 返回的识别结果
    void apply(ClassifiedExpr const &_Expr)
    {
        if (_Expr.kind != ExpressionKind_E::Env) return;
        pData__->doPush(_Expr.name, _Expr.value, false);
    }

    /// @brief 装载进程环境（envp "KEY=VALUE" 串）
    /// @param _Envp 环境串列表（无 '=' 的行忽略）
    void parse(Envs_T const &_Envp)
    {
        for (auto const &e : _Envp) {
            auto eq = e.find('=');
            if (eq == std::string::npos) continue;  // 无 '=' 的行不是合法环境项
            pData__->doPush(e.substr(0, eq), e.substr(eq + 1), false);
        }
    }

    /// @brief 键是否存在
    bool has(EnvData::Key_T const &_Key) const noexcept { return pData__->doFind(_Key); }

    /// @brief 按键查询条目（只读，未找到返回 nullptr）
    auto find(EnvData::Key_T const &_Key) const noexcept -> environment const *
    {
        return pData__->doGet(_Key);
    }

    /// @brief 读取并标记“被引用”（缺失返回空串）
    auto get(EnvData::Key_T const &_Key) const -> EnvData::Value_T
    {
        environment *p = pData__->doMutGet(_Key);
        if (!p) return {};
        p->bReferenced__ = true;
        return p->strValue;
    }

    /// @brief 读取并标记“被引用”，缺失返回兜底值
    auto getOr(EnvData::Key_T const &_Key,
               EnvData::Value_T const &_Default = {}) const -> EnvData::Value_T
    {
        environment *p = pData__->doMutGet(_Key);
        if (!p) return _Default;
        p->bReferenced__ = true;
        return p->strValue;
    }

    /// @brief 标记“被引用”（不取值）
    void touch(EnvData::Key_T const &_Key) const noexcept
    {
        environment *p = pData__->doMutGet(_Key);
        if (p) p->bReferenced__ = true;
    }

    /// @brief 条目个数
    auto size() const noexcept -> std::size_t { return pData__->doSize(); }

    /// @brief 是否为空
    bool empty() const noexcept { return pData__->doEmpty(); }

    /// @brief 被引用条目数
    auto referencedCount() const noexcept -> std::size_t
    {
        std::size_t n = 0;
        pData__->doVisit([&](environment const &e) { if (e.bReferenced__) ++n; });
        return n;
    }

    /// @brief 被修改/新建条目数
    auto changedCount() const noexcept -> std::size_t
    {
        std::size_t n = 0;
        pData__->doVisit([&](environment const &e) { if (e.bModified__) ++n; });
        return n;
    }

    /// @brief 只读遍历
    void forEach(std::function<void(environment const &)> const &_Fn) const
    {
        pData__->doVisit(_Fn);
    }

    /// @brief 环境展示（含 *>+ 标记）
    /// @param _Program 程序名（展示头部）
    auto envString(EnvData::Value_T const &_Program) const -> std::string
    {
        std::ostringstream oss;
        oss << "environments: " << _Program << "\n";
        oss << "No. : \"key\" = \"value\"\n";
        std::size_t i = 0;
        pData__->doVisit([&](environment const &e) {
            oss << i;
            if (e.bModified__) oss << '*';
            if (e.bReferenced__) oss << '>';
            if (e.bNew__) oss << '+';
            oss << " : \"" << e.strKey << "\" = \"" << e.strValue << "\"\n";
            ++i;
        });
        return oss.str();
    }

    /// @brief 环境变量展开：${NAME} / %NAME% / $NAME（命中标记“被引用”）
    /// @param _Text 待展开文本（未命中保持原样）
    auto expand(EnvData::Value_T const &_Text) const -> std::string
    {
        std::string ret;
        std::string strFallback;
        ret.reserve(_Text.size());

        for (std::size_t i = 0; i < _Text.size();) {
            char ch = _Text[i];
            if (ch == '$') {
                std::string name;
                std::size_t endPos = std::string::npos;
                bool        brace  = false;
                if (i + 1 < _Text.size() && _Text[i + 1] == '{') {
                    endPos = _Text.find('}', i + 2);
                    if (endPos != std::string::npos) {
                        std::string inner = _Text.substr(i + 2, endPos - i - 2);
                        auto        sep   = inner.find(":-");
                        if (sep != std::string::npos) {
                            name        = inner.substr(0, sep);
                            strFallback = inner.substr(sep + 2);
                        }
                        else {
                            name = inner;
                        }
                        brace = true;
                    }
                }
                else if (i + 1 < _Text.size()
                         && (std::isalpha(static_cast<unsigned char>(_Text[i + 1]))
                             || _Text[i + 1] == '_')) {
                    size_t j = i + 1;
                    while (j < _Text.size()
                           && (std::isalnum(static_cast<unsigned char>(_Text[j]))
                               || _Text[j] == '_')) {
                        ++j;
                    }
                    name   = _Text.substr(i + 1, j - i - 1);
                    endPos = j;
                }

                if (!name.empty() && endPos != std::string::npos) {
                    environment *p = pData__->doMutGet(name);
                    if (p) {
                        p->bReferenced__ = true;
                        ret += p->strValue;
                        i = brace ? endPos + 1 : endPos;
                        strFallback.clear();
                        continue;
                    }
                    if (!strFallback.empty()) {
                        ret += strFallback;
                        i = brace ? endPos + 1 : endPos;
                        strFallback.clear();
                        continue;
                    }
                }
                ret += ch;
                ++i;
                continue;
            }
            if (ch == '%') {
                auto endPos = _Text.find('%', i + 1);
                if (endPos != std::string::npos) {
                    auto name = _Text.substr(i + 1, endPos - i - 1);
                    if (!name.empty()) {
                        environment *p = pData__->doMutGet(name);
                        if (p) {
                            p->bReferenced__ = true;
                            ret += p->strValue;
                            i = endPos + 1;
                            continue;
                        }
                    }
                }
                ret += ch;
                ++i;
                continue;
            }
            ret += ch;
            ++i;
        }

        return ret;
    }

    /// @brief 把会话中的修改/新建回写到操作系统环境变量
    /// @return 实际写回的 "KEY=VALUE" 列表（写回成功后清除 modified/new 标记）
    auto apply() -> std::vector<std::string>
    {
        std::vector<std::string> applied;
        pData__->doMutVisit([&](environment &e) {
            // 被修改的（setEnv 改已有值）与本次新建的（setEnv 新建）都回写
            if (!e.bModified__ && !e.bNew__) return;
#ifdef _WIN32
            _putenv_s(e.strKey.c_str(), e.strValue.c_str());
#else
            setenv(e.strKey.c_str(), e.strValue.c_str(), 1);
#endif
            e.bModified__ = false;
            e.bNew__      = false;
            applied.emplace_back(e.strKey + "=" + e.strValue);
        });
        return applied;
    }

    // ===== Token 规则接口（编译前端） =====

    /// @brief Env 表达式序列匹配结果
    struct EnvMatch_T
    {
        ClassifiedExpr expr;
        std::size_t    nConsumed;  ///< 消耗的 Token 数
    };

    /// @brief Env 行模式序列匹配：Variable(::Variable)* [Delimiter("=") String]
    auto recognizeEnvAssign(TokenStream_T const &_Buf, std::size_t _Pos) const
        -> std::optional<EnvMatch_T>
    {
        using TT = Token::Type_E;
        if (_Pos >= _Buf.size()) return std::nullopt;
        if (_Buf[_Pos].getAttr() != TT::Variable) return std::nullopt;

        ClassifiedExpr expr;
        expr.kind      = ExpressionKind_E::Env;
        expr.bExplicit = true;
        std::size_t pos = _Pos;

        // 空间级联：Variable (:: Variable)*
        while (pos + 2 < _Buf.size()
            && _Buf[pos + 1].getAttr() == TT::ScopeSep
            && _Buf[pos + 2].getAttr() == TT::Variable) {
            expr.vecSpacePath.push_back(_Buf[pos].getName());
            pos += 2;
        }

        // 叶子名 = 最后一个 Variable
        expr.name = _Buf[pos].getName();
        ++pos;

        // 可选 '=' + 值
        if (pos < _Buf.size() && _Buf[pos].getAttr() == TT::Delimiter && _Buf[pos].getName() == "=") {
            ++pos;
            if (pos < _Buf.size()) {
                expr.value = _Buf[pos].getName();
                ++pos;
            }
        }

        expr.strRaw = expr.fullPath() + "=" + expr.value;
        return EnvMatch_T{ std::move(expr), pos - _Pos };
    }

    /// @brief 应用 Env 表达式到 EnvData
    void applyExpr(ClassifiedExpr const &_E)
    {
        if (_E.kind != ExpressionKind_E::Env) return;
        auto key = _E.fullPath();
        pData__->doPush(key, _E.value, false);
    }

private:
    EnvData *pData__ = nullptr;  ///< 底层环境数据仓库指针
};

CMD_ARGS_NAMESPACE_END
