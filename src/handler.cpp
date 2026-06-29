#pragma once

#include <DynamicOutput/Output.hpp>
#include <DynamicOutput/OutputDevice.hpp>

#include "Unreal/FText.hpp"
#include <Unreal/AActor.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/Core/Containers/ContainerAllocationPolicies.hpp>

#include "Containers/Array.hpp"
#include "Containers/FString.hpp"
#include "CoreUObject/UObject/Class.hpp"

#include "DBLink/database.cpp"
#include "Structs/TheIsleStructs.hpp"
#include "Structs/FunctionParamisator.hpp"

#include "_structs.hpp"

namespace StorageSystemComponent {
	using namespace RC::Unreal;

	static UClass* _PlayerControllerBaseClass = nullptr;
	static FProperty* _PlayerControllerPawn = nullptr;
	static FProperty* _PlayerSteamId = nullptr;

	static UClass* _DinoClass = nullptr;
	static FProperty* _DinoClassID = nullptr;

	static UFunction* _GetCharacterData = nullptr;
	static uint32 _GetCharacterData_BufferSize;
	static FProperty* _GetCharacterData_PropCharacter = nullptr;
	static FProperty* _GetCharacterData_PropForceSafe = nullptr;
	static FProperty* _GetCharacterData_ReturnProperty = nullptr;

	static UFunction* _PlayerDataToString = nullptr;
	static uint32 _PlayerDataToString_BufferSize;
	static FProperty* _PlayerDataToString_PropPlayerData = nullptr;
	static FProperty* _PlayerDataToString_ReturnProperty = nullptr;

	static UFunction* _StringToPlayerData = nullptr;
	static uint32 _StringToPlayerData_BufferSize;
	static FProperty* _StringToPlayerData_PropString = nullptr;
	static FProperty* _StringToPlayerData_ReturnProperty = nullptr;

	static UFunction* _AddSpawnRequest = nullptr;
	static uint32 _AddSpawnRequest_BufferSize;
	static FProperty* _AddSpawnRequest_PropSpawnData = nullptr;
	static FProperty* _AddSpawnRequest_PropbAddHud = nullptr;
	static FProperty* _AddSpawnRequest_PropbLoadSaved = nullptr;


	static IsleStructs::UTISaveManager* _TISaveManager = nullptr;

	static UFunction* _SetHealth = nullptr;
	static UFunction* _WaitAndDestroyCorpse = nullptr;

	static FProperty* _FuncParamPropChatMode = nullptr;
	static FProperty* _FuncParamPropMessage = nullptr;

	auto SaveDino(IsleStructs::ATIPlayerController* Player) -> void {
		IsleStructs::APawn* Pawn = *_PlayerControllerPawn->ContainerPtrToValuePtr<IsleStructs::APawn*>(Player);
		if (!Pawn || !Pawn->IsA(_DinoClass)) return;

		FString SteamId = *_PlayerSteamId->ContainerPtrToValuePtr<FString>(Player);
		IsleStructs::ATICharacterBase* Character = static_cast<IsleStructs::ATICharacterBase*>(Pawn);
		int32 DinoId = *_DinoClassID->ContainerPtrToValuePtr<int32>(Character);
		Output::send(STR("Attempt to save dino for owner: {}, dinoid: {}"), *SteamId, DinoId);

		IsleStructs::TScopedFunctionParams<IsleStructs::FGetCharacterDataParams> GetCharacterDataParams(_GetCharacterData);
		GetCharacterDataParams->Character = Character;
		GetCharacterDataParams->bForceSafe = false;
		_TISaveManager->ProcessEvent(_GetCharacterData, GetCharacterDataParams.Get());

		IsleStructs::TScopedFunctionParams<IsleStructs::FPlayerDataToStringParams> PlayerDataToStringParams(_PlayerDataToString);
		PlayerDataToStringParams->PlayerData = GetCharacterDataParams->ReturnValue;
		_TISaveManager->ProcessEvent(_PlayerDataToString, PlayerDataToStringParams.Get());
		FString Result = PlayerDataToStringParams->ReturnValue;

		if(DataBaseConnector::SaveDino(SteamId, DinoId, Result, 0)) {
			IsleStructs::FSetHealthParams SetHealthParams{0};
			Character->ProcessEvent(_SetHealth, &SetHealthParams);
			IsleStructs::FSetWaitAndDestroyCorpseParams CorpseParams{100};
			Character->ProcessEvent(_WaitAndDestroyCorpse, &CorpseParams);
			Output::send(STR("Dino removed from game, owner: {}, dinoid: {}"), *SteamId, DinoId);
		} else {
			Output::send<LogLevel::Error>(STR("Failed to save dino for owner: {}, dinoid: {}"), *SteamId, DinoId);
		}
	}

