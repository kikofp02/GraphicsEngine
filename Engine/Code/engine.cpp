//engine.cpp
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

#pragma region Utilities

bool IsPowerOf2(u32 value)
{
    return value && !(value & (value - 1));
}

static u32 Align(u32 value, u32 alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

Buffer CreateBuffer(u32 size, GLenum type, GLenum usage)
{
    Buffer buffer = {};
    buffer.size = size;
    buffer.type = type;

    GL_CHECK(glGenBuffers(1, &buffer.handle));
    GL_CHECK(glBindBuffer(type, buffer.handle));
    GL_CHECK(glBufferData(type, buffer.size, NULL, usage));
    GL_CHECK(glBindBuffer(type, 0));

    return buffer;
}

#define CreateConstantBuffer(size) CreateBuffer(size, GL_UNIFORM_BUFFER, GL_STREAM_DRAW)
#define CreateStaticVertexBuffer(size) CreateBuffer(size, GL_ARRAY_BUFFER, GL_STATIC_DRAW)
#define CreateStaticIndexBuffer(size) CreateBuffer(size, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW)

void BindBuffer(const Buffer& buffer)
{
    GL_CHECK(glBindBuffer(buffer.type, buffer.handle));
}

void MapBuffer(Buffer& buffer, GLenum access)
{
    GL_CHECK(glBindBuffer(buffer.type, buffer.handle));
    void* ptr = glMapBuffer(buffer.type, access);
    buffer.data = (u8*)ptr;

    //buffer.data = (u8*)glMapBuffer(buffer.type, access);
    buffer.head = 0;
}

void UnmapBuffer(Buffer& buffer)
{
    GL_CHECK(glBindBuffer(buffer.type, buffer.handle));
    GL_CHECK(glUnmapBuffer(buffer.type));
    GL_CHECK(glBindBuffer(buffer.type, 0));
}

void AlignHead(Buffer& buffer, u32 alignment)
{
    ASSERT(IsPowerOf2(alignment), "The alignment must be a power of 2");
    buffer.head = Align(buffer.head, alignment);
}

void PushAlignedData(Buffer& buffer, const void* data, u32 size, u32 alignment)
{
    ASSERT(buffer.data != NULL, "The buffer must be mapped first");
    AlignHead(buffer, alignment);
    memcpy((u8*)buffer.data + buffer.head, data, size);
    buffer.head += size;
}

#define PushData(buffer, data, size) PushAlignedData(buffer, data, size, 1)
#define PushUInt(buffer, value) { u32 v = value; PushAlignedData(buffer, &v, sizeof(v), 4); }
#define PushVec3(buffer, value) PushAlignedData(buffer, value_ptr(value), sizeof(value), sizeof(vec4))
#define PushVec4(buffer, value) PushAlignedData(buffer, value_ptr(value), sizeof(value), sizeof(vec4))
#define PushMat3(buffer, value) PushAlignedData(buffer, value_ptr(value), sizeof(value), sizeof(vec4))
#define PushMat4(buffer, value) PushAlignedData(buffer, value_ptr(value), sizeof(value), sizeof(vec4))

#pragma endregion

#pragma region Initialization

Model GeneratePlaneModel(App* app, float size, int subdivisions, const std::string& texturePath) {
    Model model;
    Mesh planeMesh;
    Submesh planeSubmesh;

    // Generate vertices
    float step = size / subdivisions;
    float halfSize = size * 0.5f;

    for (int z = 0; z <= subdivisions; ++z) {
        for (int x = 0; x <= subdivisions; ++x) {
            Vertex vertex;
            vertex.Position = glm::vec3(
                -halfSize + x * step,
                0.0f,
                -halfSize + z * step
            );
            vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
            vertex.TexCoords = glm::vec2(
                static_cast<float>(x) / subdivisions,
                static_cast<float>(z) / subdivisions
            );
            vertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            vertex.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);

            planeSubmesh.vertices.push_back(vertex);
        }
    }

    // Generate indices
    for (int z = 0; z < subdivisions; ++z) {
        for (int x = 0; x < subdivisions; ++x) {
            int topLeft = z * (subdivisions + 1) + x;
            int topRight = topLeft + 1;
            int bottomLeft = (z + 1) * (subdivisions + 1) + x;
            int bottomRight = bottomLeft + 1;

            // First triangle
            planeSubmesh.indices.push_back(topLeft);
            planeSubmesh.indices.push_back(bottomLeft);
            planeSubmesh.indices.push_back(topRight);

            // Second triangle
            planeSubmesh.indices.push_back(topRight);
            planeSubmesh.indices.push_back(bottomLeft);
            planeSubmesh.indices.push_back(bottomRight);
        }
    }

    // Load and assign texture
    u32 texIdx = LoadTexture2D(app, texturePath.c_str());
    if (texIdx != UINT32_MAX) {
        Texture texture;
        texture.id = app->textures_2D[texIdx].handle;
        texture.type = "texture_diffuse";
        texture.path = texturePath;

        // Add to model's loaded textures
        model.textures_loaded.push_back(texture);
        // Add to submesh textures
        planeSubmesh.textures.push_back(texture);
    }

    planeMesh.submeshes.push_back(planeSubmesh);
    planeMesh.SetupMesh();
    model.meshes.push_back(planeMesh);

    return model;
}

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

    app->camera = Camera(glm::vec3(0.0f, 4.0f, 15.0f));

    GL_CHECK(glEnable(GL_DEPTH_TEST));
    GL_CHECK(glClearColor(0.1f, 0.1f, 0.1f, 1.0f));

    InitTexturedQuad(app);
    
    app->shaders.emplace_back("shaders.glsl", "TEXTURED_MESH");
    app->texturedMeshShaderIdx = app->shaders.size() - 1;

    //Ground Plane
    Model planeModel = GeneratePlaneModel(app, 10.0f, 10, "color_white.png");
    app->models.push_back(planeModel);
    app->models.back().scale = glm::vec3(4.0f, 1.0f, 4.0f);

    //Patricks
    Model patrickModel("Patrick/Patrick.obj");
    // Define grid parameters
    int countPerSide = 4; // 3x3 grid (9 Patricks total)
    float spacing = 8.0f; // Space between models (adjust based on Patrick's size)

    // Calculate starting position to center the grid
    float totalWidth = (countPerSide - 1) * spacing;
    float startX = -totalWidth / 2.0f;
    float startZ = -totalWidth / 2.0f;

    // Create grid of Patricks
    for (int z = 0; z < countPerSide; ++z) {
        for (int x = 0; x < countPerSide; ++x) {
            // Create a copy of the original model
            Model patrickCopy = patrickModel;

            // Calculate position (keep original Y, offset X and Z)
            patrickCopy.position = glm::vec3(
                startX + x * spacing,  // X position
                3.4f, // Keep original Y
                startZ + z * spacing   // Z position
            );

            // Add to models list
            app->models.push_back(patrickCopy);
        }
    }

    // Lights
    Light sun;
    sun.type = LightType_Directional;
    sun.color = glm::vec3(1.0f, 0.9f, 0.8f);
    sun.direction = glm::vec3(-0.5f, -1.0f, -0.5f);
    app->lights.push_back(sun);

    Light pointLight;
    pointLight.type = LightType_Point;
    pointLight.color = glm::vec3(1.0f, 0.5f, 0.3f);
    pointLight.position = glm::vec3(0.0f, 5.0f, 0.0f);
    pointLight.range = 10.0f;
    app->lights.push_back(pointLight);

    // UBOs
    // Transform UBO
    GL_CHECK(glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
        reinterpret_cast<GLint*>(&app->transformsUBO.alignment)));

    // Calculate block size with alignment
    app->transformsUBO.blockSize = 2 * sizeof(glm::mat4);
    app->transformsUBO.blockSize = Align(app->transformsUBO.blockSize, app->transformsUBO.alignment);

    // Create buffer using utilities
    app->transformsUBO.buffer = CreateBuffer(
        (app->models.size() + 10) * app->transformsUBO.blockSize,
        GL_UNIFORM_BUFFER,
        GL_DYNAMIC_DRAW
    );

    // Global UBO
    GL_CHECK(glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
        reinterpret_cast<GLint*>(&app->globalParamsUBO.alignment)));

    size_t cameraPosSize = sizeof(glm::vec4); // vec3 + padding
    size_t lightCountSize = sizeof(glm::uvec4); // uint + padding (to vec4)
    size_t lightSize = 4 * sizeof(glm::vec4); // uint (as vec4) + 3 vec4s
    app->globalParamsUBO.blockSize = cameraPosSize + lightCountSize + 16 * lightSize;
    app->globalParamsUBO.blockSize = Align(app->globalParamsUBO.blockSize, app->globalParamsUBO.alignment);

    app->globalParamsUBO.buffer = CreateBuffer(
        app->globalParamsUBO.blockSize,
        GL_UNIFORM_BUFFER,
        GL_STREAM_DRAW
    );
    
    // Enable debug options
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

