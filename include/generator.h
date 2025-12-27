#pragma once

#include "parser.h"
#include <string>
#include <set>

class PythonGenerator
{
public:
    explicit PythonGenerator(const WolfParseResult &result);
    std::string generate();

private:
    const WolfParseResult &result;
    std::set<std::string> varNames;
    std::set<std::string> actionNames;
    std::set<std::string> methodNames;

    std::string indent(int level);
    std::string translateBody(const std::vector<std::string> &lines, int indentLevel);
    std::string normalizeExpression(std::string expr);
    std::string transformPrintContent(const std::string &inner);
};
