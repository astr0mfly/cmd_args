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

#include "cmd_args/error/errors.h"

#include "builder.h"
#include "interpreter.h"
#include "lexer.h"
#include "model.h"
#include "parser.h"

CMD_ARGS_NAMESPACE_BEGIN

/*
    核心服务对象
*/
class Context : public Lexer, public Interpreter, private Errors
{
public:
    struct BuildHelper
    {
        Model *pCon = nullptr;

        BuildHelper(Model *_Data)
            : pCon(_Data)
        {}

        BuildHelper &operator()(ArgumentNamed &&_Arg)
        {
            Builder(pCon).build(std::move(_Arg));
            return *this;
        }
        BuildHelper &operator()(ArgumentNamed const &_Arg) { Builder(pCon).build(_Arg); }
    };
    Context(std::string _Name)
        : m_strPrjName__(_Name)
    {}

    template <class Select_T, typename... Args_T>
    Context &build(Args_T &&..._Args)
    {
        Builder(&m_instData__).build<Select_T>(std::forward<Args_T>(_Args)...);

        return *this;
    }

    template <class G, typename T = typename G::Element_T>
    Context &build(std::initializer_list<T> _Args)
    {
        Builder(&m_instData__).build(std::move(_Args));

        return *this;
    }

    template <class T>
    Context &builds(Group<T> &&_Builds)
    {
        Builder(&m_instData__).build<T>(std::move(_Builds));

        return *this;
    }

    BuildHelper build() { return { &this->m_instData__ }; }

    Context &parse(int _Argc, char const *_Argv[], char *_Envp[])
    {
        // Lexer

        // parser
        return *this;
    }

    Context &parse(std::error_code _Ec, int _Argc, char const *_Argv[], char *_Envp[]) noexcept
    {
        try {
            return parse(_Argc, _Argv, _Envp);
        }
        catch (ErrorParse const &e) {
            _Ec = e.code();
            // TODO  print stack exception
        }
        catch (std::exception const &ex) {
            ( void )ex;
        }

        return *this;
    }

    // run once and exit(0)
    Context &once()
    {
        // TODO interpreter
        return *this;
    }

    // call loop and util input exit
    Context &loop()
    {
        // TODO interpreter and set error last
        return *this;
    }

    using Errors::dump;
    using Errors::getLast;
    using Errors::getLastMsg;
    using Errors::getLastVal;
    using Errors::isOk;

private:
    std::string m_strPrjName__;
    Model       m_instData__;
};

CMD_ARGS_NAMESPACE_END