void UpdateUBOs(App* app) {

    // Transform UBO
    // Calculate matrices
    glm::mat4 view = app->camera.GetViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(app->camera.Zoom),
        (float)app->displaySize.x / (float)app->displaySize.y,
        0.1f, 100.0f
    );

    // Map buffer using utility
    MapBuffer(app->transformsUBO.buffer, GL_WRITE_ONLY);
    app->transformsUBO.currentOffset = 0;

    for (auto& model : app->models) {
        // Calculate matrices
        glm::mat4 world = glm::mat4(1.0f);
        world = glm::translate(world, model.position);
        world = glm::rotate(world, glm::radians(model.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        world = glm::scale(world, model.scale);

        glm::mat4 mvp = projection * view * world;

        // Push data with automatic alignment
        PushMat4(app->transformsUBO.buffer, world);      // Uses alignment from utilities
        PushMat4(app->transformsUBO.buffer, mvp);        // Auto-aligns after first matrix

        // Store model metadata
        model.bufferOffset = app->transformsUBO.currentOffset;
        model.bufferSize = app->transformsUBO.blockSize;

        // Advance to next block
        app->transformsUBO.currentOffset += app->transformsUBO.blockSize;
    }

    // Finalize using utility
    UnmapBuffer(app->transformsUBO.buffer);


    // Global UBO
    MapBuffer(app->globalParamsUBO.buffer, GL_WRITE_ONLY);

    // Camera position
    PushVec3(app->globalParamsUBO.buffer, app->camera.Position);

    // Light count
    PushUInt(app->globalParamsUBO.buffer, static_cast<u32>(app->lights.size()));

    // Lights array
    for (auto& light : app->lights) {
        AlignHead(app->globalParamsUBO.buffer, 16); // Align start of each Light struct

        PushUInt(app->globalParamsUBO.buffer, static_cast<u32>(light.type));
        AlignHead(app->globalParamsUBO.buffer, 16); // Add 12 bytes padding after type

        PushVec3(app->globalParamsUBO.buffer, light.color);
        PushVec3(app->globalParamsUBO.buffer, light.direction);
        PushVec3(app->globalParamsUBO.buffer, light.position);
        // TODO pass range and use it?
        //Push(app->globalParamsUBO.buffer, light.range);
    }

    UnmapBuffer(app->globalParamsUBO.buffer);
}

void Update(App* app)
{
    //TODO_K: optimizar esto pa que no corra cada frame tos los programs
    for (Shader& shader : app->shaders)
    {
        shader.ReloadIfNeeded();
    }

    UpdateUBOs(app);

    if (!app->models.empty()) {
        float speed = 1.0f; // Adjust speed as needed
        float radius = 2.0f; // Adjust movement radius as needed

        // Calculate new position using sine/cosine for circular motion
        //app->models[0].position.x = sin(app->time * speed) * radius;
        //app->models[0].position.z = cos(app->time * speed) * radius;

        // You could also add simple rotation
        for (size_t i = 1; i < app->models.size(); ++i)
        {
            app->models[i].rotation.y = fmod(app->time * 30.0f * (i*i), 360.0f); // 30 degrees per second
        }
    }

    // You can handle app->input keyboard/mouse here
#pragma region InputHandle

    // Change mode
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

    // Camera Update
    // Handle camera movement
    if (app->input.keys[Key::K_W] == BUTTON_PRESSED)
        app->camera.ProcessKeyboard(M_FORWARD, app->deltaTime);
    if (app->input.keys[Key::K_S] == BUTTON_PRESSED)
        app->camera.ProcessKeyboard(M_BACKWARD, app->deltaTime);
    if (app->input.keys[Key::K_A] == BUTTON_PRESSED)
        app->camera.ProcessKeyboard(M_LEFT, app->deltaTime);
    if (app->input.keys[Key::K_D] == BUTTON_PRESSED)
        app->camera.ProcessKeyboard(M_RIGHT, app->deltaTime);
    if (app->input.keys[Key::K_SPACE] == BUTTON_PRESSED)
        app->camera.ProcessKeyboard(M_UP, app->deltaTime);
    if (app->input.keys[Key::K_CTRL] == BUTTON_PRESSED)
        app->camera.ProcessKeyboard(M_DOWN, app->deltaTime);

    // Handle mouse movement for camera look
    if (app->input.mouseButtons[MouseButton::RIGHT] == BUTTON_PRESSED)
    {
        float xoffset = app->input.mouseDelta.x;
        float yoffset = -app->input.mouseDelta.y;
        app->camera.ProcessMouseMovement(xoffset, yoffset);
    }

    //// Handle mouse scroll for zoom
    //if (app->input.mouseWheelDelta != 0)
    //{
    //    app->camera.ProcessMouseScroll(app->input.mouseWheelDelta);
    //    app->input.mouseWheelDelta = 0; // Reset after processing
    //}

#pragma endregion

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

            GL_CHECK(glBindBufferRange(GL_UNIFORM_BUFFER, 0, app->globalParamsUBO.buffer.handle, 0, app->globalParamsUBO.blockSize));

            for (Model& model : app->models) {
                glBindBufferRange(GL_UNIFORM_BUFFER, 1,
                    app->transformsUBO.buffer.handle,
                    model.bufferOffset,
                    app->transformsUBO.blockSize);

                model.Draw(currentShader);
            }
        }
        break;        

        default:;
    }

    if (app->enableDebugGroups) glPopDebugGroup();
}