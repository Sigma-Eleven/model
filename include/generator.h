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
    std::string normalizeExpression(const std::string &expr);
    std::string transformPrintContent(const std::string &inner);

    // State for dictionary self-reference handling
    std::string currentDictName;
    std::vector<std::string> pendingDictAssignments;

    // Generation helpers
    std::string generateImports();
    std::string generateEnums();
    std::string generateActionClasses();
    std::string generateGameClass();
    std::string generateEntryPoint();

    // Game Class parts
    std::string generateInit();
    std::string generateInitPhases();
    std::string generateSetupGame();
    std::string generateCoreHelpers();
    std::string generateDSLMethods();
};
