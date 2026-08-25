#include "catch2/catch.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;

/* ==================== Interpreter 解释执行（基类引擎） ==================== */

TEST_CASE("interpreter: once 未解释时抛 ErrorInterp(STOPPED)", "[interpreter]")
{
    Context app("ut_once_stopped");
    app.addOption<int>("timeout", "t", 30);

    // 未 parse 直接 once()：解释器未就绪，抛 STOPPED 而非退出进程
    REQUIRE_THROWS_AS(app.once(), ErrorInterp);
}

TEST_CASE("interpreter: loop 逐行解释执行，exit 退出、行间不合并", "[interpreter]")
{
    Context app("ut_loop_interp");
    app.build<ArgumentNamed>("hello", "h");
    app.addOption<int>("timeout", "t", 30);

    // 输入流重定向模拟 REPL：两行解释 + exit 退出
    std::istringstream in("--hello a --timeout 9\n--hello b\nexit\n");
    std::ostringstream out;
    std::streambuf *oldIn  = std::cin.rdbuf(in.rdbuf());
    std::streambuf *oldOut = std::cout.rdbuf(out.rdbuf());

    app.loop();  // 逐行解释执行直到 exit

    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);

    // 两行都解释成功：无错误输出
    REQUIRE(out.str().find("(parse error)") == std::string::npos);
    // 循环结束状态 = 最后一次输入（行间 reset：hello=b，timeout 回默认）
    REQUIRE(app.getArgument<std::string>("hello") == "b");
    REQUIRE(app.getOption<int>("timeout") == 30);
}

TEST_CASE("interpreter: loop 行错误展示后继续下一行", "[interpreter]")
{
    Context app("ut_loop_err2");
    app.build<ArgumentNamed>("hello", "h");

    std::istringstream in("--nope x\n--hello ok\nexit\n");
    std::ostringstream out;
    std::streambuf *oldIn  = std::cin.rdbuf(in.rdbuf());
    std::streambuf *oldOut = std::cout.rdbuf(out.rdbuf());

    app.loop();

    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);

    // 第一行错误被展示，第二行正常解释；状态为最后一次成功输入
    REQUIRE(out.str().find("(parse error)") != std::string::npos);
    REQUIRE(app.getArgument<std::string>("hello") == "ok");
}

/* ==================== Interpreter 注入模式：直接构造 Compiler 场景外的 Interpreter + Errors ==================== */

TEST_CASE("interpreter: 继承三解析器模式逐行解释（写入 *Data）", "[interpreter]")
{
    Model model;
    Errors errors;
    Interpreter interp(&model);

    // 通过 OptionBuilder 注册选项到 Model 的 OptionData
    OptionBuilder ob(&model.opts);
    ob.addOption<int>("timeout", "t", 30);

    // 正常行：--timeout 9 → 继承 parse + virtual hook 内联触发 action → 写入 *Data
    auto ec = interp.interpret("--timeout 9", errors);
    REQUIRE(!ec);
    REQUIRE(interp.prepared());

    auto *p = model.opts.mgrValues.find("timeout");
    REQUIRE(p != nullptr);
    REQUIRE(p->strValue == "9");

    // setExitCode 生效
    interp.setExitCode(7);
    REQUIRE(interp.exitCode() == 7);

    // 错误行：--nope → ec 非零，错误栈有内容
    ec = interp.interpret("--nope x", errors);
    REQUIRE(ec);
    REQUIRE(!errors.isOk());
    REQUIRE(interp.describeError(ec, errors).find("(parse error)") != std::string::npos);

    // resetData 有效：下一次 interpret 前选项还原为默认值
    REQUIRE(p->strValue == "30");
}

/* ==================== Interpreter 流式字符泵 ==================== */

