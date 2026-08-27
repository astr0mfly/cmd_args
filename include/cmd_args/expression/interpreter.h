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

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/argument/argument_parser.h"
#include "cmd_args/expression/expression_recognizer.h"
#include "cmd_args/expression/lexer.h"
#include "cmd_args/env/env_builder.h"
#include "cmd_args/env/env_parser.h"
#include "cmd_args/error/errors.h"
#include "cmd_args/error/interpreter_error.h"
#include "cmd_args/error/lexer_error.h"
#include "cmd_args/error/parser_error.h"
#include "cmd_args/error/validation_error.h"
#include "cmd_args/expression/validation.h"
#include "cmd_args/option/option_parser.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    Interpreter（流式实时引擎）：把一行命令行当做字符流，
    以成员 idxCursor__ 作输入流游标自入口（下标 0）逐字符泵入 Scanner；
    每识别出一个完整 Token 即压入 token 栈 vecStack__，
    栈内内容凑满一个 Expression（Option / Argument / 方位参数）时立刻出栈执行——
    选项赋值即刻触发 fnOnSet__ 与 action 模板，命名参数即刻 set，
    方位参数按序缓冲至流空后统一提交（数量校验与批量一致）。直到 Stream 为空。

    与批量 Compiler 的区别：
    - Compiler：先纯解析全部 Token 入库 Model，Executor 按 actionOrder 后置统一执行。
    - Interpreter：解析即执行，无需 Executor / actionOrder。
*/
class Interpreter : private ArgumentParser, private OptionParser, private EnvParser
{
public:
    Interpreter(Model *_Model)
        : ArgumentParser(&_Model->args)
        , OptionParser(&_Model->opts)
        , EnvParser(&_Model->envs)
        , pModel__(_Model)
        , rec__(static_cast<OptionParser *>(this),
                static_cast<ArgumentParser *>(this),
                static_cast<EnvParser *>(this))
    {}

    /* ===== 逐行解释 ===== */

