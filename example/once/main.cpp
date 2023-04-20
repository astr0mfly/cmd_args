#include "cmd_args/cmd_args.hpp"

int main(int _Argc, char const *_Argv[], char *_Envp[])
{
    using namespace cmd_args;

    parser      a("test_once");
    char const *args[] = { "once.exe", "-!" };

    std::string env1("hello=world");
    std::string env2("foo=2");

    char *envs[2] = { env1.data(), env2.data() };
    a.add_help_option();
    a.add_env_option();
    a.parse(ARRAY_SIZE(args), args, envs);

    a.add("debug", 'd', "open debug mode.");

    /*
        TODO:
        parser    a{
        "test_once" ,
        add_help_option() ,
        add_env_option ,
        parse(vecArgs.size(),
        vecArgs, vecEnv)
        };
    */

    return 0;
}
