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
#include <optional>
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

Scanner 支持两种分词模式：scan() 批量分词（局部 token 变量），
step() 增量逐字符喂入（复用同一状态分派表，供流式 Interpreter 使用）。

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
        : enAttr__(_T)
        , strName__(_Content)
        , locLine__(_Line)
    {}
    operator bool() const noexcept { return enAttr__ != Type_E::Unknown; }
    Type_E        getAttr() const noexcept { return enAttr__; }
    Name_T const &getName() const noexcept { return strName__; }
    char          back() const noexcept { return strName__.empty() ? '\0' : strName__.back(); }
    void          setLoc(size_t _Line) { locLine__ = _Line; }
    void          setAttr(Type_E _A) { enAttr__ = _A; }
    void          pick(char _C) { strName__ += _C; }
    void          resetName() { strName__.clear(); }
    void          setInlineEq(bool _V) { bInlineEq__ = _V; }
    /// @brief 是否由 '=' 内联分隔（--name=value 中 '=' 直接结束长名词）
    bool hasInlineEq() const noexcept { return bInlineEq__; }
    void          clear()
    {
        enAttr__    = Type_E::Unknown;
        strName__.clear();
        locLine__   = 0;
        bInlineEq__ = false;  // ← 新增复位
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
        switch (enAttr__) {
        case Type_E::LongName: return "--" + strName__;
        case Type_E::ShortName:
        case Type_E::ContinualName: return "-" + strName__;
        default: return strName__;
        }
    }