	auto LoadDino(IsleStructs::ATIPlayerController* Player, int32 DinoId) -> void {
		auto* GameMode = UObjectGlobals::FindFirstOf(STR("BP_SurvivalGameMode_C"));
		if (!GameMode) return;

		FString SteamId = *_PlayerSteamId->ContainerPtrToValuePtr<FString>(Player);
		Output::send(STR("Attempt to load dino for owner: {}, dinoid: {}"), *SteamId, DinoId);

		FString Requested;
		if(!DataBaseConnector::LoadDino(SteamId, DinoId, Requested)) {
			Output::send<LogLevel::Error>(STR("Failed to load dino for owner: {}, dinoid: {}"), *SteamId, DinoId);
			return;
		};

		IsleStructs::TScopedFunctionParams<IsleStructs::FStringToPlayerDataParams> StringToPlayerDataParams(_StringToPlayerData);
		StringToPlayerDataParams->String = Requested;
		_TISaveManager->ProcessEvent(_StringToPlayerData, StringToPlayerDataParams.Get());
		IsleStructs::FTIPlayerData PlayerData = StringToPlayerDataParams->ReturnValue;

		IsleStructs::TScopedFunctionParams<IsleStructs::FAddSpawnRequestParams> AddSpawnRequestParams(_AddSpawnRequest);
		AddSpawnRequestParams->SpawnData.RequestTime = 0.0f;
		AddSpawnRequestParams->SpawnData.Controller = Player;
		AddSpawnRequestParams->SpawnData.SteamId = SteamId;
		AddSpawnRequestParams->SpawnData.Class = PlayerData.Class;
		AddSpawnRequestParams->SpawnData.bLoadOnly = true;
		AddSpawnRequestParams->SpawnData.SpawnLocation = PlayerData.Location;
		AddSpawnRequestParams->SpawnData.CustomizerData = PlayerData.CustomizedData;
		AddSpawnRequestParams->SpawnData.PlayerData = PlayerData;
		AddSpawnRequestParams->bAddHud = false;
		AddSpawnRequestParams->bLoadSaved = true;
		GameMode->ProcessEvent(_AddSpawnRequest, AddSpawnRequestParams.Get());

		if (!DataBaseConnector::ConfirmLoadDino(SteamId, DinoId)) {
			Output::send<LogLevel::Error>(STR("Failed to update dino status for owner: {}, dinoid: {}"), *SteamId, DinoId);
		}
		Output::send(STR("Loaded dino for owner: {}, dinoid: {}"), *SteamId, DinoId);
	}

	auto HandleChatMessage(UnrealScriptFunctionCallableContext& FuncContext) -> void {
		IsleStructs::EChatMode ChatMode = *_FuncParamPropChatMode->ContainerPtrToValuePtr<IsleStructs::EChatMode>(FuncContext.TheStack.Locals());
		FText* FTextMessage = _FuncParamPropMessage->ContainerPtrToValuePtr<FText>(FuncContext.TheStack.Locals());
		StringType Message = FTextMessage->ToString();
		if (!Message.starts_with(STR("!")) || ChatMode != IsleStructs::EChatMode::Spatial) return;

		constexpr auto Prefix = STR("!load ");
		if (Message == STR("!store")) {
			SaveDino(static_cast<IsleStructs::ATIPlayerController*>(FuncContext.Context));
		} else if (Message.starts_with(Prefix)) {
			StringType IdString = Message.substr(FCString::Strlen(Prefix));
			int32 DinoId;
			auto Utf8 = to_string(IdString);
			auto [ptr, ec] = std::from_chars(Utf8.data(), Utf8.data() + Utf8.size(), DinoId);
			if (ec != std::errc() || ptr != Utf8.data() + Utf8.size()) return;
			LoadDino(static_cast<IsleStructs::ATIPlayerController*>(FuncContext.Context), DinoId);
		}
	}

