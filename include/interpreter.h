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

// -------------------------- 核心:DSL基础类型定义（匹配type_keyword） --------------------------
enum class ValueType
{
    NUMBER, // 对应 "num"
    STRING, // 对应 "str"
    BOOL,   // 对应 "bool"
    NONE    // 无值（简化:暂删OBJECT）
};

// 核心:运行时值（保留变量存储必要逻辑）
struct RuntimeValue
{
    ValueType type;
    std::variant<int, std::string, bool> value;

    // 便捷构造函数（保留必要类型）
    static RuntimeValue Number(int val) { return {ValueType::NUMBER, val}; }
    static RuntimeValue String(const std::string &val) { return {ValueType::STRING, val}; }
    static RuntimeValue Bool(bool val) { return {ValueType::BOOL, val}; }
    static RuntimeValue None() { return {ValueType::NONE, 0}; }

    // 核心:从type_keyword+原始值转换为RuntimeValue
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
            for (auto &c : low) c = (char)tolower(c);
            return Bool(low == "true");
        }
        return None();
    }
};

// -------------------------- 核心:运行时环境（保留变量/动作存储） --------------------------
class RuntimeEnv
{
public:
    // 全局变量（适配WolfParseResult::variables<map>）
    std::unordered_map<std::string, RuntimeValue> global_vars;
    // 已注册的动作（适配WolfParseResult::ActionDef）
    std::unordered_map<std::string, WolfParseResult::ActionDef> actions;

    // 游戏基础信息（字段）
    std::string game_name;
    std::vector<std::string> roles;
    // 错误标志（处理解析/运行时错误）
    bool has_error = false;
    std::string error_msg;

    // 核心:设置全局变量
    void set_var(const std::string &name, const RuntimeValue &val)
    {
        global_vars[name] = val;
    }

    // 核心:获取全局变量（无局部变量，简化）
    RuntimeValue get_var(const std::string &name)
    {
        if (global_vars.find(name) != global_vars.end())
        {
            return global_vars[name];
        }
        throw std::runtime_error("未定义的变量:" + name);
    }

    // 核心:注册动作
    void register_action(const WolfParseResult::ActionDef &action)
    {
        actions[action.name] = action;
    }

    // 核心:获取动作（校验存在性）
    WolfParseResult::ActionDef get_action(const std::string &action_name)
    {
        if (actions.find(action_name) == actions.end())
        {
            throw std::runtime_error("未定义的动作:" + action_name);
        }
        return actions[action_name];
    }

    // 核心:标记错误
    void set_error(const std::string &msg)
    {
        has_error = true;
        error_msg = msg;
    }
};

// -------------------------- 核心:解释器类(保留执行逻辑） --------------------------
class WolfDSLInterpreter
{
public:
    // 构造函数:初始化的解析结果
    explicit WolfDSLInterpreter(const WolfParseResult &parse_result)
        : parse_result_(parse_result), env_()
    {

        // 1. 初始化游戏基础信息
        env_.game_name = parse_result.gameName;
        env_.roles = parse_result.roles;

        // 2. 注册所有动作
        for (const auto &action : parse_result.actions)
        {
            env_.register_action(action);
        }

        // 3. 初始化全局变量（适配type_keyword）
        for (const auto &[var_name, var_def] : parse_result.variables)
        {
            try
            {
                RuntimeValue val = RuntimeValue::fromString(var_def.type_keyword, var_def.value);
                env_.set_var(var_name, val);
            }
            catch (const std::exception &e)
            {
                env_.set_error("变量初始化失败 [" + var_name + "]:" + e.what());
            }
        }

        // 4. 检查解析阶段错误
        if (parse_result.hasError)
        {
            env_.set_error("解析错误:" + parse_result.errorMessage);
        }
    }

    // 核心入口:仅执行阶段/步骤核心逻辑
    void run()
    {
        // 有错误直接终止
        if (env_.has_error)
        {
            std::cerr << "DSL执行终止:" << env_.error_msg << std::endl;
            return;
        }

        // 基础日志（仅保留必要输出）
        std::cout << "开始执行DSL:" << env_.game_name << std::endl;
        std::cout << "角色列表:" << env_.roles.size() << "个" << std::endl;

        // 遍历执行所有阶段
        for (const auto &phase : parse_result_.phases)
        {
            execute_phase(phase);
            if (env_.has_error)
                break; // 错误终止
        }
    }

    // 核心:导出极简JSON（供Python读取）
    std::string export_ast_to_json();

private:
    WolfParseResult parse_result_; // 解析结果（）
    RuntimeEnv env_;               // 运行时环境（）

    // 核心:执行单个阶段（仅保留必要逻辑）
    void execute_phase(const WolfParseResult::PhaseDef &phase);

    // 核心:执行单个步骤（仅保留必要逻辑）
    void execute_step(const WolfParseResult::PhaseDef::StepDef &step);
};
