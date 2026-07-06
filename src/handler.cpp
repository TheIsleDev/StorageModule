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

#include <Reflection/_include_custom.hpp>

#include "_structs.hpp"

namespace StorageSystemComponent {
	using namespace RC::Unreal;

	static UClass* PlayerControllerBaseClass{};
	static FProperty* PlayerControllerPawn{};
	static FProperty* PlayerSteamID{};

	static UClass* DinoClass{};
	static FProperty* DinoClassID{};

	static UClass* CharacterClass{};

	static UFunction* _GetCharacterData{};
	static uint32 _GetCharacterData_BufferSize;
	static FProperty* _GetCharacterData_PropCharacter{};
	static FProperty* _GetCharacterData_PropForceSafe{};
	static FProperty* _GetCharacterData_ReturnProperty{};

	static UFunction* _PlayerDataToString{};
	static uint32 _PlayerDataToString_BufferSize;
	static FProperty* _PlayerDataToString_PropPlayerData{};
	static FProperty* _PlayerDataToString_ReturnProperty{};

	static StorageSystemConfiguration::DataOfGetCharacterData GetCharacterData{};
	static StorageSystemConfiguration::DataOfPlayerDataToString PlayerDataToString{};
	static StorageSystemConfiguration::DataOfStringToPlayerData StringToPlayerData{};
	static StorageSystemConfiguration::DataOfAddSpawnRequest AddSpawnRequest{};

	static IsleStructs::UTISaveManager* _TISaveManager{};

	static FProperty* _FuncParamPropChatMode{};
	static FProperty* _FuncParamPropMessage{};

	auto SaveDino(ATIPlayerController* Player) -> void {
		static ATIGameModeBase* GameMode = static_cast<ATIGameModeBase*>(UObjectGlobals::FindFirstOf(STR("BP_SurvivalGameMode_C")));

		APawn* Pawn = *PlayerControllerPawn->ContainerPtrToValuePtr<APawn*>(Player);
		if (!Pawn || !Pawn->IsA(DinoClass)) return;

		FString SteamID = *PlayerSteamID->ContainerPtrToValuePtr<FString>(Player);
		ATICharacterBase* Character = static_cast<ATICharacterBase*>(Pawn);
		int32 DinoID = *DinoClassID->ContainerPtrToValuePtr<int32>(Character);
		// Some more requirements, you can even add them to settings
		RC::Output::send(STR("Attempt to save dino for owner: {}, dinoid: {}"), *SteamID, DinoID);

		StructsParams::FProcessEventParams FirstParams(GetCharacterData.Function, GetCharacterData.BufferSize);
		FirstParams.Set(GetCharacterData.Character, Character);
		FirstParams.Set(GetCharacterData.bForceSafe, false);
		_TISaveManager->ProcessEvent(GetCharacterData.Function, FirstParams.Data());

		StructsParams::FProcessEventParams SecondParams(PlayerDataToString.Function, PlayerDataToString.BufferSize);
		SecondParams.CopyFromAddress(SecondParams.Data(), PlayerDataToString.PlayerData, FirstParams.GetAddress<void>(GetCharacterData.ReturnValue));
		_TISaveManager->ProcessEvent(PlayerDataToString.Function, SecondParams.Data());

		FString Result = *SecondParams.GetAddress<FString>(PlayerDataToString.ReturnValue);
		if(DataBaseConnector::SaveDino(SteamID, DinoID, Result, false)) {
			Character->SetHealth(0);
			GameMode->TryToRespawn(Player, SteamID);
			Character->DestroyCorpse();

			RC::Output::send(STR("Dino removed from game, owner: {}, dinoid: {}"), *SteamID, DinoID);
		} else {
			RC::Output::send<RC::LogLevel::Error>(STR("Failed to save dino for owner: {}, dinoid: {}"), *SteamID, DinoID);
		}
	}

