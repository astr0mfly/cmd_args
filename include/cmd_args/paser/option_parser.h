#pragma once

#include "cmd_args/option/option.h"

CMD_ARGS_NAMESPACE_BEGIN

class option_parser {
public:
    void parse(std::vector<std::string>& _Tokens) {
        for (auto& t : _Tokens) {
            if (t.find_first_not_of('-') == 2) {
                auto const* pSc = short_circuit_options.doGet(t);
                if (pSc) {
                    dynamic_cast<option_action const*>(pSc)->callback();
                    std::exit(0);
                }
            } else if (t.find_first_not_of('-') == 1) {
                for (decltype(t.size()) i = 1; i < t.size(); ++i) {
                    auto const* pSc = short_circuit_options.doGet(t[i]);
                    if (pSc) {
                        dynamic_cast<option_action const*>(pSc)->callback();
                        std::exit(0);
                    }
                }
            }
        }

        for (auto it{_Tokens.begin()}; it != _Tokens.end();) {
            if (it->find_first_not_of('-') == 2) {
                auto const* p = options.doGet(*it);
                if (p) {
                    it = _Tokens.erase(it);
                    auto* pValue = dynamic_cast<option_value const*>(p);
                    if (pValue->type == "bool") {
                        pValue->value = "1";
                        continue;
                    } else if (it == _Tokens.cend()) {
                        std::cerr << "(parse error) option " << pValue->short_name << " "
                                  << pValue->long_name << " should have value" << std::endl;
                        std::exit(-1);
                    } else {
                        pValue->value = *it;
                        it = _Tokens.erase(it);
                        continue;
                    }
                }
            } else if (it->find_first_not_of('-') == 1) {
                for (decltype(it->size()) i = 1; i < it->size(); ++i) {
                    auto const* p = options.doGet(it->at(i));
                    if (p) {
                        auto* pValue = dynamic_cast<option_value const*>(p);
                        if (pValue->type == "bool") {
                            pValue->value = "1";
                            continue;
                        } else {
                            std::cerr << "(parse error) aggregation short name option must be bool"
                                      << std::endl;
                            std::exit(-1);
                        }
                    } else {
                        std::cerr << "(parse error) unrecognized short name option '" << it->at(i)
                                  << "' in " << (*it) << std::endl;
                        std::exit(-1);
                    }
                }

                it = _Tokens.erase(it);
            }
        }

        // if there are short name like options, parse it as aggregation short name options
        {
            auto pos = std::find_if(_Tokens.cbegin(), _Tokens.cend(),
                                    [](std::string const& tok) { return tok.front() == '-'; });
            if (pos != _Tokens.cend()) {
                if (pos->length() == 1) {
                    std::cerr << "(parse error) bare unexcepted '-'" << std::endl;
                    std::exit(-1);
                }
                if ((*pos)[1] == '-') {
                    std::cerr << "(parse error) unrecognized option" << (*pos) << std::endl;
                    std::exit(-1);
                }

                pos = _Tokens.erase(pos);
            }
        }
    }

    void check_add_option_sname(std::string const& key) const {
        if (key.size() != 2 || key.front() != '-') {
            std::cerr << "(build error) short option name must be `-` followed by one character"
                      << std::endl;
            std::exit(-1);
        }
        char ch = key.back();
        if (short_name_index.find(ch) != short_name_index.end()) {
            std::cerr << "(build error) short option name " << key << " already exists"
                      << std::endl;
            std::exit(-1);
        }
    }

    void check_add_option_lname(std::string const& key) const {
        if (key == "") {
            std::cerr << "(build error) long option name cannot be empty" << std::endl;
            std::exit(-1);
        }
        if (key.substr(0, 2) != "--") {
            std::cerr
                << "(build error) long option name must be `--` followed by one or more characters"
                << std::endl;
            std::exit(-1);
        }

        if (options.doGet(key) || short_circuit_options.doGet(key)) {
            std::cerr << "(build error) long option name " << key << " already exists" << std::endl;
            std::exit(-1);
        }
    }

    // add short circuit option
    void add_sc_option(std::string sname, std::string lname, std::string desc,
                       std::function<void(void)> callback) {
        // long name must not be empty
        check_add_option_lname(lname);
        // allow short name to be empty
        if (sname != "") {
            check_add_option_sname(sname);
            short_name_index[sname.back()] = lname;
            short_circuit_options.push(sname.back(), std::move(lname), std::move(desc),
                                       std::move(callback));
        } else {
            short_circuit_options.push(std::move(lname), std::move(desc), std::move(callback));
        }
    }

    template <typename T>
    void add_option(std::string sname, std::string lname, std::string help, T&& default_value) {
        using namespace util;
        if (type_string<T>() == "null") {
            std::cerr << "(build error) unsupport type for option: " << typeid(T).name()
                      << std::endl;
            std::exit(-1);
        }

        check_add_option_lname(lname);
        if (sname != "") {
            check_add_option_sname(sname);
            short_name_index[sname.back()] = lname;
        }

        options.push(std::move(sname), std::move(lname), std::move(help), type_string<T>(),
                     to_string(default_value));
    }

    void add_option(std::string sname, std::string lname, std::string help) {
        check_add_option_lname(lname);
        if (sname != "") {
            check_add_option_sname(sname);
            short_name_index[sname.back()] = lname;
            options.push(sname.back(), std::move(lname), std::move(help), std::string("bool"),
                         std::string("0"));
        } else {
            options.push(std::move(lname), std::move(help), std::string("bool"), std::string("0"));
        }
    }

    template <typename T>
    T get_option(std::string const& name) const {
        using namespace util;
        auto pos = options.find(name);
        if (!pos) { pos = options.find(name.back()); }
        if (!pos) {
            std::cerr << "(get error) option not found: " << name << std::endl;
            std::exit(-1);
        }

        if (pos->type != type_string<T>()) {
            std::cerr << "(get error) option type mismatch: set '" << pos->type
                      << "' but you try get with '" << type_string<T>() << "'" << std::endl;
            std::exit(-1);
        }

        return parse_value<T>(pos->value);
    }

    // some alias for get_option
    bool has_option(std::string const& name) const {
        return get_option<bool>(name);
    }
    bool get_option_bool(std::string const& name) const {
        return get_option<bool>(name);
    }
    int get_option_int(std::string const& name) const {
        return get_option<int>(name);
    }
    int64_t get_option_int64(std::string const& name) const {
        return get_option<int64_t>(name);
    }
    double get_option_double(std::string const& name) const {
        return get_option<double>(name);
    }
    std::string get_option_string(std::string const& name) const {
        return get_option<std::string>(name);
    }

    std::size_t calc_desc_max_length() const noexcept {
        std::size_t max_name_length = 0;

        max_name_length = short_circuit_options.calc_desc_max_length();
        max_name_length = std::max(options.calc_desc_max_length(), std::size_t(25));

        return max_name_length;
    }
    void appendHelp(std::ostringstream& _Oss, size_t& _Max) const {
        short_circuit_options.appendHelp(_Oss, _Max);
        options.appendHelp(_Oss, _Max);
    }

private:
    action_opt_mgr short_circuit_options;
    value_opt_mgr options;
    std::map<char, std::string> short_name_index;
};

CMD_ARGS_NAMESPACE_END