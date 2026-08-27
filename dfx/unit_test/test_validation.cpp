#include "catch2/catch.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "cmd_args/cmd_args.hpp"
#include "cmd_args/base/validator.h"

using namespace cmd_args;

TEST_CASE("validator: is_port 接受 1..65535，拒绝越界与非数字", "[validator]")
{
    REQUIRE(validator_detail::is_port("80") == true);
    REQUIRE(validator_detail::is_port("1") == true);
    REQUIRE(validator_detail::is_port("65535") == true);
    REQUIRE(validator_detail::is_port("0") == false);
    REQUIRE(validator_detail::is_port("65536") == false);
    REQUIRE(validator_detail::is_port("-1") == false);
    REQUIRE(validator_detail::is_port("80a") == false);
    REQUIRE(validator_detail::is_port("") == false);
}

TEST_CASE("validator: is_ipv4 四段 0..255", "[validator]")
{
    REQUIRE(validator_detail::is_ipv4("192.168.1.1") == true);
    REQUIRE(validator_detail::is_ipv4("0.0.0.0") == true);
    REQUIRE(validator_detail::is_ipv4("255.255.255.255") == true);
    REQUIRE(validator_detail::is_ipv4("256.0.0.1") == false);
    REQUIRE(validator_detail::is_ipv4("1.2.3") == false);
    REQUIRE(validator_detail::is_ipv4("1.2.3.4.5") == false);
    REQUIRE(validator_detail::is_ipv4("a.b.c.d") == false);
    REQUIRE(validator_detail::is_ipv4("") == false);
}

TEST_CASE("validator: is_ipv6 标准/压缩/内嵌 IPv4", "[validator]")
{
    REQUIRE(validator_detail::is_ipv6("2001:db8::1") == true);
    REQUIRE(validator_detail::is_ipv6("::1") == true);
    REQUIRE(validator_detail::is_ipv6("::") == true);
    REQUIRE(validator_detail::is_ipv6("1:2:3:4:5:6:7:8") == true);
    REQUIRE(validator_detail::is_ipv6("::ffff:192.168.1.1") == true);
    REQUIRE(validator_detail::is_ipv6("2001:db8::1::2") == false);  // 两次压缩
    REQUIRE(validator_detail::is_ipv6("1:2:3:4:5:6:7:8:9") == false);  // 九组
    REQUIRE(validator_detail::is_ipv6("1:2:3:4:5:6:7:") == false);  // 单冒号结尾
    REQUIRE(validator_detail::is_ipv6(":1:2") == false);            // 单冒号开头
    REQUIRE(validator_detail::is_ipv6("g::1") == false);            // 非hex
}

TEST_CASE("validator: is_float / is_double 数值语义", "[validator]")
{
    REQUIRE(validator_detail::is_float("1.5") == true);
    REQUIRE(validator_detail::is_float("-2.25e3") == true);
    REQUIRE(validator_detail::is_float("1e999") == false);  // 溢出 / inf
    REQUIRE(validator_detail::is_float("abc") == false);
    REQUIRE(validator_detail::is_float("1.2x") == false);
    REQUIRE(validator_detail::is_float("") == false);
    REQUIRE(validator_detail::is_double("0.1") == true);
    REQUIRE(validator_detail::is_double("3e-2") == true);
    REQUIRE(validator_detail::is_double("nan") == false);
    REQUIRE(validator_detail::is_double("inf") == false);
}

TEST_CASE("validator: is_date 真实日历与闰年", "[validator]")
{
    REQUIRE(validator_detail::is_date("2026-08-27") == true);
    REQUIRE(validator_detail::is_date("2024-02-29") == true);   // 闰年
    REQUIRE(validator_detail::is_date("2023-02-29") == false);  // 非闰年
    REQUIRE(validator_detail::is_date("2026-13-01") == false);
    REQUIRE(validator_detail::is_date("2026-00-10") == false);
    REQUIRE(validator_detail::is_date("2026-8-27") == false);   // 缺前导零
    REQUIRE(validator_detail::is_date("2026/08/27") == false);  // 分隔符错误
    REQUIRE(validator_detail::is_date("2026-02-30") == false);
}

TEST_CASE("validator: is_duration 数字[.小数] + 单位 s/m/h/d", "[validator]")
{
    REQUIRE(validator_detail::is_duration("30s") == true);
    REQUIRE(validator_detail::is_duration("5m") == true);
    REQUIRE(validator_detail::is_duration("1.5h") == true);
    REQUIRE(validator_detail::is_duration("2d") == true);
    REQUIRE(validator_detail::is_duration("90") == true);   // 裸数字 = 秒
    REQUIRE(validator_detail::is_duration("-5s") == false);
    REQUIRE(validator_detail::is_duration("10x") == false);
    REQUIRE(validator_detail::is_duration("") == false);
    REQUIRE(validator_detail::is_duration(".") == false);
}

