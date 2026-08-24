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
 *   5. 选项 action：.action("cmd {value}") 绑定 system_call 子进程命令，
 *      解析时命中即按命令行顺序执行（{value} 替换为本次命中的值）
 *   6. 错误捕获  parse(std::error_code&, ...) 不抛异常，错误存 std::error_code
 *   7. once()    解析成功后以退出码退出
 *
 * 运行示例（xmake）：
 *   xmake r once -- --hello hi --timeout 99 -v peter 3
 *   xmake r once -- -!                 # 只看本程序的环境变量快照
 *   xmake r once -- -?                 # 帮助
 *   xmake r once -- --env-set=A=1 --env-apply   # 新建并回写环境变量
 *   xmake r once -- --notify "deploy done"      # 命中 action，执行系统命令
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
    demo.addOption<std::string>('o', "output", "output file path", "out.txt");
    demo.addOption<int>("timeout", "timeout in seconds", 30);
    demo.addFlag('v', "verbose", "enable verbose output");

    // 选项 action（system_call 子进程命令）：解析时该选项被显式命中后，
    // 立即按命令行出现顺序通过 std::system 执行；{value} 原样替换为本次命中的值
    // （这里演示引用：echo 收到完整字符串；需要转义时不带引号会按 shell 切分）
    demo.addOption<std::string>("notify", "run echo with the value as a system call", "");
    demo.option("notify")->action("echo notify: {value}");

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
    std::cout << "hello   = " << demo.getArgument<std::string>("hello") << "\n";
    std::cout << "world   = " << demo.getArgument<std::string>("world") << "\n";
    std::cout << "name    = " << demo.getArgument<std::string>("name") << "\n";
    std::cout << "count   = " << demo.getArgument<std::string>("count") << "\n";
    std::cout << "output  = " << demo.getOption<std::string>("output") << "\n";
    std::cout << "timeout = " << demo.getOption<int>("timeout") << "\n";
    std::cout << "verbose = " << (demo.getOption<bool>("verbose") ? "on" : "off") << "\n";

    // 若命令行给了 --notify，其 action 已在解析时执行（看上面的 echo 输出）
    std::cout << "notify  = " << demo.getOption<std::string>("notify") << "\n";

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