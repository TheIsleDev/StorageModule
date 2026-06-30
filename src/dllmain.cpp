#include <string>

#include <Mod/CppUserModBase.hpp>

#include "Config/config_reader.hpp"

#include "_structs.hpp"
#include "handler.cpp"

class StorageSystem : public RC::CppUserModBase {
private:
	StorageSystemConfiguration::StorageConfig Config;

public:
	StorageSystem() : CppUserModBase()
	{
		ModName = STR("Storage");
		ModVersion = STR("1.0");
		ModDescription = STR("Hehe");
		ModAuthors = STR("Shiza");
	}

	auto on_unreal_init() -> void override {
		ModConfigReader::LoadModConfig(&Config);

		StorageSystemComponent::Initialize(Config);
	}
};

#define KISMET_DEBUGGER_MOD_API __declspec(dllexport)
extern "C"
{
	KISMET_DEBUGGER_MOD_API RC::CppUserModBase* start_mod()
	{
		return new StorageSystem();
	}

	KISMET_DEBUGGER_MOD_API void uninstall_mod(RC::CppUserModBase* mod)
	{
		StorageSystemComponent::Destroy();
		delete mod;
	}
}