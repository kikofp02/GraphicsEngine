//engine.cpp
#include "engine.h"
#include "gl_error.h"

#include <imgui.h>
#include <imgui_internal.h>
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

            planeSubmesh.indices.push_back(topLeft);
            planeSubmesh.indices.push_back(bottomLeft);
            planeSubmesh.indices.push_back(topRight);

            planeSubmesh.indices.push_back(topRight);
            planeSubmesh.indices.push_back(bottomLeft);
            planeSubmesh.indices.push_back(bottomRight);
        }
    }

    u32 texIdx = LoadTexture2D(app, texturePath.c_str());
    if (texIdx != UINT32_MAX) {
        Texture texture;
        texture.id = app->textures_2D[texIdx].handle;
        texture.type = "texture_diffuse";
        texture.path = texturePath;

        model.textures_loaded.push_back(texture);
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
    {glm::vec3(-1.0, -1.0, 0.0), glm::vec2(0.0, 0.0)},  // Bottom-left
    {glm::vec3(1.0, -1.0, 0.0), glm::vec2(1.0, 0.0)},   // Bottom-right
    {glm::vec3(1.0, 1.0, 0.0), glm::vec2(1.0, 1.0)},    // Top-right
    {glm::vec3(-1.0, 1.0, 0.0), glm::vec2(0.0, 1.0)}    // Top-left
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

void InitBuffer(App* app) {
    // Create FBO
    glGenFramebuffers(1, &app->fboHandle);
    glBindFramebuffer(GL_FRAMEBUFFER, app->fboHandle);

    // Albedo
    glGenTextures(1, &app->albedoTexture);
    glBindTexture(GL_TEXTURE_2D, app->albedoTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, app->displaySize.x, app->displaySize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, app->albedoTexture, 0);

    // Normal
    glGenTextures(1, &app->normalTexture);
    glBindTexture(GL_TEXTURE_2D, app->normalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, app->displaySize.x, app->displaySize.y, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, app->normalTexture, 0);

    // Position
    glGenTextures(1, &app->positionTexture);
    glBindTexture(GL_TEXTURE_2D, app->positionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, app->displaySize.x, app->displaySize.y, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, app->positionTexture, 0);

    // Depth
    glGenTextures(1, &app->depthTexture);
    glBindTexture(GL_TEXTURE_2D, app->depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, app->displaySize.x, app->displaySize.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, app->depthTexture, 0);

    GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, drawBuffers);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        ELOG("FBO initialization failed!");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

    app->mode = Mode_TexturedMesh;
    app->displayMode = Albedo;

    app->camera = Camera(glm::vec3(-60.0f, 25.0f, 60.0f));
    app->camera.Pitch = -20.;
    app->camera.Yaw = -45.;
    app->camera.UpdateVectors();

    GL_CHECK(glEnable(GL_DEPTH_TEST));
    GL_CHECK(glClearColor(0.1f, 0.1f, 0.1f, 1.0f));

    InitBuffer(app);
    InitTexturedQuad(app);
    
    app->shaders.emplace_back("shaders.glsl", "TEXTURED_MESH");
    app->texturedMeshShaderIdx = app->shaders.size() - 1;

    app->shaders.emplace_back("shaders.glsl", "GEOMETRY_PASS");
    app->geometryPassShaderIdx = app->shaders.size() - 1;

    app->shaders.emplace_back("shaders.glsl", "DEFERRED_LIGHTING");
    app->deferredShaderIdx = app->shaders.size() - 1;

    //Ground Plane
    Model planeModel = GeneratePlaneModel(app, 20.0f, 10, "color_white.png");
    app->models.push_back(planeModel);
    app->models.back().scale = glm::vec3(4.0f, 1.0f, 4.0f);

    // Patricks
    Model patrickModel("Patrick/Patrick.obj");
    int countPerSide = 4;
    float spacing = 15.0f;

    float totalWidth = (countPerSide - 1) * spacing;
    float startX = -totalWidth / 2.0f;
    float startZ = -totalWidth / 2.0f;

    for (int z = 0; z < countPerSide; ++z) {
        for (int x = 0; x < countPerSide; ++x) {
            Model patrickCopy = patrickModel;

            patrickCopy.position = glm::vec3(
                startX + x * spacing,   // X position
                3.4f,                   // Y position
                startZ + z * spacing    // Z position
            );
            app->models.push_back(patrickCopy);
        }
    }

    // Lights
    Light dir1;
    dir1.type = LightType_Directional;
    dir1.color = glm::vec3(1.0f, 0.9f, 0.8f);
    dir1.direction = glm::vec3(-0.5f, -1.0f, -0.5f);
    app->lights.push_back(dir1);

    /*Light dir2;
    dir2.type = LightType_Directional;
    dir2.color = glm::vec3(0.5f, 0.6f, 0.8f);
    dir2.direction = glm::vec3(0.5f, -1.0f, 0.2f);
    app->lights.push_back(dir2);*/

    const int lights_gridSize = 4;
    const float lights_spacing = 15.0f;

    float lights_totalWidth = (lights_gridSize - 1) * lights_spacing;
    float lights_startX = -lights_totalWidth / 2.0f;
    float lights_startZ = -lights_totalWidth / 2.0f;

    for (int z = 0; z < lights_gridSize; ++z) {
        for (int x = 0; x < lights_gridSize; ++x) {
            Light pointLight;
            pointLight.type = LightType_Point;
            pointLight.color = glm::vec3(
                rand() / (float)RAND_MAX * 0.5f + 0.5f,
                rand() / (float)RAND_MAX * 0.5f + 0.5f,
                rand() / (float)RAND_MAX * 0.5f + 0.5f
            );
            pointLight.position = glm::vec3(lights_startX + x * lights_spacing, 8.f, lights_startZ + z * lights_spacing);
            pointLight.range = 10.0f;
            app->lights.push_back(pointLight);
        }
    }
    

    // UBOs
    // Transform UBO
    GL_CHECK(glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
        reinterpret_cast<GLint*>(&app->transformsUBO.alignment)));

    app->transformsUBO.blockSize = (2 * sizeof(glm::mat4));
    app->transformsUBO.blockSize = Align(app->transformsUBO.blockSize, app->transformsUBO.alignment);

    app->transformsUBO.buffer = CreateBuffer(
        app->transformsUBO.blockSize * app->models.size(),
        GL_UNIFORM_BUFFER,
        GL_DYNAMIC_DRAW
    );

    // Global UBO
    GL_CHECK(glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
        reinterpret_cast<GLint*>(&app->globalParamsUBO.alignment)));

    size_t cameraPosSize = sizeof(glm::vec4);
    size_t lightCountSize = sizeof(glm::uvec4);
    size_t lightSize = 4 * sizeof(glm::vec4);
    app->globalParamsUBO.blockSize = cameraPosSize + lightCountSize + app->lights.size() * lightSize;
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

