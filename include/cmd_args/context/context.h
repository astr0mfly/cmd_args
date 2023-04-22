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
class Context :
    public Model,
    public Builder,
    public Lexer,
    public Parser,
    public Interpreter,
    private Errors
{
public:
    using Errors::dump;
    using Errors::getLast;
    using Errors::getLastMsg;
    using Errors::getLastVal;
    using Errors::isOk;
};

CMD_ARGS_NAMESPACE_END