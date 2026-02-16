#include "NodeEditor.hpp"
#include "AI/AIAssistant.hpp"
#include <imgui.h>
#include <iostream>

namespace Logicarium {

    void NodeEditor::RenderAIAssistant() {
        ImGui::Separator();

        // Collapsible header
        bool headerOpen = ImGui::CollapsingHeader("AI Assistant", ImGuiTreeNodeFlags_DefaultOpen);

        if (!headerOpen) {
            aiSectionCollapsed = true;
            return;
        }

        aiSectionCollapsed = false;

        if (!aiAssistant) {
            ImGui::TextWrapped("AI Assistant not initialized");
            return;
        }

        // Prompt input and send button
        ImGui::PushItemWidth(-80.0f); // Leave 80px for button
        bool enterPressed = ImGui::InputText("##aiprompt", aiPromptBuf, sizeof(aiPromptBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();

        ImGui::SameLine();

        AIRequestState state = aiAssistant->GetState();
        bool isProcessing = (state == AIRequestState::Connecting || state == AIRequestState::Streaming);

        // Disable send button while processing
        if (isProcessing) {
            ImGui::BeginDisabled();
        }

        bool sendClicked = ImGui::Button("Send", ImVec2(70, 0));

        if (isProcessing) {
            ImGui::EndDisabled();
        }

        // Send on button click or Enter key
        if ((sendClicked || enterPressed) && !isProcessing && aiPromptBuf[0] != '\0') {
            lastAIPrompt = aiPromptBuf;
            aiAssistant->SendRequest(aiPromptBuf, currentScript);
            // Don't clear prompt yet - user might want to see what they asked
        }

        // Hint text
        if (aiPromptBuf[0] == '\0' && !isProcessing) {
            ImGui::TextDisabled("Ask AI to create or modify circuit logic...");
        }

        ImGui::Spacing();

        // Response area
        ImGui::BeginChild("AIResponse", ImVec2(0, 150), true, ImGuiWindowFlags_HorizontalScrollbar);

        if (state == AIRequestState::Idle) {
            ImGui::TextDisabled("Enter a prompt above to get AI assistance");
        }
        else if (state == AIRequestState::Connecting) {
            ImGui::TextWrapped("Connecting to AI...");
        }
        else if (state == AIRequestState::Streaming) {
            ImGui::TextWrapped("%s", aiAssistant->GetResponse().c_str());

            // Show animated dots while streaming
            static float dotTimer = 0.0f;
            dotTimer += ImGui::GetIO().DeltaTime;
            int dotCount = ((int)(dotTimer * 2.0f)) % 4;
            std::string dots = "";
            for (int i = 0; i < dotCount; i++) dots += ".";
            ImGui::SameLine();
            ImGui::TextDisabled("%s", dots.c_str());

            // Auto-scroll to bottom while streaming
            ImGui::SetScrollHereY(1.0f);
        }
        else if (state == AIRequestState::Complete) {
            ImGui::TextWrapped("%s", aiAssistant->GetResponse().c_str());
        }
        else if (state == AIRequestState::Error) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::TextWrapped("Error: %s", aiAssistant->GetError().c_str());
            ImGui::PopStyleColor();
        }

        ImGui::EndChild();

        ImGui::Spacing();

        // Action buttons (only show when complete or error)
        if (state == AIRequestState::Complete) {
            if (ImGui::Button("Accept")) {
                // Extract code from response
                std::string response = aiAssistant->GetResponse();

                // Look for ```Logicarium ... ``` code fence
                size_t fenceStart = response.find("```Logicarium");
                if (fenceStart != std::string::npos) {
                    size_t codeStart = response.find('\n', fenceStart);
                    if (codeStart != std::string::npos) {
                        codeStart++; // Skip the newline
                        size_t codeEnd = response.find("```", codeStart);
                        if (codeEnd != std::string::npos) {
                            pendingAIScript = response.substr(codeStart, codeEnd - codeStart);
                        }
                    }
                }

                // If no code fence found, try generic ``` fence
                if (pendingAIScript.empty()) {
                    fenceStart = response.find("```");
                    if (fenceStart != std::string::npos) {
                        size_t codeStart = response.find('\n', fenceStart);
                        if (codeStart != std::string::npos) {
                            codeStart++;
                            size_t codeEnd = response.find("```", codeStart);
                            if (codeEnd != std::string::npos) {
                                pendingAIScript = response.substr(codeStart, codeEnd - codeStart);
                            }
                        }
                    }
                }

                // If still no code found, use entire response
                if (pendingAIScript.empty()) {
                    pendingAIScript = response;
                }

                // Apply to script buffer (max 8191 chars + null terminator)
                size_t copyLen = std::min(pendingAIScript.length(), (size_t)8191);
                memcpy(scriptBuf, pendingAIScript.c_str(), copyLen);
                scriptBuf[copyLen] = '\0';

                // Update nodes from script
                currentScript = scriptBuf;
                UpdateNodesFromScript();

                // Clear prompt and reset AI
                aiPromptBuf[0] = '\0';
                aiAssistant->Reset();
                pendingAIScript.clear();

                std::cout << "AI script applied" << std::endl;
            }

            ImGui::SameLine();

            if (ImGui::Button("Reject")) {
                aiPromptBuf[0] = '\0';
                aiAssistant->Reset();
                pendingAIScript.clear();
            }

            ImGui::SameLine();

            if (ImGui::Button("Copy")) {
                ImGui::SetClipboardText(aiAssistant->GetResponse().c_str());
            }
        }
        else if (state == AIRequestState::Error) {
            if (ImGui::Button("Retry")) {
                if (!lastAIPrompt.empty()) {
                    strcpy(aiPromptBuf, lastAIPrompt.c_str());
                    aiAssistant->SendRequest(lastAIPrompt, currentScript);
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Clear")) {
                aiPromptBuf[0] = '\0';
                aiAssistant->Reset();
            }
        }
        else if (isProcessing) {
            if (ImGui::Button("Cancel")) {
                aiAssistant->Cancel();
                aiPromptBuf[0] = '\0';
            }
        }
    }

}
