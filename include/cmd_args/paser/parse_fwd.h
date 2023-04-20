#pragma once

#include <list>
#include <string>
#include <vector>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

class Errors;

using Name_T      = std::string;
using NameShort_T = char;
using NameLong_T  = std::string;
using Desc_T      = std::string;
using Tokens_T    = std::list<std::string>;
using Envs_T      = std::vector<std::string>;

CMD_ARGS_NAMESPACE_END