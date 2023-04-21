#include "cmd_args/cmd_args.hpp"

int main(int _Argc, char const *_Argv[], char *_Envp[])
{
    using namespace cmd_args;

    parser      a("test_once");  // add the name of program
    char const *args[] = { "once.exe", "-!" };

    std::string env1("hello=world");
    std::string env2("foo=2");
    char       *envs[2] = { env1.data(), env2.data() };

    a.add_help_option();  // add the option of help
    a.add_env_option();   // add the option of env

    a.parse(ARRAY_SIZE(args), args, envs);  // parse args and throw

    // try to parse and return errc
    // auto r = a.tryParse(ARRAY_SIZE(args), args, envs);

    // -d, --debug support, it's mean that be true when input
    a.add(
        "debug",             // full name
        "open debug mode.",  // some help description
        'd'                  // short name
    );

    // --dummy support, it's mean that be true when input
    a.add(
        "dummy",       // full name
        "a bool flag"  // some help description
    );

    // a.add<Required>("full_name", "usage", 'f') ;
    // a.add<Required, int>("full_name", "usage", 'f') ;
    // a.add<Option, int>("full_name", "usage", 'f', default_value) ;
    // a.add<Option, int>("full_name", "usage", 'f', Action) ;
    // a.config({add<Required>("full_name", "usage", 'f'),
    //    add<Required>("full_name", "usage", 'f'), ...
    //})

    return 0;
}
