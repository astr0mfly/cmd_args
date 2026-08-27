/*
 * example / validate
 *
 * 演示 必选检查 + 值校验器 + 默认值判段 + 声明式环境条目：
 *   1. option("host")->required().semantic("ipv4")
 *   2. option("port")->range(1024, 65535)  + defaultExpr("${DEMO_PORT:-8080}")
 *   3. argument("name")->required()
 *   4. requireEnv("DB_HOST")->semantic("hostname")->defaultEnv("localhost")
 *
 * 运行示例：
 *   xmake build validation
 *   xmake r validation -- --host 127.0.0.1 --port 9090 --name peter   # 成功
 *   xmake r validation -- --host bad-ip --port 9090 --name peter     # VALIDATION_FAILED
 *   xmake r validation -- --name peter                                # MISSING_OPTION
 */
#include <iostream>
#include <string>

#include "cmd_args/cmd_args.hpp"

int main(int _Argc, char *_Argv[], char *_Envp[])
{
    using namespace cmd_args;

    Context app("validation_demo");

    app.addOption<std::string>("host", "ipv4 host", "");
    app.option("host")->required().semantic("ipv4");
    app.addOption<int>("port", "port number", 0);
    app.option("port")->range(1024, 65535).defaultExpr("${DEMO_PORT:-8080}");
    app.build<ArgumentNamed>("name", "your name");
    app.argument("name")->required();
    app.requireEnv("DB_HOST", "database host")->semantic("hostname").defaultEnv("localhost");

    std::error_code ec;
    app.parse(ec, _Argc, const_cast<char const **>(_Argv), _Envp);
    if (ec) {
        std::cerr << "(validation error) " << ec.message() << "\n" << app.dump();
        return static_cast<int>(ec.value());
    }

    std::cout << "host  = " << app.getOption<std::string>("host") << "\n";
    std::cout << "port  = " << app.getOption<int>("port") << "\n";
    std::cout << "name  = " << app.getArgument<std::string>("name") << "\n";
    std::cout << "DB_HOST = " << app.get_env_or("DB_HOST", "<defaulted>") << "\n";
    return 0;
}
