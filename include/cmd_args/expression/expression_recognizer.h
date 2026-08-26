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

#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"
#include "cmd_args/expression/classified_expr.h"
#include "cmd_args/expression/expression.h"
#include "cmd_args/expression/token.h"
#include "cmd_args/option/option_parser.h"
#include "cmd_args/argument/argument_parser.h"
#include "cmd_args/env/env_parser.h"

CMD_ARGS_NAMESPACE_BEGIN

/**
 * @brief 表达式识别器：编译前端——把类型化 Token 流归约为 Expression 序列。
 *
 * 编译前端思想：
 * - 词法分析（Lexer/Scanner）：字符流 → Token 流
 * - 语法分析（ExpressionRecognizer）：Token 流 → Expression 流（本类）
 * - 语义分析（Compiler/Interpreter）：Expression 流 → Model
 *
 * 使用方式：
 * - 流式模式（Interpreter）：逐 token 调用 feed()，产出的 Expression 追加到输出向量
 * - 批量模式（Compiler）：调用 recognizeAll() 一次识别全部
 *
 * 归约状态机：
 * - PendingKind_E：{None, LongOpt, ShortOpt, NamedArg} — 等待取值的挂起方
 * - SeqKind_E：{None, HeadStash, CondAwaitColon, CondAwaitElse} — 关系序列组装
 */
class ExpressionRecognizer
{
public:
    /// @brief 构造表达式识别器
    ExpressionRecognizer(OptionParser *_Opt, ArgumentParser *_Arg, EnvParser *_Env)
        : pOptParser__(_Opt)
        , pArgParser__(_Arg)
        , pEnvParser__(_Env)
    {}

    /// @brief 流式识别：喂入一个 Token，产出的 Expression 追加到 _Out
    void feed(Token const &_Tok, std::vector<ClassifiedExpr> &_Out)
    {
        vecBuf__.push_back(_Tok);
        bool progress = true;
        while (progress) {
            progress = false;
            if (__tryConsumePending(_Out)) { progress = true; continue; }
            if (__tryReduce(_Out, progress)) { progress = true; }
        }
    }

    /// @brief 流尾收尾：截断关系序列和挂起缺值表达式
    void finish(std::vector<ClassifiedExpr> &_Out)
    {
        // 1. 截断关系序列
        if (enSeq__ == SeqKind_E::HeadStash && !vecBuf__.empty()) {
            _Out.push_back(__makeAggregateCandidate(vecBuf__.front()));
            vecBuf__.clear();
            __clearSeq();
        }
        if (enSeq__ == SeqKind_E::CondAwaitColon || enSeq__ == SeqKind_E::CondAwaitElse) {
            ClassifiedExpr e;
            e.kind          = ExpressionKind_E::Conditional;
            e.strCondition  = strSeqHead__;
            e.strThenBranch = strSeqB__;
            e.strElseBranch = (enSeq__ == SeqKind_E::CondAwaitElse) ? strSeqC__ : "_";
            e.bExplicit     = true;
            _Out.push_back(std::move(e));
            vecBuf__.clear();
            __clearSeq();
        }
        // 2. 挂起缺值
        if (!strPendingKey__.empty()) {
            ClassifiedExpr e;
            e.bExplicit   = true;
            e.bPositional = false;
            switch (enPendingKind__) {
            case PendingKind_E::LongOpt:
                e.kind = ExpressionKind_E::Option;
                e.name = strPendingKey__;
                e.strRaw = "--" + strPendingKey__;
                break;
            case PendingKind_E::ShortOpt:
                e.kind       = ExpressionKind_E::Option;
                e.cShortName = strPendingKey__[0];
                e.name       = strPendingKey__;
                e.strRaw     = "-" + strPendingKey__;
                break;
            case PendingKind_E::NamedArg:
                e.kind   = ExpressionKind_E::Argument;
                e.name   = strPendingKey__;
                e.strRaw = "--" + strPendingKey__;
                break;
            default: break;
            }
            _Out.push_back(std::move(e));
            __clearPending();
        }
    }

