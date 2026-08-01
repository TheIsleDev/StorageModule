
// Declare local debugging if you want more logging on test run
//#define LOCAL_DEBUGGING

#include <TheIsleHelpers/ConfigReader.hpp>
#include <TheIsle/APawn.hpp>
#include <TheIsle/ATIGameModeBase.hpp>
#include <TheIsle/_simple_structs.hpp>
#include <Storage.hpp>


StorageSystem::StorageSystem() {
    ModName = STR("Storage");
    ModVersion = STR("1.0.2");
    ModDescription = STR("Hehe");
    ModAuthors = STR("Shiza");

	RC::ConfigLoader::LoadModConfig(&Config);
}

StorageSystem::~StorageSystem() {
	GetChatMessage->UnregisterHook(HookID);
}


void StorageSystem::on_unreal_init() {
	static RC::DataBase::DataBase DatabaseLink{Config.Database};
	Database = &DatabaseLink;

	// I hope in future I can find a way to skip this dummy Prop lookup and get lock and ready direct values out of the box
	GetChatMessage = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TIPlayerController:GetChatMessage"));

	// Maybe precallback + clean message later? idk... I think I need special handler for chat commands to skip resending them to other clients.
	HookID = GetChatMessage->RegisterPreHook(
		[this](UnrealScriptFunctionCallableContext& Context, void* CustomData) {
			HandleChatMessage(Context);
		}
	);
}

void StorageSystem::HandleChatMessage(UnrealScriptFunctionCallableContext& FuncContext) {
	GetChatMessageData* Data = std::bit_cast<GetChatMessageData*>(FuncContext.TheStack.Locals());
	if (Data->ChatMode != EChatMode::Spatial) return;// Не тот тип, скипаем

	RC::StringType Message = Data->NoFilterMsg.ToString();
	if (!Message.starts_with(STR("/"))) return;// Во бля то бля, лукап символов хуйни

	constexpr auto Prefix = STR("/load ");
	if (Message == STR("/store")) {
		StoreDino(static_cast<ATIPlayerController*>(FuncContext.Context));
	} else if (Message.starts_with(Prefix)) {
		if (!Config.OverrideDino) return;

		RC::StringType IdString = Message.substr(FCString::Strlen(Prefix));
		int32 DinoID;// Dont judge, 3 lines below this one is just LLM slopcode.
		auto Utf8 = RC::to_string(IdString);
		auto [ptr, ec] = std::from_chars(Utf8.data(), Utf8.data() + Utf8.size(), DinoID);
		if (ec != std::errc() || ptr != Utf8.data() + Utf8.size()) return;

		LoadDino(static_cast<ATIPlayerController*>(FuncContext.Context), DinoID);
	}
}

// Простые чеки условий
bool StorageSystem::StorageChecks(ATIDinosaurBase* Dinosaur, ATIPlayerController* Player) {
	if (Config.HealthCheck) {
		if (Dinosaur->GetHealth() != Dinosaur->GetMaxHealth()) {
			Player->ClientShowNotification(FText(STR("You need full health")));
			return false;
		}
	}
	if (Config.StaminaCheck) {
		if (Dinosaur->GetStamina() != Dinosaur->GetMaxStamina()) {
			Player->ClientShowNotification(FText(STR("You need full stamina")));
			return false;
		}

	}
	if (Config.BloodCheck) {
		if (Dinosaur->GetBlood() != Dinosaur->GetMaxBlood()) {
			Player->ClientShowNotification(FText(STR("You need full blood")));
			return false;
		}
	}
	return true;
}

void StorageSystem::StoreDino(ATIPlayerController* Player) {
	static ATIGameModeBase* GameMode = static_cast<ATIGameModeBase*>(UObjectGlobals::FindFirstOf(STR("BP_SurvivalGameMode_C")));

	APawn* Pawn = Player->Pawn();
	if (!Pawn || !Pawn->IsA(ATIDinosaurBase::StaticClass())) return;

	ATIDinosaurBase* Dinosaur = static_cast<ATIDinosaurBase*>(Pawn);
	if (Config.GrowthRequirementSave > Dinosaur->Growth()) {
		Player->ClientShowNotification(FText(STR("Growth requirement not meet")));
		return;
	};

	if (!StorageChecks(Dinosaur, Player)) return;

	FString SteamID = Player->SteamId();
#ifdef LOCAL_DEBUGGING
	int32 DinoID = Dinosaur->ID();
	RC::Output::send(STR("Attempt to save Dinosaur for SteamID: {}, DinoID: {}"), *SteamID, DinoID);
#endif
	int64_t Out{};
	if(Database->SaveDino(Dinosaur, false, false, Out)) {
		Player->ClientShowNotification(FText(fmt::format(STR("Saved DinoID: {}"), Out)));
		// Удаляем дино что бы не осталось следов как и трупа и сразу активируем лобби скрин игроку
		Dinosaur->SetHealth(0);
		GameMode->TryToRespawn(Player, SteamID);
		Dinosaur->DestroyCorpse();
#ifdef LOCAL_DEBUGGING
		RC::Output::send(STR("Dinosaur removed from game, SteamID: {}, DinoID: {}"), *SteamID, DinoID);
#endif
	} else {
		LOG_DEBUG(STR("Failed to save dino for SteamID: {}, DinoID: {}"), *SteamID, DinoID);
	}
}

