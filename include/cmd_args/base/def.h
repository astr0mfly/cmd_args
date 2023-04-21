#pragma once

#define CMD_ARGS_NAMESPACE_BEGIN namespace cmd_args {
#define CMD_ARGS_NAMESPACE_END   }  // end of namespace cmd_args
#define CMD_ARGS_NAMESPACE(_X)   cmd_args::##_X

namespace detail {
template <typename T, unsigned int N>
char (&fakeArraySize(T (&)[N]))[N];
}

#define ARRAY_SIZE(_A) sizeof(::detail::fakeArraySize(_A))
#define ENUM_NAME(_E)  #_E
