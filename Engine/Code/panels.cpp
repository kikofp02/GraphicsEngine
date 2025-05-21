// panel.cpp
#include "panels.h"
#include "engine.h"

#include <GLFW/glfw3.h>
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
    app->panelManager.AddPanel(new SystemDetailsPanel(true, flags));
    app->panelManager.AddPanel(new DocumentationPanel(true, flags));
    app->panelManager.AddPanel(new ViewerPanel());
    app->panelManager.AddPanel(new ScenePanel());
    app->panelManager.AddPanel(new DebugPanel(false));
    app->panelManager.AddPanel(new MaterialsPanel());
    app->panelManager.AddPanel(new LightingPanel());
    app->panelManager.AddPanel(new PostProcessingPanel());


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

            // TODO_K: what can we put here?
            ImGui::Text("�");

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window")) {
            if (ImGui::MenuItem("SystemDetails")) { app->panelManager.TogglePanel("SystemDetails"); }
            if (ImGui::MenuItem("Documentation")) { app->panelManager.TogglePanel("Documentation"); }
            if (ImGui::MenuItem("Viewer")) { app->panelManager.TogglePanel("Viewer"); }
            if (ImGui::MenuItem("Scene")) { app->panelManager.TogglePanel("Scene"); }
            if (ImGui::MenuItem("Materials")) { app->panelManager.TogglePanel("Materials"); }
            if (ImGui::MenuItem("Lighting")) { app->panelManager.TogglePanel("Lighting"); }
            if (ImGui::MenuItem("PostProcessing")) { app->panelManager.TogglePanel("PostProcessing"); }

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

