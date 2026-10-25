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

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "cmd_args/base/def.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    Property：参数值的"属性"载体，也是响应式数据的读写拦截点。
    - 值 + 默认值：值的唯一存储；默认值可静态给出，或由函数惰性求值
    - 写钩子 FnWrite_T：赋值准入。返回非空消息即拒绝——不写值、不涨版本、
      不触发变更回调，并把消息留存到 lastError()
    - 读钩子 FnRead_T：取值拦截，按注册顺序链式变换返回值（不改变存储值）
    - 变更回调 FnChange_T：值实际变化后触发 (old, new)
    - 版本号 version()：值修订号，仅当存储值真的变化时自增（可作缓存失效依据）

    约束：
    - T 需可默认构造、可拷贝、支持 ==（本项目参数值统一为 std::string）
    - 读钩子与变更回调都不得抛异常；钩子不得对同一 Property 重入写
    - 非线程安全；拷贝时钩子一并拷贝（钩子若捕获原对象 this 会指向旧实例，
      应改为捕获外部状态）
*/
namespace property_detail {

/// @brief 属性模板：值 + 默认值 + 写/读/变更钩子 + 修订号
/// @tparam T 值类型，需可默认构造、可拷贝、支持 ==
template <typename T>
class Property
{
public:
    using Self_T      = Property<T>;                                 ///< 自身类型（链式返回）
    using Value_T     = T;                                           ///< 值类型
    using FnWrite_T   = std::function<std::string(Value_T &)>;       ///< 写钩子：返回空串接受，非空串拒绝
    using FnRead_T    = std::function<Value_T(Value_T const &)>;     ///< 读钩子：取值变换，不得抛异常
    using FnChange_T  = std::function<void(Value_T const &, Value_T const &)>;  ///< 变更回调 (old, new)，不得抛异常
    using FnDefault_T = std::function<Value_T()>;                    ///< 默认值惰性求值函数

    /// @brief 构造：初始值同时成为默认值（bSet__ 为 false，表示"未显式赋值"）
    explicit Property(Value_T _Initial = {})
        : tValue__(std::move(_Initial))
        , tDefault__(tValue__)
    {
    }

    /* ==================== 赋值与取值 ==================== */

    /// @brief 赋值（经写钩子准入）：成功 true；被拒绝时 false 且值/版本/变更回调均不动
    bool set(Value_T _Value)
    {
        for (auto const &fnWrite : vecWrite__) {
            std::string strMsg = fnWrite(_Value);
            if (!strMsg.empty()) {
                strLastError__ = std::move(strMsg);
                return false;
            }
        }
        strLastError__.clear();
        __apply(std::move(_Value), true);
        return true;
    }

    /// @brief 赋值旁路：跳过写钩子（供配置/快照/reset 等受控来源使用），其余语义同 set 成功路径
    auto setRaw(Value_T _Value) -> Self_T &
    {
        __apply(std::move(_Value), true);
        return *this;
    }

    /// @brief 取值（经读钩子链）：不改变存储值、不涨版本
    auto get() const -> Value_T
    {
        Value_T value = tValue__;
        for (auto const &fnRead : vecRead__) {
            value = fnRead(value);
        }
        return value;
    }

    /// @brief 取原始值：不经读钩子，无副作用
    auto raw() const noexcept -> Value_T const & { return tValue__; }

    /// @brief 是否被显式赋值过（reset 后回到 false）
    bool isSet() const noexcept { return bSet__; }

    /// @brief 值修订号：仅当存储值实际变化时自增
    auto version() const noexcept -> std::uint64_t { return uVersion__; }

    /// @brief 最近一次写钩子拒绝的消息（空串表示无错误）
    auto lastError() const noexcept -> std::string const & { return strLastError__; }

    /// @brief 清空最近错误消息
    auto clearError() noexcept -> Self_T &
    {
        strLastError__.clear();
        return *this;
    }

