#pragma once

#include <application.hpp>
#include <imgui.h>
#include "../common/tournament-manager.hpp"

class BracketState : public our::State {
    void onInitialize() override {
        // Unlock mouse for UI interaction
        getApp()->getMouse().unlockMouse(getApp()->getWindow());
    }

    void onImmediateGui() override {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), 
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(800, 500), ImGuiCond_Always);

        ImGui::Begin("Tournament Bracket", nullptr, 
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        auto& tm = our::TournamentManager::getInstance();
        const auto& player = tm.getSelectedCharacter();

        // Round 1 Opponent: Blue Frost
        // Round 2 Opponent: Gold Lion
        // Round 3 Opponent: Black Shadow
        std::string currentOpponent = "Blue Frost";
        if (tm.currentRound == 2) currentOpponent = "Gold Lion";
        if (tm.currentRound == 3) currentOpponent = "Black Shadow";

        ImGui::Text("Round: %s", tm.currentRound == 1 ? "Quarterfinals" : 
                                 (tm.currentRound == 2 ? "Semifinals" : "Finals"));
        ImGui::Separator();
        ImGui::Spacing();

        // Simple visualization of the bracket
        ImGui::Columns(3, "BracketColumns", true);
        
        // Column 1: Quarterfinals
        ImGui::Text("Quarterfinals");
        drawMatch("Match 1", player.name, "Blue Frost", tm.currentRound == 1);
        drawMatch("Match 2", "Gold Lion", "Cyan Storm", false);
        drawMatch("Match 3", "Purple Night", "Orange Heat", false);
        drawMatch("Match 4", "Black Shadow", "Green Viper", false);
        
        ImGui::NextColumn();

        // Column 2: Semifinals
        ImGui::Text("Semifinals");
        drawMatch("Match 5", tm.currentRound >= 2 ? player.name : "TBD", tm.currentRound >= 2 ? "Gold Lion" : "TBD", tm.currentRound == 2);
        drawMatch("Match 6", tm.currentRound >= 2 ? "Purple Night" : "TBD", tm.currentRound >= 2 ? "Black Shadow" : "TBD", false);

        ImGui::NextColumn();

        // Column 3: Finals
        ImGui::Text("Finals");
        drawMatch("Championship", tm.currentRound >= 3 ? player.name : "TBD", tm.currentRound >= 3 ? "Black Shadow" : "TBD", tm.currentRound == 3);

        ImGui::Columns(1);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (tm.currentRound > 3) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
            ImGui::SetWindowFontScale(2.0f);
            ImGui::Text("TOURNAMENT CHAMPION: %s", player.name.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
            if (ImGui::Button("BACK TO MAIN MENU", ImVec2(-1, 50))) {
                tm.reset();
                getApp()->changeState("menu");
            }
        } else {
            if (ImGui::Button("START NEXT MATCH", ImVec2(-1, 50))) {
                // Only go to color selection on the first match
                if (tm.currentRound == 1 && !tm.arenaColorSelected) {
                    tm.arenaColorSelected = true;
                    getApp()->changeState("color-select");
                } else {
                    getApp()->changeState("play");
                }
            }
        }

        ImGui::End();
    }

private:
    void drawMatch(const char* label, std::string fighter1, std::string fighter2, bool isCurrent) {
        if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 0, 1));
        
        ImGui::BeginChild(label, ImVec2(0, 80), true);
        ImGui::Text("%s", label);
        ImGui::Separator();
        ImGui::Text("1. %s", fighter1.c_str());
        ImGui::Text("2. %s", fighter2.c_str());
        ImGui::EndChild();

        if (isCurrent) ImGui::PopStyleColor();
    }
};
