#pragma once

#include "parser.h"
#include <string>

class PythonGenerator
{
public:
    explicit PythonGenerator(const WolfParseResult &result);
    std::string generate();

private:
    const WolfParseResult &result;
    std::string indent(int level);
    std::string translateBody(const std::vector<std::string> &lines, int indentLevel);
};