TEST_CASE("validator: is_hostname / is_uuid / is_path / 存在性", "[validator]")
{
    REQUIRE(validator_detail::is_hostname("example.com") == true);
    REQUIRE(validator_detail::is_hostname("a-b.c99") == true);
    REQUIRE(validator_detail::is_hostname("-bad.com") == false);
    REQUIRE(validator_detail::is_hostname("a..b") == false);
    REQUIRE(validator_detail::is_hostname("") == false);
    REQUIRE(validator_detail::is_uuid("550e8400-e29b-41d4-a716-446655440000") == true);
    REQUIRE(validator_detail::is_uuid("550e8400-e29b-41d4") == false);
    REQUIRE(validator_detail::is_uuid("550e8400e29b41d4a716446655440000") == false);
    REQUIRE(validator_detail::is_path("C:\\tmp\\x.txt") == true);
    REQUIRE(validator_detail::is_path("") == false);
    REQUIRE(validator_detail::is_existing_file("no-such-file-8492.txt") == false);

    auto f = (std::filesystem::temp_directory_path() / "cmd_args_ut_validator.txt").string();
    { std::ofstream(f) << "x"; }
    REQUIRE(validator_detail::is_existing_file(f) == true);
    std::filesystem::remove(f);
    REQUIRE(validator_detail::is_existing_file(f) == false);
}

namespace {
/* 本地测试类型：只需继承 mixin 即可驱动链式 API */
struct Dummy : validator_detail::ValidatorMixin<Dummy> {};
}

TEST_CASE("validator: Validator 结构与工厂函数", "[validator]")
{
    REQUIRE(validator_detail::between(1.0, 10.0).validate("5") == "");
    REQUIRE(validator_detail::between(1.0, 10.0).validate("11") != "");       // 越上限
    REQUIRE(validator_detail::between(1.0, 10.0, false, false).validate("1") != "");   // 开下端
    REQUIRE(validator_detail::between(1.0, 10.0, false, false).validate("10") != "");  // 开上端
    REQUIRE(validator_detail::between(1.0, 10.0, false, false).validate("2") == "");

    REQUIRE(validator_detail::one_of({ "fast", "slow" }).validate("fast") == "");
    REQUIRE(validator_detail::one_of({ "fast", "slow" }).validate("mid") != "");

    REQUIRE(validator_detail::matches(R"(^[\w.]+@[\w.]+$)").validate("a@b.c") == "");
    REQUIRE(validator_detail::matches(R"(^[\w.]+@[\w.]+$)").validate("nope") != "");

    REQUIRE(validator_detail::semantic_validator("ipv4").validate("127.0.0.1") == "");
    REQUIRE(validator_detail::semantic_validator("ipv4").validate("999.0.0.1") != "");
    REQUIRE(validator_detail::semantic_validator("port").validate("8080") == "");

    // 未知语义名 = 用户构建错误
    REQUIRE_THROWS_AS(validator_detail::semantic_validator("nope"), ErrorArgument);
}

TEST_CASE("validator: ValidatorMixin 链式 API 与 validate 语义", "[validator]")
{
    Dummy d;
    d.required();
    REQUIRE(d.isRequired() == true);
    d.required(false);
    REQUIRE(d.isRequired() == false);

    d.range(1024, 65535);
    d.semantic("port");
    d.oneOf({ "8888", "9999" });
    REQUIRE(d.hasValidators() == true);
    REQUIRE(d.validators().size() == 3);
    REQUIRE(d.validate("8888") == "");                     // range ✓ port ✓ oneOf ✓
    REQUIRE(d.validate("80") != "");                       // range 失败在前
    REQUIRE(d.validate("2048") != "");                     // oneOf 失败

    // 语义重复绑定 = 替换而非追加
    d.semantic("port");
    REQUIRE(d.validators().size() == 3);

    // custom 校验器
    Dummy d2;
    d2.check(validator_detail::custom(
        [](std::string const &v) { return v.size() == 2; }, "len2"));
    REQUIRE(d2.validate("ab") == "");
    REQUIRE(d2.validate("abc") != "");
}

