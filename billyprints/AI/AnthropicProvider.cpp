#include "AnthropicProvider.hpp"
#include <json.hpp>
#include <sstream>

using json = nlohmann::json;

namespace Billyprints {

    std::string AnthropicProvider::EscapeJSON(const std::string& str) {
        std::string escaped;
        escaped.reserve(str.size());
        for (char c : str) {
            switch (c) {
                case '"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\b': escaped += "\\b"; break;
                case '\f': escaped += "\\f"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default:
                    if (c < 0x20) {
                        char buf[7];
                        snprintf(buf, sizeof(buf), "\\u%04x", c);
                        escaped += buf;
                    } else {
                        escaped += c;
                    }
                    break;
            }
        }
        return escaped;
    }

    std::string AnthropicProvider::BuildRequest(
        const AIConfig& config,
        const std::string& systemPrompt,
        const std::string& userMessage
    ) {
        json request;
        request["model"] = config.model;
        request["max_tokens"] = config.maxTokens;
        request["temperature"] = config.temperature;
        request["stream"] = true;
        request["system"] = systemPrompt;
        request["messages"] = json::array({
            {
                {"role", "user"},
                {"content", userMessage}
            }
        });

        return request.dump();
    }

    std::string AnthropicProvider::ParseStreamChunk(const std::string& jsonChunk) {
        try {
            json j = json::parse(jsonChunk);

            // Anthropic SSE event format:
            // {"type":"content_block_delta","delta":{"type":"text_delta","text":"..."}}
            if (j.contains("type") && j["type"] == "content_block_delta") {
                if (j.contains("delta") && j["delta"].contains("text")) {
                    return j["delta"]["text"].get<std::string>();
                }
            }

            // Also handle content_block_start which has the initial text
            if (j.contains("type") && j["type"] == "content_block_start") {
                if (j.contains("content_block") && j["content_block"].contains("text")) {
                    return j["content_block"]["text"].get<std::string>();
                }
            }

            return "";
        }
        catch (const std::exception&) {
            // Ignore parse errors for non-delta events
            return "";
        }
    }

}
