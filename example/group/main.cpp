/*
 * example / group
 *
 * 分组声明风格：group() 将选项按逻辑分组，help 文档自动分组展示。
 *
 * group(name, defs...) 注册选项的同时记录分组元数据。
 * help 输出时先按分组渲染，未分组选项单独展示在 "Options:" 下。
 *
 * 适合参数超过 10+ 的场景，分组让 help 文档更易读。
 *
 * 运行示例（xmake）：
 *   xmake r group -- -i data.txt -H myhost -p 8080 -o log.txt -v
 *   xmake r group -- -?
 */
#include <iostream>
#include <string>

#include "cmd_args/cmd_args.hpp"

int main(int _Argc, char *_Argv[], char *_Envp[])
{
    using namespace cmd_args;

    Context app("group_demo");

    /* ---------- 未分组的选项（显示在 "Options:" 下） ---------- */

    app.declare(
        OptionDef<std::string>{'i', "input", "input file to process", ""},
        OptionDef<int>{'j', "jobs", "parallel jobs", 4},
        FlagDef{'v', "verbose", "enable verbose output"}
    );

    /* ---------- 分组：网络配置 ---------- */

    app.group("网络",
        OptionDef<std::string>{'H', "host", "remote host", "localhost"},
        OptionDef<int>{'p', "port", "listen port", 8080},
        OptionDef<std::string>{'b', "bind", "bind address", "0.0.0.0"}
    );

    /* ---------- 分组：输出配置 ---------- */

    app.group("输出",
        OptionDef<std::string>{'o', "output", "output file", "out.txt"},
        OptionDef<std::string>{'f', "format", "output format", "json"},
        FlagDef{'q', "quiet", "suppress output"},
        FlagDef{'c', "color", "colorized output"}
    );

    /* ---------- 分组：安全配置 ---------- */

    app.group("安全",
        OptionDef<std::string>{'\0', "cert", "TLS certificate file", ""},
        OptionDef<std::string>{'\0', "key", "TLS private key file", ""},
        FlagDef{'\0', "tls", "enable TLS"}
    );

    /* ---------- 解析 ---------- */

    std::error_code ec;
    app.parse(ec, _Argc, const_cast<char const **>(_Argv), _Envp);
    if (ec) {
        std::cerr << "(parse error) " << ec.message() << "\n";
        return static_cast<int>(ec.value());
    }

    /* ---------- 读取结果 ---------- */

    std::cout << "input   = " << app.getOption<std::string>("input") << "\n";
    std::cout << "jobs    = " << app.getOption<int>("jobs") << "\n";
    std::cout << "verbose = " << (app.getOption<bool>("verbose") ? "on" : "off") << "\n";
    std::cout << "\n";
    std::cout << "host    = " << app.getOption<std::string>("host") << "\n";
    std::cout << "port    = " << app.getOption<int>("port") << "\n";
    std::cout << "bind    = " << app.getOption<std::string>("bind") << "\n";
    std::cout << "\n";
    std::cout << "output  = " << app.getOption<std::string>("output") << "\n";
    std::cout << "format  = " << app.getOption<std::string>("format") << "\n";
    std::cout << "quiet   = " << (app.getOption<bool>("quiet") ? "on" : "off") << "\n";
    std::cout << "color   = " << (app.getOption<bool>("color") ? "on" : "off") << "\n";
    std::cout << "\n";
    std::cout << "cert    = " << app.getOption<std::string>("cert") << "\n";
    std::cout << "key     = " << app.getOption<std::string>("key") << "\n";
    std::cout << "tls     = " << (app.getOption<bool>("tls") ? "on" : "off") << "\n";

    return 0;
}
