#include "generator.h"
#include <sstream>
#include <algorithm>

PythonGenerator::PythonGenerator(const WolfParseResult& result) : result(result) {}

std::string PythonGenerator::indent(int level) {
    return std::string(level * 4, ' ');
}

std::string PythonGenerator::translateBody(const std::vector<std::string>& lines, int indentLevel) {
    if (lines.empty()) {
        return indent(indentLevel) + "pass\n";
    }
    
    std::stringstream ss;
    for (const auto& line : lines) {
        std::string trimmed = line;
        // 移除末尾分号和空白
        trimmed.erase(std::remove(trimmed.begin(), trimmed.end(), ';'), trimmed.end());
        // 简单的语法转换
        if (trimmed.find("println") != std::string::npos) {
            size_t start = trimmed.find("(");
            size_t end = trimmed.find_last_of(")");
            if (start != std::string::npos && end != std::string::npos) {
                trimmed = "print" + trimmed.substr(start, end - start + 1);
            }
        }
        ss << indent(indentLevel) << trimmed << "\n";
    }
    return ss.str();
}

std::string PythonGenerator::generate() {
    std::stringstream ss;
    
    ss << "# Generated Python code from Wolf DSL\n";
    ss << "import time\n\n";

    // 1. 角色定义
    ss << "ROLES = " << "[";
    for (size_t i = 0; i < result.roles.size(); ++i) {
        ss << "\"" << result.roles[i] << "\"" << (i == result.roles.size() - 1 ? "" : ", ");
    }
    ss << "]\n\n";

    // 2. 全局变量
    ss << "# Global Variables\n";
    for (const auto& pair : result.variables) {
        ss << pair.first << " = " << (pair.second.value.empty() ? "None" : pair.second.value) << "\n";
    }
    ss << "\n";

    // 3. 动作定义 (作为类或函数)
    ss << "# Actions\n";
    for (const auto& action : result.actions) {
        ss << "def action_" << action.name << "(";
        for (size_t i = 0; i < action.params.size(); ++i) {
            ss << action.params[i].name << (i == action.params.size() - 1 ? "" : ", ");
        }
        ss << "):\n";
        ss << translateBody(action.bodyLines, 1);
        ss << "\n";
    }

    // 4. 阶段与步骤逻辑
    ss << "class GameFlow:\n";
    ss << indent(1) << "def __init__(self):\n";
    ss << indent(2) << "self.current_phase = None\n\n";

    for (const auto& phase : result.phases) {
        ss << indent(1) << "def phase_" << phase.name << "(self):\n";
        ss << indent(2) << "print(f\"--- Phase: " << phase.name << " ---\")\n";
        for (const auto& step : phase.steps) {
            ss << indent(2) << "# Step: " << step.name << "\n";
            if (!step.condition.empty()) {
                ss << indent(2) << "if " << step.condition << ":\n";
                ss << indent(3) << "print(f\"Executing step: " << step.name << "\")\n";
                if (!step.actionName.empty()) {
                    ss << indent(3) << "action_" << step.actionName << "()\n";
                }
                ss << translateBody(step.bodyLines, 3);
            } else {
                ss << indent(2) << "print(f\"Executing step: " << step.name << "\")\n";
                if (!step.actionName.empty()) {
                    ss << indent(2) << "action_" << step.actionName << "()\n";
                }
                ss << translateBody(step.bodyLines, 2);
            }
        }
        ss << "\n";
    }

    // 5. Setup与启动
    ss << "def run_game():\n";
    ss << indent(1) << "print(\"=== Starting " << result.gameName << " ===\")\n";
    ss << translateBody(result.setup.bodyLines, 1);
    ss << indent(1) << "flow = GameFlow()\n";
    for (const auto& phase : result.phases) {
        ss << indent(1) << "flow.phase_" << phase.name << "()\n";
    }

    ss << "\nif __name__ == \"__main__\":\n";
    ss << indent(1) << "run_game()\n";

    return ss.str();
}