void ResizeFBO(App* app) {
    // Albedo
    glBindTexture(GL_TEXTURE_2D, app->albedoTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
        app->displaySize.x, app->displaySize.y,
        0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    // Normal
    glBindTexture(GL_TEXTURE_2D, app->normalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F,
        app->displaySize.x, app->displaySize.y,
        0, GL_RGB, GL_FLOAT, NULL);

    // Position
    glBindTexture(GL_TEXTURE_2D, app->positionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F,
        app->displaySize.x, app->displaySize.y,
        0, GL_RGB, GL_FLOAT, NULL);

    // Depth
    glBindTexture(GL_TEXTURE_2D, app->depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
        app->displaySize.x, app->displaySize.y,
        0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void Gui(App* app)
{
    ImGui::Begin("Engine Dashboard");

    ImGui::Text("CONTROLS:");
    ImGui::Text("W A S D for camera movement");
    ImGui::Text("Mouse Right click + Mouse Drag -> camera rotation");
    ImGui::Text("Key 2 -> Swap render mode");

    // System Info Section
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

    // Modifiers Section
    if (ImGui::CollapsingHeader("Render Controls", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Current Mode
        const char* modeNames[] = { "Textured Quad", "Textured Mesh", "FBO Render", "Deferred" };
        ImGui::Text("Current Mode: %s", modeNames[app->mode]);

        // Mode Selector
        ImGui::Separator();
        if (ImGui::Combo("Render Mode", reinterpret_cast<int*>(&app->mode),
            "TexturedQuad\0TexturedMesh\0FBO\0Deferred\0"))
        {
            
        }

        // Display Mode Selector
        ImGui::Separator();
        if (ImGui::Combo("Buffer View", reinterpret_cast<int*>(&app->displayMode),
            "Albedo\0Normals\0Positions\0Depth\0"))
        {
            Shader& quadShader = app->shaders[app->texturedGeometryShaderIdx];
            quadShader.Use();
            quadShader.SetBool("uIsDepth", app->displayMode == Depth);
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
        app->camera.z_near, app->camera.z_far
    );

    MapBuffer(app->transformsUBO.buffer, GL_WRITE_ONLY);
    app->transformsUBO.currentOffset = 0;

    for (auto& model : app->models) {
        size_t blockStart = app->transformsUBO.currentOffset;

        glm::mat4 world = glm::mat4(1.0f);
        world = glm::translate(world, model.position);
        world = glm::rotate(world, glm::radians(model.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        world = glm::scale(world, model.scale);

        glm::mat4 mvp = projection * view;

        PushMat4(app->transformsUBO.buffer, world);
        PushMat4(app->transformsUBO.buffer, mvp);

        AlignHead(app->transformsUBO.buffer, app->transformsUBO.blockSize);

        model.bufferOffset = blockStart;
        model.bufferSize = app->transformsUBO.blockSize;

        app->transformsUBO.currentOffset += app->transformsUBO.blockSize;
    }

    UnmapBuffer(app->transformsUBO.buffer);

    // Global UBO
    MapBuffer(app->globalParamsUBO.buffer, GL_WRITE_ONLY);

    PushVec3(app->globalParamsUBO.buffer, app->camera.Position);
    PushUInt(app->globalParamsUBO.buffer, static_cast<u32>(app->lights.size()));

    // Lights array
    for (auto& light : app->lights) {
        AlignHead(app->globalParamsUBO.buffer, 16);

        PushUInt(app->globalParamsUBO.buffer, static_cast<u32>(light.type));
        AlignHead(app->globalParamsUBO.buffer, 16);

        PushVec3(app->globalParamsUBO.buffer, light.color);
        PushVec3(app->globalParamsUBO.buffer, light.direction);
        PushVec4(app->globalParamsUBO.buffer, glm::vec4(light.position, light.range));
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
        float speed = 1.0f;
        float radius = 2.0f;

        // Circular motion
        //app->models[0].position.x = sin(app->time * speed) * radius;
        //app->models[0].position.z = cos(app->time * speed) * radius;

        for (size_t i = 1; i < app->models.size(); ++i)
        {
            app->models[i].rotation.y = fmod(app->time * 30.0f * (i*i), 360.0f);
        }
    }

    // You can handle app->input keyboard/mouse here
#pragma region InputHandle

    // Change mode
    if (app->input.keys[Key::K_2] == BUTTON_RELEASE) {
        switch (app->mode)
        {
        case Mode_TexturedQuad: app->mode = Mode_TexturedMesh; break;
        case Mode_TexturedMesh: app->mode = Mode_FBORender; break;
        case Mode_FBORender: app->mode = Mode_Deferred; break;
        case Mode_Deferred: app->mode = Mode_TexturedQuad; break;
        default: break;
        }
    }

    // Camera Update
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

    if (app->input.mouseButtons[MouseButton::RIGHT] == BUTTON_PRESSED)
    {
        float xoffset = app->input.mouseDelta.x;
        float yoffset = -app->input.mouseDelta.y;
        app->camera.ProcessMouseMovement(xoffset, yoffset);
    }

    // TODO_K: Mouse scroll
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
            Shader& currentShader = app->shaders[app->texturedGeometryShaderIdx];
            currentShader.Use();
            currentShader.SetInt("uDisplayMode", 0);
            glDisable(GL_DEPTH_TEST);
            glBindVertexArray(app->vao);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, app->textures_2D[app->diceTexIdx].handle);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            glEnable(GL_DEPTH_TEST);

            if (app->enableDebugGroups) glPopDebugGroup();
        }
        break;

        case Mode_TexturedMesh:
        {
            if (app->enableDebugGroups) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 2, -1, "TexturedMesh");
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
            if (app->enableDebugGroups) glPopDebugGroup();
        }
        break;

        case Mode_FBORender: {
            if (app->enableDebugGroups) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 3, -1, "FBOTextures");

            // --- Geometry Pass ---
            glBindFramebuffer(GL_FRAMEBUFFER, app->fboHandle);
            glViewport(0, 0, app->displaySize.x, app->displaySize.y);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            Shader& geoShader = app->shaders[app->geometryPassShaderIdx];
            geoShader.Use();

            glBindBufferRange(GL_UNIFORM_BUFFER, 0, app->globalParamsUBO.buffer.handle, 0, app->globalParamsUBO.blockSize);
            for (Model& model : app->models) {
                glBindBufferRange(GL_UNIFORM_BUFFER, 1, app->transformsUBO.buffer.handle, model.bufferOffset, app->transformsUBO.blockSize);
                model.Draw(geoShader);
            }

            // --- Display Pass ---
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDisable(GL_DEPTH_TEST);
            glClear(GL_COLOR_BUFFER_BIT);

            Shader& displayShader = app->shaders[app->texturedGeometryShaderIdx];
            displayShader.Use();
            displayShader.SetInt("uDisplayMode", app->displayMode);

            glActiveTexture(GL_TEXTURE0);
            switch (app->displayMode) {
            case Albedo:
                glBindTexture(GL_TEXTURE_2D, app->albedoTexture);
                break;
            case Normals:
                glBindTexture(GL_TEXTURE_2D, app->normalTexture);
                break;
            case Positions:
                glBindTexture(GL_TEXTURE_2D, app->positionTexture);
                break;
            case Depth:
                glBindTexture(GL_TEXTURE_2D, app->depthTexture);
                glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
                break;
            }

            glBindVertexArray(app->vao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            glEnable(GL_DEPTH_TEST);

            if (app->enableDebugGroups) glPopDebugGroup();
        }
        break;

        case Mode_Deferred: {

            if (app->enableDebugGroups) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 4, -1, "Deferred");

            // --- Geometry Pass ---
            glBindFramebuffer(GL_FRAMEBUFFER, app->fboHandle);
            glViewport(0, 0, app->displaySize.x, app->displaySize.y);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            Shader& geoShader = app->shaders[app->geometryPassShaderIdx];
            geoShader.Use();

            glBindBufferRange(GL_UNIFORM_BUFFER, 0, app->globalParamsUBO.buffer.handle, 0, app->globalParamsUBO.blockSize);
            for (Model& model : app->models) {
                glBindBufferRange(GL_UNIFORM_BUFFER, 1,
                    app->transformsUBO.buffer.handle,
                    model.bufferOffset,
                    app->transformsUBO.blockSize);
                model.Draw(geoShader);
            }

            // --- Lighting Pass ---
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDisable(GL_DEPTH_TEST);
            glClear(GL_COLOR_BUFFER_BIT);

            Shader& lightShader = app->shaders[app->deferredShaderIdx];
            lightShader.Use();

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, app->albedoTexture);
            lightShader.SetInt("gAlbedo", 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, app->normalTexture);
            lightShader.SetInt("gNormal", 1);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, app->positionTexture);
            lightShader.SetInt("gPosition", 2);

            glBindVertexArray(app->vao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            glEnable(GL_DEPTH_TEST);

            if (app->enableDebugGroups) glPopDebugGroup();
        }
        break;

        default:;
    }

    if (app->enableDebugGroups) glPopDebugGroup();
}