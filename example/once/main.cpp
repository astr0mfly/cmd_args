#include <source_location>

#include "cmd_args/cmd_args.hpp"

int main(int _Argc, char const *_Argv[], char *_Envp[])
{
    using namespace cmd_args;

    parser      a("test_once");  // add the name of program
    char const *args[] = { "once.exe", "-!" };

    std::string env1("hello=world");
    std::string env2("foo=2");
    char       *envs[3] = { env1.data(), env2.data(), nullptr };

    a.add_help_option();  // add the option of help
    a.add_env_option();   // add the option of env

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

    a.add<ArgumentNamed>("named", "try to add a named argument");
    a.add<position_argument>("pos", "try to add a posd argument");

    try {
        a.parse(ARRAY_SIZE(args), args, envs);  // parse args and throw
    }
    catch (cmd_args::ErrorParse &ex) {
        std::cerr << ex.what() << std::endl;
        std::cerr << a.dump() << std::endl;
        exit(-1);
    }
    catch (std::exception &ex) {
        std::cerr << ex.what() << std::endl;
        exit(-1);
    }
    catch (...) {
        auto l = std::source_location::current();
        std::cerr << "unknwon exception at " << l.file_name() << l.line() << std::endl;
        exit(-1);
    }
    // a.add<Required>("full_name", "usage", 'f') ;
    // a.add<Required, int>("full_name", "usage", 'f') ;
    // a.add<Option, int>("full_name", "usage", 'f', default_value) ;
    // a.add<Option, int>("full_name", "usage", 'f', Action) ;
    // a.config({add<Required>("full_name", "usage", 'f'),
    //    add<Required>("full_name", "usage", 'f'), ...
    //})

    return 0;
}