TEST_CASE("option: required 命中判定（显式/环境兜底/默认值）与 usesDefault", "[required][option]")
{
    Context app("ut_req_opt");
    app.addOption<std::string>("host", "host", "");
    app.option("host")->required();
    app.option("host")->env("UT_REQ_HOST");
    app.addOption<std::string>("port", "port", "8080");

    // 注意：本用例在 T7 之后仍会随全量回归运行，因此每次 parse 都必须满足 host 的
    // required（T7 之校验引擎会把它变成真正错误）。port 为非 required，仅测默认值口径。
    std::error_code ec;
    app.parse(ec, Tokens_T{}, Envs_T{ "UT_REQ_HOST=env-h" });   // host 用 env 兜底命中
    REQUIRE(!ec);
    REQUIRE(app.option("host")->isSatisfied() == true);          // env 兜底命中
    REQUIRE(app.option("host")->bEnvHit__ == true);
    REQUIRE(app.option("port")->isSatisfied() == false);         // 默认值不算命中
    REQUIRE(app.option("port")->hasDefault() == true);
    REQUIRE(app.option("port")->usesDefault() == true);

    app.reset();
    app.parse(ec, Tokens_T{ "--host", "h1" }, Envs_T{});         // 显式命中
    REQUIRE(!ec);
    REQUIRE(app.option("host")->isSatisfied() == true);
    REQUIRE(app.option("host")->usesDefault() == false);

    app.reset();
    app.parse(ec, Tokens_T{}, Envs_T{ "UT_REQ_HOST=env-h" });    // env 兜底命中（再次）
    REQUIRE(!ec);
    REQUIRE(app.option("host")->isSatisfied() == true);
    REQUIRE(app.option("host")->usesDefault() == false);

    app.reset();
    REQUIRE(app.option("host")->isSatisfied() == false);         // reset 清 bEnvHit__/bExplicit__
}

TEST_CASE("option: 校验器挂载（range/oneOf/match/semantic）", "[validator][option]")
{
    Context app("ut_val_opt");
    app.addOption<std::string>("mode", "mode", "");
    app.option("mode")->oneOf({ "fast", "slow" });
    app.addOption<int>("port", "port", 0);
    app.option("port")->range(1024, 65535);
    app.addOption<std::string>("host", "host", "");
    app.option("host")->semantic("ipv4");
    app.addOption<std::string>("mail", "mail", "");
    app.option("mail")->match(R"(^[\w.]+@[\w.]+$)");

    REQUIRE(app.option("mode")->validate("fast") == "");
    REQUIRE(app.option("mode")->validate("mid") != "");
    REQUIRE(app.option("port")->validate("8080") == "");
    REQUIRE(app.option("port")->validate("80") != "");
    REQUIRE(app.option("host")->validate("192.168.0.1") == "");
    REQUIRE(app.option("host")->validate("192.168.0.999") != "");
    REQUIRE(app.option("mail")->validate("a@b.c") == "");
    REQUIRE(app.option("mail")->validate("nope") != "");
    REQUIRE(app.option("mode")->isRequired() == false);
}

TEST_CASE("argument: required 命中判定与默认值语义（命名+方位+env 兜底）", "[required][argument]")
{
    Context app("ut_req_arg");
    app.build<ArgumentNamed>("name", "n");
    app.argument("name")->required();
    app.argument("name")->env("UT_REQ_NAME");
    app.build<ArgumentPosition>("dest", "d");
    app.argument("dest")->required();

    // T7 之后全量回归下，每次 parse 都必须满足全部 required。
    std::error_code ec;
    app.parse(ec, Tokens_T{ "--name", "x", "out" }, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app.argument("name")->isSatisfied() == true);
    REQUIRE(app.argument("dest")->isSatisfied() == true);
    REQUIRE(app.argument("name")->usesDefault() == false);

    app.reset();
    app.parse(ec, Tokens_T{ "out" }, Envs_T{ "UT_REQ_NAME=from-env" });  // name 走 env 兜底
    REQUIRE(!ec);
    REQUIRE(app.argument("name")->isSatisfied() == true);
    REQUIRE(app.argument("name")->isEnvHit() == true);
    REQUIRE(app.argument("dest")->isSatisfied() == true);

    app.reset();
    app.parse(ec, Tokens_T{ "--name", "x", "out" }, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app.argument("name")->isSatisfied() == true);

    app.reset();
    REQUIRE(app.argument("name")->isEnvHit() == false);       // reset 清标记
    REQUIRE(app.argument("name")->isSatisfied() == false);
}

