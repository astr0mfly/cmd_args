/*
 * example / reactive
 *
 * 演示响应式数据：参数值 Property 的读/写拦截与变更通知。
 *   1. 写拦截（准入）：option("port") 的 onWrite 只接受 1..65535，拒绝时不写值、不涨版本、
 *      不触发 onChange；Property::set 返回 false 并给出 lastError()，持有方 setValue() 则抛
 *      ErrorValidation(VALIDATION_FAILED)——CLI 解析器走同一通道，错误进 ec。
 *   2. 写钩子兼做规范化：argument("name") 去首尾空白并转大写，钩子直接改候选值。
 *   3. 读拦截（默认值）：option("level") 的 onRead 在原始值为空时返回 info；getOption 拿到
 *      展开后的值，property().raw() 仍是空串。
 *   4. 变更通知与版本：onChange 打印 (old -> new)，version() 只在存储值真的变化时自增；
 *      setRaw() 是受控来源（YAML/快照/${VAR} 展开）的旁路，reset() 回到默认值并清标记。
 *
 * 运行示例：
 *   xmake build reactive
 *   xmake r reactive -- --name alice --port 9090 --level debug
 *   xmake r reactive -- --name alice --port 70000     # 写拦截：port 越界 -> VALIDATION_FAILED
 *   xmake r reactive -- --name alice                  # level 未给 -> 读拦截得到 info
 */
#include <cctype>
#include <iostream>
#include <string>
#include <utility>

#include "cmd_args/cmd_args.hpp"

namespace {

/// @brief 端口写钩子：候选值必须是十进制整数且落在 1..65535
auto checkPort(std::string &_Value) -> std::string
{
    if (_Value.empty()) {
        return "端口不能为空";
    }
    int nPort = 0;
    for (char ch : _Value) {
        if (ch < '0' || ch > '9') {
            return "端口必须是十进制整数：" + _Value;
        }
        nPort = nPort * 10 + (ch - '0');
        if (nPort > 65535) {
            return "端口超出范围 1..65535：" + _Value;
        }
    }
    if (nPort < 1) {
        return "端口超出范围 1..65535：" + _Value;
    }
    return {};
}

/// @brief 空串显示为 <empty>，便于观察变更前后的旧值
auto display(std::string const &_Text) -> std::string
{
    return _Text.empty() ? std::string("<empty>") : _Text;
}

}  // namespace

int main(int _Argc, char *_Argv[], char *_Envp[])
{
    using namespace cmd_args;

    Context app("reactive_demo");

    /* 1) 写拦截（准入）：越界/非数字直接拒绝，不写值、不涨版本、不触发变更回调 */
    app.addOption<int>("port", "listening port", 8080);
    app.option("port")->property()
        .onWrite(&checkPort)
        .onChange([](std::string const &_Old, std::string const &_New) {
            std::cout << "[onChange] port : " << display(_Old) << " -> " << _New << "\n";
        });

    /* 2) 写钩子兼做规范化：去首尾空白并转大写，改的是候选值本身 */
    app.build<ArgumentNamed>("name", "user name");
    app.argument("name")->property()
        .onWrite([](std::string &_Value) -> std::string {
            auto itBeg = _Value.begin();
            auto itEnd = _Value.end();
            while (itBeg != itEnd && std::isspace(static_cast<unsigned char>(*itBeg))) {
                ++itBeg;
            }
            while (itEnd != itBeg && std::isspace(static_cast<unsigned char>(*(itEnd - 1)))) {
                --itEnd;
            }
            std::string strNormalized(itBeg, itEnd);
            for (char &ch : strNormalized) {
                ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            }
            if (strNormalized.empty()) {
                return "name 不能为空（去空白后）";
            }
            _Value = std::move(strNormalized);
            return {};
        })
        .onChange([](std::string const &_Old, std::string const &_New) {
            std::cout << "[onChange] name : " << display(_Old) << " -> " << _New << "\n";
        });

    /* 3) 读拦截（默认值）：存储值为空时读出 info；不带 --level 也能得到可用的默认值 */
    app.addOption<std::string>("level", "log level (debug/info/warn/error)", "");
    app.option("level")->property()
        .onWrite([](std::string &_Value) -> std::string {
            static char const *const arrLevels[] = {"debug", "info", "warn", "error"};
            for (char &ch : _Value) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            for (char const *pLevel : arrLevels) {
                if (_Value == pLevel) {
                    return {};
                }
            }
            return "level 只能是 debug/info/warn/error：" + _Value;
        })
        .onRead([](std::string const &_Value) -> std::string {
            return _Value.empty() ? std::string("info") : _Value;
        });

    std::error_code ec;
    app.parse(ec, _Argc, const_cast<char const **>(_Argv), _Envp);
    if (ec) {
        /* ec 通道只保留错误码（ec.message() 是错误类别的通用文案），写钩子的具体拒绝原因
           留在 Property::lastError()；需要异常原文则用抛异常版 parse */
        std::cout << "(write rejected at parse) " << ec.message() << "\n";
        if (auto const *pPort = app.option("port")) {
            std::cout << "lastError = " << pPort->property().lastError() << "\n";
        }
        std::cout << app.dump();
        return static_cast<int>(ec.value());
    }

    auto &portProp  = app.option("port")->property();
    auto &levelProp = app.option("level")->property();

    std::cout << "port   = " << app.getOption<int>("port") << " (version " << portProp.version()
              << ", isSet " << (portProp.isSet() ? "true" : "false") << ")\n";

    /* 4) Property::set 不抛：拒绝返回 false，原因留在 lastError()，状态原封不动 */
    bool bAccepted = portProp.set("70000");
    std::cout << "port.set(\"70000\") -> " << (bAccepted ? "accepted" : "rejected")
              << (bAccepted ? std::string() : " / " + portProp.lastError()) << "\n";
    std::cout << "port after reject = " << app.getOption<int>("port") << " (version "
              << portProp.version() << ")\n";

    /* 5) 持有方通道：setValue 拒绝时抛 ErrorValidation(VALIDATION_FAILED) */
    try {
        app.option("port")->setValue("abc");
    }
    catch (ErrorValidation const &_Ex) {
        std::cout << "port.setValue(\"abc\") -> throw: " << _Ex.what() << "\n";
    }

    /* 6) 规范化：写钩子通过后存的是规范化值；多词值经 Property 读取
       （getArgument<std::string> 内部是 util::parse_value 的 istringstream >>，只取首个空白分隔
       token，会把 "ALICE SMITH" 截成 "ALICE"，故这里用 property().get()） */
    app.argument("name")->property().set("  alice smith  ");
    std::cout << "name   = " << app.argument("name")->property().get() << "\n";

    /* 7) 读拦截：raw() 仍是空串，get()/getOption 得到 info */
    std::cout << "level  = " << app.getOption<std::string>("level") << " (raw = ["
              << levelProp.raw() << "], version " << levelProp.version() << ")\n";

    /* 8) 旁路来源（YAML/快照/${VAR} 展开）：setRaw 跳过写钩子 */
    portProp.setRaw("70000");
    std::cout << "port.setRaw(\"70000\") -> 旁路成功，port = " << app.getOption<int>("port")
              << "\n";

    /* 9) reset：回到默认值，清 bSet 与错误 */
    portProp.reset();
    std::cout << "port.reset() -> " << app.getOption<int>("port") << " (isSet "
              << (portProp.isSet() ? "true" : "false") << ")\n";
    return 0;
}
