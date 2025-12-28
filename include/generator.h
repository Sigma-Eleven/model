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
    std::string translateBody(const std::vector<std::string> &lines, int indentLevel, const std::string &prefix = "self.");
    std::string normalizeExpression(const std::string &expr, const std::string &prefix = "self.");
    std::string transformPrintContent(const std::string &inner, const std::string &prefix = "self.");

    // State for dictionary self-reference handling
    std::string currentDictName;
    std::vector<std::string> pendingDictAssignments;

    // Generation helpers
    std::string mapActionToClassName(const std::string &name);
    std::string generateImports();
    std::string generateEnums();
    std::string generateActionClasses();
    std::string generateGameClass();
    std::string generateEntryPoint();

    // Game Class parts
    std::string generateInit();
    std::string generateInitPhases();
    std::string generateRunPhase();
    std::string generateGetAlivePlayers();
    std::string generateGetPlayerByRole();
    std::string generateCancel();
    std::string generateSetupGame();
    std::string generateHandleDeath();
    std::string generateHandleHunterShot();
    std::string generateCheckGameOver();
    std::string generateRunGame();
    std::string generateDSLMethods();
    std::string generateCoreStructures();
};
