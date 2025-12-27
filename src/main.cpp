#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "../include/parser.h"
#include "../include/generator.h"
#include "../include/interpreter.h"

#ifdef _WIN32
#include <windows.h>
#endif

void printUsage()
{
    std::cout << "Wolf DSL Translator Usage:\n";
    std::cout << "  translator <input.game> [output.py]\n";
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    // Set console output to UTF-8 for Windows
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (argc < 2)
    {
        printUsage();
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputFile = (argc >= 3) ? argv[2] : "";

    // 1. 读取 DSL 源文件
    std::ifstream ifs(inputFile);
    if (!ifs.is_open())
    {
        std::cerr << "错误: 无法打开输入文件 " << inputFile << std::endl;
        return 1;
    }

    std::string source((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());
    ifs.close();

    // 2. 解析 DSL
    WolfParser parser(source);
    WolfParseResult result = parser.parse();

    if (result.hasError)
    {
        std::cerr << "解析失败!" << std::endl;
        return 1;
    }

    // 3. 如果指定了输出文件，则生成 Python 代码
    if (!outputFile.empty())
    {
        std::cout << "=== 正在翻译为 Python: " << outputFile << " ===" << std::endl;
        PythonGenerator generator(result);
        std::string pythonCode = generator.generate();

        std::ofstream ofs(outputFile);
        if (!ofs.is_open())
        {
            std::cerr << "错误: 无法写入输出文件 " << outputFile << std::endl;
            return 1;
        }
        ofs << pythonCode;
        ofs.close();
        std::cout << "翻译完成！文件已写入: " << outputFile << std::endl;
    }
    else
    {
        // 4. 如果没有指定输出文件，则直接在解释器中运行（模拟执行）
        std::cout << "=== 正在解释执行 DSL: " << result.gameName << " ===" << std::endl;
        WolfDSLInterpreter interpreter(result);
        interpreter.run();
    }

    return 0;
}
