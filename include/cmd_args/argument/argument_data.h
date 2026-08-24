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

#include "cmd_args/base/def.h"

#include "argument.h"

CMD_ARGS_NAMESPACE_BEGIN

namespace argument_detail {

/*
    参数管理器的统一接口：命名参数管理器与方位参数管理器都实现它，
    使 Parser / schema 导出等可以多态地统一访问（usage/help/查找/遍历/推送）。
*/
class ArgumentMgrItf
{
public:
    using TaskVisit_T = std::function<void(Argument const &)>;

    /// @brief 生成参数格式摘要（usage 展示用）
    virtual Argument::Desc_T doGetUsage() const = 0;

    /// @brief 按键查找是否存在（命名参数按键名、方位参数按声明名）
    virtual bool doFind(Argument::Name_T const &_Key) const noexcept = 0;

    /// @brief 是否为空容器
    virtual bool doEmpty() const noexcept = 0;

    /// @brief 容器内参数个数
    virtual size_t doSize() const noexcept = 0;

    /// @brief 只读遍历（visit 回调，异常隔离）
    virtual void doVisit(TaskVisit_T &&_Fn) const = 0;

    /// @brief 输出帮助文本
    virtual void doGetHelp(std::ostringstream &_Out, std::size_t _Max) const = 0;

    /// @brief 只读查询参数对象（未找到返回 nullptr）
    virtual Argument const *doGet(std::string const &_Key) const = 0;

    /// @brief 推送（移入）参数对象
    virtual void doPush(Argument::ptr) = 0;
};

/*
    命名参数管理器：以名字为键（map）持有 ArgumentNamed。
*/
class NamedArgumentMgr : ArgumentMgrItf
{
public:
    /// @brief 推送参数对象（以名称为键）
    void doPush(Argument::ptr _Arg) override { mapArgs__.emplace(_Arg->getName(), std::move(_Arg)); }

    /// @brief 生成参数格式摘要（[=-name] 形式）
    auto doGetUsage() const -> Argument::Desc_T override
    {
        std::ostringstream oss;
        for (auto &e : mapArgs__) { oss << " [=" << e.second->getName() << ']'; }
        return oss.str();
    }

    /// @brief 按键查找是否存在
    bool doFind(Argument::Name_T const &_Key) const noexcept override
    {
        return mapArgs__.find(_Key) != mapArgs__.cend();
    }

    /// @brief 是否为空
    bool doEmpty() const noexcept override { return mapArgs__.empty(); }

    /// @brief 参数个数
    auto doSize() const noexcept -> size_t override { return mapArgs__.size(); }

    /// @brief 只读遍历（异常隔离）
    void doVisit(TaskVisit_T &&_Fn) const override
    try {
        for (auto &&[k, a] : mapArgs__) _Fn(*a);
    }
    catch (std::exception &ex) {
        std::cerr << __FILE__ << std::string(__FUNCTION__) << __LINE__ << "\n"
                  << "Failed to visit that " << ex.what() << std::endl;
    }
    catch (...) {
        std::cerr << __FILE__ << std::string(__FUNCTION__) << __LINE__ << "\n"
                  << "Failed to visit with unknown exception " << std::endl;
    }

    /// @brief 解析期可变遍历（env 兜底 / 展开）
    void doMutVisit(std::function<void(Argument &)> const &_Fn)
    {
        for (auto &&[k, a] : mapArgs__) { _Fn(*a); }
    }

    /// @brief 公开只读遍历（参数格式导出等外部只读消费）
    void doConVisit(std::function<void(Argument const &)> const &_Fn) const
    {
        for (auto const &[k, a] : mapArgs__) { _Fn(*a); }
    }

    /// @brief 输出帮助文本（Named arguments 块）
    void doGetHelp(std::ostringstream &_Out, std::size_t _Max) const override
    {
        if (mapArgs__.empty()) return;

        _Out << "\nNamed arguments:\n";
        _Max = 0;
        for (auto const &[k, arg] : mapArgs__) { _Max = std::max(_Max, arg->getName().length()); }

        _Max = std::max(_Max, std::size_t(25));
        for (auto const &[k, arg] : mapArgs__) {
            _Out << "  ";
            _Out << arg->getName();
            _Out << std::string(_Max - arg->getName().length(), ' ') << "(" << arg->getType()
                 << ") ";
            _Out << util::replace(arg->getHelp(), "\n", "\n" + std::string(_Max + 2, ' ')) << '\n';
        }
    }

    /// @brief 只读查询参数对象（未找到返回 nullptr）
    auto doGet(Argument::Name_T const &_Key) const -> Argument const * override
    {
        auto it = mapArgs__.find(_Key);
        if (it == mapArgs__.end()) return nullptr;

        return it->second.get();
    }

