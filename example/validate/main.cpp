/*
 * example / validate
 *
 * 演示 必选检查 + 值校验器 + 默认值判段 + 声明式环境条目：
 *   1. option("host")->required().semantic("ipv4")
 *   2. option("port")->range(1024, 65535)  + defaultExpr("${DEMO_PORT:-8080}")
 *   3. argument("name")->required()
 *   4. requireEnv("DB_HOST")->semantic("hostname")->defaultEnv("localhost")
 *
 * 运行示例（使用 Tokens_T 接口，与全量单测一致）：
 *   xmake build validation
 *   xmake r validation   # 场景 1：全部合法
 *   xmake r validation 2 # 场景 2：host 非法（VALIDATION_FAILED）
 *   xmake r validation 3 # 场景 3：缺 --host（MISSING_OPTION）
 *
 * 注：本示例使用 Tokens_T 接口而非 argv 接口，原因：
 *   Scanner 在 CommandLine 模式下会将含点号的值（如 127.0.0.1）拆分为
 *   多个 token（数字 + 分隔符），导致 option 值未被正确消费。
 *   Tokens_T 接口是本库所有单元测试使用的标准解析路径。
 */
#include <iostream>
#include <string>

#include "cmd_args/cmd_args.hpp"

/// @brief 场景 1：全部合法 — host/port/name 均满足约束
int scenario_ok()
{
    using namespace cmd_args;
    Context app("validation_demo");

    app.addOption<std::string>("host", "ipv4 host", "");
    app.option("host")->required().semantic("ipv4");
    app.addOption<int>("port", "port number", 0);
    app.option("port")->range(1024, 65535).defaultExpr("${DEMO_PORT:-8080}");
    app.build<ArgumentPosition>("name", "your name");
    app.argument("name")->required();
    app.requireEnv("DB_HOST", "database host")->semantic("hostname").defaultEnv("localhost");

    Tokens_T tokens = { "--host", "127.0.0.1", "--port", "9090", "peter" };
    Envs_T   envs;
    std::error_code ec;
    app.parse(ec, tokens, envs);
    if (ec) {
        std::cerr << "(validation error) " << ec.message() << "\n" << app.dump();
        return static_cast<int>(ec.value());
    }

    std::cout << "host    = " << app.getOption<std::string>("host") << "\n";
    std::cout << "port    = " << app.getOption<int>("port") << "\n";
    std::cout << "name    = " << app.getArgument<std::string>("name") << "\n";
    std::cout << "DB_HOST = " << app.get_env_or("DB_HOST", "<defaulted>") << "\n";
    return 0;
}

/// @brief 场景 2：host 非法 IPv4 — 触发 VALIDATION_FAILED
int scenario_bad_host()
{
    using namespace cmd_args;
    Context app("validation_demo");

    app.addOption<std::string>("host", "ipv4 host", "");
    app.option("host")->required().semantic("ipv4");
    app.addOption<int>("port", "port number", 0);
    app.option("port")->range(1024, 65535).defaultExpr("${DEMO_PORT:-8080}");
    app.build<ArgumentPosition>("name", "your name");
    app.argument("name")->required();
    app.requireEnv("DB_HOST", "database host")->semantic("hostname").defaultEnv("localhost");

    Tokens_T tokens = { "--host", "bad-ip", "--port", "9090", "peter" };
    Envs_T   envs;
    std::error_code ec;
    app.parse(ec, tokens, envs);
    if (ec) {
        std::cerr << "(validation error) " << ec.message() << "\n" << app.dump();
        return static_cast<int>(ec.value());
    }
    return 0;
}

/// @brief 场景 3：缺 --host — 触发 MISSING_OPTION
int scenario_missing_host()
{
    using namespace cmd_args;
    Context app("validation_demo");

    app.addOption<std::string>("host", "ipv4 host", "");
    app.option("host")->required().semantic("ipv4");
    app.addOption<int>("port", "port number", 0);
    app.option("port")->range(1024, 65535).defaultExpr("${DEMO_PORT:-8080}");
    app.build<ArgumentPosition>("name", "your name");
    app.argument("name")->required();
    app.requireEnv("DB_HOST", "database host")->semantic("hostname").defaultEnv("localhost");

    Tokens_T tokens = { "peter" };
    Envs_T   envs;
    std::error_code ec;
    app.parse(ec, tokens, envs);
    if (ec) {
        std::cerr << "(validation error) " << ec.message() << "\n" << app.dump();
        return static_cast<int>(ec.value());
    }
    return 0;
}

int main(int _Argc, char *_Argv[], char *_Envp[])
{
    // 根据命令行参数选择场景（默认场景 1）
    int scenario = 1;
    if (_Argc > 1) { scenario = std::atoi(_Argv[1]); }

    switch (scenario) {
    case 1:
        std::cout << "=== Scenario 1: valid ===\n";
        return scenario_ok();
    case 2:
        std::cout << "=== Scenario 2: bad host (VALIDATION_FAILED) ===\n";
        return scenario_bad_host();
    case 3:
        std::cout << "=== Scenario 3: missing --host (MISSING_OPTION) ===\n";
        return scenario_missing_host();
    default:
        std::cerr << "Usage: validation [1|2|3]\n";
        return 1;
    }
}
