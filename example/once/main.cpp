#include <source_location>

#include "cmd_args/cmd_args.hpp"

int main(int _Argc, char const *_Argv[], char *_Envp[])
{
    using namespace cmd_args;

    char const *args[] = { "once.exe", "-!" };

    std::string env1("hello=world");
    std::string env2("foo=2");
    char       *envs[3] = { env1.data(), env2.data(), nullptr };

    // a.add<Required>("full_name", "usage", 'f') ;
    // a.add<Required, int>("full_name", "usage", 'f') ;
    // a.add<Option, int>("full_name", "usage", 'f', default_value) ;
    // a.add<Option, int>("full_name", "usage", 'f', Action) ;
    // a.config({add<Required>("full_name", "usage", 'f'),
    //    add<Required>("full_name", "usage", 'f'), ...
    //})

    return 0;
}
