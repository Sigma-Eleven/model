#include "py_binding.h"
#include "parser.h"
#include "interpreter.h"
#include <fstream>
#include <sstream>
#include <iostream>

// 读取文件内容到字符串
static std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开文件: " + path);
    }
    return std::string((std::istreambuf_iterator<char>(file)), 
                       std::istreambuf_iterator<char>());
}

std::string parse_dsl_to_json(const std::string& dsl_file_path) {
    try {
        // 读取并解析DSL文件
        std::string source = read_file(dsl_file_path);
        WolfParser parser(source);
        WolfParseResult result = parser.parse();
        
        // 创建解释器并导出JSON
        WolfDSLInterpreter interpreter(result);
        return interpreter.export_ast_to_json();
    } catch (const std::exception& e) {
        // 返回错误信息的JSON
        return "{\"game_name\":\"\",\"roles_count\":0,\"has_error\":true,\"error_msg\":\"" + std::string(e.what()) + "\"}";
    }
}

std::string run_dsl(const std::string& dsl_file_path) {
    try {
        // 读取并解析DSL文件
        std::string source = read_file(dsl_file_path);
        WolfParser parser(source);
        WolfParseResult result = parser.parse();
        
        // 创建解释器并执行
        WolfDSLInterpreter interpreter(result);
        
        // 捕获输出
        std::ostringstream oss;
        std::streambuf* old_cout = std::cout.rdbuf(oss.rdbuf());
        
        interpreter.run();
        
        // 恢复cout
        std::cout.rdbuf(old_cout);
        
        return oss.str();
    } catch (const std::exception& e) {
        return "执行错误: " + std::string(e.what());
    }
}

void init_python_binding() {
    // 此函数为PyBind11预留，当前版本为空实现
}