//panel.h
#pragma once

#include "platform.h"
#include "model.h"
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

    bool GetPanelState(const std::string& panelName) {
        for (auto& panel : panels) {
            if (panel->name == panelName) {
                return panel->isOpen;
            }
        }
    }

    void SetPanelState(const std::string& panelName, bool state) {
        for (auto& panel : panels) {
            if (panel->name == panelName) {
                panel->isOpen = state;
            }
        }
    }
};

void InitGUI(App* app);

void UpdateMainMenu(App* app);

class SystemDetailsPanel : public GUI_Panel {
public:
    SystemDetailsPanel(bool defaultOpen = false, ImGuiWindowFlags flags = ImGuiWindowFlags_None, const std::string& name = "SystemDetails") : GUI_Panel(name, defaultOpen, flags) {}

    void Update(App* app) override;

private:

};

class ViewerPanel : public GUI_Panel {
public:
    ViewerPanel(bool defaultOpen = true, ImGuiWindowFlags flags = ImGuiWindowFlags_None, const std::string& name = "Viewer") : GUI_Panel(name, defaultOpen, flags) {}

    void Update(App* app) override;

private:

};

class ScenePanel : public GUI_Panel {
public:
    ScenePanel(bool defaultOpen = true, ImGuiWindowFlags flags = ImGuiWindowFlags_None, const std::string& name = "Scene") : GUI_Panel(name, defaultOpen, flags) {}

    void Update(App* app) override;

private:

};

class DebugPanel : public GUI_Panel {
public:
    DebugPanel(bool defaultOpen = true, ImGuiWindowFlags flags = ImGuiWindowFlags_None, const std::string& name = "Debug") : GUI_Panel(name, defaultOpen, flags) {}

    void Update(App* app) override;

private:

};

class LightingPanel : public GUI_Panel {
public:
    LightingPanel(bool defaultOpen = true, ImGuiWindowFlags flags = ImGuiWindowFlags_None, const std::string& name = "Lighting Panel") : GUI_Panel(name, defaultOpen, flags) {}

    void Update(App* app) override;

private:

};

class MaterialsPanel : public GUI_Panel {
public:
    MaterialsPanel(bool defaultOpen = true, ImGuiWindowFlags flags = ImGuiWindowFlags_None, const std::string& name = "Materials Panel") : GUI_Panel(name, defaultOpen, flags) {}

    void Update(App* app) override;
    void TextureSelector(App* app, std::string combo_name, Mat_Property* mat_prop);
};

class PostProcessingPanel : public GUI_Panel {
public:
    PostProcessingPanel(bool defaultOpen = true, ImGuiWindowFlags flags = ImGuiWindowFlags_None, const std::string& name = "Post Processing") : GUI_Panel(name, defaultOpen, flags) {}

    void Update(App* app) override;

private:

};