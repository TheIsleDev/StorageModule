
#pragma once

#include <string>
#include <Mod/CppUserModBase.hpp>
#include <Database/Database.hpp>
#include <TheIsle/ATIPlayerController.hpp>
#include <TheIsle/ATIDinosaurBase.hpp>


using namespace RC::Unreal;
using namespace RC::Unreal::TheIsle;

struct StorageConfig {
	std::string Database;
	float SaveWaitTime{60};// How long player need to stay still in seconds, need statistic module for it, no native fallback
	float GrowthRequirementSave{0.30f};// Disabled: 0.00, 0.00-1.00 growth range, so it's bottom range value, top one always 1.00
	float GrowthRequirementLoad{0.30f};// Disabled: 1.00, 0.00-1.00 growth range, so it's top range value, bottom one always 0.00
	bool OverrideDino{true};// So it can be used after spawn
	bool PreserveLoc{false};// Keeping old dino loc before loading
	bool SameClass{false};// Without override have nothing to do
	bool HealthCheck{false};
	bool BloodCheck{false};
	bool StaminaCheck{false};
	bool LockOnLoad{false};// If dino loaded it will set to true it playing in status, so you can't load it again
};

class StorageSystem : public RC::CppUserModBase {
private:
	StorageConfig Config{};
	RC::DataBase::DataBase* Database{};

	bool CreateHelpers();
	ATIGameModeBase* GameMode{};

	UFunction* ExecuteCommand{};
	int32_t HookID{};

	void HandleExecuteCommand(UnrealScriptFunctionCallableContext& FuncContext);
	bool StorageChecks(ATIDinosaurBase* Dinosaur, ATIPlayerController* Player);

public:
    StorageSystem();
	~StorageSystem() override;

	void on_unreal_init() override;

	void StoreDino(ATIPlayerController* Player);
	void LoadDino(ATIPlayerController* Player, int32 DinoID);

};