    /// @brief 批量识别：Token 流 → ClassifiedExpr 列表
    auto recognizeAll(TokenStream_T const &_Toks) -> std::vector<ClassifiedExpr>
    {
        reset();
        std::vector<ClassifiedExpr> result;
        for (auto const &tok : _Toks) { feed(tok, result); }
        finish(result);
        return result;
    }

    /// @brief 复位内部状态
    void reset()
    {
        vecBuf__.clear();
        __clearPending();
        __clearSeq();
        bOptEnd__ = false;
        vecDashLeft__.clear();
    }

    /// @brief 是否有未完成的 pending 取值
    bool hasPending() const noexcept { return !strPendingKey__.empty(); }

    /// @brief 归约缓冲深度（Interpreter getStackSize 委托此值）
    auto getBufferSize() const noexcept -> std::size_t { return vecBuf__.size(); }

private:
    enum class PendingKind_E : char { None, LongOpt, ShortOpt, NamedArg };
    enum class SeqKind_E     : char { None, HeadStash, CondAwaitColon, CondAwaitElse };

    void __clearPending()
    {
        strPendingKey__.clear();
        enPendingKind__    = PendingKind_E::None;
        bPendingInlineEq__ = false;
    }

    void __clearSeq()
    {
        enSeq__   = SeqKind_E::None;
        strSeqHead__.clear();
        strSeqB__.clear();
        strSeqC__.clear();
    }

    /// @brief 尝试消费 pending 值：buf[0] 作为值完成 pending Expression
    bool __tryConsumePending(std::vector<ClassifiedExpr> &_Out)
    {
        if (strPendingKey__.empty() || vecBuf__.empty()) return false;

        std::string text = vecBuf__.front().toText();

        ClassifiedExpr e;
        e.bExplicit = true;

        switch (enPendingKind__) {
        case PendingKind_E::LongOpt:
            if (!bPendingInlineEq__ && !text.empty() && text[0] == '-') {
                e.kind = ExpressionKind_E::Option;
                e.name = strPendingKey__;
                e.strRaw = "--" + strPendingKey__;
                _Out.push_back(std::move(e));
                __clearPending();
                vecBuf__.pop_front();
                return true;
            }
            e.kind   = ExpressionKind_E::Option;
            e.name   = strPendingKey__;
            e.value  = text;
            e.strRaw = "--" + strPendingKey__;
            _Out.push_back(std::move(e));
            __clearPending();
            vecBuf__.pop_front();
            return true;

        case PendingKind_E::ShortOpt:
            e.kind       = ExpressionKind_E::Option;
            e.cShortName = strPendingKey__[0];
            e.name       = strPendingKey__;
            e.value      = text;
            e.strRaw     = "-" + strPendingKey__;
            _Out.push_back(std::move(e));
            __clearPending();
            vecBuf__.pop_front();
            return true;

        case PendingKind_E::NamedArg:
            e.kind   = ExpressionKind_E::Argument;
            e.name   = strPendingKey__;
            e.value  = text;
            e.strRaw = "--" + strPendingKey__;
            _Out.push_back(std::move(e));
            __clearPending();
            vecBuf__.pop_front();
            return true;

        default: return false;
        }
    }

