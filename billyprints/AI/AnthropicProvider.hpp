#pragma once

#include "ConfigManager.hpp"
#include <string>

namespace Billyprints {

    class AnthropicProvider {
    public:
        // Build JSON request payload for Anthropic API
        static std::string BuildRequest(
            const AIConfig& config,
            const std::string& systemPrompt,
            const std::string& userMessage
        );

        // Parse a streaming chunk from Anthropic SSE format
        // Returns the text delta extracted from the chunk
        static std::string ParseStreamChunk(const std::string& jsonChunk);

    private:
        // Escape JSON strings
        static std::string EscapeJSON(const std::string& str);
    };

}
