
// Declare local debugging if you want more logging on test run
//#define LOCAL_DEBUGGING

#include "Hooks/Hooks.hpp"
#include <TheIsleHelpers/ConfigReader.hpp>
#include <TheIsle/APawn.hpp>
#include <TheIsle/ATIGameModeBase.hpp>
#include <TheIsle/UTICommandManager.hpp>
#include <TheIsle/_simple_structs.hpp>
#include <Storage.hpp>


bool StorageSystem::CreateHelpers() {
	static int TicksFired = 0;
	static constexpr int TickRate{300};

	if (++TicksFired < TickRate) return false;
	TicksFired = 0;

	GameMode = static_cast<ATIGameModeBase*>(UObjectGlobals::FindFirstOf(STR("BP_SurvivalGameMode_C")));
	if (!GameMode) return false;

	UTICommandManager* CommandsHolder = GameMode->CommandExecutorInstance();
	if (!CommandsHolder) return false;

	TArray<FCommandInfo> Commands = CommandsHolder->AvailableCommands();
	bool FoundStore = false;
	bool FoundLoad = false;
	for (FCommandInfo Command : Commands) {
		if (Command.Command == FString(STR("store"))) {
			FoundStore = true;
			continue;
		} else if (Command.Command == FString(STR("load"))) {
			FoundLoad = true;
			continue;
		}
		Command.bIsDeveloperAdvanced = false;
		Command.bIsDeveloperBasic = false;
		Command.bIsAdmin = true;
	}
	// Оно кста не регает команды, там чтоль хардкода дохуя? Я пытался поискать где строит их структуру + кастомное описание под поля, но ничего не нашел.
	// Это лютое количество хардкода + 0 синхронизации с сервером, сука взяли бы меня девом я бы им все это говно переделал... мое любимое занятие! Есть кал!
	if (!FoundStore) Commands.Add(FCommandInfo{FString(STR("store")), FString(STR("Stores your dino")), true, true, true});
	if (!FoundLoad) Commands.Add(FCommandInfo{FString(STR("load")), FString(STR("Loads your dino")), true, true, true});
	return true;
}


StorageSystem::StorageSystem() {
    ModName = STR("Storage");
    ModVersion = STR("1.0.2");
    ModDescription = STR("Hehe");
    ModAuthors = STR("Shiza");

	RC::ConfigLoader::LoadModConfig(&Config);
}

StorageSystem::~StorageSystem() {
	ExecuteCommand->UnregisterHook(HookID);
	if (InitializeCallBackID) Hook::UnregisterCallback(InitializeCallBackID);
}


void StorageSystem::on_unreal_init() {
	Database = std::make_unique<RC::DataBase::DataBase>(Config.Database);
	Database->PrepareStorage();

	ExecuteCommand = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/TheIsle.TIPlayerController:ServerExecuteChatCommand"));
	HookID = ExecuteCommand->RegisterPreHook(
		[this](UnrealScriptFunctionCallableContext& Context, void* CustomData) {
			HandleExecuteCommand(Context);
		}
	);

	InitializeCallBackID = Hook::RegisterEngineTickPreCallback(
		[this](Hook::TCallbackIterationData<void>& info, UEngine* Context, float DeltaSeconds, bool bIdleMode) {
			if (!CreateHelpers()) return;

			InitializeCallBackID = 0;
			info.RemoveSelf();
		}
		, {false, true, STR("Storage"), STR("Initialize")}
	);
}

void StorageSystem::HandleExecuteCommand(UnrealScriptFunctionCallableContext& FuncContext) {
	ServerExecuteChatCommandData* Data = std::bit_cast<ServerExecuteChatCommandData*>(FuncContext.TheStack.Locals());

	// Команды лишь дают инфу после слеша, из плюсов они не пишутся в чат сами по себе и реже тригерят этот хандлер
	if (Data->CommandLine == FString(STR("store"))) {
		StoreDino(static_cast<ATIPlayerController*>(FuncContext.Context));
	} else if (Data->CommandLine.StartsWith(FString(STR("load ")))) {
		if (!Config.OverrideDino) return;

		// Dont judge, 5 lines below this one is just LLM slopcode.
		RC::StringType IdString = *Data->CommandLine;
		auto Utf8 = RC::to_string(IdString.substr(FCString::Strlen(STR("load "))));
		int32 DinoID;
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
