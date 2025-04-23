#include "engine.h"
#include "gl_error.h"

#include <imgui.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <filesystem>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#pragma region Loaders

Image LoadImage(const char* filename)
{
    Image img = {};
    stbi_set_flip_vertically_on_load(true);
    img.pixels = stbi_load(filename, &img.size.x, &img.size.y, &img.nchannels, 0);
    if (img.pixels)
    {
        img.stride = img.size.x * img.nchannels;
    }
    else
    {
        ELOG("Could not open file %s", filename);
    }
    return img;
}

void FreeImage(Image image)
{
    stbi_image_free(image.pixels);
}

GLuint CreateTexture2DFromImage(Image image)
{
    GLUtils::ErrorGuard guard("CreateTexture2d");

    GLenum internalFormat = GL_RGB8;
    GLenum dataFormat     = GL_RGB;
    GLenum dataType       = GL_UNSIGNED_BYTE;

    switch (image.nchannels)
    {
        case 3: dataFormat = GL_RGB; internalFormat = GL_RGB8; break;
        case 4: dataFormat = GL_RGBA; internalFormat = GL_RGBA8; break;
        default: ELOG("LoadTexture2D() - Unsupported number of channels");
    }

    GLuint texHandle;
    GL_CHECK(glGenTextures(1, &texHandle));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, texHandle));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.size.x, image.size.y, 0, dataFormat, dataType, image.pixels));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glGenerateMipmap(GL_TEXTURE_2D));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));

    return texHandle;
}

u32 LoadTexture2D(App* app, const char* filepath)
{
    for (u32 texIdx = 0; texIdx < app->textures_2D.size(); ++texIdx)
        if (app->textures_2D[texIdx].filepath == filepath)
            return texIdx;

    Image image = LoadImage(filepath);

    if (image.pixels)
    {
        Texture_2D tex = {};
        tex.handle = CreateTexture2DFromImage(image);
        tex.filepath = filepath;

        u32 texIdx = app->textures_2D.size();
        app->textures_2D.push_back(tex);

        FreeImage(image);
        return texIdx;
    }
    else
    {
        return UINT32_MAX;
    }
}

#pragma endregion

#pragma region Initialization

void InitTexturedQuad(App* app) {
    // - vertex buffers
    struct VertexV3V2 {
        glm::vec3 pos;
        glm::vec2 uv;
    };

    const VertexV3V2 vertices[] = {
        {glm::vec3(-0.5, -0.5, 0.0), glm::vec2(0.0, 0.0)}, //bottom-left
        {glm::vec3(0.5, -0.5, 0.0), glm::vec2(1.0, 0.0)}, //bottom-right
        {glm::vec3(0.5, 0.5, 0.0), glm::vec2(1.0, 1.0)}, //top-right
        {glm::vec3(-0.5, 0.5, 0.0), glm::vec2(0.0, 1.0)}, //top-left
    };

    const u16 indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    // - element/index buffers
    //Geometry
    GL_CHECK(glGenBuffers(1, &app->embeddedVertices));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, app->embeddedVertices));
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL_CHECK(glGenBuffers(1, &app->embeddedElements));
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->embeddedElements));
    GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW));
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    // - vaos
    //Attribute state
    GL_CHECK(glGenVertexArrays(1, &app->vao));
    GL_CHECK(glBindVertexArray(app->vao));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, app->embeddedVertices));
    GL_CHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)0));
    GL_CHECK(glEnableVertexAttribArray(0));
    GL_CHECK(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)12));
    GL_CHECK(glEnableVertexAttribArray(1));
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->embeddedElements));
    GL_CHECK(glBindVertexArray(0));

    // - programs (and retrieve uniform indices)
    app->shaders.emplace_back("shaders.glsl", "TEXTURED_GEOMETRY");
    app->texturedGeometryShaderIdx = app->shaders.size() - 1;

    // - textures
    app->diceTexIdx = LoadTexture2D(app, "dice.png");

    if (app->enableDebugGroups) {
        glObjectLabel(GL_VERTEX_ARRAY, app->vao, -1, "MainVAO");
        glObjectLabel(GL_BUFFER, app->embeddedVertices, -1, "QuadVertices");
        glObjectLabel(GL_BUFFER, app->embeddedElements, -1, "QuadIndices");
    }
}