TEST_CASE("interpreter: 流式 interpret 与批量 parse 结果一致", "[interpreter]")
{
    // 批量参照
    Context app("ut_stream_batch");
    app.build<ArgumentNamed>("hello", "h");
    app.addOption<int>("timeout", "t", 30);
    std::error_code ec;
    char const *argv1[] = { "test", "--hello", "a", "--timeout", "9", nullptr };
    app.parse(ec, 5, argv1, nullptr);
    REQUIRE(!ec);
    REQUIRE(app.getArgument<std::string>("hello") == "a");
    REQUIRE(app.getOption<int>("timeout") == 9);

    // 流式：同一命令行逐字符解释
    Model model;
    Errors errors;
    Interpreter interp(&model);
    ArgumentBuilder ab(&model.args);
    ab.build<ArgumentNamed>("hello", "h");
    OptionBuilder ob(&model.opts);
    ob.addOption<int>("timeout", "t", 30);

    auto ec2 = interp.interpret("--hello a --timeout 9", errors);
    REQUIRE(!ec2);
    REQUIRE(interp.prepared());
    auto *pH = model.args.mgrName.doGet("hello");
    REQUIRE(pH != nullptr);
    REQUIRE(pH->getValue() == "a");
    auto *pT = model.opts.mgrValues.find("timeout");
    REQUIRE(pT != nullptr);
    REQUIRE(pT->strValue == "9");
}

TEST_CASE("interpreter: 流式逐 Token 立即执行 fnOnSet 且带值/聚合/错误语义正确", "[interpreter]")
{
    Model model;
    Errors errors;
    Interpreter interp(&model);

    OptionBuilder ob(&model.opts);
    std::vector<std::string> fired;
    ob.addFlag('a', "ena", "flag a");
    ob.addFlag('b', "enb", "flag b");
    ob.addOption<std::string>("step", "s", "")
        ->onSet([&](std::string const &_V) { fired.emplace_back(_V); });

    // 聚合布尔 + 带值选项混合：每个 Expression 完成即执行
    auto ec1 = interp.interpret("-ab --step one", errors);
    REQUIRE(!ec1);
    REQUIRE(fired == std::vector<std::string>{ "one" });
    REQUIRE(model.opts.mgrValues.find("ena")->strValue == "1");
    REQUIRE(model.opts.mgrValues.find("enb")->strValue == "1");

    // 行间 reset：第二次 interpret 前选项还原
    auto ec2 = interp.interpret("--step two", errors);
    REQUIRE(!ec2);
    REQUIRE(fired == std::vector<std::string>{ "one", "two" });
    REQUIRE(model.opts.mgrValues.find("ena")->strValue == "0");  // reset 后回默认

    // 内联 '=' 带值：--step=x 形态
    auto ec3 = interp.interpret("--step=x", errors);
    REQUIRE(!ec3);
    REQUIRE(fired.back() == "x");

    // 错误行：未知长选项报错且不崩溃
    auto ec4 = interp.interpret("--nope x", errors);
    REQUIRE(ec4);
    REQUIRE(!errors.isOk());

    // 值选项缺值：--step 后无更多 token → NEED_VALUE
    auto ec5 = interp.interpret("--step", errors);
    REQUIRE(ec5 == make_error_code(ParseError_E::NEED_VALUE));
}

TEST_CASE("interpreter: 未识别选项报 UNKNOWN_OPTION 且不污染方位参数校验", "[interpreter]")
{
    Model model;
    Errors errors;
    Interpreter interp(&model);
    ArgumentBuilder ab(&model.args);
    ab.build<ArgumentNamed>("hello", "h");

    auto ec1 = interp.interpret("--nope x", errors);
    REQUIRE(ec1 == make_error_code(ParseError_E::UNKNOWN_OPTION));

    // 方位参数存在时：未知 dash token 不占用方位槽位、也不被赋值
    Model model2;
    Errors errors2;
    Interpreter interp2(&model2);
    ArgumentBuilder ab2(&model2.args);
    ab2.build<ArgumentPosition>("p1", "first");
    auto ec2 = interp2.interpret("--unknown v", errors2);
    REQUIRE(ec2 == make_error_code(ParseError_E::UNKNOWN_OPTION));
    auto *p = model2.args.mgrPos.mutGetBySeq(0);
    REQUIRE(p != nullptr);
    REQUIRE_FALSE(p->hasValue());   // 未被 "--unknown" 污染
}

