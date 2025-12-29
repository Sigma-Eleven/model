#pragma once

#include "generator.h"

class WerewolfGenerator : public PythonGenerator
{
public:
    explicit WerewolfGenerator(const WolfParseResult &result);

protected:
    std::string generateImports() override;
    std::string generateCoreStructures() override;
    std::string generateBaseStructures() override;
    std::string generateEnums() override;
    std::string generateActionClasses() override;
    std::string generateGameClass() override;
    std::string generateInitPhases() override;
    std::string generateCancel() override;
    std::string generateInit() override;
    std::string generateSetupGame() override;
    std::string generateHandleDeath() override;
    std::string generateHandleHunterShot() override;
    std::string generateCheckGameOver() override;
    std::string generateEntryPoint() override;
    std::string generateDSLMethods() override;
    std::string generateActionBody(const std::string &actionName) override;
};
