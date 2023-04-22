/*
 * Copyright [2023] [ValenciaFly]
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

#include "cmd_args/base/def.h"

#include "cmd_args/argument/argument_builder.h"

#include "model.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    构筑参数，选项，环境变量的对象
*/
class Builder : public ArgumentBuilder
{
public:
    Builder(Model *);
    ~Builder();
    // template <typename T = option>
    //  void add(NameLong_T _Long, Desc_T _Desc)
    //{
    //      if constexpr (std::is_base_of_v<Argument, T>) { argument_parser::add<T>(_Long, _Desc); }
    //      else {
    //          option_parser::add_option(' ', _Long, _Desc);
    //      }
    //  }
    //  void add(NameLong_T _Long, Desc_T _Desc, NameShort_T _Short)
    //{
    //      option_parser::add_option(_Short, _Long, _Desc);
    //  }

    //// add argument or option
    // void add(NameLong_T _Long, Desc_T _Desc, NameShort_T _Short, bool _Required) {}

    // parser &add_help_option()
    //{
    //     option_parser::add_sc_option("-?", "--help", "show this help message", [this]() {
    //         print_help();
    //     });

    //    return *this;
    //}

    // parser &add_env_option()
    //{
    //     option_parser::add_sc_option("-!", "--env", "show this env message", [this]() {
    //         print_env();
    //     });

    //    return *this;
    //}
};

Builder::Builder(Model *_Data)
    : ArgumentBuilder(&_Data->m_args__)
{}

Builder::~Builder() {}

CMD_ARGS_NAMESPACE_END