    /* ==================== 默认值 ==================== */

    /// @brief 只读默认值
    auto defaultValue() const noexcept -> Value_T const & { return tDefault__; }

    /// @brief 设置默认值：未显式赋值时同步当前值（守护 "读默认值" 语义）
    auto defaultValue(Value_T _Value) -> Self_T &
    {
        tDefault__ = std::move(_Value);
        if (!bSet__) { __apply(tDefault__, false); }
        return *this;
    }

    /// @brief 设置默认值求值函数：保存后立即求值一次
    auto defaultFn(FnDefault_T _Fn) -> Self_T &
    {
        fnDefault__ = std::move(_Fn);
        if (fnDefault__) {
            tDefault__ = fnDefault__();
            if (!bSet__) { __apply(tDefault__, false); }
        }
        return *this;
    }

    /// @brief 清空默认值求值函数（保留当前默认值）
    auto clearDefaultFn() noexcept -> Self_T &
    {
        fnDefault__ = {};
        return *this;
    }

    /// @brief 还原默认值：重新求值默认值 → 同步当前值 → 清 bSet__/错误
    auto reset() -> Self_T &
    {
        if (fnDefault__) { tDefault__ = fnDefault__(); }
        strLastError__.clear();
        __apply(tDefault__, false);
        bSet__ = false;
        return *this;
    }

    /* ==================== 钩子注册（链式，& / && 双版本） ==================== */

    /// @brief 注册写钩子（准入检查/规范化）
    auto onWrite(FnWrite_T _Fn) & -> Self_T &
    {
        vecWrite__.emplace_back(std::move(_Fn));
        return *this;
    }
    auto onWrite(FnWrite_T _Fn) && -> Self_T
    {
        vecWrite__.emplace_back(std::move(_Fn));
        return std::move(*this);
    }

    /// @brief 注册读钩子（默认值/变换拦截）
    auto onRead(FnRead_T _Fn) & -> Self_T &
    {
        vecRead__.emplace_back(std::move(_Fn));
        return *this;
    }
    auto onRead(FnRead_T _Fn) && -> Self_T
    {
        vecRead__.emplace_back(std::move(_Fn));
        return std::move(*this);
    }

    /// @brief 注册变更回调（仅值实际变化时触发）
    auto onChange(FnChange_T _Fn) & -> Self_T &
    {
        vecChange__.emplace_back(std::move(_Fn));
        return *this;
    }
    auto onChange(FnChange_T _Fn) && -> Self_T
    {
        vecChange__.emplace_back(std::move(_Fn));
        return std::move(*this);
    }

private:
    /// @brief 写值核心：值变化时换值 + 涨版本 + 触发变更回调；_MarkSet 决定是否标记显式赋值
    void __apply(Value_T _Value, bool _MarkSet)
    {
        if (tValue__ != _Value) {
            Value_T tOld = tValue__;
            tValue__     = std::move(_Value);
            ++uVersion__;
            for (auto const &fnChange : vecChange__) {
                fnChange(tOld, tValue__);
            }
        }
        if (_MarkSet) { bSet__ = true; }
    }

    Value_T                 tValue__;         ///< 当前值（唯一存储）
    Value_T                 tDefault__;       ///< 默认值
    FnDefault_T             fnDefault__;      ///< 默认值惰性求值函数
    bool                    bSet__    = false;///< 是否被显式赋值
    std::uint64_t           uVersion__ = 0;   ///< 值修订号（仅值变化时自增）
    std::string             strLastError__;   ///< 最近一次写拒绝消息
    std::vector<FnWrite_T>  vecWrite__;       ///< 写钩子链
    std::vector<FnRead_T>   vecRead__;        ///< 读钩子链
    std::vector<FnChange_T> vecChange__;      ///< 变更回调链
};

}  // namespace property_detail

using property_detail::Property;

CMD_ARGS_NAMESPACE_END
