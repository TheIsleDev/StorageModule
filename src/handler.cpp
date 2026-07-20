#include <cstdint>

#include <DynamicOutput/Output.hpp>
#include <DynamicOutput/OutputDevice.hpp>

#include <Unreal/AActor.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/Core/Containers/ContainerAllocationPolicies.hpp>

#include <DBLink/database.cpp>
#include <Reflection/_include_custom.hpp>

#include "_structs.hpp"

// Declare local debugging if you want more logging on test run
//#define LOCAL_DEBUGGING
namespace StorageSystemComponent {
	using namespace RC::Unreal;

	static StorageSystemConfiguration::StorageConfig LoadedConfig;

	static FProperty* GetChatMessagePropChatMode{};
	static FProperty* GetChatMessagePropMessage{};

	auto doChecks(ATIDinosaurBase* Dinosaur, ATIPlayerController* Player) -> bool {
		if (LoadedConfig.HealthCheck) {
			if (Dinosaur->FGetHealth() != Dinosaur->GetMaxHealth()) {
				Player->ClientShowNotification(FText(STR("You need full health")));
				return false;
			}
		}
		if (LoadedConfig.StaminaCheck) {
			if (Dinosaur->FGetStamina() != Dinosaur->GetMaxStamina()) {
				Player->ClientShowNotification(FText(STR("You need full stamina")));
				return false;
			}

		}
		if (LoadedConfig.BloodCheck) {
			if (Dinosaur->FGetBlood() != Dinosaur->GetMaxBlood()) {
				Player->ClientShowNotification(FText(STR("You need full blood")));
				return false;
			}
		}
		return true;
	}

	auto StoreDino(ATIPlayerController* Player) -> void {
		static ATIGameModeBase* GameMode = static_cast<ATIGameModeBase*>(UObjectGlobals::FindFirstOf(STR("BP_SurvivalGameMode_C")));

		APawn* Pawn = Player->GetPawn();
		if (!Pawn || !Pawn->IsA(ATIDinosaurBase::StaticClass())) return;

		ATIDinosaurBase* Dinosaur = static_cast<ATIDinosaurBase*>(Pawn);
		if (LoadedConfig.GrowthRequirementSave > Dinosaur->GetGrowth()) {
			Player->ClientShowNotification(FText(STR("Growth requirement not meet")));
			return;
		};

		if (!doChecks(Dinosaur, Player)) return;

		FString SteamID = Player->GetSteamId();
		int32 DinoID = Dinosaur->GetID();
#ifdef LOCAL_DEBUGGING
		RC::Output::send(STR("Attempt to save Dinosaur for SteamID: {}, DinoID: {}"), *SteamID, DinoID);
#endif
		int64_t Out{};
		if(DataBaseConnector::SaveDino(Dinosaur, false, false, Out)) {
			Player->ClientShowNotification(FText(fmt::format(STR("Saved DinoID: {}"), Out)));
			Dinosaur->SetHealth(0);
			GameMode->TryToRespawn(Player, SteamID);
			Dinosaur->DestroyCorpse();
#ifdef LOCAL_DEBUGGING
			RC::Output::send(STR("Dinosaur removed from game, SteamID: {}, DinoID: {}"), *SteamID, DinoID);
#endif
		} else {
#ifdef LOCAL_DEBUGGING
			RC::Output::send<RC::LogLevel::Error>(STR("Failed to save dino for SteamID: {}, DinoID: {}"), *SteamID, DinoID);
#endif
		}
	}

