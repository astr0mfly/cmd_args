# cmd_args

## 来源

主用于命令行的解析库已经有很多，`cmd_args`广泛参考并借鉴了几个库的思路，尝试融入对环境变量的展示与交互。
同时想做到其他命令行解析库尚未做到的部分。

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

## 参考

[Xmake参考手册](https://xmake.io/#/getting_started)
[argpars](https://github.com/0382/util/tree/main/cpp/argparse)
[cmdline](https://github.com/tanakh/cmdline)
[Catch 使用教程](https://github.com/catchorg/Catch2/blob/master/docs/tutorial.md)