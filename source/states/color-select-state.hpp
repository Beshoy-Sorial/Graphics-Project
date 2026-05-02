#pragma once

#include <application.hpp>
#include <imgui.h>
#include <glm/glm.hpp>
#include <vector>
#include "../common/tournament-manager.hpp"

class ColorSelectState : public our::State {
private:
    struct ArenaColor {
        std::string name;
        glm::vec4 color;
    };

    std::vector<ArenaColor> arenaColors;

    int selectedColorIndex = 0;

public:
    void onInitialize() override {
        // Unlock mouse for UI interaction
        getApp()->getMouse().unlockMouse(getApp()->getWindow());

        arenaColors = {
            {"Dark Gray",     glm::vec4(0.18f, 0.18f, 0.18f, 1.0f)},
            {"White",         glm::vec4(0.9f,  0.9f,  0.9f,  1.0f)},
            {"Deep Blue",     glm::vec4(0.1f,  0.2f,  0.5f,  1.0f)},
            {"Forest Green",  glm::vec4(0.1f,  0.4f,  0.1f,  1.0f)},
            {"Dark Red",      glm::vec4(0.5f,  0.1f,  0.1f,  1.0f)},
            {"Golden",        glm::vec4(0.7f,  0.6f,  0.1f,  1.0f)},
            {"Purple",        glm::vec4(0.4f,  0.1f,  0.5f,  1.0f)},
            {"Ocean Blue",    glm::vec4(0.1f,  0.5f,  0.7f,  1.0f)},
        };

        const auto &config = getApp()->getConfig();
        if (config.contains("scene") && config["scene"].contains("arenaColorOptions") &&
            config["scene"]["arenaColorOptions"].is_array()) {
            std::vector<ArenaColor> loadedColors;
            for (const auto &entry : config["scene"]["arenaColorOptions"]) {
                if (!entry.is_object() || !entry.contains("name") || !entry.contains("color")) continue;
                if (!entry["name"].is_string() || !entry["color"].is_array() || entry["color"].size() < 3) continue;

                glm::vec4 color(1.0f);
                color.r = entry["color"][0].get<float>();
                color.g = entry["color"][1].get<float>();
                color.b = entry["color"][2].get<float>();
                color.a = entry["color"].size() > 3 ? entry["color"][3].get<float>() : 1.0f;
                loadedColors.push_back({entry["name"].get<std::string>(), color});
            }
            if (!loadedColors.empty()) arenaColors = loadedColors;
        }
        
        // Try to find the current color in the list
        auto &tm = our::TournamentManager::getInstance();
        for (size_t i = 0; i < arenaColors.size(); ++i) {
            if (arenaColors[i].color == tm.selectedArenaColor) {
                selectedColorIndex = static_cast<int>(i);
                break;
            }
        }
    }

    void onImmediateGui() override {
        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_Always);

        ImGui::Begin("Select Arena Color", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse);

        ImGui::Text("Choose your arena's floor color:");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        auto &tm = our::TournamentManager::getInstance();

        // Display color options in a 2x4 grid
        ImGui::Columns(2, "ColorGrid", false);
        for (int i = 0; i < (int)arenaColors.size(); ++i) {
            ImGui::PushID(i);

            // Button background color based on arena color
            ImVec4 col = ImVec4(arenaColors[i].color.r, 
                               arenaColors[i].color.g,
                               arenaColors[i].color.b, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, col);
            
            ImVec4 hoverCol = ImVec4(
                glm::min(col.x * 1.3f, 1.0f), 
                glm::min(col.y * 1.3f, 1.0f), 
                glm::min(col.z * 1.3f, 1.0f), 1.0f
            );
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverCol);

            // If this color is selected, show a border or highlight
            bool isSelected = (selectedColorIndex == i);
            if (isSelected) {
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 4.0f);
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 1));
            }

            if (ImGui::Button(arenaColors[i].name.c_str(), ImVec2(260, 100))) {
                selectedColorIndex = i;
                tm.selectedArenaColor = arenaColors[i].color;
            }

            if (isSelected) {
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
            }

            ImGui::PopStyleColor(2);
            ImGui::PopID();
            ImGui::NextColumn();
        }
        ImGui::Columns(1);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Selected: %s", arenaColors[selectedColorIndex].name.c_str());

        ImGui::Spacing();
        ImGui::Spacing();
        
        if (ImGui::Button("START WITH SELECTED COLOR", ImVec2(-1, 50))) {
            tm.arenaColorSelected = true;
            getApp()->changeState("play");
        }

        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::Button("USE DEFAULT RING TEXTURE", ImVec2(-1, 50))) {
            tm.arenaColorSelected = false;
            getApp()->changeState("play");
        }

        ImGui::End();
    }

    void onDestroy() override {
        // Nothing to clean up
    }
};
