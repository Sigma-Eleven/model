#include <iostream>
#include <fstream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif
#include "../include/parser.h"
#include "interpreter.h"
#include "py_binding.h"
#include "generator.h"

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
    SetConsoleOutputCP(65001); 
#endif
    if (argc < 2)
    {
        std::cerr << "用法: " << argv[0] << " <dsl_file_path> [output_py_path]" << std::endl;
        return 1;
    }

    std::string dsl_path = argv[1];
    std::string py_path = (argc >= 3) ? argv[2] : "";

    try
    {
        std::string source = read_file(dsl_path);

        WolfParser parser(source);
        WolfParseResult result = parser.parse();

        if (result.hasError)
        {
            std::cerr << "解析错误: " << result.errorMessage << std::endl;
            return 1;
        }

        if (!py_path.empty())
        {
            std::cout << "=== 正在翻译为 Python: " << py_path << " ===" << std::endl;
            PythonGenerator generator(result);
            std::string py_code = generator.generate();

            std::ofstream out(py_path, std::ios::out | std::ios::trunc);
            if (!out.is_open())
            {
                throw std::runtime_error("无法创建输出文件: " + py_path);
            }
            out << py_code;
            out.flush();
            out.close();
            std::cout << "翻译完成！文件已写入: " << py_path << std::endl;
        }
        else
        {
            // 默认执行逻辑
            std::cout << "=== 解析DSL文件: " << dsl_path << " ===" << std::endl;
            WolfDSLInterpreter interpreter(result);
            std::cout << "核心信息: " << interpreter.export_ast_to_json() << std::endl;

            std::cout << "\n=== 执行DSL流程 ===" << std::endl;
            interpreter.run();
            std::cout << "\n=== 执行完成 ===" << std::endl;
        }

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n错误: " << e.what() << std::endl;
        return 1;
    }
}