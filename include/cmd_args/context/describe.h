/*
 * Copyright[2026][ValenciaFly]
 * Email: pengwanring@live.com
 *
 *     Licensed under the Apache License,
 *     Version 2.0(the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <sstream>
#include <string>

#include "cmd_args/base/def.h"
#include "cmd_args/env/env_parser.h"

#include "model.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace describe_detail
{

/// @brief usage 行：程序名 + 参数概览 + 选项概览
/// @param _M 底层模型
/// @param _Program 程序名（空串时回落 Model::strProgramName）
inline auto usageOf(Model const &_M, Name_T const &_Program) -> std::string
{
    std::ostringstream oss;
    oss << "usage: " << (_Program.empty() ? _M.strProgramName : _Program) << " [arguments]";
    oss << _M.args.mgrName.doGetUsage();
    oss << _M.args.mgrPos.doGetUsage();
    oss << _M.opts.usageString();
    oss << "\n";
    return oss.str();
}

/// @brief 完整帮助文本：usage + 命名参数帮助 + 方位参数帮助 + 选项帮助
inline auto helpOf(Model const &_M) -> std::string
{
    std::ostringstream oss;
    oss << usageOf(_M, _M.strProgramName);
    _M.args.mgrName.doGetHelp(oss, 0);
    _M.args.mgrPos.doGetHelp(oss, 0);
    if (!_M.opts.empty()) {
        oss << "\nOptions:\n";
        size_t maxLen = _M.opts.calcDescMaxLength();
        _M.opts.appendHelp(oss, maxLen);
    }
    oss << "\n";
    return oss.str();
}

/// @brief 环境变量快照文本（含回写标记）
inline auto envOf(Model &_M) -> std::string
{
    return EnvParser(&_M.envs).envString(_M.strProgramName);
}

}  // namespace describe_detail

using describe_detail::usageOf;  ///< 导出：usage 描述
using describe_detail::helpOf;   ///< 导出：完整帮助描述
using describe_detail::envOf;    ///< 导出：环境快照描述

CMD_ARGS_NAMESPACE_END
