#pragma once

#include <string>

namespace Billyprints {

    struct AIConfig {
        std::string provider = "anthropic";
        std::string endpoint = "https://api.anthropic.com/v1/messages";
        std::string apiKey = "";
        std::string model = "claude-3-5-sonnet-20241022";
        int maxTokens = 4096;
        float temperature = 0.7f;
    };

    class ConfigManager {
    public:
        // Load config from JSON file
        // Returns default config if file doesn't exist or has errors
        static AIConfig LoadConfig(const std::string& path);

        // Save config to JSON file
        // Returns true on success, false on error
        static bool SaveConfig(const AIConfig& config, const std::string& path);

        // Get default configuration
        static AIConfig GetDefaultConfig();
    };

}
