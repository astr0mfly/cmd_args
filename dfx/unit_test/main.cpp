/*
    unit_test 入口：仅提供 Catch2 的 main（CATCH_CONFIG_MAIN）。
    测试用例按库子模块拆分在同目录的 test_*.cpp 中：
      test_lexer.cpp       词法（Lexer / Scanner 流式）
      test_parser.cpp      三解析器 token→Expression 批量流程
      test_builder.cpp     构筑重名检查
      test_context.cpp     Context 解析 / 错误码 / 短回路 / loop / 容器化
      test_env.cpp         环境变量三件套 + Context 层联动
      test_config.cpp      YAML 配置 / schema / 往返 / load-save e2e
      test_option.cpp      选项 action（system_call）
      test_interpreter.cpp 流式解释引擎
      test_expression.cpp  Expression 类层次
      test_model.cpp       Model 查询 + Executor 批量执行
*/
#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#include "cmd_args/cmd_args.hpp"

using namespace cmd_args;
