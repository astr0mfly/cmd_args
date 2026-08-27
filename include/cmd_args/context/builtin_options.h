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

#include <iostream>
#include <string>

#include "cmd_args/base/util.h"
#include "cmd_args/config/config_store.h"
#include "cmd_args/env/env_builder.h"
#include "cmd_args/env/env_parser.h"
#include "cmd_args/option/option_builder.h"

#include "describe.h"
#include "model.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace builtin_detail
{

/// @brief 注册内置选项（--help/-?、--env/-!、--env-apply、--env-set、--env-unset、--load-config、--save-config）
/// @param _M 底层模型
/// @note 幂等：未开启内置选项或已注册过则直接返回（懒注册一次）
inline void registerBuiltinOptions(Model *_M)
{
    if (!_M->bBuiltinOptions || _M->bBuiltinsRegistered) return;
    _M->bBuiltinsRegistered = true;

    Model        *m = _M;
    OptionBuilder ob(&_M->opts);
    ob.addShortCircuit('?', "help", "show this help message", [m] {
        std::cout << describe_detail::helpOf(*m);
    });
    ob.addShortCircuit('!', "env", "show environments of this program", [m] {
        std::cout << describe_detail::envOf(*m);
    });
    ob.addShortCircuit('\0', "env-apply", "write changed environments back to the OS", [m] {
        auto applied = EnvParser(&m->envs).apply();
        for (auto const &a : applied) { std::cout << "applied " << a << "\n"; }
        std::cout << describe_detail::envOf(*m);
    });
    ob.addOption<std::string>("env-set", "set an environment variable, KEY=VALUE", "")
        ->bBuiltin__ = true;
    _M->opts.mgrValues.mutFind("env-set")->onSet([m](std::string const &_V) {
        EnvBuilder b(&m->envs);
        auto eq = _V.find('=');
        if (eq == std::string::npos) {
            b.setEnv(_V, "");
        }
        else {
            b.setEnv(_V.substr(0, eq), _V.substr(eq + 1));
        }
    });
    ob.addOption<std::string>("env-unset", "unset an environment variable, KEY", "")
        ->bBuiltin__ = true;
    _M->opts.mgrValues.mutFind("env-unset")->onSet(
        [m](std::string const &_V) { EnvBuilder(&m->envs).unsetEnv(_V); });

    ob.addOption<std::string>(
          "load-config", "load a yaml config file before later args, PATH", "")
        ->bBuiltin__ = true;
    _M->opts.mgrValues.mutFind("load-config")->onSet(
        [m](std::string const &_Path) {
            auto text = config::read_file(_Path);
            if (!text) {
                std::cerr << "(load-config) cannot read file: " << _Path << "\n";
                return;
            }
            config::from_yaml(*m, *text);
        });
    ob.addOption<std::string>(
          "save-config", "write the resolved parameter/env state to a yaml file, PATH", "")
        ->bBuiltin__ = true;
    _M->opts.mgrValues.mutFind("save-config")->onSet(
        [m](std::string const &_Path) { m->strSaveConfigPath = _Path; });
}

/// @brief 还原 ArgumentData/OptionData 到内置默认值（Env 会话态保留）
/// @param _M 底层模型
inline void resetModelData(Model *_M)
{
    _M->args.mgrName.doMutVisit([](Argument &a) { a.reset(); });
    _M->args.mgrPos.doMutVisit([](Argument &a) { a.reset(); });
    _M->opts.mgrValues.forEachValue([](OptionValue &o) { o.reset(); });
    _M->envs.doClear();  ///< 清空会话环境条目（EnvSpec 保留）
}

}  // namespace builtin_detail

using builtin_detail::registerBuiltinOptions;  ///< 导出：内置选项懒注册
using builtin_detail::resetModelData;          ///< 导出：参数/选项数据还原

CMD_ARGS_NAMESPACE_END
