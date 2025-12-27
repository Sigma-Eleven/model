#include "py_binding.h"
#include "../include/parser.h"
#include "interpreter.h"
#include <fstream>
#include <sstream>
#include <iostream>

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
        std::string source = read_file(dsl_file_path);
        WolfParser parser(source);
        WolfParseResult result = parser.parse();

        WolfDSLInterpreter interpreter(result);
        return interpreter.export_ast_to_json();
    } catch (const std::exception& e) {
        return "{\"game_name\":\"\",\"roles_count\":0,\"has_error\":true,\"error_msg\":\"" + std::string(e.what()) + "\"}";
    }
}

std::string run_dsl(const std::string& dsl_file_path) {
    try {
        std::string source = read_file(dsl_file_path);
        WolfParser parser(source);
        WolfParseResult result = parser.parse();

        WolfDSLInterpreter interpreter(result);

        std::ostringstream oss;
        std::streambuf* old_cout = std::cout.rdbuf(oss.rdbuf());
        
        interpreter.run();

        std::cout.rdbuf(old_cout);
        
        return oss.str();
    } catch (const std::exception& e) {
        return "执行错误: " + std::string(e.what());
    }
}

void init_python_binding() {
//保留python绑定
}