/*
 * Copyright [2023] [ValenciaFly] <pengwanring@live.com>
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

#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "cmd_args/context/model.h"

#include "cmd_args/env/env_builder.h"

#include "cmd_args/error/errors.h"

#include "config/parameter.h"
#include "config/yaml.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    三类外界环境的转换桥接（config_store）：
    - 参数格式 schema / 参数快照 snapshot 的提取与回填
    - Model ↔ YAML 配置互转（save_config / load_config）
    - 环境变量 ↔ YAML 定向互转（env ↔ yaml）

    YAML 采用扁平点号分区键，避免命名参数与选项同名冲突：
    arguments.xxx / positionals.xxx / options.xxx / flags.xxx / environment.XXX

    优先级链：CLI 显式值 > 配置文件(YAML) > 环境变量 > 内置默认值。
    配置导入走 setExternal（记录 bExternal__），env 兜底跳过此类值；
    CLI 后续出现仍可无条件覆盖（CLI 赋值路径直接 set）。
*/

namespace config {

/* ---------- 参数格式 / 快照 ---------- */

parameter_schema schema_of(Model const &_M)
{
    parameter_schema sch;
    sch.program = _M.m_strProgramName__;

    _M.m_args__.mgrName.doConVisit([&](Argument const &a) {
        parameter_entry e;
        e.name         = a.getName();
        e.kind         = "named";
        e.type         = a.getType();
        e.value        = a.getValue();
        e.env          = a.getEnvName();
        e.external_set = a.isExternal();
        sch.entries.emplace_back(std::move(e));
    });

    _M.m_args__.mgrPos.doConVisit([&](Argument const &a) {
        parameter_entry e;
        e.name         = a.getName();
        e.kind         = "positional";
        e.type         = a.getType();
        e.value        = a.getValue();
        e.env          = a.getEnvName();
        e.external_set = a.isExternal();
        sch.entries.emplace_back(std::move(e));
    });

    _M.m_opts__.mgrValues.forEachValue([&](OptionValue const &o) {
        if (o.bBuiltin__) return;  // 内置交互选项不参与参数格式导出
        parameter_entry e;
        e.name         = o.strLongName;
        e.kind         = (o.strType == "bool") ? "flag" : "option";
        e.type         = o.strType;
        e.value        = o.strValue;
        e.env          = o.strEnvName__;
        e.explicit_set = o.bExplicit__;
        e.external_set = o.bExternal__;
        sch.entries.emplace_back(std::move(e));
    });

    _M.m_opts__.mgrActions.forEachAction([&](OptionAction const &a) {
        parameter_entry e;
        e.name = a.strLongName;
        e.kind = "short_circuit";
        e.type = "";
        sch.entries.emplace_back(std::move(e));
    });

    return sch;
}

/* 本程序相关环境条目：store 中被引用/修改/新建的 ∪ schema 中 .env() 绑定的键 */
static std::vector<std::pair<std::string, std::string>> __relevant_envs(Model const &_M)
{
    std::vector<std::pair<std::string, std::string>> out;
    std::set<std::string>                            seen;

    _M.m_envs__.doVisit([&](environment const &e) {
        if (e.bReferenced__ || e.bModified__ || e.bNew__) {
            out.emplace_back(e.strKey, e.strValue);
            seen.insert(e.strKey);
        }
    });

    auto sch = schema_of(_M);
    for (auto const &e : sch.entries) {
        if (e.env.empty() || seen.count(e.env)) continue;
        auto const *p = _M.m_envs__.doGet(e.env);
        if (!p) continue;  // 绑定键在当前 store 中不存在 → 不导出（避免导入时造出空变量）
        out.emplace_back(e.env, p->strValue);
        seen.insert(e.env);
    }
    return out;
}

parameter_snapshot snapshot_of(Model const &_M)
{
    parameter_snapshot snap;
    auto               sch = schema_of(_M);
    snap.values            = std::move(sch.entries);  // 值与 schema 对齐
    snap.envs              = __relevant_envs(_M);
    return snap;
}

/* 快照回填：参数走 setExternal，环境走 set_env（不写回 OS） */
void apply_snapshot(Model &_M, parameter_snapshot const &_Snap)
{
    for (auto const &e : _Snap.values) {
        if (e.kind == "named") {
            auto *a = _M.m_args__.mgrName.mutGet(e.name);
            if (a) a->setExternal(e.value);
        }
        else if (e.kind == "positional") {
            auto *a = _M.m_args__.mgrPos.mutGet(e.name);
            if (a) a->setExternal(e.value);
        }
        else if (e.kind == "option" || e.kind == "flag") {
            auto *o = _M.m_opts__.mgrValues.mutFind(e.name);
            if (o) o->setExternal(e.value);
        }
    }
    for (auto const &[k, v] : _Snap.envs) { EnvBuilder(&_M.m_envs__).setEnv(k, v); }
}

/* ---------- YAML 互转 ---------- */

static std::string __yamlSection(parameter_entry const &_E)
{
    if (_E.kind == "named") return "arguments." + _E.name;
    if (_E.kind == "positional") return "positionals." + _E.name;
    if (_E.kind == "flag") return "flags." + _E.name;
    if (_E.kind == "option") return "options." + _E.name;
    return {};  // short_circuit 不入配置
}

std::string to_yaml(Model const &_M)
{
    yaml_doc doc;

    auto sch = schema_of(_M);
    for (auto const &e : sch.entries) {
        std::string key = __yamlSection(e);
        if (!key.empty()) doc.set(std::move(key), e.value);
    }
    for (auto const &[k, v] : __relevant_envs(_M)) { doc.set("environment." + k, v); }

    std::string head = "# cmd_args config for: "
                       + (sch.program.empty() ? std::string("unknown") : sch.program)
                       + "\n# format: yaml v1\n";
    return head + doc.dump();
}

std::string env_to_yaml(Model const &_M)
{
    yaml_doc doc;
    for (auto const &[k, v] : __relevant_envs(_M)) { doc.set("environment." + k, v); }
    return doc.dump();
}

static void __warn(Errors *_W, std::string const &_Msg)
{
    if (_W) _W->setMsg(_Msg);
}

/* 按 schema 类型校验字符串可转换性；返回 false 表示类型不匹配 */
static bool __typeOk(std::string const &_Type, std::string const &_Value)
{
    if (_Type.empty() || _Type == "string") return true;
    if (_Type == "bool") {
        return _Value == "true" || _Value == "false" || _Value == "TRUE" || _Value == "FALSE"
               || _Value == "yes" || _Value == "no" || _Value == "on" || _Value == "off"
               || _Value == "1" || _Value == "0";
    }
    if (_Type == "int" || _Type == "int64_t" || _Type == "size_t"
        || _Type == "unsigned int") {
        std::istringstream is(_Value);
        long long          v{};
        is >> v;
        return !is.fail();
    }
    if (_Type == "double" || _Type == "float") {
        std::istringstream is(_Value);
        double             v{};
        is >> v;
        return !is.fail();
    }
    if (_Type == "char") { return _Value.size() == 1; }
    return true;  // 未知类型放行
}

/* YAML 文本 → 会话（environment 分区 → set_env；参数分区 → setExternal） */
bool from_yaml(Model &_M, std::string const &_Text, Errors *_Warnings = nullptr)
{
    std::string err;
    auto        doc = yaml_doc::parse(_Text, &err);
    if (!doc) {
        __warn(_Warnings, err);
        return false;
    }

    for (auto const &[k, v] : doc->entries()) {
        if (util::starts_with(k, "environment.")) {
            EnvBuilder(&_M.m_envs__).setEnv(k.substr(12), v);
            continue;
        }

        std::string name;
        bool        found = false;

        if (util::starts_with(k, "arguments.")) {
            name  = k.substr(10);
            auto *a = _M.m_args__.mgrName.mutGet(name);
            if (!a) { __warn(_Warnings, "config: unknown named argument '" + name + "'"); continue; }
            if (!__typeOk(a->getType(), v)) {
                __warn(_Warnings, "config: type mismatch for '" + k + "', ignored"); continue;
            }
            if (a->hasValue()) continue;  // CLI 已赋值 → 跳过配置覆盖（CLI > 配置）
            a->setExternal(v);
            found = true;
        }
        else if (util::starts_with(k, "positionals.")) {
            name  = k.substr(12);
            auto *a = _M.m_args__.mgrPos.mutGet(name);
            if (!a) { __warn(_Warnings, "config: unknown position argument '" + name + "'"); continue; }
            if (!__typeOk(a->getType(), v)) {
                __warn(_Warnings, "config: type mismatch for '" + k + "', ignored"); continue;
            }
            if (a->hasValue()) continue;  // CLI 已赋值 → 跳过配置覆盖
            a->setExternal(v);
            found = true;
        }
        else if (util::starts_with(k, "options.")) {
            name  = k.substr(8);
            auto *o = _M.m_opts__.mgrValues.mutFind(name);
            if (!o) { __warn(_Warnings, "config: unknown option '" + name + "'"); continue; }
            if (!__typeOk(o->strType, v)) {
                __warn(_Warnings, "config: type mismatch for '" + k + "', ignored"); continue;
            }
            if (o->bExplicit__) continue;  // CLI 已赋值 → 跳过配置覆盖
            o->setExternal(v);
            found = true;
        }
        else if (util::starts_with(k, "flags.")) {
            name  = k.substr(6);
            auto *o = _M.m_opts__.mgrValues.mutFind(name);
            if (!o) { __warn(_Warnings, "config: unknown flag '" + name + "'"); continue; }
            if (o->bExplicit__) continue;  // CLI 已赋值 → 跳过配置覆盖
            o->setExternal(v);
            found = true;
        }

        if (!found) { __warn(_Warnings, "config: unknown key '" + k + "', ignored"); }
    }
    return true;
}

/* 仅 environment 分区 → set_env（其余分区忽略） */
bool yaml_to_env(Model &_M, std::string const &_Text, Errors *_Warnings = nullptr)
{
    std::string err;
    auto        doc = yaml_doc::parse(_Text, &err);
    if (!doc) {
        __warn(_Warnings, err);
        return false;
    }
    for (auto const &[k, v] : doc->entries()) {
        if (util::starts_with(k, "environment.")) { EnvBuilder(&_M.m_envs__).setEnv(k.substr(12), v); }
    }
    return true;
}

/* ---------- 文件读写 ---------- */

std::optional<std::string> read_file(std::string const &_Path)
{
    std::ifstream ifs(_Path, std::ios::binary);
    if (!ifs) return std::nullopt;
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

bool write_file(std::string const &_Path, std::string const &_Text)
{
    std::ofstream ofs(_Path, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    ofs << _Text;
    return static_cast<bool>(ofs);
}

}  // namespace config

CMD_ARGS_NAMESPACE_END