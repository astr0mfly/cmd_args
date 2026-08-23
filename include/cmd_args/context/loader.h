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

/*
    加载器（占位）：负责配置文件的加载（dll 等扩展加载）。
    当前配置（YAML）走 config/config_store.h 的 read_file/write_file；
    此处保留给未来的 dll 动态补丁 / 额外源加载扩展。
*/