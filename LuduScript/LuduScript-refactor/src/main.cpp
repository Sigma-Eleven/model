#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#include "lexer.h"
#include "parser.h"
#include "codegen.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: ludu <input_file>" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::ifstream file(inputPath);
    if (!file.is_open())
    {
        std::cerr << "Could not open file: " << inputPath << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // 1. Lexing
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();

    // Debug tokens
    // for (const auto& t : tokens) {
    //     std::cout << t.toString() << std::endl;
    // }

    // 2. Parsing
    try
    {
        Parser parser(tokens);
        auto gameDecl = parser.parse();

        // 3. Code Generation
        std::string outputPath = inputPath.substr(0, inputPath.find_last_of('.')) + ".py";
        std::ofstream outFile(outputPath);

        PythonGenerator generator(outFile);
        generator.generate(*gameDecl);

        std::cerr << "Successfully compiled to " << outputPath << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Compilation failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
