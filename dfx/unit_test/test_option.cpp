#include "catch2/catch.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

/* ==================== 选项 action（system_call 子进程执行） ==================== */

namespace {
/* action 测试辅助：把 {value} 写入临时目录文件，作为"命令真实执行过"的可观测副作用 */
std::string action_test_cmd(std::string const &_File, bool _Append)
{
#ifdef _WIN32
    return "cmd /c echo {value}" + std::string(_Append ? ">>" : ">") + "\"" + _File + "\"";
#else
    return "echo {value} " + std::string(_Append ? ">>" : ">") + "\"" + _File + "\"";
#endif
}

std::string action_test_file(std::string const &_Tag)
{
    return (std::filesystem::temp_directory_path()
            / ("cmd_args_action_" + _Tag + ".txt"))
        .string();
}

/* Windows 的 echo 输出带 \r\n，读回行时剥掉 \r */
void strip_cr(std::string &_S)
{
    if (!_S.empty() && _S.back() == '\r') { _S.pop_back(); }
}
}  // namespace

TEST_CASE("context: 选项 action 命中即执行 system_call，{value} 原样替换", "[action]")
{
    auto file = action_test_file("echo");
    std::remove(file.c_str());

    Context app("ut_action_echo");
    app.addOption<std::string>("task", "run a system command", "");
    app.option("task")->action(action_test_cmd(file, false));

    std::error_code ec;
    app.parse(ec, Tokens_T{ "--task", "Hello World" }, Envs_T{});
    REQUIRE(!ec);

    // 子进程命令确实被执行：临时文件被写出，且 {value} 被替换为选项值（含空格）
    std::ifstream ifs(file);
    REQUIRE(ifs.good());
    std::string line;
    std::getline(ifs, line);
    strip_cr(line);
    REQUIRE(line == "Hello World");

    // action 属于执行配置而非取值：不进入 schema / YAML 导出
    REQUIRE(app.schema_string().find("cmd_args_action_") == std::string::npos);
    REQUIRE(app.to_yaml().find("{value}") == std::string::npos);
}

TEST_CASE("context: action 三次命中同一选项时由 Executor 按 key 执行一次", "[action]")
{
    auto file = action_test_file("order");
    std::remove(file.c_str());

    Context app("ut_action_order");
    app.addOption<std::string>("step", "run a step", "");
    app.option("step")->action(action_test_cmd(file, true));

    std::error_code ec;
    app.parse(ec, Tokens_T{ "--step", "one", "--step", "two", "--step", "three" }, Envs_T{});
    REQUIRE(!ec);

    // Executor 按 key 执行：actionOrder 去重后只有 "step"，用最终值 "three"
    std::ifstream ifs(file);
    REQUIRE(ifs.good());
    std::string line;
    std::getline(ifs, line);
    strip_cr(line);
    REQUIRE(line == "three");  // 最终值
}

TEST_CASE("context: 环境变量兜底与 YAML 外部赋值不触发 action", "[action]")
{
    auto file = action_test_file("fallback");
    std::remove(file.c_str());

    Context app("ut_action_fb");
    app.addOption<std::string>("task", "run a command", "default");
    app.option("task")->action(action_test_cmd(file, false));
    app.option("task")->env("UT_ACTION_TASK");

    std::error_code ec;
    app.parse(ec, Tokens_T{}, Envs_T{ "UT_ACTION_TASK=from-env" });
    REQUIRE(!ec);
    REQUIRE(app.getOption<std::string>("task") == "from-env");  // 环境变量兜底值生效
    REQUIRE_FALSE(std::ifstream(file).good());  // 但 action 未被触发（未写文件）

    // YAML 外部赋值（setExternal 路径）同样不触发 action
    app.from_yaml("options.task: \"from-yaml\"");
    REQUIRE_FALSE(std::ifstream(file).good());
}

TEST_CASE("context: action 命令失败向 stderr 告警且不中断解析", "[action]")
{
    Context app("ut_action_fail");
    app.addOption<int>("a", "a", 0);
    app.option("a")->action(
#ifdef _WIN32
        "cmd /c exit 1"
#else
        "false"
#endif
    );
    app.addOption<int>("b", "b", 0);

    std::ostringstream oss;
    std::streambuf *old = std::cerr.rdbuf(oss.rdbuf());  // 捕获 stderr
    std::error_code ec;
    app.parse(ec, Tokens_T{ "--a", "1", "--b", "2" }, Envs_T{});
    std::cerr.rdbuf(old);

    REQUIRE(!ec);                              // 失败不中断解析
    REQUIRE(app.getOption<int>("b") == 2);    // 后续选项照常赋值
    REQUIRE(oss.str().find("(action)") != std::string::npos);  // 有告警输出
}
