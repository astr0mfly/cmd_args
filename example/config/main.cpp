/*
 * example / config
 *
 * 三类外界环境的统一抽象与自然转换演示：
 *   ① 环境变量      —— 一等公民（已有）
 *   ② 实时命令行    —— argv 解析（已有）
 *   ③ YAML 配置脚本 —— 本次新增：
 *         save_config() / --save-config PATH   参数 + 环境 → YAML 文件
 *         load_config() / --load-config PATH   YAML → 环境变量反哺 + 参数外部赋值
 *   优先级：CLI 显式值 > 配置文件(YAML) > 环境变量 > 内置默认值
 *
 * 运行示例（xmake）：
 *   xmake r config_demo -- --hello hi --timeout 99 -v peter 3
 *        —— 解析并打印，随后生成 demo.yaml（当前工作目录）
 *   xmake r config_demo -- --load-config demo.yaml --hello cli-hi
 *        —— 加载配置反哺，CLI 的 hello 覆盖配置值
 */
#include <iostream>
#include <string>

#include "cmd_args/cmd_args.hpp"

int main(int _Argc, char *_Argv[], char *_Envp[])
{
    using namespace cmd_args;

    /* ---------- 1. 构筑命令行模型 ---------- */
    Context app("config_demo");

    app.build<ArgumentNamed>("hello", "Say hello")
        .build<ArgumentNamed>("world", "Say world too");
    app.build<ArgumentPosition>("name", "your name")
        .build<ArgumentPosition>("count", "how many times");
    app.addOption<std::string>('o', "output", "output file path", "out.txt");
    app.addOption<int>("timeout", "timeout in seconds", 30);
    app.addFlag('v', "verbose", "enable verbose output");

    // 环境变量联动（①）：命令行显式值 > 环境变量 > 内置默认值
    app.option("output")->env("CMD_ARGS_DEMO_OUTPUT");
    app.option("timeout")->env("CMD_ARGS_DEMO_TIMEOUT");
    app.argument("world")->env("CMD_ARGS_DEMO_WORLD");
    app.set_env("CMD_ARGS_DEMO_WORLD", "env-says-hi");

    /* ---------- 2. 参数格式（build 完成后的描述） ---------- */
    std::cout << "===== schema =====\n" << app.schema_string();

    /* ---------- 3. 解析 argv（②：实时命令行；内置 --load-config / --save-config 按 token 顺序生效） ---------- */
    std::error_code ec;
    app.parse(ec, _Argc, const_cast<char const **>(_Argv), _Envp);
    if (ec) {
        std::cerr << "(parse error) " << ec.message() << "\n";
        return static_cast<int>(ec.value());
    }

    /* ---------- 4. 读取结果 ---------- */
    std::cout << "===== values =====\n";
    std::cout << "hello   = " << app.getArgument<std::string>("hello") << "\n";
    std::cout << "world   = " << app.getArgument<std::string>("world") << "\n";
    std::cout << "name    = " << app.getArgument<std::string>("name") << "\n";
    std::cout << "count   = " << app.getArgument<std::string>("count") << "\n";
    std::cout << "output  = " << app.getOption<std::string>("output") << "\n";
    std::cout << "timeout = " << app.getOption<int>("timeout") << "\n";
    std::cout << "verbose = " << (app.getOption<bool>("verbose") ? "on" : "off") << "\n";

    /* ---------- 5. 存储为 YAML（③：参数 + 环境 → 配置文件，供下次调用） ---------- */
    std::cout << "===== to_yaml() =====\n" << app.to_yaml();
    if (app.save_config("demo.yaml")) {
        std::cout << "saved demo.yaml\n";
    }
    else {
        std::cerr << "save_config failed\n";
        return 1;
    }

    /* ---------- 6. 下一次调用：YAML → 环境变量反哺 + 参数外部赋值 ---------- */
    std::cout << "===== load_config(demo.yaml) 反哺 =====\n";
    app.reset();  // 模拟一次全新调用
    if (!app.load_config("demo.yaml")) {
        std::cerr << "load_config failed\n";
        return 1;
    }
    std::cout << "--- 反哺后的环境变量（environment 分区已 set_env） ---\n";
    std::cout << app.env_string();
    std::cout << "timeout = " << app.getOption<int>("timeout")
              << "  (来自配置 arguments 分区的外部赋值)\n";
    std::cout << "world   = " << app.getArgument<std::string>("world") << "\n";

    /* ---------- 7. 退出 ---------- */
    app.once();
    return 0;
}