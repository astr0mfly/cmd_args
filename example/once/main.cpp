#include "cmd_args/cmd_args.hpp"

int main(int _Argc, char const *_Argv[], char *_Envp[])
{
    using namespace cmd_args;

    parser                   a("test_once");
    std::vector<std::string> vecArgs = { "once.exe", "-!" };
    std::vector<std::string> vecEnv;
    for (int i = 0; _Envp[i]; ++i) { vecEnv.emplace_back(_Envp[i]); }
    a.add_help_option();
    a.add_env_option();
    a.parse(vecArgs.size(), vecArgs, vecEnv);

    return 0;
}
