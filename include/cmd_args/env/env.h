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
#include "cmd_args/base/validator.h"
#include "cmd_args/expression/expression.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    单条环境变量记录。

    - bReferenced__: 该变量被程序读取/引用过（作为默认值兜底、被 ${VAR} 展开命中等）
    - bModified__  : 值在本会话中被修改过
    - bNew__       : 变量在本会话中新建（进程环境里原本不存在）
    这三类标记用于 `-!`/`--env` 的环境展示，让用户一眼看出
    “程序关心了谁、改了谁、造了谁”——这是本库区别于其他命令行解析库的特色之一。
*/
struct environment : public Expression
{
    std::string strKey;
    std::string strValue;
    bool        bReferenced__ = false;
    bool        bModified__   = false;
    bool        bNew__        = false;

    // 纯键构造（查找用）：kind = Env，name = Key
    environment(std::string const &_Key)
        : Expression(ExpressionKind_E::Env, _Key)
        , strKey(_Key)
    {}

    // 完整构造
    environment(std::string _Key, std::string _Value, bool _New)
        : Expression(ExpressionKind_E::Env, _Key)
        , strKey(std::move(_Key))
        , strValue(std::move(_Value))
        , bModified__(false)
        , bNew__(_New)
    {}

    /// @brief 覆盖基类 getName()：返回自有 strKey（避免双份存储）
    auto getName() const noexcept -> Name_T const & override { return strKey; }

    bool operator<(environment const &_Rhs) const noexcept { return strKey < _Rhs.strKey; }
};

/**
 * @struct EnvSpec
 * @brief 声明式环境条目：程序对某个环境变量的"期望"（必选 / 校验器 / 默认值）。
 *
 * 与 environment（运行时条目）不同：EnvSpec 是构建期声明，解析完成后由校验引擎统一处理——
 * - isRequired()：缺失（进程环境与会话均无或为空）报 MISSING_ENV
 * - validate()   ：存在时依次校验
 * - defaultEnv()：缺失时以该值补全会话（标记新建，env 导出可见） */
struct EnvSpec : public ValidatorMixin<EnvSpec>
{
    using Self_T = EnvSpec;

    std::string strKey;       ///< 环境变量键
    std::string strHelp;      ///< 帮助说明
    std::string strDefault__; ///< 缺失补全值（非空时缺失即补全会话，标 new）

    EnvSpec() = default;

    EnvSpec(std::string _Key, std::string _Help = {})
        : strKey(std::move(_Key))
        , strHelp(std::move(_Help))
    {}

    /// @brief 设置缺失补全值（左值链式）
    auto defaultEnv(std::string _Value) & -> Self_T &
    {
        strDefault__ = std::move(_Value);
        return *this;
    }

    /// @brief 设置缺失补全值（右值链式）
    auto defaultEnv(std::string _Value) && -> Self_T
    {
        strDefault__ = std::move(_Value);
        return std::move(*this);
    }
};

using envs_t = std::set<environment>;

CMD_ARGS_NAMESPACE_END
