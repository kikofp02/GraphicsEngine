// panel.cpp
#include "panels.h"
#include "engine.h"

#include <imgui_internal.h>

// cosas de imgui:
/*
    // Color Editor (compact)       Este tiene varias flags pa customizar
    ImGui::ColorEdit4("Color Picker", glm::value_ptr(app->bg_color), ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);

    // Color Picker (full palette)
    ImGui::ColorPicker4("Color Picker", glm::value_ptr(app->bg_color));

*/

void InitGUI(App* app) {
    // Initialize Panels
    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysVerticalScrollbar;
    app->panelManager.AddPanel(new SystemDetailsPanel(false, flags));
    app->panelManager.AddPanel(new ViewerPanel());
    app->panelManager.AddPanel(new ScenePanel());
    app->panelManager.AddPanel(new DebugPanel(false));
    app->panelManager.AddPanel(new MaterialsPanel());
    //app->panelManager.AddPanel(new LightingPanel());
    //app->panelManager.AddPanel(new PostProcessingPanel());


    // OpenGl info for imgui panel
    app->oglInfo.glVersion = std::string(reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    app->oglInfo.glRenderer = std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    app->oglInfo.glVendor = std::string(reinterpret_cast<const char*>(glGetString(GL_VENDOR)));

    app->oglInfo.glslVersion = std::string(reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
    GLint num_extensions;
    glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
    for (size_t i = 0; i < num_extensions; i++)
    {
        app->oglInfo.glExtensions.push_back(std::string(reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION))));
    }
}

