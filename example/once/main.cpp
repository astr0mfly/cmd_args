#include <source_location>

#include "cmd_args/cmd_args.hpp"
#include "cmd_args/error/exception_handler.h"

static exception_init sg_initor;

int main(int _Argc, char const *_Argv[], char *_Envp[])
{
    using namespace cmd_args;

    (void)sg_initor;
    char const *args[] = { "once.exe", "-!" };

    std::string env1("hello=world");
    std::string env2("foo=2");
    char       *envs[3] = { env1.data(), env2.data(), nullptr };

    Context a("sample_once");

    a.build<ArgumentNamed>("hello", "Say hello").build<ArgumentNamed>("world", "say world");
    a.build<Group<ArgumentNamed>>(
        { ArgumentNamed("foo", "has foo"), ArgumentNamed("foo", "has foo") });
    a.build()(ArgumentNamed())(Argument().name("a").help(" has a "));

    a.parse(ARRAY_SIZE(args), args, envs);
    // TODO Lexer and tokens of parser and interpreter

    return 0;
}
