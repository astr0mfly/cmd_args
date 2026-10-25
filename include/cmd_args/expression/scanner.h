/*
 *Copyright [2023] [ValenciaFly]
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
#include <functional>
#include <map>
#include <optional>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/error/lexer_error.h"
#include "cmd_args/expression/token.h"
#include "cmd_args/extern/magic_enum/magic_enum.hpp"

CMD_ARGS_NAMESPACE_BEGIN

namespace expression_detail {

using Token = ::cmd_args::Token;

using Source_T = std::vector<std::string>;
using Pc_T     = Source_T::value_type::const_iterator;

class Scanner
{
public:
    using Tokens_T   = std::vector<Token>;
    using ScanProc_T = std::function<void(char const _Cur, Token &_T)>;

    /// @brief 词法模式
    enum class LexMode_E : char
    {
        CommandLine,  ///< 命令行模式（argv 输入）
        EnvLine       ///< 环境行模式（envp 输入：支持空间级联分隔符与 = 断句）
    };

    enum class Phrase_E
    {
        BEGIN,
        IN_KEYWORD,           // 加载关键字
        IN_STRING,            // 加载字符串
        IN_INTEGER,           // 加载整数
        IN_REAL,              // 加载实数
        IN_SIENCE,            // 科学计数法
        IN_LONG_NAME,         // 解析长名词
        IN_SHORT_NAME,        // 解析缩写
        IN_CONTINUAL_NAME,    // 解析连续缩写
        IN_VARIABLE,          // 专有名词，用作子模块或者其他参数
        IN_SINGLE_DELIMITER,  // 其他分隔符
        IN_DOUBLE_DELIMITER,  // 其他分隔符
        IN_SCOPE_SEP,         // EnvLine：空间分隔符匹配中（已吞入前缀，等待后续字符）
        IN_ENV_VALUE,         // EnvLine：'=' 之后的值吸收态
        END                   // 解析结束
    };
    using ScanTbl_T = std::map<Phrase_E, ScanProc_T>;
    // 源按值持有：Scanner({}) 等临时源构造不再悬垂（成员引用绑定临时对象是 UB，
    // 流式 step() 逐字符触碰 source__，必须保证其生命周期覆盖整个 Scanner）
    explicit Scanner(Source_T _Source, LexMode_E _Mode = LexMode_E::CommandLine)
        : source__(std::move(_Source))
        , enMode__(_Mode)
    {
        if (!source__.empty()) { itPc__ = source__.front().cbegin(); }
        __initScanTable();
    }

    Tokens_T scan()
    {
        Token tDefault;
        Tokens_T res;
        while (!__isSourceEnd()) {
            if (__isLineEnd()) {
                __flush(tDefault);
                __nextLine();
                continue;
            }
            tblScanInput__[enState__](*itPc__, tDefault);
            if (tResultSingle__) {
                res.push_back(std::move(*tResultSingle__));
                tResultSingle__.reset();
            }
        }
        __flush(tDefault);
        if (tResultSingle__) {
            res.push_back(std::move(*tResultSingle__));
            tResultSingle__.reset();
        }
        return res;
    }

    /// @brief 增量模式：逐字符喂入，token 完成时返回之（Interpreter 流式模式）
    /// @param _Cur 字符；'\0' 表示流结束（强制终结当前 token）
    /// @return 有 token 完成则返回之，否则 nullopt
    std::optional<Token> step(char _Cur)
    {
        if (_Cur == '\0') {
            __flush(tCur__);
        }
        else {
            tblScanInput__[enState__](_Cur, tCur__);
            if (enState__ == Phrase_E::END) {
                __processEnd('\0', tCur__);
                if (_Cur != '\0' && !(enMode__ == LexMode_E::CommandLine && _Cur == '=')) {
                    tblScanInput__[Phrase_E::BEGIN](_Cur, tCur__);
                    if (enState__ == Phrase_E::END) { __processEnd('\0', tCur__); }
                }
            }
        }
        if (tResultSingle__) {
            auto tok = std::move(*tResultSingle__);
            tResultSingle__.reset();
            return tok;
        }
        return std::nullopt;
    }

private:
    void __initScanTable()
    {
        namespace sp = std::placeholders;
        tblScanInput__ = {
            { Phrase_E::BEGIN, std::bind(&Scanner::__processBegin, this, sp::_1, sp::_2) },
            { Phrase_E::IN_KEYWORD, std::bind(&Scanner::__processKeyWord, this, sp::_1, sp::_2) },
            { Phrase_E::IN_STRING, std::bind(&Scanner::__processString, this, sp::_1, sp::_2) },
            { Phrase_E::IN_INTEGER, std::bind(&Scanner::__processInteger, this, sp::_1, sp::_2) },
            { Phrase_E::IN_REAL, std::bind(&Scanner::__processReal, this, sp::_1, sp::_2) },
            { Phrase_E::IN_SIENCE, std::bind(&Scanner::__processSience, this, sp::_1, sp::_2) },
            { Phrase_E::IN_LONG_NAME,
              std::bind(&Scanner::__processLongName, this, sp::_1, sp::_2) },
            { Phrase_E::IN_SHORT_NAME,
              std::bind(&Scanner::__processShortName, this, sp::_1, sp::_2) },
            { Phrase_E::IN_CONTINUAL_NAME,
              std::bind(&Scanner::__processContinualName, this, sp::_1, sp::_2) },
            { Phrase_E::IN_VARIABLE, std::bind(&Scanner::__processVariable, this, sp::_1, sp::_2) },
            { Phrase_E::IN_SINGLE_DELIMITER,
              std::bind(&Scanner::__processSingleDelimiter, this, sp::_1, sp::_2) },
            { Phrase_E::IN_DOUBLE_DELIMITER,
              std::bind(&Scanner::__processDoubleDelimiter, this, sp::_1, sp::_2) },
            { Phrase_E::IN_SCOPE_SEP,
              std::bind(&Scanner::__processScopeSep, this, sp::_1, sp::_2) },
            { Phrase_E::IN_ENV_VALUE,
              std::bind(&Scanner::__processEnvValue, this, sp::_1, sp::_2) },
            { Phrase_E::END, std::bind(&Scanner::__processEnd, this, sp::_1, sp::_2) }
        };
    }

    void __flush(Token &_T)
    {
        if (enState__ == Phrase_E::BEGIN || enState__ == Phrase_E::END) return;
        if (enState__ == Phrase_E::IN_STRING) {
            throw ErrorLexer(LexError_E::UNTERMINATED_STRING);
        }

        switch (enState__) {
        case Phrase_E::IN_KEYWORD: _T.setAttr(Token::Type_E::Keyword); break;
        case Phrase_E::IN_SINGLE_DELIMITER:
        case Phrase_E::IN_DOUBLE_DELIMITER: _T.setAttr(Token::Type_E::Delimiter); break;
        case Phrase_E::IN_INTEGER: _T.setAttr(Token::Type_E::Integer); break;
        case Phrase_E::IN_REAL:
        case Phrase_E::IN_SIENCE: _T.setAttr(Token::Type_E::Real); break;
        case Phrase_E::IN_SHORT_NAME: _T.setAttr(Token::Type_E::ShortName); break;
        case Phrase_E::IN_CONTINUAL_NAME: _T.setAttr(Token::Type_E::ContinualName); break;
        case Phrase_E::IN_LONG_NAME: _T.setAttr(Token::Type_E::LongName); break;
        case Phrase_E::IN_VARIABLE: _T.setAttr(Token::Type_E::Variable); break;
        case Phrase_E::IN_SCOPE_SEP:
            _T.setAttr(nScopeMatched__ >= ENV_SCOPE_SEP_LEN ? Token::Type_E::ScopeSep
                                                            : Token::Type_E::Delimiter);
            break;
        case Phrase_E::IN_ENV_VALUE:   _T.setAttr(Token::Type_E::String);   break;
        default: _T.setAttr(Token::Type_E::Unknown); break;
        }

        __processEnd('\0', _T);
    }

    void __processBegin(char const _Cur, Token &_T)
    {
        if (std::isspace(static_cast<unsigned char>(_Cur))) {
            __nextChar();
            return;
        }
        if (enMode__ == LexMode_E::EnvLine && bEnvValuePos__) {
            __setState(Phrase_E::IN_ENV_VALUE);
            _T.pick(_Cur);
            __nextChar();
            return;
        }
        if (enMode__ == LexMode_E::EnvLine && _Cur == ENV_SCOPE_SEP[0]) {
            _T.pick(_Cur);
            nScopeMatched__ = 1;
            __nextChar();
            if (ENV_SCOPE_SEP_LEN <= 1) {
                _T.setAttr(Token::Type_E::ScopeSep);
                __setState(Phrase_E::END);
                return;
            }
            __setState(Phrase_E::IN_SCOPE_SEP);
            return;
        }
        if (std::isdigit(static_cast<unsigned char>(_Cur)) || _Cur == '-' || _Cur == '+') {
            __setState(Phrase_E::IN_INTEGER);
        }
        else if (std::isalpha(static_cast<unsigned char>(_Cur))) {
            __setState(Phrase_E::IN_VARIABLE);
        }
        else if (_Cur == '\"' || _Cur == '\'') {
            __setState(Phrase_E::IN_STRING);
        }
        else {
            __setState(Phrase_E::IN_SINGLE_DELIMITER);
        }

        _T.pick(_Cur);
        __nextChar();
    }

    void __processInteger(char const _Cur, Token &_T)
    {
        if (std::isdigit(static_cast<unsigned char>(_Cur))) {
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_INTEGER);
        }
        else if (_Cur == '.') {
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_REAL);
        }
        else if (std::isalpha(static_cast<unsigned char>(_Cur))) {
            _T.resetName();
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_SHORT_NAME);
        }
        else if (_Cur == '-') {
            _T.resetName();
            __nextChar();
            __setState(Phrase_E::IN_LONG_NAME);
        }
        else if (std::isspace(static_cast<unsigned char>(_Cur))) {
            _T.setAttr(Token::Type_E::Integer);
            __setState(Phrase_E::END);
        }
        else {
            _T.resetName();
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_SHORT_NAME);
        }
    }

    void __processReal(char const _Cur, Token &_T)
    {
        if (std::isdigit(static_cast<unsigned char>(_Cur))) {
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_REAL);
        }
        else if (_Cur == 'e' || _Cur == 'E') {
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_SIENCE);
        }
        else {
            _T.setAttr(Token::Type_E::Real);
            __setState(Phrase_E::END);
        }
    }

    void __processSience(char const _Cur, Token &_T)
    {
        if ((_T.back() == 'e' || _T.back() == 'E') && _Cur == '-') {
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_SIENCE);
            return;
        }

        if (std::isdigit(static_cast<unsigned char>(_Cur))) {
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_SIENCE);
        }
        else {
            _T.setAttr(Token::Type_E::Real);
            enState__ = Phrase_E::END;
        }
    }

    void __processShortName(char const _Cur, Token &_T)
    {
        if (std::isalpha(static_cast<unsigned char>(_Cur))) {
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_CONTINUAL_NAME);
        }
        else if (_Cur == '-') {
            _T.resetName();
            __nextChar();
            __setState(Phrase_E::IN_LONG_NAME);
        }
        else {
            _T.setAttr(Token::Type_E::ShortName);
            __setState(Phrase_E::END);
        }
    }

    void __processContinualName(char const _Cur, Token &_T)
    {
        if (std::isalpha(static_cast<unsigned char>(_Cur))) {
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_CONTINUAL_NAME);
        }
        else {
            _T.setAttr(Token::Type_E::ContinualName);
            __setState(Phrase_E::END);
        }
    }

    void __processLongName(char const _Cur, Token &_T)
    {
        if (_Cur == '=' || std::isspace(static_cast<unsigned char>(_Cur))) {
            _T.setAttr(Token::Type_E::LongName);
            if (_Cur == '=') { _T.setInlineEq(true); }
            __setState(Phrase_E::END);
            return;
        }
        _T.pick(_Cur);
        __nextChar();
        __setState(Phrase_E::IN_LONG_NAME);
    }

    void __processString(char const _Cur, Token &_T)
    {
        if (_Cur == '\"' || _Cur == '\'') {
            _T.pick(_Cur);
            _T.setAttr(Token::Type_E::String);
            __setState(Phrase_E::END);
        }
        else {
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_STRING);
        }
    }

    void __processVariable(char const _Cur, Token &_T)
    {
        if (enMode__ == LexMode_E::EnvLine
            && (_Cur == ':' || _Cur == '=' || _Cur == ENV_SCOPE_SEP[0])) {
            _T.setAttr(Token::Type_E::Variable);
            __setState(Phrase_E::END);
            return;
        }

        if (!std::isspace(static_cast<unsigned char>(_Cur))) {
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_VARIABLE);
            return;
        }

        if (enMode__ != LexMode_E::EnvLine) {
            auto k = magic_enum::enum_cast<Token::KeyWord_E>(_T.getName());
            if (k.has_value()) {
                enState__ = Phrase_E::IN_KEYWORD;
                return;
            }
        }
        _T.setAttr(Token::Type_E::Variable);
        __setState(Phrase_E::END);
    }

    void __processKeyWord(char const _Cur, Token &_T)
    {
        _T.setAttr(Token::Type_E::Keyword);
        __setState(Phrase_E::END);
    }

    void __processSingleDelimiter(char const _Cur, Token &_T)
    {
        if (enMode__ == LexMode_E::EnvLine && _Cur == ENV_SCOPE_SEP[0]) {
            _T.pick(_Cur);
            nScopeMatched__ = 1;
            __nextChar();
            if (ENV_SCOPE_SEP_LEN <= 1) {
                _T.setAttr(Token::Type_E::ScopeSep);
                __setState(Phrase_E::END);
                return;
            }
            __setState(Phrase_E::IN_SCOPE_SEP);
            return;
        }

        switch (_Cur) {
        case '>':
        case '<':
        case '&':
        case '|':
        {
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_DOUBLE_DELIMITER);
            return;
        }
        default: break;
        }

        if (enMode__ == LexMode_E::EnvLine && _Cur == '=') {
            bEnvValuePos__ = true;
        }

        _T.setAttr(Token::Type_E::Delimiter);
        __setState(Phrase_E::END);
    }

    void __processDoubleDelimiter(char const _Cur, Token &_T)
    {
        if (_Cur == _T.back()) {
            _T.pick(_Cur);
            __nextChar();
        }
        _T.setAttr(Token::Type_E::Delimiter);
        __setState(Phrase_E::END);
    }

    void __processEnd(char const _Cur, Token &_T)
    {
        (void)_Cur;
        _T.setLoc(cntLoc__);
        tResultSingle__ = _T;
        _T.clear();
        __setState(Phrase_E::BEGIN);
    }

    void __processScopeSep(char const _Cur, Token &_T)
    {
        if (nScopeMatched__ < ENV_SCOPE_SEP_LEN && _Cur == ENV_SCOPE_SEP[nScopeMatched__]) {
            _T.pick(_Cur);
            ++nScopeMatched__;
            __nextChar();
            if (nScopeMatched__ >= ENV_SCOPE_SEP_LEN) {
                _T.setAttr(Token::Type_E::ScopeSep);
                __setState(Phrase_E::END);
            }
            return;
        }
        // 未匹配：已吞入的前缀退化为普通分隔符，当前字符留给 BEGIN 重新处理
        _T.setAttr(Token::Type_E::Delimiter);
        __setState(Phrase_E::END);
    }

    void __processEnvValue(char const _Cur, Token &_T)
    {
        if (std::isspace(static_cast<unsigned char>(_Cur))) {
            _T.setAttr(Token::Type_E::String);
            __setState(Phrase_E::END);
            return;
        }
        _T.pick(_Cur);
        __nextChar();
    }

    inline void __setState(Phrase_E _State) noexcept { enState__ = _State; }
    inline void __nextChar() noexcept
    {
        if (cntLoc__ >= source__.size()) return;
        if (itPc__ == source__[cntLoc__].cend()) return;
        ++itPc__;
    }
    inline void __nextLine() noexcept
    {
        ++cntLoc__;
        if (cntLoc__ < source__.size()) { itPc__ = source__[cntLoc__].cbegin(); }
    }

    bool __isSourceEnd() const noexcept { return cntLoc__ >= source__.size(); }
    bool __isLineEnd() const noexcept
    {
        if (cntLoc__ >= source__.size()) return true;
        return itPc__ == source__[cntLoc__].cend();
    }

    Source_T              source__;
    Phrase_E              enState__ = Phrase_E::BEGIN;
    Pc_T                  itPc__;
    size_t                cntLoc__ = 0;
    std::optional<Token>  tResultSingle__;  ///< 单 token 缓冲（替代旧 tResult__ vector）
    Token                 tCur__;
    ScanTbl_T             tblScanInput__;
    LexMode_E             enMode__       = LexMode_E::CommandLine;
    bool                  bEnvValuePos__ = false;
    std::size_t           nScopeMatched__ = 0;  ///< EnvLine：空间分隔符已匹配字符数
};

}  // namespace expression_detail

CMD_ARGS_NAMESPACE_END