void UpdateMainMenu(App* app) {

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("General")) {

            // TODO_K: K conio pongo?
            ImGui::Text("ª");

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window")) {
            if (ImGui::MenuItem("SystemDetails")) { app->panelManager.TogglePanel("SystemDetails"); }
            if (ImGui::MenuItem("Viewer")) { app->panelManager.TogglePanel("Viewer"); }
            if (ImGui::MenuItem("Scene")) { app->panelManager.TogglePanel("Scene"); }
            if (ImGui::MenuItem("Materials")) { app->panelManager.TogglePanel("Materials"); }

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

void SystemDetailsPanel::Update(App* app) {
    ImGui::Text("CONTROLS:");
    ImGui::Text("W A S D for camera movement");
    ImGui::Text("Mouse Right click + Mouse Drag -> camera rotation");
    ImGui::Text("Key 2 -> Swap render mode");

    if (ImGui::CollapsingHeader("System Information", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::CollapsingHeader("FPS Graph", ImGuiTreeNodeFlags_DefaultOpen)) {
            static float frameTimes[100] = {};
            static int frameOffset = 0;
            static float maxFrameTime = 0.0f;

            frameTimes[frameOffset] = app->deltaTime * 1000.0f;
            frameOffset = (frameOffset + 1) % IM_ARRAYSIZE(frameTimes);
            maxFrameTime = *std::max_element(frameTimes, frameTimes + IM_ARRAYSIZE(frameTimes)) * 1.1f;

            ImGui::PlotLines("Frame Time (ms)", frameTimes, IM_ARRAYSIZE(frameTimes), frameOffset,
                nullptr, 0.0f, maxFrameTime, ImVec2(300, 60));

            ImGui::SameLine();
            ImGui::Text("FPS: %.1f\n%.1f ms", 1.0f / app->deltaTime, app->deltaTime * 1000.0f);
        }

        if (ImGui::TreeNodeEx("OpenGL Details", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Renderer: %s", app->oglInfo.glRenderer.c_str());
            ImGui::Text("Vendor: %s", app->oglInfo.glVendor.c_str());
            ImGui::Text("Version: %s", app->oglInfo.glVersion.c_str());
            ImGui::Text("GLSL Version: %s", app->oglInfo.glslVersion.c_str());
            ImGui::TreePop();
        }

        // Extensions List
        if (ImGui::CollapsingHeader("OpenGL Extensions"))
        {
            static ImGuiTextFilter extensionsFilter;
            extensionsFilter.Draw("Filter", 200);

            ImGui::BeginChild("ExtensionsScrolling", ImVec2(0, 150), true);
            for (size_t i = 0; i < app->oglInfo.glExtensions.size(); i++)
            {
                if (extensionsFilter.PassFilter(app->oglInfo.glExtensions[i].c_str()))
                {
                    ImGui::Text("%s", app->oglInfo.glExtensions[i].c_str());
                }
            }
            ImGui::EndChild();
        }
    }
}

void ViewerPanel::Update(App* app) {
    
    // Modifiers Section
    if (ImGui::CollapsingHeader("Render Controls", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Current Mode
        const char* modeNames[] = { "Forward", "Debug FBO", "Deferred" };
        ImGui::Text("Current Mode: %s", modeNames[app->mode]);

        // Mode Selector
        ImGui::Separator();
        if (ImGui::Combo("Render Mode", reinterpret_cast<int*>(&app->mode),
            "Forward\0Debug FBO\0Deferred\0"))
        {

        }

        // Display Mode Selector
        ImGui::Separator();
        if (ImGui::Combo("Buffer View", reinterpret_cast<int*>(&app->displayMode),
            "Albedo\0Normals\0Positions\0Depth\0"))
        {
            Shader& quadShader = app->shaders[app->debugTexturesShaderIdx];
            quadShader.Use();
            quadShader.SetBool("uIsDepth", app->displayMode == Display_Depth);
        }
    }
}

void ScenePanel::Update(App* app) {

    // TODO_K: Este panel va a ser donde se configuren cosas generales de la escena:
    /*
        1.MODEL:
            transform
            material: pbr/matcap
            shadeing: lit/shadeless
        2.CAMERA:
            FOV
            clip dist
        3.WIREFRAME
            color
            transparency
        4.BG
            color//image//environment

    */

    ImGui::Separator();
    ImGui::Text("Model");
    ImGui::Separator();
    //dropdown selector to select the selected_model from the models list
    if (ImGui::BeginCombo("Select Model", app->selectedModel ? app->selectedModel->name.c_str() : "None"))
    {
        for (size_t i = 0; i < app->models.size(); ++i)
        {
            bool is_selected = (app->selectedModel == &app->models[i]);
            if (ImGui::Selectable(app->models[i].name.c_str(), is_selected))
            {
                app->selectedModel = &app->models[i];
            }

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::Dummy(ImVec2(0.0f, 20.0f));
    // Rotation editor
    if (app->selectedModel) {
        ImGui::SliderFloat3("Rotate XYZ", glm::value_ptr(app->selectedModel->rotation), -180.0f, 180.0f, "%.1f deg");
    }
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    if (app->selectedModel) {
        ImGui::SliderFloat3("Scale XYZ", glm::value_ptr(app->selectedModel->scale), 0.01f, 5.0f, "%.2f");
    }
    ImGui::Dummy(ImVec2(0.0f, 20.0f));
    ImGui::Checkbox("Rotate Models", &app->rotate_models);

    ImGui::Separator();
    ImGui::Text("Camera");
    ImGui::Separator();
    // slider from 1 to 175 that controls the app->camera.zoom that is a float
    ImGui::SliderFloat("FOV", &app->camera.Zoom, 1.0f, 175.0f);
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    // slider from 0.0001 to 1 that controls the app->camera.z_near that is a float
    ImGui::SliderFloat("Near", &app->camera.z_near, 0.0001f, 1.0f, "%.4f");

    //Whireframe editor?

    ImGui::Separator();
    ImGui::Text("Background");
    ImGui::Separator();
    ImGui::ColorEdit4("Background Color", glm::value_ptr(app->bg_color), ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);

    ImGui::Separator();
    ImGui::Text("Dev kit");
    ImGui::Separator();
    bool debugPanelState = app->panelManager.GetPanelState("Debug");
    if(ImGui::Checkbox("Debug Panel", &debugPanelState)) {
        app->panelManager.SetPanelState("Debug", debugPanelState);
    }
}

void DebugPanel::Update(App* app) {

    if (!app->panelManager.GetPanelState("ScenePanel")) { app->panelManager.SetPanelState("Debug", false); return; }

    // In this panel just display debug necesary things: logs, errors, etc.

    // Scene Info Section
    if (ImGui::CollapsingHeader("Scene Information", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Camera Position
        ImGui::Text("Position:");
        ImGui::Text("X: %7.2f", app->camera.Position.x);
        ImGui::SameLine();
        ImGui::Text("Y: %7.2f", app->camera.Position.y);
        ImGui::SameLine();
        ImGui::Text("Z: %7.2f", app->camera.Position.z);

        ImGui::Text("Front: %5.2f, %5.2f, %5.2f", app->camera.Front.x, app->camera.Front.y, app->camera.Front.z);
        ImGui::Text("Right: %5.2f, %5.2f, %5.2f", app->camera.Right.x, app->camera.Right.y, app->camera.Right.z);
        ImGui::Text("Up:    %5.2f, %5.2f, %5.2f", app->camera.Up.x, app->camera.Up.y, app->camera.Up.z);

        ImGui::Text("Pitch: %5.2f", app->camera.Pitch);
        ImGui::Text("Yaw: %5.2f", app->camera.Yaw);

        // Models/Lights
        ImGui::Separator();
        ImGui::Text("Loaded Models: %zu", app->models.size());
        ImGui::Text("Active Lights: %zu", app->lights.size());
    }
}

void LightingPanel::Update(App* app) {
    // TODO_K: Este panel va a ser donde se configuren las luces:
    /*
        1.GROUND SHADES (shadow catcher):
            intensity
            y pos
            size (scale)
            border fade
        2.LIGHT

    */
}

void MaterialsPanel::TextureSelector(App* app, std::string combo_name, Mat_Property* mat_prop) {
    if (ImGui::BeginCombo(combo_name.c_str(), mat_prop->texture ? mat_prop->texture->name.c_str() : "None"))
    {
        for (size_t i = 0; i < app->textures_loaded.size(); ++i)
        {
            bool is_selected = (mat_prop->texture == app->textures_loaded[i]);
            if (ImGui::Selectable(app->textures_loaded[i]->name.c_str(), is_selected))
            {
                mat_prop->texture = app->textures_loaded[i];
            }

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (mat_prop->texture) {
        ImGui::SameLine();
        ImGui::Checkbox(("Use##" + combo_name).c_str(), &mat_prop->tex_enabled);
    }
}

void MaterialsPanel::Update(App* app) {
    // TODO_K: Este panel va a ser donde se configuren los pospo filters:
    /*
        1.MATERIAL SELECTOR     dropdown
            highlight selected togle

        2.PBR MAPS
            base color
            metalic
            specular
        3.ANISOTROPY
            
        4.ROUGHNES/GLOSSINESS

        5.DISPLACEMENT

        6.NORMAL/BUMP

        7.ALPHA MASK

        8.EMISSION

        9.FACES RENDERING
            double/single sided
    */

    ImGui::Separator();
    ImGui::Text("Material");
    ImGui::Separator();
    //dropdown selector to select the selected_model from the models list
    if (ImGui::BeginCombo("Select Material", app->selectedMaterial ? app->selectedMaterial->name.c_str() : "None"))
    {
        for (size_t i = 0; i < app->selectedModel->materials.size(); ++i)
        {
            bool is_selected = (app->selectedMaterial == app->selectedModel->materials[i]);
            if (ImGui::Selectable(app->selectedModel->materials[i]->name.c_str(), is_selected))
            {
                app->selectedMaterial = app->selectedModel->materials[i];
            }

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::Dummy(ImVec2(0.0f, 20.0f));

    ImGui::Separator();
    ImGui::Text("Load Texture");
    ImGui::Separator();
    static char texPath[256] = "";
    ImGui::InputText("Texture Path", texPath, IM_ARRAYSIZE(texPath));

    ImGui::SameLine();
    if (ImGui::Button("Load Texture")) {
        Model::LoadTexture(app, texPath);
    }

    ImGui::Separator();
    ImGui::Text("PBR Maps");
    ImGui::Separator();
    ImGui::ColorEdit4("Base Color", glm::value_ptr(app->selectedMaterial->diffuse.color), ImGuiColorEditFlags_NoInputs);
    TextureSelector(app, "D_Texture", &app->selectedMaterial->diffuse);

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ImGui::ColorEdit4("Specular", glm::value_ptr(app->bg_color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker);
    TextureSelector(app, "S_Texture", &app->selectedMaterial->specular);

    ImGui::Dummy(ImVec2(0.0f, 20.0f));
    ImGui::Separator();
    ImGui::Text("Roughness/Glossiness");
    ImGui::SameLine();
    ImGui::Checkbox("##Roughness", &app->selectedMaterial->roughness.prop_enabled);
    ImGui::Separator();
    ImGui::ColorEdit4("Roughness/Glossiness", glm::value_ptr(app->selectedMaterial->roughness.color), ImGuiColorEditFlags_NoInputs);
    TextureSelector(app, "R/G_Texture", &app->selectedMaterial->roughness);

    ImGui::Dummy(ImVec2(0.0f, 20.0f));
    ImGui::Separator();
    ImGui::Text("Displacement");
    ImGui::SameLine();
    ImGui::Checkbox("##Height", &app->selectedMaterial->height.prop_enabled);
    ImGui::Separator();
    ImGui::ColorEdit4("Height", glm::value_ptr(app->selectedMaterial->height.color), ImGuiColorEditFlags_NoInputs);
    TextureSelector(app, "H_Texture", &app->selectedMaterial->height);

    ImGui::Dummy(ImVec2(0.0f, 20.0f));
    ImGui::Separator();
    ImGui::Text("Normal");
    ImGui::SameLine();
    ImGui::Checkbox("##Normal", &app->selectedMaterial->normal.prop_enabled);
    ImGui::Separator();
    ImGui::ColorEdit4("Normal", glm::value_ptr(app->selectedMaterial->normal.color), ImGuiColorEditFlags_NoInputs);
    TextureSelector(app, "N_Texture", &app->selectedMaterial->normal);
}

void PostProcessingPanel::Update(App* app) {
    // TODO_K: Este panel va a ser donde se configuren los pospo filters:
    /*
        1.BLOOM                     ON/OFF
            threshold
            intensity
            radius
        2.SSAO                      ON/OFF
            radius
            intensity
            bias
        3.DEPTH OF FIELD            ON/OFF
            foreground blurr
            bg blurr
        4.CHROMATIC ABERRATIONS     ON/OFF
            %
        5.VIGNETTE                  ON/OFF
            ammount
            hardness
        6.GRAIN (old screen eff)    ON/OFF
            opacity
            animated togle
        ETC....

    */
}