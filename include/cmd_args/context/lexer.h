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
#include <list>
#include <map>
#include <vector>

#include "cmd_args/base/def.h"

#include "cmd_args/error/lexer_error.h"

#include "cmd_args/extern/magic_enum/magic_enum.hpp"

CMD_ARGS_NAMESPACE_BEGIN

/*
解析 argv ,envp中，不符合语法的部分，进行纠错

number: ([0-9])+
decimal:  (-?\d+)(\.\d+)?((e|E)(-?\d+))?
ident: ([a-zA-Z_])+[\w]

*/

namespace detail {

using Source_T = std::vector<std::string>;
using Pc_T     = Source_T::value_type::const_iterator;

class Token
{
public:
    using Name_T = std::string;
    enum class KeyWord_E : char
    {
        Env_,     // 读取环境变量
        File_,    // 读取文件
        Plugin_,  // 读取脚本
    };
    enum class Type_E : char
    {
        Unknown,        // 未知类型
        LongName,       // --[a-zA-z_-] 变量名
        ShortName,      // -[a-zA-z]
        ContinualName,  // -[a-zA-z]+
        Variable,       // [a-zA-z_][\w]* 名字，被作用为子模块，或者参数
        Integer,        // [0-9]+ 整数，赋值
        Real,           // (+|-)?[0-9]+(.[0-9])?(.[0-9](e|E)(+|-)[0-9]+) 实数
        String,         // "[a-zA-z_-]"
        EnvName,
        Delimiter,  // 分隔符， [环境变量名] 加载Env (文件路径) 加载file  "字符串"
                    // 加载字符，无视关键字
        Keyword
    };
    Token() = default;
    Token(Type_E _T, Name_T _Content, size_t _Line)
        : m_enAttr__(_T)
        , m_strName__(_Content)
        , m_locLine__(_Line)
    {}
    operator bool() const noexcept { return m_enAttr__ != Type_E::Unknown; }
    Type_E        getAttr() const noexcept { return m_enAttr__; }
    Name_T const &getName() const noexcept { return m_strName__; }
    char          back() const noexcept { return m_strName__.empty() ? '\0' : m_strName__.back(); }
    void          setLoc(size_t _Line) { m_locLine__ = _Line; }
    void          setAttr(Type_E _A) { m_enAttr__ = _A; }
    void          pick(char _C) { m_strName__ += _C; }
    void          resetName() { m_strName__.clear(); }
    void          clear()
    {
        m_enAttr__ = Type_E::Unknown;
        m_strName__.clear();
        m_locLine__ = 0;
    }

    /*
        归一化成命令行文本：
        LongName      -> "--name"
        ShortName     -> "-x"
        ContinualName -> "-xyz"
        其余保持原样
    */
    std::string toText() const
    {
        switch (m_enAttr__) {
        case Type_E::LongName: return "--" + m_strName__;
        case Type_E::ShortName:
        case Type_E::ContinualName: return "-" + m_strName__;
        default: return m_strName__;
        }
    }

private:
    Type_E m_enAttr__ = Type_E::Unknown;
    Name_T m_strName__;
    size_t m_locLine__ = 0;
};

class Scanner
{
public:
    using Tokens_T   = std::vector<Token>;
    using ScanProc_T = std::function<void(char const _Cur, Token &_T)>;

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
        END                   // 解析结束
    };
    using ScanTbl_T = std::map<Phrase_E, ScanProc_T>;
    Scanner(Source_T const &_Source)
        : m_refSource__(_Source)
    {
        if (!_Source.empty()) { m_itPc__ = _Source.front().cbegin(); }
        __initScanTable();
    }

    Tokens_T scan()
    {
        Token tDefault;
        while (!__isSourceEnd()) {
            if (__isLineEnd()) {
                // 行尾：强行终结当前 token（字符串除外，直接报错）
                __flush(tDefault);
                __nextLine();
                continue;
            }
            m_tblScanInput__[m_enState__](*m_itPc__, tDefault);
        }
        __flush(tDefault);

        return std::move(m_tResult__);
    }

private:
    /*
        状态机：按当前状态分派字符处理函数。
        分派表必须是成员（绑定各自的 this）——函数内 static 会绑到
        第一个 Scanner 实例上，后续实例全部成了悬垂调用。
    */
    void __initScanTable()
    {
        namespace sp = std::placeholders;
        m_tblScanInput__ = {
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
            { Phrase_E::END, std::bind(&Scanner::__processEnd, this, sp::_1, sp::_2) }
        };
    }
    /*
        按状态决定未终结 token 的类型：
        字符串无法在行内终结 -> 抛出词法错误
    */
    void __flush(Token &_T)
    {
        if (m_enState__ == Phrase_E::BEGIN || m_enState__ == Phrase_E::END) return;
        if (m_enState__ == Phrase_E::IN_STRING) {
            throw ErrorLexer(LexError_E::UNTERMINATED_STRING);
        }

        switch (m_enState__) {
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
        default: _T.setAttr(Token::Type_E::Unknown); break;
        }

        __processEnd('\0', _T);
    }