// Загрузка динозавра с одинаковым ID иногда может не прокатить либо багаться, по этому лучше чистить
void StorageSystem::LoadDino(ATIPlayerController* Player, int32 DinoID) {
	static ATIGameModeBase* GameMode = static_cast<ATIGameModeBase*>(UObjectGlobals::FindFirstOf(STR("BP_SurvivalGameMode_C")));

	FString SteamID = Player->SteamId();
#ifdef LOCAL_DEBUGGING
	RC::Output::send(STR("Attempt to load dino for SteamID: {}, DinoID: {}"), *SteamID, DinoID);
#endif

	APawn* Pawn = Player->Pawn();
	ATIDinosaurBase* Dinosaur{};
	if (Pawn && Pawn->IsA(ATIDinosaurBase::StaticClass())) {
		Dinosaur = static_cast<ATIDinosaurBase*>(Pawn);
		if (Dinosaur->GetGrowth() > Config.GrowthRequirementLoad) {
			Player->ClientShowNotification(FText(STR("Growth requirement not meet")));
			return;
		};
	}

	if (Dinosaur && !StorageChecks(Dinosaur, Player)) return;

	FString Requested;
	if(!Database->LoadDino(SteamID, DinoID, Requested)) {
		LOG_DEBUG(STR("Failed to load dino for SteamID: {}, DinoID: {}"), *SteamID, DinoID);
		return;
	};

	FTIPlayerData PlayerData = UTISaveManager::StringToPlayerData(Requested);
	if (Config.SameClass && Dinosaur) {
		if (PlayerData.Class() != FString(Dinosaur->GetClassPrivate()->GetPathName())) {
			Player->ClientShowNotification(FText(STR("Only same type of dino")));
			return;
		};
	}

	FTISpawnData SpawnData;
	SpawnData.New();
	FVector Location{};
	if (Config.PreserveLoc) {
		Location = Dinosaur->K2_GetActorLocation();
		PlayerData.Location() = Location;
	}
	else Location = PlayerData.Location();

	SpawnData.Controller() = Player;
	SpawnData.SteamId() = SteamID;
	SpawnData.Class() = PlayerData.Class();
	SpawnData.bKeepActualLoc() = Config.PreserveLoc;// Safe to change
	SpawnData.bLoadOnly() = false;// don't change
	SpawnData.bDefaultSpawn() = false;// don't change
	SpawnData.SpawnLocation() = Location;
	SpawnData.CustomizerData() = PlayerData.CustomizedData();
	SpawnData.bUseQuickRespawn() = false;// don't change
	SpawnData.bPreserveSpawnLocation() = Config.PreserveLoc;// Safe to change
	SpawnData.PlayerData() = PlayerData;

	// Чистим что бы не было трупика и обновляем данные в дб
	if (Dinosaur) {
		Database->SaveDino(Dinosaur, false, true);
		Dinosaur->SetHealth(0);
		Dinosaur->DestroyCorpse();
	}

	GameMode->TryToRespawn(Player, SteamID);// Magic WORD! Sometimes you just have to believe me
	GameMode->AddSpawnRequest(SpawnData, true, true);

	Dinosaur = static_cast<ATIDinosaurBase*>(Player->Pawn());
	if (!Dinosaur || Dinosaur->ID() != DinoID) {
		LOG_DEBUG(STR("Failed to load dino for SteamID: {}, DinoID: {}"), *SteamID, DinoID);
		return;
	}

	Player->ClientShowNotification(FText(fmt::format(STR("Loaded DinoID: {}"), DinoID)));
	if (Config.LockOnLoad && !Database->ConfirmLoadDino(DinoID)) {
		LOG_DEBUG(STR("Failed to update dino status for SteamID: {}, DinoID: {}"), *SteamID, DinoID);
	}
#ifdef LOCAL_DEBUGGING
	RC::Output::send(STR("Loaded dino for SteamID: {}, DinoID: {}"), *SteamID, DinoID);
#endif
}


#define MOD_API __declspec(dllexport)
extern "C" {
	MOD_API RC::CppUserModBase* start_mod() {
		return new StorageSystem();
	}

	MOD_API void uninstall_mod(RC::CppUserModBase* mod) {
		delete mod;
	}
}
