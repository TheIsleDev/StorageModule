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

	static StorageSystemConfiguration::DataOfGetCharacterData GetCharacterData{};
	static StorageSystemConfiguration::DataOfPlayerDataToString PlayerDataToString{};
	static StorageSystemConfiguration::DataOfStringToPlayerData StringToPlayerData{};
	static StorageSystemConfiguration::DataOfAddSpawnRequest AddSpawnRequest{};

	static IsleStructs::UTISaveManager* _TISaveManager = nullptr;

	static UFunction* _TryToRespawn = nullptr;
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
		// Some more requirements, you can even add them to settings
		Output::send(STR("Attempt to save dino for owner: {}, dinoid: {}"), *SteamId, DinoId);

		StructsParams::FProcessEventParams FirstParams(GetCharacterData.Function, GetCharacterData.BufferSize);
		FirstParams.Set(GetCharacterData.Character, Character);
		FirstParams.Set(GetCharacterData.bForceSafe, false);
		_TISaveManager->ProcessEvent(GetCharacterData.Function, FirstParams.Data());

		StructsParams::FProcessEventParams SecondParams(PlayerDataToString.Function, PlayerDataToString.BufferSize);
		SecondParams.CopyFromAddress(SecondParams.Data(), PlayerDataToString.PlayerData, FirstParams.GetAddress<void>(GetCharacterData.ReturnValue));
		_TISaveManager->ProcessEvent(PlayerDataToString.Function, SecondParams.Data());

		FString Result = *SecondParams.GetAddress<FString>(PlayerDataToString.ReturnValue);
		if(DataBaseConnector::SaveDino(SteamId, DinoId, Result, false)) {
			IsleStructs::FSetHealthParams SetHealthParams{0};
			Character->ProcessEvent(_SetHealth, &SetHealthParams);
			IsleStructs::FSetWaitAndDestroyCorpseParams CorpseParams{2400};
			Character->ProcessEvent(_WaitAndDestroyCorpse, &CorpseParams);
			Output::send(STR("Dino removed from game, owner: {}, dinoid: {}"), *SteamId, DinoId);
		} else {
			Output::send<LogLevel::Error>(STR("Failed to save dino for owner: {}, dinoid: {}"), *SteamId, DinoId);
		}
	}

	// Remember, if you try to load dino with ID that already exist in world, youll be fucked. Joking, it just wont load.
	auto LoadDino(IsleStructs::ATIPlayerController* Player, int32 DinoId) -> void {
		auto* GameMode = UObjectGlobals::FindFirstOf(STR("BP_SurvivalGameMode_C"));// Idk why I don't cache it, me is dum dum.
		if (!GameMode) return;

		FString SteamId = *_PlayerSteamId->ContainerPtrToValuePtr<FString>(Player);
		Output::send(STR("Attempt to load dino for owner: {}, dinoid: {}"), *SteamId, DinoId);

		FString Requested;
		if(!DataBaseConnector::LoadDino(SteamId, DinoId, Requested)) {
			Output::send<LogLevel::Error>(STR("Failed to load dino for owner: {}, dinoid: {}"), *SteamId, DinoId);
			return;
		};

		// You can add here checks later for same dino type and etc.
		StructsParams::FProcessEventParams FirstParams(StringToPlayerData.Function, StringToPlayerData.BufferSize);
		FirstParams.Set(StringToPlayerData.String, Requested);
		_TISaveManager->ProcessEvent(StringToPlayerData.Function, FirstParams.Data());

		void* PlayerData = FirstParams.GetAddress<void>(StringToPlayerData.ReturnValue);
		void* Class = FirstParams.GetAddressInContainer<void>(PlayerData, StringToPlayerData.PlayerData_Class);
		void* Location = FirstParams.GetAddressInContainer<void>(PlayerData, StringToPlayerData.PlayerData_Location);
		void* CustomizedData = FirstParams.GetAddressInContainer<void>(PlayerData, StringToPlayerData.PlayerData_CustomizedData);
	
		StructsParams::FProcessEventParams SecondParams(AddSpawnRequest.Function, AddSpawnRequest.BufferSize);
		void* Container = SecondParams.GetAddress<void>(AddSpawnRequest.SpawnData);
		SecondParams.SetInContainer(Container, AddSpawnRequest.SpawnData_Controller, Player);
		SecondParams.SetInContainer(Container, AddSpawnRequest.SpawnData_SteamId, SteamId);
		SecondParams.CopyFromAddress(Container, AddSpawnRequest.SpawnData_Class, Class);
		SecondParams.SetInContainer(Container, AddSpawnRequest.SpawnData_bKeepActualLoc, false);// Only this one is safe to change out of all bools
		SecondParams.SetInContainer(Container, AddSpawnRequest.SpawnData_bLoadOnly, false);// DO | DO | DO
		SecondParams.SetInContainer(Container, AddSpawnRequest.SpawnData_bDefaultSpawn, false);// NOT | NOT | NOT
		SecondParams.CopyFromAddress(Container, AddSpawnRequest.SpawnData_SpawnLocation, Location);
		SecondParams.CopyFromAddress(Container, AddSpawnRequest.SpawnData_CustomizerData, CustomizedData);
		SecondParams.SetInContainer(Container, AddSpawnRequest.SpawnData_bUseQuickRespawn, false);// REMOVE | ASK QUESTIONS | TRY TO FIGURE OUT WHY
		SecondParams.CopyFromAddress(Container, AddSpawnRequest.SpawnData_PlayerData, PlayerData);
		SecondParams.Set(AddSpawnRequest.bAddHud, true);
		SecondParams.Set(AddSpawnRequest.bLoadSaved, true);

		IsleStructs::FTryToRespawnParams SetTryToRespawnParams{Player, SteamId};
		GameMode->ProcessEvent(_TryToRespawn, &SetTryToRespawnParams);// Magic WORD! Sometimes you just have to belive me
		GameMode->ProcessEvent(AddSpawnRequest.Function, SecondParams.Data());

		if (!DataBaseConnector::ConfirmLoadDino(SteamId, DinoId)) {// For now I don't care for tracking load, if it really loaded or not. You can add later to check Id field on dino, its enough
			Output::send<LogLevel::Error>(STR("Failed to update dino status for owner: {}, dinoid: {}"), *SteamId, DinoId);
		}
		Output::send(STR("Loaded dino for owner: {}, dinoid: {}"), *SteamId, DinoId);
	}

	auto HandleChatMessage(UnrealScriptFunctionCallableContext& FuncContext) -> void {
		auto FuncLocals = FuncContext.TheStack.Locals();
		IsleStructs::EChatMode ChatMode = *_FuncParamPropChatMode->ContainerPtrToValuePtr<IsleStructs::EChatMode>(FuncLocals);
		if (ChatMode != IsleStructs::EChatMode::Spatial) return;// Early return, it's not a 32bit, game one thread,
		FText* FTextMessage = _FuncParamPropMessage->ContainerPtrToValuePtr<FText>(FuncLocals);
		StringType Message = FTextMessage->ToString();
		if (!Message.starts_with(STR("!"))) return;// server side render, but I still have opti flashbacks

		constexpr auto Prefix = STR("!load ");
		if (Message == STR("!store")) {
			SaveDino(static_cast<IsleStructs::ATIPlayerController*>(FuncContext.Context));
		} else if (Message.starts_with(Prefix)) {
			StringType IdString = Message.substr(FCString::Strlen(Prefix));
			int32 DinoId;// Dont judge, 3 lines below this one is just LLM slopcode.
			auto Utf8 = to_string(IdString);
			auto [ptr, ec] = std::from_chars(Utf8.data(), Utf8.data() + Utf8.size(), DinoId);
			if (ec != std::errc() || ptr != Utf8.data() + Utf8.size()) return;
			LoadDino(static_cast<IsleStructs::ATIPlayerController*>(FuncContext.Context), DinoId);
		}
	}

	static UFunction* GetChatMessage = nullptr;
	static int32_t HookId;
	auto Initialize(StorageSystemConfiguration::StorageConfig Config) -> void {
		if (!DataBaseConnector::Initialize(Config.Database)) {
			Output::send<LogLevel::Error>(STR("DB connection failed, con string: {}"), to_wstring(Config.Database));
		}

		_PlayerControllerBaseClass = UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, STR("/Script/TheIsle.TIPlayerController"));
		_PlayerControllerPawn = _PlayerControllerBaseClass->GetPropertyByNameInChain(STR("Pawn"));
		_PlayerSteamId = _PlayerControllerBaseClass->GetPropertyByNameInChain(STR("SteamId"));

		_DinoClass = UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, STR("/Script/TheIsle.TIDinosaurBase"));
		_DinoClassID = _DinoClass->GetPropertyByNameInChain(STR("ID"));

		GetCharacterData.Function = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TISaveManager:GetCharacterData"));
		GetCharacterData.Initialize();
		PlayerDataToString.Function = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TISaveManager:PlayerDataToString"));
		PlayerDataToString.Initialize();
		StringToPlayerData.Function = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TISaveManager:StringToPlayerData"));
		StringToPlayerData.Initialize();
		AddSpawnRequest.Function = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TIGameModeBase:AddSpawnRequest"));
		AddSpawnRequest.Initialize();

		_TISaveManager = UObjectGlobals::StaticFindObject<IsleStructs::UTISaveManager*>(nullptr, nullptr, STR("/Script/TheIsle.Default__TISaveManager"), false);

		_TryToRespawn = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TIGameModeBase:TryToRespawn"));
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
		if (!HookId) return;// Idk if this is possible in prod to hit case where we dont get it, however there no safe checks down the road, so better to safe check here
		GetChatMessage->UnregisterHook(HookId);
	}
}