    /*
        token的起始状态
        -[0-9] 表示可能是数字
        [a-z]+ 表示可能是变量
        " ' 表示是字符串
        其他的表示单分隔符
        空白在 BEGIN 态直接跳过（不产生 token）
    */
    void __processBegin(char const _Cur, Token &_T)
    {
        if (std::isspace(static_cast<unsigned char>(_Cur))) {
            __nextChar();
            return;
        }
        if (std::isdigit(static_cast<unsigned char>(_Cur)) || _Cur == '-' || _Cur == '+') {
            // (+|-)[0-9]+ 开始解析数字
            __setState(Phrase_E::IN_INTEGER);
        }
        else if (std::isalpha(static_cast<unsigned char>(_Cur))) {
            // [a-zA-z] 专有名词
            __setState(Phrase_E::IN_VARIABLE);
        }
        else if (_Cur == '\"' || _Cur == '\'') {
            // "[\s\W]"
            __setState(Phrase_E::IN_STRING);
        }
        else {
            __setState(Phrase_E::IN_SINGLE_DELIMITER);
        }

        _T.pick(_Cur);
        __nextChar();
    }

    /*
        // (+|-)[0-9]+ 开始解析数字
    */
    void __processInteger(char const _Cur, Token &_T)
    {
        if (std::isdigit(static_cast<unsigned char>(_Cur))) {
            // 如果是数字，解析下一个
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_INTEGER);
        }
        else if (_Cur == '.') {
            // 发现小数点，转入解析实数
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_REAL);
        }
        else if (std::isalpha(static_cast<unsigned char>(_Cur))) {
            // -a
            _T.resetName();
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_SHORT_NAME);
        }
        else if (_Cur == '-') {
            // --a：清掉开头已 pick 的单个 '-'，长名词只保留名字部分
            _T.resetName();
            __nextChar();
            __setState(Phrase_E::IN_LONG_NAME);
        }
        else if (std::isspace(static_cast<unsigned char>(_Cur))) {
            // 空白结束数字：交给 BEGIN 态跳过
            _T.setAttr(Token::Type_E::Integer);
            __setState(Phrase_E::END);
        }
        else {
            // -? / -! 等短选项的非字母字符：按短名词处理（名字即该字符）
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
            m_enState__ = Phrase_E::END;
        }
    }
    void __processShortName(char const _Cur, Token &_T)
    {
        if (std::isalpha(static_cast<unsigned char>(_Cur))) {
            // -aaaa
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
            // -aaaa
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
            // --aaa= 或 --aaa 后跟空白，结束长名词
            _T.setAttr(Token::Type_E::LongName);
            __setState(Phrase_E::END);
            return;
        }
        // --aaa--aa=
        _T.pick(_Cur);
        __nextChar();
        __setState(Phrase_E::IN_LONG_NAME);
    }
    void __processString(char const _Cur, Token &_T)
    {
        if (_Cur == '\"' || _Cur == '\'') {
            // 字符结束
            _T.pick(_Cur);
            _T.setAttr(Token::Type_E::String);
            __setState(Phrase_E::END);
        }
        else {
            // 继续解析字符
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_STRING);
        }
    }
    void __processVariable(char const _Cur, Token &_T)
    {
        // 值 token：吸收到空白为止。argv 的每个元素本就按空白切分好了，
        // 值里的 . - = \ / : @ 等一律并入，避免 out.txt、C:\path、a=b 被拆碎
        if (!std::isspace(static_cast<unsigned char>(_Cur))) {
            _T.pick(_Cur);
            __nextChar();
            __setState(Phrase_E::IN_VARIABLE);
            return;
        }

        auto k = magic_enum::enum_cast<Token::KeyWord_E>(_T.getName());
        if (k.has_value()) {
            m_enState__ = Phrase_E::IN_KEYWORD;
        }
        else {
            _T.setAttr(Token::Type_E::Variable);
            __setState(Phrase_E::END);
        }
    }
    void __processKeyWord(char const _Cur, Token &_T)
    {
        _T.setAttr(Token::Type_E::Keyword);
        __setState(Phrase_E::END);
    }
    void __processSingleDelimiter(char const _Cur, Token &_T)
    {
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

        _T.setAttr(Token::Type_E::Delimiter);
        __setState(Phrase_E::END);
    }
    void __processDoubleDelimiter(char const _Cur, Token &_T)
    {
        // && || >> << 合并成双字符分隔符
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
        _T.setLoc(m_cntLoc__);
        m_tResult__.emplace_back(_T);

        _T.clear();
        __setState(Phrase_E::BEGIN);
    }

    inline void __setState(Phrase_E _State) noexcept { m_enState__ = _State; }
    inline void __nextChar() noexcept
    {
        if (m_cntLoc__ >= m_refSource__.size()) return;
        if (m_itPc__ == m_refSource__[m_cntLoc__].cend()) return;
        ++m_itPc__;
    }
    inline void __nextLine() noexcept
    {
        ++m_cntLoc__;
        if (m_cntLoc__ < m_refSource__.size()) { m_itPc__ = m_refSource__[m_cntLoc__].cbegin(); }
    }

    bool __isSourceEnd() const noexcept { return m_cntLoc__ >= m_refSource__.size(); }
    bool __isLineEnd() const noexcept
    {
        if (m_cntLoc__ >= m_refSource__.size()) return true;
        return m_itPc__ == m_refSource__[m_cntLoc__].cend();
    }

    Source_T const &m_refSource__;
    Phrase_E        m_enState__ = Phrase_E::BEGIN;  // 解析的状态
    Pc_T            m_itPc__;
    size_t          m_cntLoc__ = 0;
    Tokens_T        m_tResult__;
    ScanTbl_T       m_tblScanInput__;  // 状态分派表（成员，各自绑定本实例 this）
};

