#include "ConfigManager.hpp"
#include <fstream>
#include <iostream>
#include <json.hpp>

using json = nlohmann::json;

namespace Billyprints {

    AIConfig ConfigManager::GetDefaultConfig() {
        AIConfig config;
        config.provider = "anthropic";
        config.endpoint = "https://api.anthropic.com/v1/messages";
        config.apiKey = "";
        config.model = "claude-3-5-sonnet-20241022";
        config.maxTokens = 4096;
        config.temperature = 0.7f;
        return config;
    }

    AIConfig ConfigManager::LoadConfig(const std::string& path) {
        AIConfig config = GetDefaultConfig();

        std::ifstream file(path);
        if (!file.is_open()) {
            std::cout << "AI config not found at " << path << ", using defaults" << std::endl;
            return config;
        }

        try {
            json j;
            file >> j;

            if (j.contains("provider")) config.provider = j["provider"].get<std::string>();
            if (j.contains("endpoint")) config.endpoint = j["endpoint"].get<std::string>();
            if (j.contains("apiKey")) config.apiKey = j["apiKey"].get<std::string>();
            if (j.contains("model")) config.model = j["model"].get<std::string>();
            if (j.contains("maxTokens")) config.maxTokens = j["maxTokens"].get<int>();
            if (j.contains("temperature")) config.temperature = j["temperature"].get<float>();

            std::cout << "AI config loaded from " << path << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Error parsing AI config: " << e.what() << ", using defaults" << std::endl;
        }

        file.close();
        return config;
    }

    bool ConfigManager::SaveConfig(const AIConfig& config, const std::string& path) {
        try {
            json j;
            j["provider"] = config.provider;
            j["endpoint"] = config.endpoint;
            j["apiKey"] = config.apiKey;
            j["model"] = config.model;
            j["maxTokens"] = config.maxTokens;
            j["temperature"] = config.temperature;

            std::ofstream file(path);
            if (!file.is_open()) {
                std::cerr << "Failed to open " << path << " for writing" << std::endl;
                return false;
            }

            file << j.dump(2); // Pretty print with 2-space indent
            file.close();

            std::cout << "AI config saved to " << path << std::endl;
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Error saving AI config: " << e.what() << std::endl;
            return false;
        }
    }

}