	static UFunction* GetChatMessage = nullptr;
	static int32_t HookId;
	auto Initialize(StorageSystemConfig::StorageConfig Config) -> void {
		if (!DataBaseConnector::Initialize(Config.Database)) {
			Output::send<LogLevel::Error>(STR("DB connection failed, con string: {}"), to_wstring(Config.Database));
		}
		_PlayerControllerBaseClass = UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, STR("/Script/TheIsle.TIPlayerController"));
		_PlayerControllerPawn = _PlayerControllerBaseClass->GetPropertyByNameInChain(STR("Pawn"));
		_PlayerSteamId = _PlayerControllerBaseClass->GetPropertyByNameInChain(STR("SteamId"));

		_DinoClass = UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, STR("/Script/TheIsle.TIDinosaurBase"));
		_DinoClassID = _DinoClass->GetPropertyByNameInChain(STR("ID"));

		_GetCharacterData = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TISaveManager:GetCharacterData"));
		_GetCharacterData_BufferSize = _GetCharacterData->GetParmsSize();
		_GetCharacterData_PropCharacter = _GetCharacterData->GetPropertyByNameInChain(STR("Character"));
		_GetCharacterData_PropForceSafe = _GetCharacterData->GetPropertyByNameInChain(STR("bForceSafe"));
		_GetCharacterData_ReturnProperty = _GetCharacterData->GetReturnProperty();

		_PlayerDataToString = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TISaveManager:PlayerDataToString"));
		_PlayerDataToString_BufferSize = _PlayerDataToString->GetParmsSize();
		_PlayerDataToString_PropPlayerData = _PlayerDataToString->GetPropertyByNameInChain(STR("PlayerData"));
		_PlayerDataToString_ReturnProperty = _PlayerDataToString->GetReturnProperty();

		_StringToPlayerData = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TISaveManager:StringToPlayerData"));
		_StringToPlayerData_BufferSize = _StringToPlayerData->GetParmsSize();
		_StringToPlayerData_PropString = _StringToPlayerData->GetPropertyByNameInChain(STR("String"));
		_StringToPlayerData_ReturnProperty = _StringToPlayerData->GetReturnProperty();
		
		_AddSpawnRequest = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TIGameModeBase:AddSpawnRequest"));
		_AddSpawnRequest_BufferSize = _AddSpawnRequest->GetParmsSize();
		_AddSpawnRequest_PropSpawnData = _AddSpawnRequest->GetPropertyByNameInChain(STR("SpawnData"));
		_AddSpawnRequest_PropbAddHud = _AddSpawnRequest->GetPropertyByNameInChain(STR("bAddHud"));
		_AddSpawnRequest_PropbLoadSaved = _AddSpawnRequest->GetPropertyByNameInChain(STR("bLoadSaved"));

		_TISaveManager = UObjectGlobals::StaticFindObject<IsleStructs::UTISaveManager*>(nullptr, nullptr, STR("/Script/TheIsle.Default__TISaveManager"), false);
		
		_SetHealth = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TICharacterBase:SetHealth"));// Safe way to kill it
		_WaitAndDestroyCorpse = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TICharacterBase:WaitAndDestroyCorpse"));// Safe way to clean up it

		GetChatMessage = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TIPlayerController:GetChatMessage"));
		_FuncParamPropChatMode = GetChatMessage->GetPropertyByNameInChain(STR("ChatMode"));
		_FuncParamPropMessage = GetChatMessage->GetPropertyByNameInChain(STR("NoFilterMsg"));

		HookId = GetChatMessage->RegisterPostHook(
			[](UnrealScriptFunctionCallableContext& FuncContext, void*) -> void {HandleChatMessage(FuncContext);
		});
	}

	auto Destroy() -> void {
		DataBaseConnector::Destroy();
		if (!HookId) return;
		GetChatMessage->UnregisterHook(HookId);
	}
}