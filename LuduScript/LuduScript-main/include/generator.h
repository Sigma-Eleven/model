#pragma once

#include "parser.h"
#include <string>
#include <set>

class PythonGenerator
{
public:
    explicit PythonGenerator(const WolfParseResult &result);
    virtual ~PythonGenerator() = default;
    std::string generate();

protected:
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
    virtual std::string generateImports();
    virtual std::string generateBaseStructures();
    virtual std::string generateEnums();
    virtual std::string generateActionClasses();
    virtual std::string generateGameClass();
    virtual std::string generateEntryPoint();

    // Game Class parts
    virtual std::string generateInit();
    virtual std::string generateInitPhases();
    virtual std::string generateCancel();
    virtual std::string generateSetupGame();
    virtual std::string generateHandleDeath();
    virtual std::string generateHandleHunterShot();
    virtual std::string generateCheckGameOver();
    virtual std::string generateDSLMethods();
    virtual std::string generateCoreStructures();

    // Specific Action Body generators (can be overridden)
    virtual std::string generateActionBody(const std::string &actionName);
};
