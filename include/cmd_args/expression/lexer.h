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

#include <optional>
#include <string>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/expression/token.h"
#include "cmd_args/expression/scanner.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
解析 argv ,envp中，不符合语法的部分，进行纠错

number: ([0-9])+
decimal:  (-?\d+)(\.\d+)?((e|E)(-?\d+))?
ident: ([a-zA-Z_])+[\w]

Scanner 支持两种分词模式：scan() 批量分词（局部 token 变量），
step() 增量逐字符喂入（复用同一状态分派表，供流式 Interpreter 使用）。

*/

namespace expression_detail {

class Lexer
{
public:
    /// @brief argv 字节流入口
    Lexer(int _Argc, char const *_Argv[], char *_Envp[] = nullptr)
    {
        __loadInput(_Argc, _Argv);
        __loadEnv(_Envp);
    }

    /// @brief 整行入口（REPL）
    explicit Lexer(std::string const &_Line)
    {
        if (!_Line.empty()) { srcInput__.emplace_back(_Line); }
    }

    /// @brief 归一化串列表入口（兼容 parse(Tokens_T const &) 转类型化）
    explicit Lexer(Tokens_T const &_Strs)
    {
        for (auto const &s : _Strs) { srcInput__.emplace_back(s); }
    }

    // ===== 批量：一次吐出全部 Token =====

    /// @brief 命令行模式批量分词（类型化输出）
    auto scan() -> TokenStream_T
    {
        return Scanner(srcInput__).scan();
    }

    /// @brief 整行分词（类型化输出）
    static auto scanLine(std::string const &_Line) -> TokenStream_T
    {
        if (_Line.empty()) return {};
        Source_T src{ _Line };
        return Scanner(src).scan();
    }

    /// @brief 归一化字符串列表重扫描（还原类型化信息）
    static auto scanStrings(Tokens_T const &_Src) -> TokenStream_T
    {
        TokenStream_T result;
        for (auto const &s : _Src) {
            if (s.empty()) continue;
            Source_T src{ s };
            auto toks = Scanner(src).scan();
            std::move(toks.begin(), toks.end(), std::back_inserter(result));
        }
        return result;
    }

    /// @brief Env 行模式批量分词
    static auto scanEnvLine(std::string const &_Line) -> TokenStream_T
    {
        if (_Line.empty()) return {};
        Source_T src{ _Line };
        return Scanner(src, Scanner::LexMode_E::EnvLine).scan();
    }

    // ===== 增量：逐个吐出 =====

    /// @brief 是否还有 Token 可 pop（内部缓存 peek）
    bool hasNext()
    {
        if (!tPeeked__.has_value()) { tPeeked__ = __pumpToken(); }
        return tPeeked__.has_value() && tPeeked__->operator bool();
    }

    /// @brief 吐出一个 Token；流空返回无效 Token（operator bool 为 false）
    auto pop() -> Token
    {
        if (tPeeked__) {
            auto t = std::move(*tPeeked__);
            tPeeked__.reset();
            return t;
        }
        return __pumpToken().value_or(Token{});
    }

    // ===== 兼容外观（基于类型化流归一化的字符串流）=====

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

    /// @brief 归一化字符串流（兼容旧接口）
    auto tokens() -> Tokens_T
    {
        if (srcInput__.empty()) return {};
        return __normalize(scan());
    }

    /// @brief REPL/loop 场景：对一整行输入分词
    static auto tokensFromLine(std::string const &_Line) -> Tokens_T
    {
        if (_Line.empty()) return {};
        return __normalize(scanLine(_Line));
    }

private:
    /// @brief 内部增量泵：从当前行流中逐字符喂入 Scanner，产出一个完整 Token
    auto __pumpToken() -> std::optional<Token>
    {
        while (idxLine__ < srcInput__.size()) {
            if (!sc___.has_value()) {
                sc___.emplace(srcInput__);
                idxChar__ = 0;
            }
            auto const &line = srcInput__[idxLine__];
            while (idxChar__ < line.size()) {
                auto tok = sc___->step(line[idxChar__]);
                ++idxChar__;
                if (tok) return tok;
            }
            // 行尾：flush
            auto tok = sc___->step('\0');
            idxLine__++;
            sc___.reset();
            if (tok && tok->operator bool()) return tok;
        }
        return std::nullopt;
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
    size_t           idxLine__ = 0;    ///< 增量泵：当前行索引
    size_t           idxChar__ = 0;    ///< 增量泵：当前行内字符偏移
    std::optional<Scanner> sc___;      ///< 增量泵：当前行的 Scanner 实例
    std::optional<Token>   tPeeked__;  ///< hasNext() 缓存
};

}  // namespace expression_detail

using expression_detail::Lexer;

CMD_ARGS_NAMESPACE_END