TEST_CASE("interpreter: 短选项带值接受 dash 值（-t=-5 与 -t -5 均与批量一致）", "[interpreter]")
{
    // 批量参照：短路径无条件接受下一个 token（option_parser.h 无 dash 检查）
    Context app1("ut_short_dash_eq");
    app1.addOption<int>('t', "timeout", "timeout", 30);
    std::error_code ecB1;
    char const *argv1[] = { "test", "-t=-5", nullptr };
    app1.parse(ecB1, 2, argv1, nullptr);
    REQUIRE(!ecB1);
    REQUIRE(app1.getOption<int>("timeout") == -5);

    Context app2("ut_short_dash_sp");
    app2.addOption<int>('t', "timeout", "timeout", 30);
    std::error_code ecB2;
    char const *argv2[] = { "test", "-t", "-5", nullptr };
    app2.parse(ecB2, 3, argv2, nullptr);
    REQUIRE(!ecB2);
    REQUIRE(app2.getOption<int>("timeout") == -5);

    // 流式：同一输入与批量结果一致（短名 't' 经 4 参 addOption 注册）
    Model model;
    Errors errors;
    Interpreter interp(&model);
    OptionBuilder ob(&model.opts);
    ob.addOption<int>('t', "timeout", "timeout", 30);

    auto ec1 = interp.interpret("-t=-5", errors);
    REQUIRE(!ec1);
    REQUIRE(model.opts.mgrValues.find("timeout")->strValue == "-5");

    auto ec2 = interp.interpret("-t -5", errors);
    REQUIRE(!ec2);
    REQUIRE(model.opts.mgrValues.find("timeout")->strValue == "-5");
}

/* ==================== 实时解析状态机：idxCursor 游标 + token 栈归约 ==================== */

TEST_CASE("interpreter: 实时解析凑满一个 Expression 立即执行且时点可观测", "[interpreter]")
{
    Model model;
    Errors errors;
    Interpreter interp(&model);
    OptionBuilder ob(&model.opts);

    std::vector<size_t> cursors;   // onSet 触发瞬间的游标快照
    std::vector<size_t> stacks;    // onSet 触发瞬间的栈深快照
    ob.addOption<std::string>("step", "s", "")
        ->onSet([&](std::string const &) {
            cursors.emplace_back(interp.getCursor());
            stacks.emplace_back(interp.getStackSize());
        });

    // "--step one"：'one' 在流结束符处终结成 token，
    // 与挂起的 "step" 凑满一个 Expression → 此刻立即执行：
    // 游标停在 10（流尾），值 token 已随归约出栈（栈深 0）
    auto ec = interp.interpret("--step one", errors);
    REQUIRE(!ec);

    REQUIRE(cursors.size() == 1);
    REQUIRE(cursors[0] == 10);
    REQUIRE(stacks[0] == 0);
    REQUIRE(model.opts.mgrValues.find("step")->strValue == "one");

    // 整行解释完：游标抵达流尾、栈已排干
    REQUIRE(interp.getCursor() == std::string("--step one").size());
    REQUIRE(interp.getStackSize() == 0);
}

TEST_CASE("interpreter: 多个 Expression 按流序逐个即时执行", "[interpreter]")
{
    Model model;
    Errors errors;
    Interpreter interp(&model);
    OptionBuilder ob(&model.opts);

    std::vector<std::string> trace;
    ob.addOption<std::string>("alpha", "a", "")
        ->onSet([&](std::string const &_V) { trace.emplace_back("alpha=" + _V); });
    ob.addOption<std::string>("beta", "b", "")
        ->onSet([&](std::string const &_V) { trace.emplace_back("beta=" + _V); });

    auto ec = interp.interpret("--beta 2 --alpha 1", errors);
    REQUIRE(!ec);
    // 栈每凑满一个 Expression 就地执行一个：顺序 = 流序，而非行尾统一提交
    REQUIRE(trace == std::vector<std::string>{ "beta=2", "alpha=1" });
    REQUIRE(interp.getCursor() == std::string("--beta 2 --alpha 1").size());
    REQUIRE(interp.getStackSize() == 0);
}

TEST_CASE("interpreter: 长选项挂起遇 dash 值当场报 NEED_VALUE 且不吞后续 token", "[interpreter]")
{
    Model model;
    Errors errors;
    Interpreter interp(&model);
    OptionBuilder ob(&model.opts);

    std::vector<std::string> trace;
    ob.addOption<std::string>("alpha", "a", "");
    ob.addOption<std::string>("mark", "m", "")
        ->onSet([&](std::string const &_V) { trace.emplace_back("mark=" + _V); });

    // "--alpha" 挂起等值，下一个 token "--mark" 以 dash 开头 → 归约当场抛 NEED_VALUE，
    // "v" 根本不会被处理（mark 回调不触发）
    auto ec = interp.interpret("--alpha --mark v", errors);
    REQUIRE(ec == make_error_code(ParseError_E::NEED_VALUE));
    REQUIRE(trace.empty());
}