	// Remember, if you try to load dino with ID that already exist in world, youll be fucked. Joking, it just wont load.
	auto LoadDino(ATIPlayerController* Player, int32 DinoID) -> void {
		static ATIGameModeBase* GameMode = static_cast<ATIGameModeBase*>(UObjectGlobals::FindFirstOf(STR("BP_SurvivalGameMode_C")));

		FString SteamID = *PlayerSteamID->ContainerPtrToValuePtr<FString>(Player);
		RC::Output::send(STR("Attempt to load dino for owner: {}, dinoid: {}"), *SteamID, DinoID);

		FString Requested;
		if(!DataBaseConnector::LoadDino(SteamID, DinoID, Requested)) {
			RC::Output::send<RC::LogLevel::Error>(STR("Failed to load dino for owner: {}, dinoid: {}"), *SteamID, DinoID);
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
		SecondParams.SetInContainer(Container, AddSpawnRequest.SpawnData_SteamId, SteamID);
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

		APawn* Pawn = *PlayerControllerPawn->ContainerPtrToValuePtr<APawn*>(Player);
		if (Pawn && Pawn->IsA(CharacterClass)) {
			ATICharacterBase* Character = static_cast<ATICharacterBase*>(Pawn);
			Character->SetHealth(0);
			Character->DestroyCorpse();
		}

		GameMode->TryToRespawn(Player, SteamID);// Magic WORD! Sometimes you just have to believe me
		GameMode->ProcessEvent(AddSpawnRequest.Function, SecondParams.Data());

		if (!DataBaseConnector::ConfirmLoadDino(DinoID)) {// For now I don't care for tracking load, if it really loaded or not. You can add later to check Id field on dino, its enough
			RC::Output::send<RC::LogLevel::Error>(STR("Failed to update dino status for owner: {}, dinoid: {}"), *SteamID, DinoID);
		}
		RC::Output::send(STR("Loaded dino for owner: {}, dinoid: {}"), *SteamID, DinoID);
	}

	auto HandleChatMessage(UnrealScriptFunctionCallableContext& FuncContext) -> void {
		auto FuncLocals = FuncContext.TheStack.Locals();
		IsleStructs::EChatMode ChatMode = *_FuncParamPropChatMode->ContainerPtrToValuePtr<IsleStructs::EChatMode>(FuncLocals);
		if (ChatMode != IsleStructs::EChatMode::Spatial) return;// Early return, it's not a 32bit, game one thread,
		FText* FTextMessage = _FuncParamPropMessage->ContainerPtrToValuePtr<FText>(FuncLocals);
		RC::StringType Message = FTextMessage->ToString();
		if (!Message.starts_with(STR("!"))) return;// server side render, but I still have opti flashbacks

		constexpr auto Prefix = STR("!load ");
		if (Message == STR("!store")) {
			SaveDino(static_cast<ATIPlayerController*>(FuncContext.Context));
		} else if (Message.starts_with(Prefix)) {
			RC::StringType IdString = Message.substr(FCString::Strlen(Prefix));
			int32 DinoID;// Dont judge, 3 lines below this one is just LLM slopcode.
			auto Utf8 = RC::to_string(IdString);
			auto [ptr, ec] = std::from_chars(Utf8.data(), Utf8.data() + Utf8.size(), DinoID);
			if (ec != std::errc() || ptr != Utf8.data() + Utf8.size()) return;
			LoadDino(static_cast<ATIPlayerController*>(FuncContext.Context), DinoID);
		}
	}

	static UFunction* GetChatMessage{};
	static int32_t HookID{};
	auto Initialize(StorageSystemConfiguration::StorageConfig Config) -> void {
		if (!DataBaseConnector::Initialize(Config.Database)) {
			RC::Output::send<RC::LogLevel::Error>(STR("DB connection failed, con string: {}"), RC::to_wstring(Config.Database));
		} else DataBaseConnector::PrepareStorage();

		PlayerControllerBaseClass = UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, STR("/Script/TheIsle.TIPlayerController"));
		PlayerControllerPawn = PlayerControllerBaseClass->GetPropertyByNameInChain(STR("Pawn"));
		PlayerSteamID = PlayerControllerBaseClass->GetPropertyByNameInChain(STR("SteamId"));

		DinoClass = UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, STR("/Script/TheIsle.TIDinosaurBase"));
		DinoClassID = DinoClass->GetPropertyByNameInChain(STR("ID"));

		CharacterClass = UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, STR("/Script/TheIsle.TICharacterBase"));

		GetCharacterData.Function = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TISaveManager:GetCharacterData"));
		GetCharacterData.Initialize();
		PlayerDataToString.Function = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TISaveManager:PlayerDataToString"));
		PlayerDataToString.Initialize();
		StringToPlayerData.Function = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TISaveManager:StringToPlayerData"));
		StringToPlayerData.Initialize();

		_TISaveManager = UObjectGlobals::StaticFindObject<IsleStructs::UTISaveManager*>(nullptr, nullptr, STR("/Script/TheIsle.Default__TISaveManager"), false);

		GetChatMessage = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TIPlayerController:GetChatMessage"));
		_FuncParamPropChatMode = GetChatMessage->GetPropertyByNameInChain(STR("ChatMode"));
		_FuncParamPropMessage = GetChatMessage->GetPropertyByNameInChain(STR("NoFilterMsg"));

		HookID = GetChatMessage->RegisterPostHook(
			[](UnrealScriptFunctionCallableContext& FuncContext, void*) -> void {HandleChatMessage(FuncContext);
		});
	}

	auto Destroy() -> void {
		DataBaseConnector::Destroy();
		if (!HookID) return;// Idk if this is possible in prod to hit case where we dont get it, however there no safe checks down the road, so better to safe check here
		GetChatMessage->UnregisterHook(HookID);
	}
}