class Lexer
{
public:
    Lexer(int _Argc, char const *_Argv[], char *_Envp[] = nullptr)
    {
        __loadInput(_Argc, _Argv);
        __loadEnv(_Envp);
    }

    Scanner::Tokens_T run()
    {
        Scanner::Tokens_T res;
        if (m_srcEnvs__.size()) {
            res = Scanner(m_srcEnvs__).scan();
            for (auto &r : res) {
                if (r.getAttr() == Token::Type_E::Variable) { r.setAttr(Token::Type_E::EnvName); }
            }
        }

        auto resInput = Scanner(m_srcInput__).scan();
        std::move(resInput.begin(), resInput.end(), std::back_inserter(res));

        return res;
    }

    /*
        归一化的命令行 token 列表（供 Parser 消费）：
        - 只有 argv 输入，不包含环境变量
        - `--name=value` 合并为单个 token
        - 跳过空/纯空白 token
    */
    Tokens_T tokens()
    {
        if (m_srcInput__.empty()) return {};
        return __normalize(Scanner(m_srcInput__).scan());
    }

    /*
        REPL/loop 场景：对一整行输入分词（空白分隔，引号感知）
    */
    static Tokens_T tokensFromLine(std::string const &_Line)
    {
        if (_Line.empty()) return {};
        Source_T src{ _Line };
        return __normalize(Scanner(src).scan());
    }

private:
    static Tokens_T __normalize(Scanner::Tokens_T const &_Raw)
    {
        Tokens_T res;
        for (size_t i = 0; i < _Raw.size(); ++i) {
            auto const &t   = _Raw[i];
            auto        attr = t.getAttr();

            // --name=value / -x=value / KEY=VALUE 合并为单个 token
            if (i + 2 < _Raw.size()
                && _Raw[i + 1].getAttr() == Token::Type_E::Delimiter
                && _Raw[i + 1].getName() == "="
                && (attr == Token::Type_E::LongName || attr == Token::Type_E::ShortName
                    || attr == Token::Type_E::ContinualName
                    || attr == Token::Type_E::Variable)) {
                res.emplace_back(t.toText() + "=" + _Raw[i + 2].toText());
                i += 2;
                continue;
            }

            auto text = t.toText();
            if (text.empty()) continue;
            res.emplace_back(std::move(text));
        }

        return res;
    }

    // 读取环境变量
    void __loadEnv(char *_Envp[])
    {
        if (!_Envp) return;
        for (int i = 0; _Envp[i]; ++i) { m_srcEnvs__.emplace_back(_Envp[i]); }
    }
    void __loadInput(int _Argc, char const *_Argv[])
    {
        for (int i = 0; i < _Argc; ++i) { m_srcInput__.emplace_back(_Argv[i]); }
    }
    /* data */
    Source_T m_srcInput__;  // 输入的源码
    Source_T m_srcEnvs__;   // 环境变量里的源码
};

}  // namespace detail

using detail::Lexer;

CMD_ARGS_NAMESPACE_END