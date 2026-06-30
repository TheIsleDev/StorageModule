#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include <Unreal/FText.hpp>
#include <Unreal/AActor.hpp>
#include <Helpers/String.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/AGameModeBase.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include "Engine/UDataTable.hpp"

using namespace RC::Unreal;

namespace StorageSystemConfiguration {
	struct StorageConfig {
		std::string Database;
		float SaveWaitTime;// How long player need to stay still in seconds, need statistic module for it, no native fallback
		float GrowthRequirement;// 0.00-1.00 growth range, so it's bottom range value, top one always 1.00
		bool OverrideDino;// So it can be used after spawn
		bool SameClass;// Without override have nothing to do
		bool HealthCheck;
		bool BloodCheck;
		bool StaminaCheck;
	};

	//								STRUCTS
	struct BaseForFuncGetter {
		UFunction* Function = nullptr;
		uint32 BufferSize;

		auto Initialize() -> void {
			BufferSize = Function->GetParmsSize();
			InitializeSub();
		}

		protected:
			virtual void InitializeSub() {}
	};

	//								SEPARATE PART
	struct DataOfGetCharacterData : public BaseForFuncGetter {
		FProperty* Character = nullptr;
		FProperty* bForceSafe = nullptr;
		FStructProperty* ReturnValue = nullptr;

    	UScriptStruct* PlayerDataStruct = nullptr;

		void InitializeSub() override {
			for (FProperty* Prop : TFieldRange<FProperty>(Function, EFieldIterationFlags::IncludeDeprecated)) {
				auto Name = Prop->GetName();
				if (Name == STR("Character"))
					Character = Prop;
				else if (Name == STR("bForceSafe"))
					bForceSafe = Prop;
				else if (Name == STR("ReturnValue")) {
					ReturnValue = CastField<FStructProperty>(Prop);
					PlayerDataStruct = ReturnValue->GetStruct();
				}
			}
		}
	};

	struct DataOfPlayerDataToString : public BaseForFuncGetter {
		FStructProperty* PlayerData = nullptr;
		FProperty* ReturnValue = nullptr;

    	UScriptStruct* PlayerDataStruct = nullptr;

		void InitializeSub() override {
			for (FProperty* Prop : TFieldRange<FProperty>(Function, EFieldIterationFlags::IncludeDeprecated)) {
				auto Name = Prop->GetName();
				if (Name == STR("PlayerData")) {
					PlayerData = CastField<FStructProperty>(Prop);
					PlayerDataStruct = PlayerData->GetStruct();
				} else if (Name == STR("ReturnValue"))
					ReturnValue = Prop;
			}
		}
	};

	//								SEPARATE PART
	struct DataOfStringToPlayerData : public BaseForFuncGetter {
		FProperty* String = nullptr;
		FStructProperty* ReturnValue = nullptr;

    	UScriptStruct* PlayerDataStruct = nullptr;
		FProperty* PlayerData_Class = nullptr;
		FProperty* PlayerData_Location = nullptr;
		FProperty* PlayerData_CustomizedData = nullptr;

		void InitializeSub() override {
			for (FProperty* Prop : TFieldRange<FProperty>(Function, EFieldIterationFlags::IncludeDeprecated)) {
				auto Name = Prop->GetName();
				if (Name == STR("String"))
					String = Prop;
				else if (Name == STR("ReturnValue")) {
					ReturnValue = CastField<FStructProperty>(Prop);
					PlayerDataStruct = ReturnValue->GetStruct();
				}
			}

			for (FProperty* Prop : TFieldRange<FProperty>(PlayerDataStruct, EFieldIterationFlags::IncludeDeprecated)) {
				auto Name = Prop->GetName();
				if (Name == STR("Class"))
					PlayerData_Class = Prop;
				else if (Name == STR("Location"))
					PlayerData_Location = Prop;
				else if (Name == STR("CustomizedData"))
					PlayerData_CustomizedData = Prop;
			}
		}
	};

	struct DataOfAddSpawnRequest : public BaseForFuncGetter {
    	FStructProperty* SpawnData = nullptr;
		FProperty* bAddHud = nullptr;
		FProperty* bLoadSaved = nullptr;

    	UScriptStruct* SpawnDataStruct = nullptr;
		FProperty* SpawnData_Controller = nullptr;
		FProperty* SpawnData_SteamId = nullptr;
		FProperty* SpawnData_Class = nullptr;
		FProperty* SpawnData_bKeepActualLoc = nullptr;
		FProperty* SpawnData_bLoadOnly = nullptr;
		FProperty* SpawnData_bDefaultSpawn = nullptr;
		FProperty* SpawnData_SpawnLocation = nullptr;
		FProperty* SpawnData_CustomizerData = nullptr;
		FProperty* SpawnData_bUseQuickRespawn = nullptr;
		FProperty* SpawnData_PlayerData = nullptr;

		void InitializeSub() override {
			for (FProperty* Prop : TFieldRange<FProperty>(Function, EFieldIterationFlags::IncludeDeprecated)) {
				auto Name = Prop->GetName();
				if (Name == STR("SpawnData")) {
					SpawnData = CastField<FStructProperty>(Prop);
					SpawnDataStruct = SpawnData->GetStruct();
				} else if (Name == STR("bAddHud"))
					bAddHud = Prop;
				else if (Name == STR("bLoadSaved"))
					bLoadSaved = Prop;
			}

			for (FProperty* Prop : TFieldRange<FProperty>(SpawnDataStruct, EFieldIterationFlags::IncludeDeprecated)) {// To be fair I don't think this is good ~enough solution, however I didn't managed to make LLM give me better idea
				auto Name = Prop->GetName();
				if (Name == STR("Controller"))
					SpawnData_Controller = Prop;
				else if (Name == STR("SteamId"))
					SpawnData_SteamId = Prop;
				else if (Name == STR("Class"))
					SpawnData_Class = Prop;
				else if (Name == STR("bKeepActualLoc"))
					SpawnData_bKeepActualLoc = Prop;
				else if (Name == STR("bLoadOnly"))
					SpawnData_bLoadOnly = Prop;
				else if (Name == STR("bDefaultSpawn"))
					SpawnData_bDefaultSpawn = Prop;
				else if (Name == STR("SpawnLocation"))
					SpawnData_SpawnLocation = Prop;
				else if (Name == STR("CustomizerData"))
					SpawnData_CustomizerData = Prop;
				else if (Name == STR("bUseQuickRespawn"))
					SpawnData_bUseQuickRespawn = Prop;
				else if (Name == STR("PlayerData"))
					SpawnData_PlayerData = Prop;
			}
		}
	};
}