    /// @brief 解释执行一行：reset → idxCursor 逐字符泵入 Scanner → token 压栈 → 凑满一个 Expression 立即执行 → 方位参数提交 → finalize
    std::error_code interpret(std::string const &_Line, Errors &_Errors)
    {
        pModel__->args.mgrName.doMutVisit([](Argument &a) { a.reset(); });
        pModel__->args.mgrPos.doMutVisit([](Argument &a) { a.reset(); });
        pModel__->opts.mgrValues.forEachValue([](OptionValue &o) { o.reset(); });
        _Errors.clearLast();

        try {
            cmd_args::detail::Scanner sc({});
            __lineReset();                                       // 游标归零指向输入流入口，栈与挂起状态清空

            while (idxCursor__ < _Line.size()) {
                __feedChar(sc, _Line[idxCursor__]);              // 逐字符识别：完整 token 压栈并尝试归约
                ++idxCursor__;
            }
            __feedChar(sc, '\0');                                // 流结束：flush 最后一个 token

            // 流空后仍有挂起取值（--opt / --name 后无值）：与批量路径一致报 NEED_VALUE
            if (!strPendingKey__.empty()) {
                if (enPendingKind__ == PendingKind_E::NamedArg) {
                    throw ErrorParse(ParseError_E::NEED_VALUE,
                        "named argument '--" + strPendingKey__ + "' need a value");
                }
                if (enPendingKind__ == PendingKind_E::LongOpt) {
                    throw ErrorParse(ParseError_E::NEED_VALUE,
                        "option --" + strPendingKey__ + " need a value");
                }
                throw ErrorParse(ParseError_E::NEED_VALUE,
                    "option -" + strPendingKey__ + " need a value");
            }

            // 未识别 dash token：先于方位参数提交报 UNKNOWN_OPTION（UNKNOWN_OPTION 优先，
            // 且绝不占用方位槽位、不产生赋值副作用——批量语义中 dash token 不进方位缓冲）。
            if (!vecDashLeft__.empty()) {
                throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                    "unrecognized option or argument: " + vecDashLeft__.front());
            }

            __assignPositional(vecPos__);                        // 数量校验 + 按序提交

            __finalize();
            bParsed__ = true;
            return make_error_code(ParseError_E::OK);
        }
        catch (ErrorParse const &e) { _Errors.setLast(e.code()); return e.code(); }
        catch (ErrorLexer const &e) { _Errors.setLast(e.code()); return e.code(); }
        catch (ErrorValidation const &e) { _Errors.setLast(e.code()); return e.code(); }
        catch (std::exception const &ex) {
            _Errors.setMsg(ex.what());
            return make_error_code(ParseError_E::UNKNWON_ARGS);
        }
    }

    /* ===== 解析后置：seed 环境兜底 + ${VAR} 展开 ===== */

    void finalize()
    {
        __seedEnvDefaults();
        __expandEnvValues();

        if (!pModel__->strSaveConfigPath.empty()) {
            auto text = config::to_yaml(*pModel__);
            if (!config::write_file(pModel__->strSaveConfigPath, text)) {
                std::cerr << "(save-config) cannot write file: " << pModel__->strSaveConfigPath << "\n";
            }
        }
    }

    /* ===== once / loop ===== */

    void setExitCode(int _Code) noexcept { nExitCode__ = _Code; }
    int  exitCode() const noexcept { return nExitCode__; }
    void markParsed() noexcept { bParsed__ = true; }
    bool prepared() const noexcept { return bParsed__; }

    std::string describeError(std::error_code const &_Ec, Errors &_Errors) const
    {
        return std::string("(parse error) ") + _Ec.message() + "\n" + _Errors.dump();
    }

    /* ===== 实时解析观测 ===== */

    /// @brief 当前输入流游标（指向下一个待识别字符；行首为 0，整行解释完等于行长）
    auto getCursor() const noexcept -> size_t { return idxCursor__; }

    /// @brief 当前 token 栈深（已识别但尚未凑满 Expression 的 token 数；整行解释完应为 0）
    auto getStackSize() const noexcept -> size_t { return rec__.getBufferSize(); }

    [[noreturn]] void once()
    {
        if (!bParsed__) { throw ErrorInterp(InterpError_E::STOPPED); }
        std::exit(nExitCode__);
    }

    void loop(Errors &_Errors)
    {
        std::cout << "cmd_args> " << std::flush;
        std::string line;
        while (std::getline(std::cin, line)) {
            auto cmd = util::trim(line);
            if (cmd.empty()) {
                std::cout << "cmd_args> " << std::flush;
                continue;
            }
            if (cmd == "exit" || cmd == "quit" || cmd == "exit()") break;

            auto ec = interpret(cmd, _Errors);
            if (ec) { std::cout << describeError(ec, _Errors); }

            std::cout << "cmd_args> " << std::flush;
        }
        std::cout << "\n";
    }