	// Remember, if you try to load dino with ID that already exist in world, youll be fucked. Joking, it just wont load.
	auto LoadDino(ATIPlayerController* Player, int32 DinoID) -> void {
		static ATIGameModeBase* GameMode = static_cast<ATIGameModeBase*>(UObjectGlobals::FindFirstOf(STR("BP_SurvivalGameMode_C")));

		FString SteamID = Player->GetSteamId();
#ifdef LOCAL_DEBUGGING
		RC::Output::send(STR("Attempt to load dino for SteamID: {}, DinoID: {}"), *SteamID, DinoID);
#endif

		APawn* Pawn = Player->GetPawn();
		ATIDinosaurBase* Dinosaur{};
		if (Pawn && Pawn->IsA(ATIDinosaurBase::StaticClass())) {
			Dinosaur = static_cast<ATIDinosaurBase*>(Pawn);
			if (Dinosaur->GetGrowth() > LoadedConfig.GrowthRequirementLoad) {
				Player->ClientShowNotification(FText(STR("Growth requirement not meet")));
				return;
			};
		}

		if (Dinosaur && !doChecks(Dinosaur, Player)) return;

		FString Requested;
		if(!DataBaseConnector::LoadDino(SteamID, DinoID, Requested)) {
#ifdef LOCAL_DEBUGGING
			RC::Output::send<RC::LogLevel::Error>(STR("Failed to load dino for SteamID: {}, DinoID: {}"), *SteamID, DinoID);
#endif
			return;
		};

		FTIPlayerData PlayerData = UTISaveManager::StringToPlayerData(Requested);
		if (LoadedConfig.SameClass && Dinosaur) {
			if (PlayerData.GetClass() != FString(Dinosaur->GetClassPrivate()->GetPathName())) {
				Player->ClientShowNotification(FText(STR("Only same type of dino")));
				return;
			};
		}

		FTISpawnData SpawnData;
		SpawnData.New();
		FVector Location{};
		if (LoadedConfig.PreserveLoc) {
			Location = Dinosaur->K2_GetActorLocation();
			PlayerData.GetLocation() = Location;
		}
		else Location = PlayerData.GetLocation();

		SpawnData.GetController() = Player;
		SpawnData.GetSteamId() = SteamID;
		SpawnData.GetClass() = PlayerData.GetClass();
		SpawnData.GetbKeepActualLoc() = LoadedConfig.PreserveLoc;// Safe to change
		SpawnData.GetbLoadOnly() = false;// don't change
		SpawnData.GetbDefaultSpawn() = false;// don't change
		SpawnData.GetSpawnLocation() = Location;
		SpawnData.GetCustomizerData() = PlayerData.GetCustomizedData();
		SpawnData.GetbUseQuickRespawn() = false;// don't change
		SpawnData.GetbPreserveSpawnLocation() = LoadedConfig.PreserveLoc;// Safe to change
		SpawnData.GetPlayerData() = PlayerData;

		if (Dinosaur) {
			DataBaseConnector::SaveDino(Dinosaur, false, true);
			Dinosaur->SetHealth(0);
			Dinosaur->DestroyCorpse();
		}

		GameMode->TryToRespawn(Player, SteamID);// Magic WORD! Sometimes you just have to believe me
		GameMode->AddSpawnRequest(SpawnData, true, true);

		Dinosaur = static_cast<ATIDinosaurBase*>(Player->GetPawn());
		if (!Dinosaur || Dinosaur->GetID() != DinoID) {
#ifdef LOCAL_DEBUGGING
			RC::Output::send<RC::LogLevel::Error>(STR("Failed to load dino for SteamID: {}, DinoID: {}"), *SteamID, DinoID);
#endif
			return;
		}

		Player->ClientShowNotification(FText(fmt::format(STR("Loaded DinoID: {}"), DinoID)));
		if (LoadedConfig.LockOnLoad && !DataBaseConnector::ConfirmLoadDino(DinoID)) {
#ifdef LOCAL_DEBUGGING
			RC::Output::send<RC::LogLevel::Error>(STR("Failed to update dino status for SteamID: {}, DinoID: {}"), *SteamID, DinoID);
#endif
		}
#ifdef LOCAL_DEBUGGING
		RC::Output::send(STR("Loaded dino for SteamID: {}, DinoID: {}"), *SteamID, DinoID);
#endif
	}

	auto HandleChatMessage(UnrealScriptFunctionCallableContext& FuncContext) -> void {
		auto FuncLocals = FuncContext.TheStack.Locals();
		EChatMode ChatMode = *GetChatMessagePropChatMode->ContainerPtrToValuePtr<EChatMode>(FuncLocals);
		if (ChatMode != EChatMode::Spatial) return;// Early return, it's not a 32bit, game one thread,

		FText* FTextMessage = GetChatMessagePropMessage->ContainerPtrToValuePtr<FText>(FuncLocals);
		RC::StringType Message = FTextMessage->ToString();
		if (!Message.starts_with(STR("!"))) return;// server side render, but I still have opti flashbacks

		constexpr auto Prefix = STR("!load ");
		if (Message == STR("!store")) {
			StoreDino(static_cast<ATIPlayerController*>(FuncContext.Context));
		} else if (Message.starts_with(Prefix)) {
			if (!LoadedConfig.OverrideDino) return;

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
		LoadedConfig = Config;
		if (DataBaseConnector::Initialize(Config.Database)) DataBaseConnector::PrepareStorage();
#ifdef LOCAL_DEBUGGING
		else RC::Output::send<RC::LogLevel::Error>(STR("DB connection failed, con string: {}"), RC::to_wstring(Config.Database));
#endif

		// I hope in future I can find a way to skip this dummy Prop lookup and get lock and ready direct values out of the box
		GetChatMessage = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TIPlayerController:GetChatMessage"));
		GetChatMessagePropChatMode = GetChatMessage->GetPropertyByNameInChain(STR("ChatMode"));
		GetChatMessagePropMessage = GetChatMessage->GetPropertyByNameInChain(STR("NoFilterMsg"));

		// Maybe precallback + clean message later? idk... I think I need special handler for chat commands to skip resending them to other clients.
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