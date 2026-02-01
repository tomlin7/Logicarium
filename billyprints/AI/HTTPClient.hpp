#pragma once

#include <string>
#include <map>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>

namespace Billyprints {

    class HTTPClient {
    public:
        HTTPClient();
        ~HTTPClient();

        // Set the base URL (e.g., "https://api.anthropic.com")
        void SetEndpoint(const std::string& url);

        // Set HTTP headers (e.g., API key, content type)
        void SetHeader(const std::string& key, const std::string& value);

        // Clear all headers
        void ClearHeaders();

        // Perform a streaming POST request
        // Callbacks are called from a background thread
        void PostStreaming(
            const std::string& path,              // e.g., "/v1/messages"
            const std::string& body,              // JSON payload
            std::function<void(const std::string&)> onChunk,     // Called for each chunk
            std::function<void(const std::string&)> onError,     // Called on error
            std::function<void()> onComplete                     // Called on completion
        );

        // Cancel the current request (if any)
        void Cancel();

        // Check if a request is currently in progress
        bool IsRunning() const;

    private:
        std::string endpoint;
        std::map<std::string, std::string> headers;
        std::unique_ptr<std::thread> workerThread;
        std::atomic<bool> shouldCancel;
        std::atomic<bool> isRunning;

        void WorkerThreadFunc(
            const std::string& path,
            const std::string& body,
            std::function<void(const std::string&)> onChunk,
            std::function<void(const std::string&)> onError,
            std::function<void()> onComplete
        );
    };

}
