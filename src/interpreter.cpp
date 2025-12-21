#include "interpreter.h"
#include <stdexcept>
#include <iostream>
#include <sstream>

// -------------------------- 核心辅助方法 --------------------------
// JSON字符串简单转义（适配导出JSON功能）
static std::string escape_json(const std::string& s) {
    std::string res;
    for (char c : s) {
        switch (c) {
            case '"': res += "\\\""; break;
            case '\\': res += "\\\\"; break;
            default: res += c;
        }
    }
    return res;
}

// -------------------------- WolfDSLInterpreter 方法实现（匹配头文件） --------------------------
// 注：若头文件中已内联实现，此处仅需补充未内联的核心逻辑；以下为完整独立实现版本

// 执行单个阶段（核心逻辑）
void WolfDSLInterpreter::execute_phase(const WolfParseResult::PhaseDef& phase) {
    if (env_.has_error) return;

    std::cout << "\n[阶段] " << phase.name << std::endl;
    // 遍历执行阶段下所有步骤
    for (const auto& step : phase.steps) {
        execute_step(step);
        if (env_.has_error) break;
    }
}

// 执行单个步骤（核心逻辑）
void WolfDSLInterpreter::execute_step(const WolfParseResult::PhaseDef::StepDef& step) {
    if (env_.has_error) return;

    std::cout << "  [步骤] " << step.name << std::endl;
    
    // 1. 输出参与角色（极简版：仅输出第一个角色）
    if (!step.rolesInvolved.empty()) {
        std::cout << "    参与角色：" << step.rolesInvolved[0];
        if (step.rolesInvolved.size() > 1) {
            std::cout << " 等" << step.rolesInvolved.size() << "人";
        }
        std::cout << std::endl;
    }

    // 2. 校验并执行关联动作
    if (!step.actionName.empty()) {
        try {
            // 校验动作存在性（核心）
            WolfParseResult::ActionDef action = env_.get_action(step.actionName);
            std::cout << "    执行动作：" << action.name << std::endl;

            // 极简版：仅输出动作参数数量
            if (!action.params.empty()) {
                std::cout << "    动作参数：" << action.params.size() << "个" << std::endl;
            }
        } catch (const std::runtime_error& e) {
            env_.set_error("步骤 [" + step.name + "] 执行失败：" + e.what());
            std::cerr << "    错误：" << e.what() << std::endl;
        }
    }

    // 3. 输出步骤条件（可选，仅展示）
    if (!step.condition.empty()) {
        std::cout << "    执行条件：" << step.condition << std::endl;
    }
}

// 扩展：若需要完善的JSON导出功能，可补充此实现（替换头文件中的极简版）
std::string WolfDSLInterpreter::export_ast_to_json() {
    std::ostringstream oss;
    oss << "{";
    // 游戏名称（转义特殊字符）
    oss << "\"game_name\":\"" << escape_json(env_.game_name) << "\",";
    // 角色数量
    oss << "\"roles_count\":" << env_.roles.size() << ",";
    // 动作数量
    oss << "\"actions_count\":" << env_.actions.size() << ",";
    // 阶段数量
    oss << "\"phases_count\":" << parse_result_.phases.size() << ",";
    // 错误状态
    oss << "\"has_error\":" << (env_.has_error ? "true" : "false");
    // 错误信息（若有）
    if (env_.has_error) {
        oss << ",\"error_msg\":\"" << escape_json(env_.error_msg) << "\"";
    }
    oss << "}";
    return oss.str();
}