void SystemDetailsPanel::Update(App* app) {

    if (ImGui::CollapsingHeader("System Information", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::CollapsingHeader("FPS Graph", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Circular buffer for FPS values (smoothed)
            static float fpsValues[100] = {};
            static int fpsOffset = 0;

            // Exponential moving average for smoother FPS
            static float smoothedFPS = 60.0f;
            const float alpha = 0.1f; // Smoothing factor (0.0-1.0, lower = smoother)
            float currentFPS = 1.0f / app->deltaTime;
            smoothedFPS = smoothedFPS * (1.0f - alpha) + currentFPS * alpha;

            // Store the smoothed value
            fpsValues[fpsOffset] = smoothedFPS;
            fpsOffset = (fpsOffset + 1) % IM_ARRAYSIZE(fpsValues);

            // Auto-scaling Y-axis
            float maxFPS = *std::max_element(fpsValues, fpsValues + IM_ARRAYSIZE(fpsValues)) * 1.1f;
            maxFPS = ImMax(maxFPS, 60.0f); // Minimum scale

            // Style setup
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.05f, 0.8f, 0.95f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.15f, 0.7f));

            // FPS graph
            ImGui::PlotLines(
                "##FPS",
                fpsValues,
                IM_ARRAYSIZE(fpsValues),
                fpsOffset,
                nullptr,
                0.0f,
                maxFPS,
                ImVec2(300, 60)
            );
            ImGui::PopStyleColor(2);

            // Display current FPS (smoothed)
            //ImGui::SameLine();
            ImGui::TextColored(
                ImVec4(0.05f, 0.8f, 0.95f, 1.0f),
                "%.0f FPS",
                smoothedFPS
            );

            // Optional: Add a subtle indicator for target FPS (e.g., 60 FPS)
            if (maxFPS > 60.0f) {
                ImGui::SameLine(0.0f, 10.0f);
                ImGui::TextDisabled("| Target: 60 FPS");
            }
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

void DocumentationPanel::Update(App* app) {

    // Section styling
    const ImVec4 headerColor = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);
    const ImVec4 textColor = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
    const float indentSpacing = 20.0f;

    // Engine Overview
    ImGui::PushStyleColor(ImGuiCol_Text, headerColor);
    if (ImGui::CollapsingHeader("Engine Overview", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::Indent(indentSpacing);

        ImGui::TextWrapped(
            "This engine provides real-time 3D rendering with support for:\n"
            "- Forward and Deferred rendering pipelines\n"
            "- PBR materials with texture mapping\n"
            "- Dynamic lighting system\n"
            "- Post-processing effects\n"
            "- Model loading via Assimp"
        );

        ImGui::Unindent(indentSpacing);
    }
    else
    {
        ImGui::PopStyleColor();
    }

    // Controls Section
    ImGui::PushStyleColor(ImGuiCol_Text, headerColor);
    if (ImGui::CollapsingHeader("Controls"))
    {
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::Indent(indentSpacing);

        ImGui::TextWrapped("Camera Controls:");
        ImGui::BulletText("WASD: Move camera");
        ImGui::BulletText("SPACE/CTRL: Move up/down");
        ImGui::BulletText("Right Mouse + Drag: Rotate camera");
        ImGui::BulletText("Mouse Wheel: Zoom in/out");
        ImGui::BulletText("F: Toggle between Free and Orbit Camera Modes");

        ImGui::TextWrapped("\nMode Switching:");
        ImGui::BulletText("2: Cycle through render modes");
        ImGui::BulletText("3: Cycle through debug views");
        ImGui::Unindent(indentSpacing);
    }
    else
    {
        ImGui::PopStyleColor();
    }

    // Rendering Features
    ImGui::PushStyleColor(ImGuiCol_Text, headerColor);
    if (ImGui::CollapsingHeader("Rendering Features"))
    {
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::Indent(indentSpacing);

        ImGui::TextWrapped("Available Rendering Modes:");
        ImGui::BulletText("Forward: Basic rendering pipeline");
        ImGui::BulletText("Deferred: Advanced lighting pipeline");
        ImGui::BulletText("Debug Views: Inspect individual buffers");
        ImGui::Indent();
        ImGui::BulletText("Albedo: Surface color textures");
        ImGui::BulletText("Normals: World-space normal vectors");
        ImGui::BulletText("Positions: World-space fragment positions");
        ImGui::BulletText("Depth: Linear depth buffer (white=near, black=far)");
        ImGui::BulletText("MatProps: Material properties (metallic/roughness/height)");
        ImGui::BulletText("LightPass: Final lighting calculations");
        ImGui::BulletText("Brightness: High-intensity areas for bloom");
        ImGui::BulletText("Blur: Bloom effect intermediate buffers");
        ImGui::Unindent();

        ImGui::TextWrapped("\nPost-processing Effects:");
        ImGui::BulletText("Bloom: Light bleeding effect");

        ImGui::Unindent(indentSpacing);
    }
    else
    {
        ImGui::PopStyleColor();
    }

    // Material System
    ImGui::PushStyleColor(ImGuiCol_Text, headerColor);
    if (ImGui::CollapsingHeader("Material System"))
    {
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::Indent(indentSpacing);

        ImGui::TextWrapped("Supported Material Properties:");
        ImGui::BulletText("Albedo/Diffuse color");
        ImGui::BulletText("Metallic maps");
        ImGui::BulletText("Roughness maps");
        ImGui::BulletText("Normal maps");
        ImGui::BulletText("Height maps (parallax oclussion mapping)");

        ImGui::Unindent(indentSpacing);
    }
    else
    {
        ImGui::PopStyleColor();
    }

}

void ViewerPanel::Update(App* app) {
    
    // Modifiers Section
    if (ImGui::CollapsingHeader("Render Controls", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Separator();
        if (ImGui::Checkbox("V-Sync", &app->vsyncEnabled)) {
            glfwSwapInterval(app->vsyncEnabled ? 1 : 0);
        }
        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        // Current Mode
        const char* modeNames[] = { "Forward", "Debug FBO", "Deferred" };
        ImGui::Text("Current Mode: %s", modeNames[app->mode]);

        // Mode Selector
        ImGui::Separator();
        if (ImGui::Combo("Render Mode", reinterpret_cast<int*>(&app->mode),
            "Forward\0Debug FBO\0Deferred\0"))
        {

        }

        if (app->mode == Mode::Mode_DebugFBO) {
            // Display Mode Selector
            ImGui::Dummy(ImVec2(0.0f, 20.0f));
            ImGui::Separator();
            if (ImGui::Combo("Buffer View", reinterpret_cast<int*>(&app->displayMode),
                "Albedo\0Normals\0Positions\0Depth\0MatProps\0LightPass\0Brightness\0Blurr\0"))
            {
                Shader& quadShader = app->shaders[app->debugTexturesShaderIdx];
                quadShader.Use();
                quadShader.SetBool("uIsDepth", app->displayMode == Display_Depth);
            }
        }

        if (app->mode == Mode::Mode_Forward) { ImGui::Text("Post processing disabled on forward rendering mode"); }
    }
}

void ScenePanel::Update(App* app) {

    // TODO_K: This will be the general scene parameters section:
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
                app->selectedMaterial = app->selectedModel->materials[0];
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
    ImGui::Checkbox("Render All", &app->renderAll);
    ImGui::Dummy(ImVec2(0.0f, 20.0f));
    ImGui::Checkbox("Rotate Models", &app->rotate_models);
    ImGui::SliderFloat("Rotate Speed", &app->rotate_speed, 0.01f, 5.0f, "%.2f");

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

    if (!app->panelManager.GetPanelState("Scene")) { app->panelManager.SetPanelState("Debug", false); return; }

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

        const char* mode = (app->camera.Mode == CAMERA_FREE) ? "FREE" : "ORBIT";
        ImGui::Text("Mode: %s", mode);


        // Models/Lights
        ImGui::Separator();
        ImGui::Text("Loaded Models: %zu", app->models.size());
        ImGui::Text("Active Lights: %zu", app->lights.size());
    }
}

void LightingPanel::Update(App* app) {
    // TODO_K: Here the light settings section:
    /*
        1.GROUND SHADES (shadow catcher):
            intensity
            y pos
            size (scale)
            border fade
        2.LIGHT

    */

    ImGui::Separator();
    ImGui::Text("Light");
    ImGui::Separator();
    //dropdown selector to select the selected_model from the models list
    if (ImGui::BeginCombo("Selected Light", app->selectedLight ? app->selectedLight->name.c_str() : "None"))
    {
        for (size_t i = 0; i < app->lights.size(); ++i)
        {
            bool is_selected = (app->selectedLight == &app->lights[i]);
            if (ImGui::Selectable(app->lights[i].name.c_str(), is_selected))
            {
                app->selectedLight = &app->lights[i];
            }

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::Dummy(ImVec2(0.0f, 20.0f));

    if (app->selectedLight)
    {
        Light& light = *app->selectedLight;

        // Type (display only)
        const char* typeName = (light.type == LightType_Directional) ? "Directional" : "Point";
        ImGui::Text("Type: %s", typeName);

        ImGui::Checkbox("Light ON/OFF", &light.enabled);

        // Light Color
        ImGui::ColorEdit3("Color", (float*)&light.color);

        // Intensity
        ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 20.0f);

        if (light.type == LightType_Point)
        {
            // Point Light specific
            ImGui::InputFloat3("Position", (float*)&light.position);
            ImGui::SliderFloat("Range", &light.range, 0.1f, 100.0f);
        }
        else if (light.type == LightType_Directional)
        {
            // Directional Light specific
            ImGui::InputFloat3("Direction", (float*)&light.direction);
        }
    }
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
    // TODO_K: Model Materials settings section:
    /*
        1.MATERIAL SELECTOR     dropdown
            highlight selected togle

        2.PBR MAPS
            base color
            metallic
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
    if (ImGui::BeginCombo("Selected Material", app->selectedMaterial ? app->selectedMaterial->name.c_str() : "None"))
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
    ImGui::ColorEdit4("Metallic", glm::value_ptr(app->selectedMaterial->metallic.color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker);
    TextureSelector(app, "M_Texture", &app->selectedMaterial->metallic);

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
    ImGui::Text("Normal");
    ImGui::SameLine();
    ImGui::Checkbox("##Normal", &app->selectedMaterial->normal.prop_enabled);
    ImGui::Separator();
    ImGui::ColorEdit4("Normal", glm::value_ptr(app->selectedMaterial->normal.color), ImGuiColorEditFlags_NoInputs);
    TextureSelector(app, "N_Texture", &app->selectedMaterial->normal);

    ImGui::Dummy(ImVec2(0.0f, 20.0f));
    ImGui::Separator();
    ImGui::Text("Displacement");
    ImGui::SameLine();
    ImGui::Checkbox("##Height", &app->selectedMaterial->height.prop_enabled);
    ImGui::Separator();
    ImGui::ColorEdit4("Height", glm::value_ptr(app->selectedMaterial->height.color), ImGuiColorEditFlags_NoInputs);
    TextureSelector(app, "H_Texture", &app->selectedMaterial->height);

    if (app->selectedMaterial->height.prop_enabled)
    {
        ImGui::Text("Parallax Oclusion Settings");
        ImGui::DragFloat("Parallax Scale", &app->parallax_scale, 0.1f, 0.0f, 2.0f);
        ImGui::DragFloat("Number of Layers", &app->parallax_layers, 1.0f, 0.0f, 20.0f);
    }

    // TODO_K: Alpha Masking not working properly, to hard to implement correctly
    /*ImGui::Dummy(ImVec2(0.0f, 20.0f));
    ImGui::Separator();
    ImGui::Text("Alpha Mask");
    ImGui::SameLine();
    ImGui::Checkbox("##AlphaMask", &app->selectedMaterial->alphaMask.prop_enabled);
    ImGui::Separator();
    ImGui::ColorEdit4("Alpha Mask", glm::value_ptr(app->selectedMaterial->alphaMask.color), ImGuiColorEditFlags_NoInputs);
    TextureSelector(app, "AM_Texture", &app->selectedMaterial->alphaMask);*/
}

void PostProcessingPanel::Update(App* app) {
    // TODO_K: Posproduction filters settings section:
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

    ImGui::Separator();
    ImGui::Text("Bloom");
    ImGui::Separator();

    ImGui::Text("Bloom Active");
    ImGui::SameLine();
    ImGui::Checkbox("##Bloom", &app->bloomEnable);
    if (app->bloomEnable)
    {
        ImGui::SliderInt("Gaussian Amount", &app->bloomAmount, 0, 10);
        ImGui::DragFloat("Exposure", &app->bloomExposure, 0.01f, 0.0f, 3.0f);
        ImGui::DragFloat("Gamma", &app->bloomGamma, 0.01f, 0.0f, 3.0f);
    }
}