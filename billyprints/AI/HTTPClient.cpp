#include "HTTPClient.hpp"
#include <httplib.h>

#include <iostream>
#include <sstream>

namespace Billyprints {

    HTTPClient::HTTPClient() : shouldCancel(false), isRunning(false) {
    }

    HTTPClient::~HTTPClient() {
        Cancel();
        if (workerThread && workerThread->joinable()) {
            workerThread->join();
        }
    }

    void HTTPClient::SetEndpoint(const std::string& url) {
        endpoint = url;
    }

    void HTTPClient::SetHeader(const std::string& key, const std::string& value) {
        headers[key] = value;
    }

    void HTTPClient::ClearHeaders() {
        headers.clear();
    }

    bool HTTPClient::IsRunning() const {
        return isRunning.load();
    }

    void HTTPClient::Cancel() {
        shouldCancel = true;
        if (workerThread && workerThread->joinable()) {
            workerThread->join();
        }
        shouldCancel = false;
    }

    void HTTPClient::PostStreaming(
        const std::string& path,
        const std::string& body,
        std::function<void(const std::string&)> onChunk,
        std::function<void(const std::string&)> onError,
        std::function<void()> onComplete
    ) {
        // Cancel any existing request
        if (isRunning) {
            Cancel();
        }

        // Start new worker thread
        shouldCancel = false;
        isRunning = true;
        workerThread = std::make_unique<std::thread>(
            &HTTPClient::WorkerThreadFunc,
            this,
            path,
            body,
            onChunk,
            onError,
            onComplete
        );
    }

    void HTTPClient::WorkerThreadFunc(
        const std::string& path,
        const std::string& body,
        std::function<void(const std::string&)> onChunk,
        std::function<void(const std::string&)> onError,
        std::function<void()> onComplete
    ) {
        try {
            // Parse endpoint URL
            std::string scheme, host, basePath;
            int port = 443;

            // Simple URL parsing
            size_t schemeEnd = endpoint.find("://");
            if (schemeEnd != std::string::npos) {
                scheme = endpoint.substr(0, schemeEnd);
                size_t hostStart = schemeEnd + 3;
                size_t pathStart = endpoint.find('/', hostStart);
                if (pathStart != std::string::npos) {
                    host = endpoint.substr(hostStart, pathStart - hostStart);
                    basePath = endpoint.substr(pathStart);
                } else {
                    host = endpoint.substr(hostStart);
                    basePath = "";
                }
            } else {
                onError("Invalid endpoint URL");
                isRunning = false;
                return;
            }

            // Check for port in host
            size_t portPos = host.find(':');
            if (portPos != std::string::npos) {
                port = std::stoi(host.substr(portPos + 1));
                host = host.substr(0, portPos);
            } else {
                port = (scheme == "https") ? 443 : 80;
            }

            // Set headers
            httplib::Headers reqHeaders;
            for (const auto& [key, value] : headers) {
                reqHeaders.insert({key, value});
            }

            // Full path
            std::string fullPath = basePath + path;

            // Response handler
            httplib::Result res;

            // Content receiver for streaming response
            auto receiver = [&](const char* data, size_t dataLen) -> bool {
                if (shouldCancel) {
                    return false; // Stop streaming
                }

                std::string chunk(data, dataLen);

                // Parse SSE format: "data: {...}\n\n"
                // Split by lines
                std::istringstream stream(chunk);
                std::string line;

                while (std::getline(stream, line)) {
                    if (shouldCancel) {
                        return false;
                    }

                    // Skip empty lines
                    if (line.empty() || line == "\r") {
                        continue;
                    }

                    // Check for SSE data line
                    if (line.rfind("data: ", 0) == 0) {
                        std::string jsonData = line.substr(6);

                        // Skip [DONE] message
                        if (jsonData == "[DONE]" || jsonData == "[DONE]\r") {
                            continue;
                        }

                        // Call chunk callback
                        if (onChunk) {
                            onChunk(jsonData);
                        }
                    }
                }

                return true; // Continue streaming
            };

            // Perform POST based on scheme
            // Note: This version uses blocking request - full response received at once
            // For true streaming, would need a different HTTP library or async approach
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
            if (scheme == "https") {
                httplib::SSLClient cli(host, port);
                cli.set_read_timeout(60, 0);  // Longer timeout for AI responses
                cli.set_connection_timeout(10, 0);
                res = cli.Post(fullPath.c_str(), reqHeaders, body, "application/json");
            } else {
                httplib::Client cli(host, port);
                cli.set_read_timeout(60, 0);
                cli.set_connection_timeout(10, 0);
                res = cli.Post(fullPath.c_str(), reqHeaders, body, "application/json");
            }
#else
            if (scheme == "https") {
                onError("HTTPS not supported. Please install OpenSSL and rebuild with CPPHTTPLIB_OPENSSL_SUPPORT defined.");
                isRunning = false;
                return;
            }
            httplib::Client cli(host, port);
            cli.set_read_timeout(60, 0);
            cli.set_connection_timeout(10, 0);
            res = cli.Post(fullPath.c_str(), reqHeaders, body, "application/json");
#endif

            // Process the response body as SSE stream (even though received all at once)
            if (res && res->status == 200) {
                receiver(res->body.c_str(), res->body.size());
            }

            if (shouldCancel) {
                isRunning = false;
                return;
            }

            // Check response
            if (!res) {
                if (onError) {
                    onError("Connection failed: " + httplib::to_string(res.error()));
                }
            } else if (res->status != 200) {
                if (onError) {
                    std::string errorMsg = "HTTP " + std::to_string(res->status);
                    if (res->status == 401) {
                        errorMsg += ": Invalid API key. Check ai_config.json";
                    } else if (res->status == 429) {
                        errorMsg += ": Rate limit exceeded. Wait and retry.";
                    } else if (res->status >= 500) {
                        errorMsg += ": Server error. Try again later.";
                    } else {
                        errorMsg += ": " + res->body;
                    }
                    onError(errorMsg);
                }
            } else {
                // Success
                if (onComplete) {
                    onComplete();
                }
            }
        }
        catch (const std::exception& e) {
            if (onError) {
                onError(std::string("Exception: ") + e.what());
            }
        }

        isRunning = false;
    }

}
