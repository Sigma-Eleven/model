#include <iostream>
#include <fstream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif
#include "parser.h"
#include "interpreter.h"
#include "py_binding.h" // 复用解析和执行逻辑

// 读取文件内容到字符串
std::string read_file(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw std::runtime_error("无法打开文件: " + path);
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(65001); // 设置控制台为UTF-8编码
#endif
    if (argc != 2)
    {
        std::cerr << "用法: " << argv[0] << " <dsl_file_path>" << std::endl;
        return 1;
    }

    std::string dsl_path = argv[1];
    try
    {
        // 1. 解析DSL并输出核心信息
        std::cout << "=== 解析DSL文件: " << dsl_path << " ===" << std::endl;
        std::cout.flush();

        std::string json_info = parse_dsl_to_json(dsl_path);
        std::cout << "核心信息: " << json_info << std::endl;
        std::cout.flush();

        // 2. 执行DSL并输出日志
        std::cout << "\n=== 执行DSL流程 ===" << std::endl;
        std::cout.flush();

        std::string exec_log = run_dsl(dsl_path);
        std::cout << exec_log << std::endl;
        std::cout.flush();

        std::cout << "\n=== 执行完成 ===" << std::endl;
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n错误: " << e.what() << std::endl;
        return 1;
    }
}