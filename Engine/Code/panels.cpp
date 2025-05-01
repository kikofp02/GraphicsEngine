// panel.cpp
#include "panels.h"
#include "engine.h"

#include <imgui_internal.h>

void InitGUI(App* app) {
    // Initialize Panels
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysVerticalScrollbar;
    app->panelManager.AddPanel(new InfoPanel("Info", false, flags));
    app->panelManager.AddPanel(new ViewerPanel());


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

		if (ImGui::BeginMenu("Display")) {

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

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window")) {
            if (ImGui::MenuItem("Info")) { app->panelManager.TogglePanel("Info"); }
            if (ImGui::MenuItem("Viewer")) { app->panelManager.TogglePanel("Viewer"); }

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

void InfoPanel::Update(App* app) {
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

void ViewerPanel::Update(App* app) {
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