    /// @brief 尝试归约：buf 内容匹配产生式 → emit Expression
    bool __tryReduce(std::vector<ClassifiedExpr> &_Out, bool &_Progress)
    {
        if (vecBuf__.empty()) return false;

        // 在 erase 前缓存必要信息（避免 tok 悬垂引用）
        auto const tokAttr = vecBuf__[0].getAttr();
        auto const tokName = vecBuf__[0].getName();
        auto const tokText = vecBuf__[0].toText();
        auto const tokInln = vecBuf__[0].hasInlineEq();

        // ── 1. '--' 切换纯方位模式 ──
        if (tokAttr == Token::Type_E::LongName && tokName.empty()) {
            bOptEnd__ = true;
            vecBuf__.pop_front();
            _Progress = true;
            return true;
        }

        // ── 2. bOptEnd__：一律方位参数 ──
        if (bOptEnd__) {
            _Out.push_back(__makePositional(vecBuf__[0]));
            vecBuf__.pop_front();
            _Progress = true;
            return true;
        }

        // ── 3. LongName{n}：选项/命名参数 ──
        if (tokAttr == Token::Type_E::LongName && !tokName.empty()) {
            auto const &name = tokName;

            // 3a. 短回路 action
            if (pOptParser__ && pOptParser__->fireAction(name)) {
                ClassifiedExpr e;
                e.kind      = ExpressionKind_E::Option;
                e.name      = name;
                e.bIsAction = true;
                e.bExplicit = true;
                e.strRaw    = tokText;
                _Out.push_back(std::move(e));
                vecBuf__.pop_front();
                _Progress = true;
                return true;
            }

            // 3b. 已注册长选项
            if (pOptParser__ && pOptParser__->knowsLong(name)) {
                if (pOptParser__->isBoolLong(name)) {
                    ClassifiedExpr e;
                    e.kind      = ExpressionKind_E::Option;
                    e.name      = name;
                    e.bIsBool   = true;
                    e.bExplicit = true;
                    e.strRaw    = tokText;
                    _Out.push_back(std::move(e));
                    vecBuf__.pop_front();
                    _Progress = true;
                    return true;
                }
                strPendingKey__    = name;
                enPendingKind__    = PendingKind_E::LongOpt;
                bPendingInlineEq__ = tokInln;
                vecBuf__.pop_front();
                _Progress = true;
                return true;
            }

            // 3c. 命名参数
            if (pArgParser__ && pArgParser__->recognizeToken(vecBuf__[0])) {
                strPendingKey__ = name;
                enPendingKind__ = PendingKind_E::NamedArg;
                vecBuf__.pop_front();
                _Progress = true;
                return true;
            }

            // 3d. 未识别 → dash-left
            vecDashLeft__.push_back(tokText);
            vecBuf__.pop_front();
            _Progress = true;
            return true;
        }

        // ── 4. 关系序列组装 ──
        if (enSeq__ == SeqKind_E::HeadStash) {
            // 快速路径：vecBuf__ 有两个 token 且第二个不可能是关系尾
            if (vecBuf__.size() > 1) {
                auto const nextAttr = vecBuf__[1].getAttr();
                if (nextAttr == Token::Type_E::Variable
                    || nextAttr == Token::Type_E::Integer
                    || nextAttr == Token::Type_E::String
                    || nextAttr == Token::Type_E::Real) {
                    // 不可能是关系尾（'?' 是 Delimiter，'>' 是 ContinualName 前缀）
                    // 直接 emit aggregate，跳过 HeadStash 等待
                    _Out.push_back(__makeAggregateCandidate(vecBuf__.front()));
                    vecBuf__.pop_front();
                    __clearSeq();
                    _Progress = true;
                    return true;
                }
            }
            // vecBuf__[0]=head, vecBuf__[1]=current — check vecBuf__[1] for disambiguation
            if (vecBuf__.size() > 1
                && vecBuf__[1].getAttr() == Token::Type_E::Delimiter
                && vecBuf__[1].getName() == "?") {
                // Conditional: head + '?' → CondAwaitColon
                strSeqHead__ = vecBuf__[0].getName();
                vecBuf__.clear();
                enSeq__ = SeqKind_E::CondAwaitColon;
                _Progress = true;
                return true;
            }
            if (vecBuf__.size() > 1
                && vecBuf__[1].getAttr() == Token::Type_E::ContinualName
                && !vecBuf__[1].getName().empty() && vecBuf__[1].getName()[0] == '>') {
                // Dependency: head + ">target"
                ClassifiedExpr e;
                e.kind         = ExpressionKind_E::Dependency;
                e.strDepSource = vecBuf__[0].getName();
                e.strDepTarget = vecBuf__[1].getName().substr(1);
                e.bExplicit    = true;
                _Out.push_back(std::move(e));
                vecBuf__.clear();
                __clearSeq();
                _Progress = true;
                return true;
            }
            // disambiguation 失败：head 作为普通 aggregate candidate
            _Out.push_back(__makeAggregateCandidate(vecBuf__.front()));
            vecBuf__.clear();
            __clearSeq();
            _Progress = true;
            return true;
        }

        if (enSeq__ == SeqKind_E::CondAwaitColon) {
            if (tokAttr == Token::Type_E::Variable) {
                strSeqB__ = tokName;
                vecBuf__.pop_front();
                if (!vecBuf__.empty() && vecBuf__[0].getAttr() == Token::Type_E::Delimiter
                    && vecBuf__[0].getName() == ":") {
                    enSeq__ = SeqKind_E::CondAwaitElse;
                    vecBuf__.pop_front();
                } else {
                    ClassifiedExpr e;
                    e.kind          = ExpressionKind_E::Conditional;
                    e.strCondition  = strSeqHead__;
                    e.strThenBranch = strSeqB__;
                    e.strElseBranch = "_";
                    e.bExplicit     = true;
                    _Out.push_back(std::move(e));
                    __clearSeq();
                }
                _Progress = true;
                return true;
            }
            ClassifiedExpr e;
            e.kind          = ExpressionKind_E::Conditional;
            e.strCondition  = strSeqHead__;
            e.strThenBranch = strSeqB__;
            e.strElseBranch = "_";
            e.bExplicit     = true;
            _Out.push_back(std::move(e));
            __clearSeq();
            _Progress = true;
            return true;
        }

        if (enSeq__ == SeqKind_E::CondAwaitElse) {
            if (tokAttr == Token::Type_E::Variable) {
                ClassifiedExpr e;
                e.kind          = ExpressionKind_E::Conditional;
                e.strCondition  = strSeqHead__;
                e.strThenBranch = strSeqB__;
                e.strElseBranch = tokName;
                e.bExplicit     = true;
                _Out.push_back(std::move(e));
                vecBuf__.pop_front();
                __clearSeq();
                _Progress = true;
                return true;
            }
            ClassifiedExpr e;
            e.kind          = ExpressionKind_E::Conditional;
            e.strCondition  = strSeqHead__;
            e.strThenBranch = strSeqB__;
            e.strElseBranch = "_";
            e.bExplicit     = true;
            _Out.push_back(std::move(e));
            __clearSeq();
            _Progress = true;
            return true;
        }

        // ── 5. ContinualName：可能是 aggregate 或关系 head ──
        if (tokAttr == Token::Type_E::ContinualName && enSeq__ == SeqKind_E::None) {
            if (vecBuf__.size() == 1) {
                enSeq__      = SeqKind_E::HeadStash;
                strSeqHead__ = tokName;
                return false;  // 无进展，等下一次 feed
            }
        }

        // ── 6. 其他 Token ──
        if (tokAttr == Token::Type_E::Delimiter && tokName == "=") {
            vecBuf__.pop_front();
            _Progress = true;
            return true;
        }

        _Out.push_back(__makePositional(vecBuf__[0]));
        vecBuf__.pop_front();
        _Progress = true;
        return true;
    }