TEST_CASE("argument: 校验器挂载（check/semantic）", "[validator][argument]")
{
    Context app("ut_val_arg");
    app.build<ArgumentNamed>("port", "p");
    app.argument("port")->range(1024, 65535);
    app.build<ArgumentNamed>("host", "h");
    app.argument("host")->semantic("ipv4");
    app.build<ArgumentNamed>("len", "l");
    app.argument("len")->check([](std::string const &v) { return v.size() <= 3; }, "len3");

    REQUIRE(app.argument("port")->validate("8080") == "");
    REQUIRE(app.argument("port")->validate("80") != "");
    REQUIRE(app.argument("host")->validate("10.0.0.1") == "");
    REQUIRE(app.argument("host")->validate("10.0.0.999") != "");
    REQUIRE(app.argument("len")->validate("abc") == "");
    REQUIRE(app.argument("len")->validate("abcd") != "");
}

TEST_CASE("env: requireEnv 声明式条目与句柄稳定性（重复注册不覆盖）", "[required][env]")
{
    Context app("ut_env_spec");
    auto *p = app.requireEnv("DB_HOST", "database host");
    REQUIRE(p != nullptr);
    REQUIRE(p->isRequired() == true);
    p->semantic("hostname");
    p->defaultEnv("localhost");

    auto *q = app.requireEnv("DB_PORT", "database port");
    q->range(1, 65535);

    auto *r = app.requireEnv("DB_HOST", "another help");   // 重复注册：返回既有句柄
    REQUIRE(r == p);
    REQUIRE(r->isRequired() == true);
    REQUIRE(r->strHelp == "database host");                // 未被覆盖

    REQUIRE(app.envSpec("DB_HOST") == p);
    REQUIRE(app.envSpec("NOPE") == nullptr);

    // 校验器与默认值经句柄生效（`isEnvHit` 之类似是 Argument 的方法，此处用 mixin 查询）
    REQUIRE(p->hasValidators() == true);
    REQUIRE(p->validators().size() == 1);
    REQUIRE(p->validate("localhost") == "");
    REQUIRE(p->validate("bad host!") != "");
    REQUIRE(p->strDefault__ == "localhost");
}

TEST_CASE("validation: ValidationError 错误域注册与异常", "[validation]")
{
    auto ec = make_error_code(ValidationError_E::MISSING_OPTION);
    REQUIRE(ec.value() == static_cast<int>(ValidationError_E::MISSING_OPTION));
    REQUIRE(ec.message() == "missing required option");

    REQUIRE(make_error_code(ValidationError_E::OK).value() == 0);
    REQUIRE(make_error_code(ValidationError_E::MISSING_ARGUMENT).message() == "missing required argument");
    REQUIRE(make_error_code(ValidationError_E::MISSING_ENV).message() == "missing required environment");
    REQUIRE(make_error_code(ValidationError_E::VALIDATION_FAILED).message() == "value validation failed");
}

TEST_CASE("validation: ErrorValidation 携带描述且 what() 可读", "[validation]")
{
    ErrorValidation ev(ValidationError_E::MISSING_ARGUMENT, std::string("argument 'name'"));
    REQUIRE(ev.code() == make_error_code(ValidationError_E::MISSING_ARGUMENT));
    REQUIRE(std::string(ev.what()) == "argument 'name'");

    ErrorValidation ev2(ValidationError_E::VALIDATION_FAILED, std::string("value out of range"));
    REQUIRE(std::string(ev2.what()) == "value out of range");
}

TEST_CASE("validation: 批量管线 required/校验失败进入错误码 Errors 根", "[validation]")
{
    Context app("ut_val_e2e");
    app.addOption<std::string>("host", "host", "");
    app.option("host")->required();
    app.addOption<int>("port", "port", 0);
    app.option("port")->range(1024, 65535);
    app.build<ArgumentNamed>("name", "n");
    app.argument("name")->required();

    std::error_code ec;
    app.parse(ec, Tokens_T{ "--host", "h1", "--port", "80", "--name", "ok" }, Envs_T{});
    REQUIRE(ec.value() == static_cast<int>(ValidationError_E::VALIDATION_FAILED));
    REQUIRE(app.getLastVal() == static_cast<int>(ValidationError_E::VALIDATION_FAILED));

    // 全部合法 → 通过
    app.reset();
    app.parse(ec, Tokens_T{ "--host", "h1", "--port", "8080", "--name", "ok" }, Envs_T{});
    REQUIRE(!ec);

    // 缺必选 → MISSING_OPTION（首个缺失决定错误码）
    Context app2("ut_val_e2e2");
    app2.addOption<std::string>("host", "h", "");
    app2.option("host")->required();
    app2.parse(ec, Tokens_T{}, Envs_T{});
    REQUIRE(ec.value() == static_cast<int>(ValidationError_E::MISSING_OPTION));

    // 缺必选参数 → MISSING_ARGUMENT
    Context app3("ut_val_e2e3");
    app3.build<ArgumentNamed>("name", "n");
    app3.argument("name")->required();
    app3.parse(ec, Tokens_T{}, Envs_T{});
    REQUIRE(ec.value() == static_cast<int>(ValidationError_E::MISSING_ARGUMENT));

    // 抛异常通道
    REQUIRE_THROWS_AS(app3.parse(Tokens_T{}, Envs_T{}), ErrorValidation);
}

