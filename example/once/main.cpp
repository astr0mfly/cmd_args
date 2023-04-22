#include <source_location>

#include "cmd_args/cmd_args.hpp"

int main(int _Argc, char const *_Argv[], char *_Envp[])
{
    using namespace cmd_args;

    char const *args[] = { "once.exe", "-!" };

    std::string env1("hello=world");
    std::string env2("foo=2");
    char       *envs[3] = { env1.data(), env2.data(), nullptr };

    Context a("sample_once");

    a.build<ArgumentNamed>("hello", "Say hello").build<ArgumentNamed>("world", "say world");
    a.builds<ArgumentNamed>({ ArgumentNamed("foo", "has foo"), ArgumentNamed("bar", "has bar") });

    return 0;
}
