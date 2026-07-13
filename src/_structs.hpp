#pragma once

#include <string>

namespace StorageSystemConfiguration {
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
	};
}