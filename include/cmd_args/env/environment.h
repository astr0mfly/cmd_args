/*
 * Copyright[2023][ValenciaFly]
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

#include <set>
#include <string>

#include "cmd_args/base/def.h"
#include "cmd_args/base/util.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    单条环境变量记录。

    - bReferenced__: 该变量被程序读取/引用过（作为默认值兜底、被 ${VAR} 展开命中等）
    - bModified__  : 值在本会话中被修改过
    - bNew__       : 变量在本会话中新建（进程环境里原本不存在）
    这三类标记用于 `-!`/`--env` 的环境展示，让用户一眼看出
    “程序关心了谁、改了谁、造了谁”——这是本库区别于其他命令行解析库的特色之一。
*/
struct environment
{
    std::string         strKey;
    mutable std::string strValue;
    mutable bool        bReferenced__ = false;
    mutable bool        bModified__   = false;
    mutable bool        bNew__        = false;

    /*
        纯键构造：用于 std::set<environment>::find/erase 等按名查找；
        值保持为空（查找临时对象只比较 strKey，operator< 只看键）
    */
    environment(std::string const &_Key)
        : strKey(_Key)
    {}

    environment(std::string _Key, std::string _Value, bool _New)
        : strKey(std::move(_Key))
        , strValue(std::move(_Value))
        , bModified__(false)
        , bNew__(_New)
    {}

    bool operator<(environment const &_Rhs) const noexcept { return strKey < _Rhs.strKey; }
};

using envs_t = std::set<environment>;

CMD_ARGS_NAMESPACE_END