void Init(App* app)
{
    GLUtils::InitDebugging(app);

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

    // TODO: Initialize your resources here!
    app->mode = Mode_TexturedMesh;

    app->camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f));

    GL_CHECK(glEnable(GL_DEPTH_TEST));
    GL_CHECK(glClearColor(0.1f, 0.1f, 0.1f, 1.0f));

    InitTexturedQuad(app);
    
    app->shaders.emplace_back("shaders.glsl", "TEXTURED_MESH");
    app->texturedMeshShaderIdx = app->shaders.size() - 1;

    Model myModel("Patrick/Patrick.obj");
    app->models.push_back(myModel);

    

    //Enable debug options
    if (app->enableDebugGroups) {
        for (Model model : app->models) {
            for (Mesh mesh : model.meshes) {
                glObjectLabel(GL_VERTEX_ARRAY, mesh.VAO, -1, "ModelVAO");
                glObjectLabel(GL_BUFFER, mesh.VBO, -1, "ModelVBO");
                glObjectLabel(GL_BUFFER, mesh.EBO, -1, "ModelEBO");
            }
        }

        for (Model model : app->models) {
            for (Texture texture : model.textures_loaded) {
                glObjectLabel(GL_TEXTURE, texture.id, -1,
                    texture.path.c_str());
            }
        }

        for (Texture_2D texture : app->textures_2D) {
            glObjectLabel(GL_TEXTURE, texture.handle, -1,
                texture.filepath.c_str());
        }
    }
}

#pragma endregion