    auto __makePositional(Token const &_Tok) -> ClassifiedExpr
    {
        ClassifiedExpr e;
        e.kind       = ExpressionKind_E::Argument;
        e.name       = _Tok.toText();
        e.value      = _Tok.toText();
        e.bExplicit  = true;
        e.bPositional = true;
        e.strRaw     = _Tok.toText();
        return e;
    }

    auto __makeAggregateCandidate(Token const &_Tok) -> ClassifiedExpr
    {
        ClassifiedExpr e;
        e.kind       = ExpressionKind_E::Option;
        e.cShortName = _Tok.getName().empty() ? '\0' : _Tok.getName()[0];
        e.name       = _Tok.getName().empty() ? "" : std::string(1, _Tok.getName()[0]);
        e.bIsBool    = true;
        e.bExplicit  = true;
        e.strRaw     = _Tok.toText();
        return e;
    }

    // ===== 内部状态 =====
    std::deque<Token>        vecBuf__;
    std::string              strPendingKey__    = {};
    PendingKind_E            enPendingKind__    = PendingKind_E::None;
    bool                     bPendingInlineEq__ = false;
    bool                     bOptEnd__          = false;
    std::vector<std::string> vecDashLeft__;

    SeqKind_E          enSeq__   = SeqKind_E::None;
    std::string        strSeqHead__;
    std::string        strSeqB__;
    std::string        strSeqC__;

    OptionParser  *pOptParser__  = nullptr;
    ArgumentParser *pArgParser__ = nullptr;
    EnvParser     *pEnvParser__  = nullptr;
};

CMD_ARGS_NAMESPACE_END
