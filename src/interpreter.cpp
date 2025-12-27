#include "../include/interpreter.h"
#include <stdexcept>
#include <iostream>
#include <sstream>

static std::string escape_json(const std::string &s)
{
    std::string res;
    for (char c : s)
    {
        switch (c)
        {
        case '"':
            res += "\\\"";
            break;
        case '\\':
            res += "\\\\";
            break;
        default:
            res += c;
        }
    }
    return res;
}

void WolfDSLInterpreter::execute_phase(const WolfParseResult::PhaseDef &phase)
{
    if (env_.has_error)
        return;

    std::cout << "\n[阶段] " << phase.name << std::endl;

    for (const auto &step : phase.steps)
    {
        execute_step(step);
        if (env_.has_error)
            break;
    }
}

void WolfDSLInterpreter::execute_step(const WolfParseResult::PhaseDef::StepDef &step)
{
    if (env_.has_error)
        return;

    std::cout << "  [步骤] " << step.name << std::endl;

    if (!step.rolesInvolved.empty())
    {
        std::cout << "    参与角色：" << step.rolesInvolved[0];
        if (step.rolesInvolved.size() > 1)
        {
            std::cout << " 等" << step.rolesInvolved.size() - 1 << "人";
        }
        std::cout << std::endl;
    }

    if (!step.actionName.empty())
    {
        try
        {
            WolfParseResult::ActionDef action = env_.get_action(step.actionName);
            std::cout << "    执行动作：" << action.name << std::endl;

            if (!action.params.empty())
            {
                std::cout << "    动作参数：" << action.params.size() << "个" << std::endl;
            }
        }
        catch (const std::runtime_error &e)
        {
            env_.set_error("步骤 [" + step.name + "] 执行失败：" + e.what());
            std::cerr << "    错误：" << e.what() << std::endl;
        }
    }

    if (!step.condition.empty())
    {
        std::cout << "    执行条件：" << step.condition << std::endl;
    }
}

std::string WolfDSLInterpreter::export_ast_to_json()
{
    std::ostringstream oss;
    oss << "{";
    oss << "\"game_name\":\"" << escape_json(env_.game_name) << "\",";
    oss << "\"roles_count\":" << env_.roles.size() << ",";
    oss << "\"actions_count\":" << env_.actions.size() << ",";
    oss << "\"phases_count\":" << parse_result_.phases.size() << ",";

    oss << "\"has_error\":" << (env_.has_error ? "true" : "false");
    if (env_.has_error)
    {
        oss << ",\"error_msg\":\"" << escape_json(env_.error_msg) << "\"";
    }
    oss << "}";
    return oss.str();
}