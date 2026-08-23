/*
 * example / once
 *
 * once 模式：一次完成 构筑 -> 解析 -> 展示，随后退出。
 *
 * 演示 cmd_args 的核心能力：
 *   1. 命名参数  --name=value / --name value
 *   2. 方位参数  剩余 token 按声明顺序赋值
 *   3. 选项      -x/--long，支持 -abc 聚合 / -tvalue / -t value
 *   4. 环境变量一等公民：
 *        .env("VAR")       命令行显式值 > 环境变量 > 内置默认值
 *        set_env/env_string/expand_env/apply_env  会话内交互与回写操作系统
 *      内置选项：-?/--help、-!/--env、--env-set KEY=VALUE、
 *               --env-unset KEY、--env-apply
 *   5. 错误捕获  parse(std::error_code&, ...) 不抛异常，错误存 std::error_code
 *   6. once()    解析成功后以退出码退出
 *
 * 运行示例（xmake）：
 *   xmake r once -- --hello hi --timeout 99 -v peter 3
 *   xmake r once -- -!                 # 只看本程序的环境变量快照
 *   xmake r once -- -?                 # 帮助
 *   xmake r once -- --env-set=A=1 --env-apply   # 新建并回写环境变量
 */
#include <iostream>
#include <string>

#include "cmd_args/cmd_args.hpp"
#include "cmd_args/error/exception_handler.h"

// 崩溃兜底：纯虚调用 / 未捕获异常 / 系统故障时生成 minidump
static exception_init sg_exception_guard;

int main(int _Argc, char *_Argv[], char *_Envp[])
{
    using namespace cmd_args;

    (void)sg_exception_guard;

    /* ---------- 1. 构筑命令行模型 ---------- */

    Context demo("once_demo");

    // 命名参数：--hello=值 / --hello 值
    demo.build<ArgumentNamed>("hello", "Say hello")
        .build<ArgumentNamed>("world", "Say world too");

    // 方位参数：剩余 token 依次赋值
    demo.build<ArgumentPosition>("name", "your name")
        .build<ArgumentPosition>("count", "how many times");

    // 值选项 + 标志选项
    demo.add_option<std::string>('o', "output", "output file path", "out.txt");
    demo.add_option<int>("timeout", "timeout in seconds", 30);
    demo.add_flag('v', "verbose", "enable verbose output");

    /* ---------- 2. 环境变量联动 ---------- */

    // 选项/参数绑定环境变量：命令行显式值 > 环境变量 > 内置默认值
    demo.option("output")->env("CMD_ARGS_DEMO_OUTPUT");
    demo.option("timeout")->env("CMD_ARGS_DEMO_TIMEOUT");

    // world 未在命令行显式给出，取会话环境变量 CMD_ARGS_DEMO_WORLD 兜底；
    // 预置该会话环境变量（parse 前 set_env，等同 --env-set 的编程写法）
    demo.argument("world")->env("CMD_ARGS_DEMO_WORLD");
    demo.set_env("CMD_ARGS_DEMO_WORLD", "env-says-hi");

    /* ---------- 3. 解析 argv（错误进 std::error_code，不抛异常） ---------- */

    std::error_code ec;
    demo.parse(ec, _Argc, const_cast<char const **>(_Argv), _Envp);
    if (ec) {
        std::cerr << "(parse error) " << ec.message() << "\n";
        return static_cast<int>(ec.value());
    }

    /* ---------- 4. 读取解析结果 ---------- */

    std::cout << "--- values ---\n";
    std::cout << "hello   = " << demo.get_argument<std::string>("hello") << "\n";
    std::cout << "world   = " << demo.get_argument<std::string>("world") << "\n";
    std::cout << "name    = " << demo.get_argument<std::string>("name") << "\n";
    std::cout << "count   = " << demo.get_argument<std::string>("count") << "\n";
    std::cout << "output  = " << demo.get_option<std::string>("output") << "\n";
    std::cout << "timeout = " << demo.get_option<int>("timeout") << "\n";
    std::cout << "verbose = " << (demo.get_option<bool>("verbose") ? "on" : "off") << "\n";

    /* ---------- 5. 环境变量交互（差异化能力） ---------- */

    std::cout << "\n--- environment snapshot ---\n";
    std::cout << demo.env_string();

    std::cout << "raw     = " << demo.get_env_or("CMD_ARGS_DEMO_WORLD", "<missing>") << "\n";
    std::cout << "expand  = " << demo.expand_env("${CMD_ARGS_DEMO_WORLD}!") << "\n";

    // 会话内新建一个环境变量，演示回写标记（+）
    demo.set_env("CMD_ARGS_DEMO_NEW", "created-in-session");
    std::cout << "--- after set_env(CMD_ARGS_DEMO_NEW) ---\n";
    std::cout << demo.env_string();

    /* ---------- 6. 回写会话变更到操作系统环境 ---------- */

    auto applied = demo.apply_env();
    std::cout << "applied " << applied << " variable(s) back to the OS environment\n";

    /* ---------- 7. 解析成功即退出 ---------- */

    demo.once();  // exit(0)
    return 0;
}