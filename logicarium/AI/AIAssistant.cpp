#include "AIAssistant.hpp"
#include "SystemPrompt.hpp"
#include "AnthropicProvider.hpp"
#include <iostream>

namespace Logicarium {

    AIAssistant::AIAssistant()
        : state(AIRequestState::Idle)
    {
        httpClient = std::make_unique<HTTPClient>();
        LoadSystemPrompt();
    }

    AIAssistant::~AIAssistant() {
        Cancel();
    }

    void AIAssistant::LoadSystemPrompt() {
        systemPrompt = SYSTEM_PROMPT;
    }

    void AIAssistant::LoadConfig(const std::string& configPath) {
        config = ConfigManager::LoadConfig(configPath);
    }

    void AIAssistant::SaveConfig(const std::string& configPath) {
        ConfigManager::SaveConfig(config, configPath);
    }

    void AIAssistant::SendRequest(const std::string& userPrompt, const std::string& currentScript) {
        // Cancel any existing request
        if (state != AIRequestState::Idle) {
            Cancel();
        }

        // Reset state
        currentResponse.clear();
        errorMessage.clear();
        pendingChunks.clear();
        state = AIRequestState::Connecting;

        // Build user message with context
        std::string userMessage;
        if (!currentScript.empty()) {
            userMessage = "Current script:\n```Logicarium\n" + currentScript + "\n```\n\n" + userPrompt;
        } else {
            userMessage = userPrompt;
        }

        // Build request payload
        std::string requestBody = AnthropicProvider::BuildRequest(config, systemPrompt, userMessage);

        // Configure HTTP client
        httpClient->SetEndpoint(config.endpoint);
        httpClient->ClearHeaders();
        httpClient->SetHeader("x-api-key", config.apiKey);
        httpClient->SetHeader("anthropic-version", "2023-06-01");
        httpClient->SetHeader("content-type", "application/json");

        // Send request
        httpClient->PostStreaming(
            "",  // Path is in endpoint
            requestBody,
            [this](const std::string& chunk) { this->OnChunkReceived(chunk); },
            [this](const std::string& error) { this->OnError(error); },
            [this]() { this->OnComplete(); }
        );

        std::cout << "AI request sent: " << userPrompt << std::endl;
    }

    void AIAssistant::Update() {
        // Process pending chunks (thread-safe)
        std::lock_guard<std::mutex> lock(responseMutex);
        if (!pendingChunks.empty()) {
            currentResponse += pendingChunks;
            pendingChunks.clear();

            // Update state to streaming if we're getting data
            if (state == AIRequestState::Connecting) {
                state = AIRequestState::Streaming;
            }
        }
    }

    void AIAssistant::Cancel() {
        if (httpClient) {
            httpClient->Cancel();
        }
        state = AIRequestState::Idle;
    }

    void AIAssistant::Reset() {
        Cancel();
        currentResponse.clear();
        errorMessage.clear();
        pendingChunks.clear();
        state = AIRequestState::Idle;
    }

    void AIAssistant::OnChunkReceived(const std::string& jsonChunk) {
        // Parse the chunk to extract text
        std::string textDelta = AnthropicProvider::ParseStreamChunk(jsonChunk);

        if (!textDelta.empty()) {
            // Accumulate in thread-safe buffer
            std::lock_guard<std::mutex> lock(responseMutex);
            pendingChunks += textDelta;
        }
    }

    void AIAssistant::OnError(const std::string& error) {
        std::lock_guard<std::mutex> lock(responseMutex);
        errorMessage = error;
        state = AIRequestState::Error;
        std::cerr << "AI request error: " << error << std::endl;
    }

    void AIAssistant::OnComplete() {
        // Update method will handle final state transition
        if (state == AIRequestState::Streaming || state == AIRequestState::Connecting) {
            state = AIRequestState::Complete;
            std::cout << "AI request completed" << std::endl;
        }
    }

}
