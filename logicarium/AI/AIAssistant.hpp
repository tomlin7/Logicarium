#pragma once

#include "ConfigManager.hpp"
#include "HTTPClient.hpp"
#include <string>
#include <memory>
#include <mutex>

namespace Logicarium {

    enum class AIRequestState {
        Idle,
        Connecting,
        Streaming,
        Complete,
        Error
    };

    class AIAssistant {
    public:
        AIAssistant();
        ~AIAssistant();

        // Load configuration from file
        void LoadConfig(const std::string& configPath);

        // Save configuration to file
        void SaveConfig(const std::string& configPath);

        // Send a request to the AI (non-blocking)
        // currentScript: The current script content for context
        // userPrompt: The user's natural language request
        void SendRequest(const std::string& userPrompt, const std::string& currentScript);

        // Update method - call this every frame to process streaming updates
        void Update();

        // Cancel the current request
        void Cancel();

        // Reset to idle state, clearing response and error
        void Reset();

        // Getters
        AIRequestState GetState() const { return state; }
        const std::string& GetResponse() const { return currentResponse; }
        const std::string& GetError() const { return errorMessage; }
        const AIConfig& GetConfig() const { return config; }

    private:
        AIConfig config;
        AIRequestState state;
        std::string currentResponse;
        std::string errorMessage;
        std::string systemPrompt;

        std::unique_ptr<HTTPClient> httpClient;

        // Thread-safe response accumulation
        std::mutex responseMutex;
        std::string pendingChunks;

        void LoadSystemPrompt();
        void OnChunkReceived(const std::string& jsonChunk);
        void OnError(const std::string& error);
        void OnComplete();
    };

}
