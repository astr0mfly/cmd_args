# cmd_args

## 来源

主用于命令行的解析库已经有很多，`cmd_args`广泛参考并借鉴了几个库的思路，尝试融入对环境变量的展示与交互。
同时想做到其他命令行解析库尚未做到的部分。

## 语法内容

- `<>` 必选参数
- `{}` 必选参数，内部使用
- `[]` 可选参数
- `()` 参数默认值，用于`{}`中
- `|` 用于表示互斥关系，只能出现一个
- `...` 前面的内容可重复出现多次
- `&&` 连接多个命令
- `--` 长选项，可跟参数，通过 空格 或 = 指定
- `-` 短选项，可跟参数，通可选的 空格 指定
- `.` 当前文件夹路径


以`Git`的帮助信息为例

```shell
F:\code\cmd_args>git
usage: git [-v | --version] [-h | --help] [-C <path>] [-c <name>=<value>]
           [--exec-path[=<path>]] [--html-path] [--man-path] [--info-path]
           [-p | --paginate | -P | --no-pager] [--no-replace-objects] [--bare]
           [--git-dir=<path>] [--work-tree=<path>] [--namespace=<name>]
           [--super-prefix=<path>] [--config-env=<name>=<envvar>]
           <command> [<args>]
```

- `[-v | --version]` 表示 短参数名`-v`和长参数名`--version` 都能生效，且可选
- `[-C <path>]`  表示 在`-C` 后面必须跟随一个参数
- `-c <name>=<value>` 表示 在`-c` 后面必须跟随一个参数对


## 内容

根据个人经验理解，一个合格的项目框架需要有

1. `asset` 主要用于放置各种参考文件
2. `build` 主要存放编译时产生的中间文件
3. `include` 主要存放当前项目的整体头文件
4. `deps` 主要存放当前项目的第三方依赖
5. `dfx`    主要存放当前项目的bench、tests相关
    - `bench` 主要存放bench_mark相关的文件
    - `tests` 主要存放tests相关的文件
    - `meta` 对编译期类型做单元测试
    - `internal` dfx 构建所需要的相关头文件
6.  `scripts` 主要存放当前项目启动时可能需要的脚本

## 核心对象

1. `builder`  负责构建`argument`、`option`、`env`等选项
2. `lexer` 负责解析命令行输入，是否符合词法规则
3. `parser` 负责解析`tokens`，是否满足对象需要
4. `interpreter` 负责运行`actions`, 常驻后台时运行命令
5. `loader` 负责加载脚本等配置

## 特色

- 基于`cpp`的跨平台项目工程框架
- 优化了各个模式下的编译选项
- 支持性能`bench_mark`、功能`unit_test`

## 展望
- 支持`error_code`来进行错误解析，支持`exit`退出或者`exception`退出
- 支持`once`模式运行命令，命令行解析完毕之后退出进程
- 支持`loop`模式运行命令，持续执行对应的命令
- 支持`env`环境变量交互
- 支持`xmake`编译
- 支持配置文件(`json`)加载，动态化选项
- 支持`option with action`, 使用`lua` `luajit`来解释运行，也可以尝试考虑加入编译。
- 支持`dll`动态加载补丁，运行期间可以直接当作`action`使用
- 支持异常捕获规范，用airbag， throw_with_nest()...

## 编译步骤

```shell
# 进入build文件夹
cd build

# 展开编译
build.cmd

# 删除临时文件
clean.cmd
```

## 参考

[Xmake参考手册](https://xmake.io/#/getting_started)

[argpars](https://github.com/0382/util/tree/main/cpp/argparse)

[cmdline](https://github.com/tanakh/cmdline)

[CLI11](https://github.com/CLIUtils/CLI11)

[lyra](https://github.com/bfgroup/Lyra)

[Clara](https://github.com/catchorg/Clara)

[Catch 使用教程](https://github.com/catchorg/Catch2/blob/master/docs/tutorial.md)