void Gui(App* app)
{
    ImGui::Begin("Info");
    ImGui::Text("FPS: %f", 1.0f / app->deltaTime);

    if (ImGui::CollapsingHeader("OpenGL Information", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::TreeNodeEx("Version Information", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("OpenGL Version:");
            ImGui::Text(" -> %s", app->oglInfo.glVersion.c_str());

            ImGui::Text("OpenGL Renderer:");
            ImGui::Text(" -> %s", app->oglInfo.glRenderer.c_str());

            ImGui::Text("OpenGL Vendor:");
            ImGui::Text(" -> %s", app->oglInfo.glVendor.c_str());

            ImGui::Text("OpenGL GLSL Version:");
            ImGui::Text(" -> %s", app->oglInfo.glslVersion.c_str());
            ImGui::TreePop();
        }

        // For extensions with a filter (optional)
        static ImGuiTextFilter extensionsFilter;
        char extensionsLabel[128];
        snprintf(extensionsLabel, 128, "Extensions (%d)###Extensions", (int)app->oglInfo.glExtensions.size());

        if (ImGui::TreeNodeEx(extensionsLabel, ImGuiTreeNodeFlags_DefaultOpen))
        {
            extensionsFilter.Draw("Filter");
            ImGui::BeginChild("ExtensionsScrolling", ImVec2(0, 200), true);
            for (size_t i = 0; i < app->oglInfo.glExtensions.size(); i++)
            {
                if (extensionsFilter.PassFilter(app->oglInfo.glExtensions[i].c_str()))
                {
                    ImGui::Text(" -> %s", app->oglInfo.glExtensions[i].c_str());
                }
            }
            ImGui::EndChild();
            ImGui::TreePop();
        }
    }
    ImGui::End();
}

void Update(App* app)
{
    //TODO_K: optimizar esto pa que no corra cada frame tos los programs
    for (Shader& shader : app->shaders)
    {
        shader.ReloadIfNeeded();
    }

    if (app->input.keys[Key::K_2] == BUTTON_RELEASE) {
        switch (app->mode)
        {
        case Mode_TexturedQuad:
            app->mode = Mode_TexturedMesh;
            break;
        case Mode_TexturedMesh:
            app->mode = Mode_TexturedQuad;
            break;
        case Mode_Count:
            break;
        default:
            break;
        }
    }

    if (!app->models.empty()) {
        float speed = 1.0f; // Adjust speed as needed
        float radius = 2.0f; // Adjust movement radius as needed

        // Calculate new position using sine/cosine for circular motion
        //app->models[0].position.x = sin(app->time * speed) * radius;
        //app->models[0].position.z = cos(app->time * speed) * radius;

        // You could also add simple rotation
        app->models[0].rotation.y = fmod(app->time * 30.0f, 360.0f); // 30 degrees per second
    }

    // You can handle app->input keyboard/mouse here

    //Camera Update
    // Handle camera movement
    if (app->input.keys[Key::K_W] == BUTTON_PRESSED)
        app->camera.ProcessKeyboard(M_FORWARD, app->deltaTime);
    if (app->input.keys[Key::K_S] == BUTTON_PRESSED)
        app->camera.ProcessKeyboard(M_BACKWARD, app->deltaTime);
    if (app->input.keys[Key::K_A] == BUTTON_PRESSED)
        app->camera.ProcessKeyboard(M_LEFT, app->deltaTime);
    if (app->input.keys[Key::K_D] == BUTTON_PRESSED)
        app->camera.ProcessKeyboard(M_RIGHT, app->deltaTime);

    // Handle mouse movement for camera look
    if (app->input.mouseButtons[MouseButton::RIGHT] == BUTTON_PRESSED)
    {
        float xoffset = app->input.mouseDelta.x;
        float yoffset = -app->input.mouseDelta.y; // Reversed since y-coordinates go from bottom to top
        app->camera.ProcessMouseMovement(xoffset, yoffset);
    }

    //// Handle mouse scroll for zoom
    //if (app->input.mouseWheelDelta != 0)
    //{
    //    app->camera.ProcessMouseScroll(app->input.mouseWheelDelta);
    //    app->input.mouseWheelDelta = 0; // Reset after processing
    //}



    app->time += app->deltaTime;
}

void Render(App* app)
{
    GLUtils::ErrorGuard renderGuard("MainRender");

    if (app->enableDebugGroups) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "MainRenderPass");

    // - clear the framebuffer
    
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    // - set the viewport
    GL_CHECK(glViewport(0, 0, app->displaySize.x, app->displaySize.y));

    switch (app->mode)
    {
        case Mode_TexturedQuad:
        {
            if (app->enableDebugGroups) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 1, -1, "TexturedQuad");
            // TODO: Draw your textured quad here!
            
            

            // - set the blending state
            // - bind the texture into unit 0
            // - bind the program 
            //   (...and make its texture sample from unit 0)
            // - bind the vao
            Shader& currentShader = app->shaders[app->texturedGeometryShaderIdx];
            currentShader.Use();
            GL_CHECK(glBindVertexArray(app->vao));

            GL_CHECK(glEnable(GL_BLEND));
            GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

            GL_CHECK(glUniform1i(app->programUniformTexture, 0));
            GL_CHECK(glActiveTexture(GL_TEXTURE0));
            GLuint textureHandle = app->textures_2D[app->diceTexIdx].handle;
            if (textureHandle == 0) {
                ELOG("Invalid texture handle!");
                return;
            }
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureHandle));

            // - glDrawElements() !!!
            GL_CHECK(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0));

            GL_CHECK(glBindVertexArray(0));
            GL_CHECK(glUseProgram(0));

            if (app->enableDebugGroups) glPopDebugGroup();
        }
        break;

        case Mode_TexturedMesh:
        {
            Shader& currentShader = app->shaders[app->texturedMeshShaderIdx];
            currentShader.Use();

            // Calculate matrices
            glm::mat4 projection = glm::perspective(
                glm::radians(app->camera.Zoom),
                (float)app->displaySize.x / (float)app->displaySize.y,
                0.1f, 100.0f);
            glm::mat4 view = app->camera.GetViewMatrix();

            currentShader.SetMat4("uProjection", projection);
            currentShader.SetMat4("uView", view);
            currentShader.SetVec3("uViewPos", app->camera.Position);

            for (Model model : app->models) {
                if (app->enableDebugGroups) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 2, -1, "TexturedMesh");
                model.Draw(currentShader);
                if (app->enableDebugGroups) glPopDebugGroup();
            }
        }
        break;        

        default:;
    }

    if (app->enableDebugGroups) glPopDebugGroup();
}