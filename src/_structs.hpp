#pragma once

#include <string>

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
}