TEST_CASE("validation: 环境 spec 缺省补全为 MISSING_ENV", "[validation][env]")
{
    Context app("ut_env_spec_e2e");
    app.requireEnv("DB_HOST", "host")->semantic("hostname");
    app.requireEnv("DB_PORT", "port")->range(1, 65535).defaultEnv("5432");

    std::error_code ec;
    app.parse(ec, Tokens_T{}, Envs_T{});
    REQUIRE(ec.value() == static_cast<int>(ValidationError_E::MISSING_ENV));

    app.reset();
    app.parse(ec, Tokens_T{}, Envs_T{ "DB_HOST=localhost" });
    REQUIRE(!ec);
    REQUIRE(app.get_env("DB_PORT") == "5432");    // 缺省补全写入会话

    // 无效值 → VALIDATION_FAILED
    app.reset();
    app.parse(ec, Tokens_T{}, Envs_T{ "DB_HOST=bad..host" });
    REQUIRE(ec.value() == static_cast<int>(ValidationError_E::VALIDATION_FAILED));
}

TEST_CASE("validation: 流式 loop 行末校验 action 即时执行保持", "[validation][interpreter]")
{
    Context app("ut_val_stream");
    app.addOption<std::string>("host", "h", "");
    app.option("host")->required();
    app.addOption<int>("port", "p", 0);
    app.option("port")->range(1024, 65535);

    // 第一行: port=80 越界 → 行末校验 VALIDATION_FAILED → 输出 (parse error)
    // 第二行: 全部合法 → 无错误；行间 reset 保证 port 恢复默认
    std::istringstream in("--port 80\n--port 8080 --host ok\nexit\n");
    std::ostringstream out;
    std::streambuf *oldIn  = std::cin.rdbuf(in.rdbuf());
    std::streambuf *oldOut = std::cout.rdbuf(out.rdbuf());

    app.loop();

    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);

    REQUIRE(out.str().find("(parse error)") != std::string::npos);
    REQUIRE(app.getOption<std::string>("host") == "ok");
    REQUIRE(app.getOption<int>("port") == 8080);
}

TEST_CASE("defaults: defaultExpr 支持 ${VAR:-fallback} 回退与优先级", "[defaults]")
{
    Context app("ut_def_expr");
    app.addOption<int>("port", "port", 0);
    app.option("port")->defaultExpr("${CMD_DEF_PORT:-8080}");

    std::error_code ec;
    app.parse(ec, Tokens_T{}, Envs_T{ "CMD_DEF_PORT=9090" });
    REQUIRE(!ec);
    REQUIRE(app.getOption<int>("port") == 9090);          // 环境变量命中

    app.reset();
    app.parse(ec, Tokens_T{}, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app.getOption<int>("port") == 8080);          // 回退值
    // 显式 CLI 覆盖表达式
    app.parse(ec, Tokens_T{ "--port", "7000" }, Envs_T{ "CMD_DEF_PORT=9090" });
    REQUIRE(!ec);
    REQUIRE(app.getOption<int>("port") == 7000);
}

TEST_CASE("defaults: defaultFn 每次 reset 重算（loop 单行语义）", "[defaults]")
{
    Context app("ut_def_fn");
    int tick = 0;
    app.addOption<int>("n", "n", 0);
    app.option("n")->defaultFn([&] { return std::to_string(++tick); });

    std::error_code ec;
    app.parse(ec, Tokens_T{}, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app.getOption<int>("n") == 1);

    app.reset();
    app.parse(ec, Tokens_T{}, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app.getOption<int>("n") == 2);
}

TEST_CASE("defaults: Argument defaultExpr 同样生效", "[defaults]")
{
    Context app("ut_def_expr_arg");
    app.build<ArgumentNamed>("host", "h");
    app.argument("host")->defaultExpr("${CMD_DEF_HOST:-localhost}");

    std::error_code ec;
    app.parse(ec, Tokens_T{}, Envs_T{});
    REQUIRE(!ec);
    REQUIRE(app.getArgument<std::string>("host") == "localhost");

    app.reset();
    app.parse(ec, Tokens_T{}, Envs_T{ "CMD_DEF_HOST=my.host" });
    REQUIRE(!ec);
    REQUIRE(app.getArgument<std::string>("host") == "my.host");
}