private:
    // ===== 流式解析状态机 =====

    /// @brief 行首复位：游标归零指向输入流入口，token 栈与挂起状态全部清空
    void __lineReset()
    {
        idxCursor__        = 0;
        vecStack__.clear();
        strPendingKey__.clear();
        enPendingKind__    = PendingKind_E::None;
        bPendingInlineEq__ = false;
        bOptEnd__          = false;
        vecPos__.clear();
        vecDashLeft__.clear();
    }

    /// @brief 泵入单字符：Scanner 识别出完整 token 即压入 token 栈，随后尝试归约执行
    void __feedChar(detail::Scanner &_Sc, char _C)
    {
        auto tokOpt = _Sc.step(_C);
        if (!tokOpt) return;
        vecStack__.emplace_back(std::move(*tokOpt));  ///< 识别即压栈
        __reduceStack();                              ///< 栈凑满一个 Expression 就立即执行
    }

    /**
     * @brief 归约：检查 token 栈顶，凑满一个 Expression 即出栈立即执行
     * @note 归约规则（与批量 Parser 语义逐条对齐）：
     *       1. 有挂起取值方：栈顶 token 就是值 → 组合立即执行并清除挂起；
     *       2. '--' 之后一律方位参数；'--' 本身切换纯方位模式；
     *       3. --name[=v]：短回路/bool 即刻完成；非 bool 等下个 token 凑满（头出栈转挂起键）；
     *       4. -x / -abc 聚合：bool 位即刻完成；值选项吃余串或挂起等下个 token；
     *       5. 普通 token 入方位缓冲（流空后统一校验数量再按序提交）。
     */
    void __reduceStack()
    {
        auto tok = std::move(vecStack__.back());
        vecStack__.pop_back();
        std::string text      = tok.toText();
        bool        bInlineEq = tok.hasInlineEq();

        // ── 1. 挂起取值：当前 token 就是值 ──
        if (!strPendingKey__.empty()) {
            // 消费规则（批量语义）：
            // - LongOpt：拒绝 dash 开头值，除非挂起方带内联 '='（--opt=-5）
            // - ShortOpt：无条件接受下一个 token（option_parser.h 短路径无 dash 检查）
            // - NamedArg：无条件接受下一个 token（ArgumentParser::parseNamedOnly 语义）
            if (enPendingKind__ == PendingKind_E::LongOpt && !bPendingInlineEq__
                && util::starts_with(text, "-")) {
                throw ErrorParse(ParseError_E::NEED_VALUE,
                    "option --" + strPendingKey__ + " need a value");
            }
            if (enPendingKind__ == PendingKind_E::NamedArg) {
                if (auto *pArg = pModel__->args.mgrName.mutGet(strPendingKey__)) {
                    pArg->set(text);
                }
            }
            else if (enPendingKind__ == PendingKind_E::ShortOpt) {
                // 短名按 char 查（mutFind(std::string) 只按长名匹配）
                if (auto *pOpt = pModel__->opts.mgrValues.mutFind(strPendingKey__.front())) {
                    __executeOptionImmediate(pOpt, text, pOpt->strType == "bool");
                }
            }
            else if (auto *pOpt = pModel__->opts.mgrValues.mutFind(strPendingKey__)) {
                __executeOptionImmediate(pOpt, text, pOpt->strType == "bool");
            }
            strPendingKey__.clear();
            enPendingKind__    = PendingKind_E::None;
            bPendingInlineEq__ = false;
            return;
        }

        // ── 2. '--' 之后全部方位参数 ──
        if (bOptEnd__) { vecPos__.emplace_back(text); return; }

        if (text == "--") { bOptEnd__ = true; return; }

        // ── 3. 长选项 / 命名参数：--name[=value] ──
        if (util::starts_with(text, "--") && text.size() > 2) {
            auto        body  = text.substr(2);
            auto        eq    = body.find('=');
            std::string name  = body.substr(0, eq == std::string::npos ? body.size() : eq);
            std::string value = eq == std::string::npos ? "" : body.substr(eq + 1);

            if (auto const *pSc = pModel__->opts.mgrActions.doGet(name)) {
                static_cast<OptionAction const *>(pSc)->fnCallback();  // 短回路：立即回调
                return;
            }
            if (auto *pOpt = pModel__->opts.mgrValues.mutFind(name)) {
                // 无内联值：等待下一 token 取值。bInlineEq 表达「值在下个 token
                // 且允许 dash 开头」（--opt=value 被 Scanner 在 '=' 处切开）
                if (value.empty() && (bInlineEq || pOpt->strType != "bool")) {
                    strPendingKey__    = name;
                    enPendingKind__    = PendingKind_E::LongOpt;
                    bPendingInlineEq__ = bInlineEq;
                    return;
                }
                __executeOptionImmediate(pOpt, value, pOpt->strType == "bool");
                return;
            }
            if (auto *pArg = pModel__->args.mgrName.mutGet(name)) {
                if (value.empty()) {
                    strPendingKey__    = name;
                    enPendingKind__    = PendingKind_E::NamedArg;
                    bPendingInlineEq__ = false;
                    return;
                }
                pArg->set(std::move(value));  // 命名参数表达式：立即 set
                return;
            }
            vecDashLeft__.emplace_back(text);  // 未识别长选项：独立缓冲，流末统一报 UNKNOWN_OPTION
            return;
        }

        // ── 4. 短选项：-x / 聚合 -abc / 关系 -a?b:c / -a->b ──
        if (util::starts_with(text, "-") && text.size() >= 2) {
            // 关系语法检测
            if (__isRelationText(text)) {
                auto expr = __classifyRelationText(text);
                __handleRelation(expr);
                return;
            }

            for (size_t i = 1; i < text.size(); ++i) {
                char key = text[i];
                if (auto const *pSc = pModel__->opts.mgrActions.doGet(key)) {
                    static_cast<OptionAction const *>(pSc)->fnCallback();
                    continue;
                }
                auto *pOpt = pModel__->opts.mgrValues.mutFind(key);
                if (!pOpt) {
                    throw ErrorParse(ParseError_E::UNKNOWN_OPTION,
                        "unrecognized short option '" + std::string(1, key) + "' in " + text);
                }
                if (pOpt->strType == "bool") {
                    __executeOptionImmediate(pOpt, "", true);
                    continue;
                }
                std::string val = text.substr(i + 1);  // 值选项：余下即值（-tvalue / -t=value）
                if (!val.empty() && val.front() == '=') { val = val.substr(1); }
                if (val.empty()) {
                    // 值在下一个 token（含聚合串末尾，如 -bt）：批量短路径无条件接受
                    strPendingKey__.assign(1, key);
                    enPendingKind__    = PendingKind_E::ShortOpt;
                    bPendingInlineEq__ = false;
                    return;
                }
                __executeOptionImmediate(pOpt, val, false);
                return;                              // 聚合串中值选项即终结
            }
            return;
        }

        // ── 5. 普通 token → 方位参数 ──
        vecPos__.emplace_back(text);
    }

    /// @brief 处理关系语法：条件（-a?b:c）和依赖（-a->b）
    /// @note 流式模式：条件边即时判断并激活目标；依赖边按序执行
    void __handleRelation(ClassifiedExpr const &_Expr)
    {
        if (_Expr.kind == ExpressionKind_E::Conditional) {
            // 条件关系：检查条件选项状态
            bool condState = false;
            if (auto const *pOpt = pModel__->opts.mgrValues.find(_Expr.strCondition)) {
                condState = util::parse_bool(pOpt->strValue);
            }

            if (condState) {
                // 条件为真：激活 then 分支
                if (_Expr.strThenBranch != "_") {
                    if (auto *pOpt = pModel__->opts.mgrValues.mutFind(_Expr.strThenBranch)) {
                        __executeOptionImmediate(pOpt, "1", true);
                    }
                }
            }
            else {
                // 条件为假：激活 else 分支
                if (!_Expr.strElseBranch.empty() && _Expr.strElseBranch != "_") {
                    if (auto *pOpt = pModel__->opts.mgrValues.mutFind(_Expr.strElseBranch)) {
                        __executeOptionImmediate(pOpt, "1", true);
                    }
                }
            }
        }
        else if (_Expr.kind == ExpressionKind_E::Dependency) {
            // 依赖关系：先执行 source，再执行 target
            // 源选项已被激活（解析序保证），现在激活目标
            if (auto *pOpt = pModel__->opts.mgrValues.mutFind(_Expr.strDepTarget)) {
                __executeOptionImmediate(pOpt, "1", true);
            }
        }
    }

    /// @brief 判断文本是否为关系语法（-a?b:c / -a->b）
    bool __isRelationText(std::string const &_Text) const
    {
        if (_Text.size() < 4 || _Text[0] != '-') return false;
        for (size_t i = 1; i < _Text.size(); ++i) {
            if (_Text[i] == '?') return true;
            if (_Text[i] == '-' && i + 1 < _Text.size() && _Text[i + 1] == '>') return true;
        }
        return false;
    }

    /// @brief 从关系文本构造 ClassifiedExpr
    auto __classifyRelationText(std::string const &_Text) const -> ClassifiedExpr
    {
        ClassifiedExpr expr;
        expr.bExplicit = true;

        size_t qPos = std::string::npos;
        size_t dPos = std::string::npos;
        for (size_t i = 1; i < _Text.size(); ++i) {
            if (_Text[i] == '?' && qPos == std::string::npos) { qPos = i; break; }
            if (_Text[i] == '-' && i + 1 < _Text.size() && _Text[i + 1] == '>') {
                dPos = i;
                break;
            }
        }

        if (qPos != std::string::npos) {
            expr.kind         = ExpressionKind_E::Conditional;
            expr.strCondition = _Text.substr(1, qPos - 1);
            auto colonPos     = _Text.find(':', qPos);
            if (colonPos != std::string::npos) {
                expr.strThenBranch = _Text.substr(qPos + 1, colonPos - qPos - 1);
                expr.strElseBranch = _Text.substr(colonPos + 1);
            }
            else {
                expr.strThenBranch = _Text.substr(qPos + 1);
                expr.strElseBranch = "_";
            }
        }
        else if (dPos != std::string::npos) {
            expr.kind         = ExpressionKind_E::Dependency;
            expr.strDepSource = _Text.substr(1, dPos - 1);
            expr.strDepTarget = _Text.substr(dPos + 2);
        }

        return expr;
    }

    /// @brief 选项赋值 + 立即执行（流式专用）：值 → 标记 → fnOnSet__ → action 模板
    /// @note 流式路径不经 OptionParser::__assign，内置选项回调在此触发（等价其内联语义）
    void __executeOptionImmediate(OptionValue *_Opt, std::string const &_Value, bool _IsBool)
    {
        if (_IsBool) {
            _Opt->strValue = _Value.empty() ? "1" : (util::parse_bool(_Value) ? "1" : "0");
        }
        else {
            _Opt->strValue = _Value;
        }
        _Opt->bExplicit__ = true;

        // 自定义回调：内置（--env-set 等）与用户选项一律即时触发（解析即执行）
        if (_Opt->fnOnSet__) { _Opt->fnOnSet__(_Opt->strValue); }

        // action 模板 → 子进程命令；非 0 告警不中断
        if (!_Opt->strActionTemplate__.empty()) {
            auto cmd = util::replace(_Opt->strActionTemplate__, "{value}", _Opt->strValue);
            if (!cmd.empty()) {
                int ret = std::system(cmd.c_str());
                if (ret != 0) {
                    std::cerr << "(action) command exit " << ret << ": " << cmd << "\n";
                }
            }
        }
    }

    /// @brief 按声明顺序提交方位参数（数量校验与批量 ArgumentParser 一致）
    void __assignPositional(std::vector<std::string> const &_VecPos)
    {
        size_t total = pModel__->args.mgrPos.doSize();
        size_t need  = 0;
        pModel__->args.mgrPos.doConVisit([&](Argument const &a) {
            if (!a.isExternal()) ++need;
        });
        if (_VecPos.size() < need || _VecPos.size() > total) {
            std::ostringstream oss;
            oss << "give " << _VecPos.size() << ", but need " << need << ".." << total << '\n';
            oss << "uncaptured command line arguments:\n";
            for (auto const &tok : _VecPos) { oss << tok << '\n'; }
            throw ErrorParse(ParseError_E::UNKNWON_ARGS, oss.str());
        }

        bool   exact = (_VecPos.size() == need);
        size_t vi    = 0;
        for (size_t seq = 0; seq < total && vi < _VecPos.size(); ++seq) {
            auto *p = pModel__->args.mgrPos.mutGetBySeq(seq);
            if (!p) continue;
            if (exact && p->isExternal()) continue;
            p->set(_VecPos[vi++]);
        }
    }

    void __seedEnvDefaults()
    {
        pModel__->args.mgrName.doMutVisit([&](Argument &a) {
            if (a.getEnvName().empty() || a.hasValue() || a.isExternal()) return;
            if (this->EnvParser::has(a.getEnvName())) { a.set(this->EnvParser::get(a.getEnvName())); a.markEnvHit(); }
        });
        pModel__->args.mgrPos.doMutVisit([&](Argument &a) {
            if (a.getEnvName().empty() || a.hasValue() || a.isExternal()) return;
            if (this->EnvParser::has(a.getEnvName())) { a.set(this->EnvParser::get(a.getEnvName())); a.markEnvHit(); }
        });
        pModel__->opts.mgrValues.forEachValue([&](OptionValue &o) {
            if (o.strEnvName__.empty() || o.bExplicit__ || o.bExternal__) return;
            if (this->EnvParser::has(o.strEnvName__)) {
                o.strValue  = this->EnvParser::get(o.strEnvName__);
                o.bEnvHit__ = true;
            }
        });
    }

    void __expandEnvValues()
    {
        pModel__->args.mgrName.doMutVisit([&](Argument &a) {
            if (a.hasValue()) { a.set(this->EnvParser::expand(a.getValue())); }
        });
        pModel__->args.mgrPos.doMutVisit([&](Argument &a) {
            if (a.hasValue()) { a.set(this->EnvParser::expand(a.getValue())); }
        });
        pModel__->opts.mgrValues.forEachValue([&](OptionValue &o) {
            if (!o.strValue.empty()) { o.strValue = this->EnvParser::expand(o.strValue); }
        });
    }

    void __finalize()
    {
        __seedEnvDefaults();
        __expandEnvValues();

        auto vr = validate_detail::validate(*pModel__);
        if (!vr.ok()) {
            throw ErrorValidation(static_cast<ValidationError_E>(vr.ec.value()), std::move(vr.msg));
        }

        if (!pModel__->strSaveConfigPath.empty()) {
            auto text = config::to_yaml(*pModel__);
            if (!config::write_file(pModel__->strSaveConfigPath, text)) {
                std::cerr << "(save-config) cannot write file: " << pModel__->strSaveConfigPath << "\n";
            }
        }
    }

