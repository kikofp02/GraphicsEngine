// engine.h
#pragma once

#include "platform.h"
#include "model.h"
#include "shader.h"
#include "camera.h"
#include "panels.h"
#include <glad/glad.h>

typedef glm::vec2  vec2;
typedef glm::vec3  vec3;
typedef glm::vec4  vec4;
typedef glm::ivec2 ivec2;
typedef glm::ivec3 ivec3;
typedef glm::ivec4 ivec4;

enum Mode
{
    Mode_Forward,
    Mode_DebugFBO,
    Mode_Deferred
};

enum DisplayMode
{
    Display_Albedo,
    Display_Normals,
    Display_Positions,
    Display_Depth,
    Display_MatProps,
    Display_Bloom
};

struct OpenGLInfo {
    std::string glVersion;
    std::string glRenderer;
    std::string glVendor;
    std::string glslVersion;
    std::vector<std::string> glExtensions;
};

struct Buffer {
    GLuint handle;
    GLenum type;
    u32 size;
    u32 head;
    void* data;
};

struct UniformBuffer {
    Buffer buffer;
    u32 currentOffset;
    u32 blockSize;
    u32 alignment;
};

//Lights
enum LightType {
    LightType_Directional,
    LightType_Point
};

struct Light {
    std::string name;
    bool enabled = true;
    LightType type;
    glm::vec3 color;
    glm::vec3 direction;
    glm::vec3 position;
    float range;
    float intensity;
};

struct App
{
    // Core
    f32  deltaTime;
    f32 time;
    bool isRunning;

    ivec2 displaySize;

    Input input;

    // Graphics Details
    char gpuName[64];
    char openGlVersion[64];
    OpenGLInfo oglInfo;

    bool enableDebugGroups = true;

    // Engine
    std::vector<Model>                          models;
    std::vector<Shader>                         shaders;
    std::vector<Light>                          lights;
    std::vector<std::shared_ptr<Texture>>       textures_loaded;

    Camera      camera;
    Model*      selectedModel;
    Light*      selectedLight;

    std::shared_ptr<Material> selectedMaterial;
    glm::vec4 bg_color = glm::vec4(0.f, 0.f, 0.f, 1.0f);

    float rotate_speed  = 1.0;
    bool rotate_models  = true;
    bool renderAll      = false;

    Mode mode;
    DisplayMode displayMode;

    // program indices
    // TODO_K: k mierda, mejor quitar esto y usar los names en shaders
    u32 debugTexturesShaderIdx;
    u32 forwardShaderIdx;
    u32 deferredLightingShaderIdx;
    u32 geometryPassShaderIdx;
    u32 bloomShaderIdx;

    //UBOs
    UniformBuffer transformsUBO;
    UniformBuffer globalParamsUBO;

    // Framebuffer resources
    GLuint fboHandle;
    GLuint albedoTexture;
    GLuint normalTexture;
    GLuint positionTexture;
    GLuint depthTexture;
    GLuint materialPropsTexture;
    GLuint bloomTexture;

    GLuint embeddedVertices;
    GLuint embeddedElements;
    GLuint vao;

    // GUI
    GUI_PanelManager panelManager;
};

void Init(App* app);

void Gui(App* app);

void Update(App* app);

void Render(App* app);

void ResizeFBO(App* app);