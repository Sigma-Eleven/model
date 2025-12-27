#pragma once

// 引入项目头文件
#include "parser.h"
#include "lexer.h"

// 引入标准库
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <variant>
#include <stdexcept>
#include <iostream>
#include <cstdlib>

enum class ValueType
{
    NUMBER,
    STRING,
    BOOL,
    NONE
};

struct RuntimeValue
{
    ValueType type;
    std::variant<int, std::string, bool> value;

    static RuntimeValue Number(int val) { return {ValueType::NUMBER, val}; }
    static RuntimeValue String(const std::string &val) { return {ValueType::STRING, val}; }
    static RuntimeValue Bool(bool val) { return {ValueType::BOOL, val}; }
    static RuntimeValue None() { return {ValueType::NONE, 0}; }

    static RuntimeValue fromString(const std::string &type_keyword, const std::string &value_str)
    {
        // 去除首尾空格
        std::string trimmed = value_str;
        trimmed.erase(0, trimmed.find_first_not_of(" "));
        trimmed.erase(trimmed.find_last_not_of(" ") + 1);

        if (type_keyword == "num")
        {
            return Number(std::atoi(trimmed.c_str()));
        }
        else if (type_keyword == "str")
        {
            return String(trimmed);
        }
        else if (type_keyword == "bool")
        {
            std::string low = trimmed;
            for (auto &c : low)
                c = (char)tolower(c);
            return Bool(low == "true");
        }
        return None();
    }
};

class RuntimeEnv
{
public:
    // 全局变量（适配WolfParseResult::variables<map>）
    std::unordered_map<std::string, RuntimeValue> global_vars;
    std::unordered_map<std::string, WolfParseResult::ActionDef> actions;

    std::string game_name;
    std::vector<std::string> roles;

    bool has_error = false;
    std::string error_msg;

    void set_var(const std::string &name, const RuntimeValue &val)
    {
        global_vars[name] = val;
    }

    RuntimeValue get_var(const std::string &name)
    {
        if (global_vars.find(name) != global_vars.end())
        {
            return global_vars[name];
        }
        throw std::runtime_error("未定义的变量: " + name);
    }

    void register_action(const WolfParseResult::ActionDef &action)
    {
        actions[action.name] = action;
    }

    WolfParseResult::ActionDef get_action(const std::string &action_name)
    {
        if (actions.find(action_name) == actions.end())
        {
            throw std::runtime_error("未定义的动作: " + action_name);
        }
        return actions[action_name];
    }

    void set_error(const std::string &msg)
    {
        has_error = true;
        error_msg = msg;
    }
};

class WolfDSLInterpreter
{
public:
    explicit WolfDSLInterpreter(const WolfParseResult &parse_result)
        : parse_result_(parse_result), env_()
    {

        env_.game_name = parse_result.gameName;
        env_.roles = parse_result.roles;

        for (const auto &action : parse_result.actions)
        {
            env_.register_action(action);
        }

        for (const auto &[var_name, var_def] : parse_result.variables)
        {
            try
            {
                RuntimeValue val = RuntimeValue::fromString(var_def.type_keyword, var_def.value);
                env_.set_var(var_name, val);
            }
            catch (const std::exception &e)
            {
                env_.set_error("变量初始化失败 [" + var_name + "]: " + e.what());
            }
        }

        if (parse_result.hasError)
        {
            env_.set_error("解析错误: " + parse_result.errorMessage);
        }
    }

    void run()
    {
        if (env_.has_error)
        {
            std::cerr << "DSL执行终止: " << env_.error_msg << std::endl;
            return;
        }

        // 基础日志
        std::cout << "开始执行DSL: " << env_.game_name << std::endl;
        std::cout << "角色列表: " << env_.roles.size() << " 个" << std::endl;

        for (const auto &phase : parse_result_.phases)
        {
            execute_phase(phase);
            if (env_.has_error)
                break;
        }
    }

    std::string export_ast_to_json();

private:
    WolfParseResult parse_result_;
    RuntimeEnv env_;

    void execute_phase(const WolfParseResult::PhaseDef &phase);

    void execute_step(const WolfParseResult::PhaseDef::StepDef &step);
};