    /// @brief 解析期可变查询
    auto mutGet(Argument::Name_T const &_Key) -> Argument *
    {
        auto it = mapArgs__.find(_Key);
        if (it == mapArgs__.end()) return nullptr;

        return it->second.get();
    }

private:
    using ConArgs_T = std::map<Argument::Name_T, Argument::ptr>;
    ConArgs_T mapArgs__;  ///< 名称为键的参数容器
};

/*
    方位参数管理器：以声明序号 iSeq 为键（map）持有 ArgumentPosition；
    数量校验与"剩余 token 按序赋值"都依赖这个顺序。
*/
class PositionArgumentMgr : ArgumentMgrItf
{
public:
    /// @brief 推送参数对象（以声明序号为键）
    void doPush(Argument::ptr _Arg) override
    {
        auto seq = dynamic_cast<ArgumentPosition *>(_Arg.get())->getSeq();
        mapArgs__.emplace(seq, std::move(_Arg));
    }

    /// @brief 生成参数格式摘要（[name@seq] 形式）
    std::string doGetUsage() const override
    {
        std::ostringstream oss;
        for (auto &e : mapArgs__) { oss << " [" << e.second->getName() << '@' << e.first << ']'; }
        return oss.str();
    }

    /// @brief 按键（声明名）查找是否存在
    bool doFind(std::string const &_Key) const noexcept override
    {
        return std::find_if(
                   mapArgs__.cbegin(),
                   mapArgs__.cend(),
                   [&_Key](decltype(mapArgs__)::const_reference arg) {
                       return arg.second->getName() == _Key;
                   })
               != mapArgs__.cend();
    }

    /// @brief 是否为空
    bool doEmpty() const noexcept { return mapArgs__.empty(); }

    /// @brief 参数个数
    auto doSize() const noexcept -> size_t override { return mapArgs__.size(); }

    /// @brief 只读遍历（异常隔离）
    void doVisit(TaskVisit_T &&_Fn) const override
    try {
        for (auto &&e : mapArgs__) _Fn(*e.second);
    }
    catch (std::exception &ex) {
        std::cerr << __FILE__ << std::string(__FUNCTION__) << __LINE__ << "\n"
                  << "Failed to visit that " << ex.what() << std::endl;
    }
    catch (...) {
        std::cerr << __FILE__ << std::string(__FUNCTION__) << __LINE__ << "\n"
                  << "Failed to visit with unknown exception " << std::endl;
    }

    /// @brief 解析期可变遍历（env 兜底 / 展开）
    void doMutVisit(std::function<void(Argument &)> const &_Fn)
    {
        for (auto &&e : mapArgs__) { _Fn(*e.second); }
    }

    /// @brief 公开只读遍历（参数格式导出等外部只读消费）
    void doConVisit(std::function<void(Argument const &)> const &_Fn) const
    {
        for (auto const &e : mapArgs__) { _Fn(*e.second); }
    }

    /// @brief 输出帮助文本（Position arguments 块）
    void doGetHelp(std::ostringstream &_Out, std::size_t _Max) const override
    {
        if (mapArgs__.empty()) return;
        _Out << "\nPosition arguments:\n";
        _Max = 0;
        for (auto const &arg : mapArgs__) { _Max = std::max(_Max, arg.second->getName().length()); }
        _Max = std::max(_Max, std::size_t(25));
        for (auto const &arg : mapArgs__) {
            _Out << "  ";
            _Out << arg.second->getName();
            _Out << std::string(_Max - arg.second->getName().length(), ' ') << "("
                 << arg.second->getType() << ") ";
            _Out << util::replace(arg.second->getHelp(), "\n", "\n" + std::string(_Max + 2, ' '))
                 << '\n';
        }
    }

    /// @brief 只读查询参数对象（未找到返回 nullptr）
    auto doGet(std::string const &_Key) const -> Argument const * override
    {
        auto it = std::find_if(
            mapArgs__.begin(),
            mapArgs__.end(),
            [&](decltype(mapArgs__)::const_reference _V) { return _V.second->getName() == _Key; });
        if (it == mapArgs__.end()) return nullptr;

        return it->second.get();
    }

    /// @brief 解析期可变查询
    auto mutGet(std::string const &_Key) -> Argument *
    {
        auto it = std::find_if(
            mapArgs__.begin(),
            mapArgs__.end(),
            [&](decltype(mapArgs__)::reference _V) { return _V.second->getName() == _Key; });
        if (it == mapArgs__.end()) return nullptr;

        return it->second.get();
    }

    /// @brief 按序号取（方位参数赋值）
    auto mutGetBySeq(size_t _Seq) -> Argument *
    {
        auto it = mapArgs__.find(_Seq);
        if (it == mapArgs__.end()) return nullptr;

        return it->second.get();
    }

private:
    using ConArgs_T = std::map<size_t, Argument::ptr>;
    ConArgs_T mapArgs__;  ///< 声明序号为键的参数容器
};

/* 参数数据仓库：命名参数 + 方位参数两组管理器 */
struct ArgumentData
{
    NamedArgumentMgr    mgrName;  ///< 命名参数管理器
    PositionArgumentMgr mgrPos;   ///< 方位参数管理器
};

}  // namespace detail

using argument_detail::ArgumentData;

CMD_ARGS_NAMESPACE_END
