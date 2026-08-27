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

#include <string>
#include <vector>

#include "cmd_args/base/def.h"
#include "cmd_args/base/validator.h"
#include "cmd_args/error/validation_error.h"
#include "cmd_args/env/env_builder.h"
#include "cmd_args/context/model.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    校验引擎（赋值完成后、动作执行前）：三类条目统一校验。
    - checkEnvSpecs：环境 spec 缺省补全 + 必存在性 + 值校验（可变：缺省补全）
    - checkRequired：参数/选项必选命中判定（首错即返）
    - checkValues  ：参数/选项值依次过校验器（首错即返）
    - validate     ：统一入口（先 spec 后 required 再 values）
*/

namespace validate_detail {

/// @brief 校验结果：ec 非 0 表示失败；msg 为失败描述
struct Result
{
    std::error_code ec;
    std::string     msg;

    bool ok() const noexcept { return !ec; }
};

/// @brief 环境声明条目：缺省补全 + 必存在性 + 值校验（可变：补全写入会话）
inline Result checkEnvSpecs(Model &_M)
{
    std::string msg;
    _M.envs.forEachSpec([&](EnvSpec const &spec) {
        if (!msg.empty()) return;
        auto *p = _M.envs.doMutGet(spec.strKey);
        bool present = p && !p->strValue.empty();

        if (!present && !spec.strDefault__.empty()) {
            EnvBuilder(&_M.envs).setEnv(spec.strKey, spec.strDefault__);
            p = _M.envs.doMutGet(spec.strKey);
            present = p && !p->strValue.empty();
        }

        if (!present && spec.isRequired()) {
            msg = "missing required environment: " + spec.strKey;
            return;
        }
        if (present) {
            auto vmsg = spec.validate(p->strValue);
            if (!vmsg.empty()) {
                msg = "environment " + spec.strKey + ": " + vmsg;
            }
        }
    });

    if (!msg.empty()) {
        bool req = msg.find("missing required environment") == 0;
        return Result{
            make_error_code(req ? ValidationError_E::MISSING_ENV : ValidationError_E::VALIDATION_FAILED),
            std::move(msg)
        };
    }
    return Result{ make_error_code(ValidationError_E::OK), {} };
}

/// @brief 必选检查：命名/方位参数 + 值选项（首错即返）
inline Result checkRequired(Model const &_M)
{
    std::string msg;
    ValidationError_E ec = ValidationError_E::OK;

    if (msg.empty()) {
        _M.args.mgrName.doConVisit([&](Argument const &a) {
            if (msg.empty() && a.isRequired() && !a.isSatisfied()) {
                msg = "missing required argument: " + a.getName();
                ec  = ValidationError_E::MISSING_ARGUMENT;
            }
        });
    }
    if (msg.empty()) {
        _M.args.mgrPos.doConVisit([&](Argument const &a) {
            if (msg.empty() && a.isRequired() && !a.isSatisfied()) {
                msg = "missing required argument: " + a.getName();
                ec  = ValidationError_E::MISSING_ARGUMENT;
            }
        });
    }
    if (msg.empty()) {
        _M.opts.mgrValues.forEachValue([&](OptionValue const &o) {
            if (msg.empty() && o.isRequired() && !o.isSatisfied()) {
                msg = "missing required option: --" + o.strLongName;
                ec  = ValidationError_E::MISSING_OPTION;
            }
        });
    }

    if (!msg.empty()) return Result{ make_error_code(ec), std::move(msg) };
    return Result{ make_error_code(ValidationError_E::OK), {} };
}

/// @brief 值校验：命名/方位参数 + 值选项依次过校验器（首错即返）
inline Result checkValues(Model const &_M)
{
    std::string msg;

    _M.args.mgrName.doConVisit([&](Argument const &a) {
        if (!msg.empty() || !a.hasValidators() || !a.hasValue()) return;
        auto vmsg = a.validate(a.getValue());
        if (!vmsg.empty()) { msg = "argument " + a.getName() + ": " + vmsg; }
    });
    if (msg.empty()) {
        _M.args.mgrPos.doConVisit([&](Argument const &a) {
            if (!msg.empty() || !a.hasValidators() || !a.hasValue()) return;
            auto vmsg = a.validate(a.getValue());
            if (!vmsg.empty()) { msg = "argument " + a.getName() + ": " + vmsg; }
        });
    }
    if (msg.empty()) {
        _M.opts.mgrValues.forEachValue([&](OptionValue const &o) {
            if (!msg.empty() || o.strValue.empty() || !o.hasValidators()) return;
            auto vmsg = o.validate(o.strValue);
            if (!vmsg.empty()) { msg = "option --" + o.strLongName + ": " + vmsg; }
        });
    }

    if (!msg.empty()) {
        return Result{ make_error_code(ValidationError_E::VALIDATION_FAILED), std::move(msg) };
    }
    return Result{ make_error_code(ValidationError_E::OK), {} };
}

/// @brief 统一校验入口：环境 spec → 必选 → 值校验
inline Result validate(Model &_M)
{
    auto r = checkEnvSpecs(_M);
    if (!r.ok()) return r;
    r = checkRequired(_M);
    if (!r.ok()) return r;
    return checkValues(_M);
}

}  // namespace validate_detail

using validate_detail::Result;

CMD_ARGS_NAMESPACE_END