private:
    Type_E enAttr__ = Type_E::Unknown;
    Name_T strName__;
    size_t locLine__ = 0;
    bool   bInlineEq__ = false;
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
    // 源按值持有：Scanner({}) 等临时源构造不再悬垂（成员引用绑定临时对象是 UB，
    // 流式 step() 逐字符触碰 source__，必须保证其生命周期覆盖整个 Scanner）
    Scanner(Source_T _Source)
        : source__(std::move(_Source))
    {
        if (!source__.empty()) { itPc__ = source__.front().cbegin(); }
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
            tblScanInput__[enState__](*itPc__, tDefault);
        }
        __flush(tDefault);

        return std::move(tResult__);
    }

    /// @brief 增量模式：逐字符喂入，token 完成时返回之（Interpreter 流式模式）
    /// @param _Cur 字符；'\0' 表示流结束（强制终结当前 token）
    /// @return 有 token 完成则返回之，否则 nullopt
    std::optional<Token> step(char _Cur)
    {
        if (_Cur == '\0') {
            // __flush 内 __processEnd 已把状态复位 BEGIN，无需再手动复位
            __flush(tCur__);   // IN_STRING 态抛 UNTERMINATED_STRING；BEGIN 态早退
        }
        else {
            tblScanInput__[enState__](_Cur, tCur__);
            if (enState__ == Phrase_E::END) {
                __processEnd('\0', tCur__);  // 排水当前 token，状态复位 BEGIN
                if (_Cur != '\0' && _Cur != '=') {
                    // 与批量 scan() 一致：终结字符在 BEGIN 态重派，避免丢字符
                    // （空白是无害 no-op；'=' 已由 hasInlineEq 表达，重派会产生
                    //   Task 10 误读的赘余 Delimiter "=" token）
                    tblScanInput__[Phrase_E::BEGIN](_Cur, tCur__);
                    // BEGIN 态不会立即完成，此守卫仅作保险
                    if (enState__ == Phrase_E::END) { __processEnd('\0', tCur__); }
                }
            }
        }
        if (tResult__.empty()) return std::nullopt;
        auto tok = std::move(tResult__.front());
        tResult__.erase(tResult__.begin());  // 每次 step 至多产出 1 个 token
        return tok;
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
            { Phrase_E::END, std::bind(&Scanner::__processEnd, this, sp::_1, sp::_2) }
        };
    }
    /*
        按状态决定未终结 token 的类型：
        字符串无法在行内终结 -> 抛出词法错误
    */
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
            enState__ = Phrase_E::END;
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
            if (_Cur == '=') { _T.setInlineEq(true); }  // ← 新增
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
            enState__ = Phrase_E::IN_KEYWORD;
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
        _T.setLoc(cntLoc__);
        tResult__.emplace_back(_T);

        _T.clear();
        __setState(Phrase_E::BEGIN);
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

    Source_T        source__;  // 输入源（值持有，见构造函数注释）
    Phrase_E        enState__ = Phrase_E::BEGIN;  // 解析的状态
    Pc_T            itPc__;
    size_t          cntLoc__ = 0;
    Tokens_T        tResult__;
    Token           tCur__;  ///< 流式模式当前处理中的 token（scan() 使用局部变量，不经此成员）
    ScanTbl_T       tblScanInput__;  // 状态分派表（成员，各自绑定本实例 this）
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
        if (srcEnvs__.size()) {
            res = Scanner(srcEnvs__).scan();
            for (auto &r : res) {
                if (r.getAttr() == Token::Type_E::Variable) { r.setAttr(Token::Type_E::EnvName); }
            }
        }

        auto resInput = Scanner(srcInput__).scan();
        std::move(resInput.begin(), resInput.end(), std::back_inserter(res));

        return res;
    }

    /*
        归一化的命令行 token 列表（供 Compiler 消费）：
        - 只有 argv 输入，不包含环境变量
        - `--name=value` 合并为单个 token
        - 跳过空/纯空白 token
    */
    Tokens_T tokens()
    {
        if (srcInput__.empty()) return {};
        return __normalize(Scanner(srcInput__).scan());
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

            // ── 关系语法合并 ──
            // Scanner 对 -a?b:c 产生: ContinualName("a") + Delimiter("?") + Var("b") + Delimiter(":") + Var("c")
            // Scanner 对 -a->b  产生: ContinualName("a") + ContinualName(">b")（`>` 被吸进下一个 token）
            if (attr == Token::Type_E::ContinualName && i + 1 < _Raw.size()) {
                auto const &next = _Raw[i + 1];

                // -a?b:c / -a?b:_ / -a?b（条件关系）：下一 token 是 "?" Delimiter
                if (next.getAttr() == Token::Type_E::Delimiter && next.getName() == "?") {
                    std::string merged = t.toText();  // "-a"
                    merged += "?";                      // "-a?"
                    size_t j = i + 2;
                    // 收集 ? 后面的 token 直到遇到非 Variable/Delimiter(":") token
                    while (j < _Raw.size()) {
                        auto const &cur = _Raw[j];
                        if (cur.getAttr() == Token::Type_E::Variable
                            || (cur.getAttr() == Token::Type_E::Delimiter
                                && cur.getName() == ":")) {
                            merged += cur.toText();
                            ++j;
                        }
                        else {
                            break;
                        }
                    }
                    i = j - 1;
                    res.emplace_back(std::move(merged));
                    continue;
                }

                // -a->b（依赖关系）：下一 token 是 ContinualName 且以 '>' 开头
                // Scanner 把 `>` 吸入下一个 ContinualName（name = ">link"，toText = "->link"）
                if (next.getAttr() == Token::Type_E::ContinualName
                    && !next.getName().empty() && next.getName()[0] == '>') {
                    std::string merged = t.toText();  // "-a"
                    merged += next.toText();            // "-a" + "->link" = "-a->link"
                    ++i;
                    res.emplace_back(std::move(merged));
                    continue;
                }
            }

            // ── = 合并 ──
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
        for (int i = 0; _Envp[i]; ++i) { srcEnvs__.emplace_back(_Envp[i]); }
    }
    void __loadInput(int _Argc, char const *_Argv[])
    {
        for (int i = 0; i < _Argc; ++i) { srcInput__.emplace_back(_Argv[i]); }
    }
    /* data */
    Source_T srcInput__;  // 输入的源码
    Source_T srcEnvs__;   // 环境变量里的源码
};

}  // namespace detail

using detail::Lexer;

CMD_ARGS_NAMESPACE_END