private:
    // ===== 引擎常驻状态 =====
    Model              *pModel__   = nullptr;
    ExpressionRecognizer rec__;   ///< 表达式识别器（类型化管道预留）
    bool                 bParsed__   = false;
    int                  nExitCode__ = 0;

    // ===== 流式解析状态（每次 interpret 经 __lineReset 复位） =====

    /// 挂起方类型：决定取值消费规则与 NEED_VALUE 文案（对齐批量语义）
    enum class PendingKind_E : char { None, LongOpt, ShortOpt, NamedArg };

    size_t                     idxCursor__        = 0;                    ///< 输入流游标：指向当前行下一待识别字符（入口为 0）
    std::vector<detail::Token> vecStack__;                                ///< token 栈：识别即压入，凑满一个 Expression 立即归约执行
    std::string                strPendingKey__    = {};                   ///< 等待取值的选项/命名参数名
    PendingKind_E              enPendingKind__    = PendingKind_E::None;  ///< 挂起取值方类型
    bool                       bPendingInlineEq__ = false;                ///< 挂起方是否由 '=' 内联分隔（仅 LongOpt 有意义）
    bool                       bOptEnd__          = false;                ///< '--' 之后（纯方位模式）
    std::vector<std::string>   vecPos__;                                  ///< 方位参数缓冲（流空后统一校验数量按序提交）
    std::vector<std::string>   vecDashLeft__;                             ///< 未识别 dash token 缓冲（流末统一报 UNKNOWN_OPTION）
};

CMD_ARGS_NAMESPACE_END
