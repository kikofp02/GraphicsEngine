//panel.h
#pragma once

#include "platform.h"
#include "imgui.h"

#include <vector>
#include <string>

class App; // Forward declaration

class GUI_Panel {
public:
    GUI_Panel(const std::string& name, bool defaultOpen = true, ImGuiWindowFlags flags = ImGuiWindowFlags_None)
        : name(name), isOpen(defaultOpen), flags(flags) {}

    virtual ~GUI_Panel() = default;
    virtual void Update(App* app) = 0;

    std::string name;
    bool isOpen;
    ImGuiWindowFlags flags;
};

class GUI_PanelManager {
private:
    std::vector<GUI_Panel*> panels;

public:
    void AddPanel(GUI_Panel* panel) {
        panels.push_back(std::move(panel));
    }

    void UpdatePanels(App* app) {
        for (auto& panel : panels) {
            if (panel->isOpen) {
                ImGui::Begin(panel->name.c_str(), &panel->isOpen, panel->flags);
                panel->Update(app);
                ImGui::End();
            }
        }
    }

    void TogglePanel(const std::string& panelName) {
        for (auto& panel : panels) {
            if (panel->name == panelName) {
                panel->isOpen = !panel->isOpen;
            }
        }
    }
};

void InitGUI(App* app);

void UpdateMainMenu(App* app);

class InfoPanel : public GUI_Panel {
public:
    InfoPanel(const std::string& name = "Info", bool defaultOpen = false, ImGuiWindowFlags flags = ImGuiWindowFlags_None) : GUI_Panel(name, defaultOpen, flags) {}

    void Update(App* app) override;

private:

};

class ViewerPanel : public GUI_Panel {
public:
    ViewerPanel(const std::string& name = "Viewer", bool defaultOpen = true, ImGuiWindowFlags flags = ImGuiWindowFlags_None) : GUI_Panel(name, defaultOpen, flags) {}

    void Update(App* app) override;

private:

};