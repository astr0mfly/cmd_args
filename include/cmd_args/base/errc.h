#pragma once

#include <string>
#include <system_error>
#include "def.h"

CMD_ARGS_NAMESPACE_BEGIN

// 用于执行的错误码
enum class ActionError_E { SUCC = 0, NOT_FOUND, MISMATCH };

class ActionErrorCategory : public std::error_category {
public:
    ActionErrorCategory()
        : error_category() {}
    const char* name() const noexcept override {
        return "action error";
    }
    std::string message(int ev) const override {
        switch (static_cast<ActionError_E>(ev)) {
            case ActionError_E::SUCC:
                return "no error";
            case ActionError_E::NOT_FOUND:
                return "target not found";
            case ActionError_E::MISMATCH:
                return "target's type mismatch";
            default:
                return "unknown error";
        }
    }

    static ActionErrorCategory const& instance() {
        static ActionErrorCategory instance;
        return instance;
    }
};

std::error_code make_error_code(ActionError_E code) {
    return {
        static_cast<int>(code),
        ActionErrorCategory::instance(),  // 这里暂时用自带的 category
    };
}

enum class ParseCondition_E {
    OK = 0,
    UNRECOGNIZED_NAME,
    NEED_VALUE,
    MUST_BE_BOOL,
    UNEXCEPTED_BARE,
    WRONG_PREFIX
};

class ParseConditionCategory : public std::error_category {
public:
    ParseConditionCategory()
        : error_category() {}
    const char* name() const noexcept override {
        return "parse error";
    }
    std::string message(int ev) const override {
        switch (static_cast<ParseCondition_E>(ev)) {
            case ParseCondition_E::OK:
                return "no error";
            case ParseCondition_E::UNRECOGNIZED_NAME:
                return "create a libmodbus context for TCP/IPv4 failed";
            case ParseCondition_E::NEED_VALUE:
                return "The slave number is invalid";
            case ParseCondition_E::MUST_BE_BOOL:
                return "establish a Modbus connection failed";
            case ParseCondition_E::UNEXCEPTED_BARE:
                return "set timeout for response failed";
            case ParseCondition_E::WRONG_PREFIX:
                return "set timeout for response failed";
            default:
                return "unknown error";
        }
    }

    bool equivalent(std::error_code const& code, int condition) const noexcept override {
        auto const& category = std::error_code(ParseCondition_E{}).category();
        if (code.category() == category) {
            switch (static_cast<ParseCondition_E>(condition)) {
                case ParseCondition_E::OK:
                    return code == ActionError_E::OK;
            }
        }
        return false;
    }

    static ParseConditionCategory const& instance() {
        static ParseConditionCategory instance;
        return instance;
    }
};

std::error_code make_error_code(ParseCondition_E code) {
    return {
        static_cast<int>(code),
        ParseConditionCategory::instance(),  // 这里暂时用自带的 category
    };
}

enum class BuildCondition_E { OK = 0, already_exists, cannot_be_empty };

class BuildConditionCategory : public std::error_category {
public:
    BuildConditionCategory()
        : error_category() {}
    const char* name() const noexcept override {
        return "parse error";
    }
    std::string message(int ev) const override {
        switch (static_cast<BuildCondition_E>(ev)) {
            case BuildCondition_E::OK:
                return "no error";
            default:
                return "unknown error";
        }
    }

    bool equivalent(std::error_code const& code, int condition) const noexcept override {
        auto const& category = std::error_code(BuildCondition_E{}).category();
        if (code.category() == category) {
            switch (static_cast<BuildCondition_E>(condition)) {
                case BuildCondition_E::OK:
                    return code == BuildCondition_E::OK;
            }
        }
        return false;
    }

    static BuildConditionCategory const& instance() {
        static BuildConditionCategory instance;
        return instance;
    }
};

std::error_code make_error_code(BuildCondition_E code) {
    return {
        static_cast<int>(code),
        BuildConditionCategory::instance(),  // 这里暂时用自带的 category
    };
}

CMD_ARGS_NAMESPACE_END

namespace std {
template <>
struct is_error_code_enum<ActionError_E> : true_type {};

template <>
struct is_error_condition_enum<ParseCondition_E> : public true_type {};

template <>
struct is_error_condition_enum<BuildCondition_E> : public true_type {};

}  // namespace std