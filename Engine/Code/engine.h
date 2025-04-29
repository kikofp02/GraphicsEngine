// engine.h: This file contains the types and functions relative to the engine.
#pragma once

#include "platform.h"
#include "model.h"
#include "shader.h"
#include "camera.h"
#include <glad/glad.h>

typedef glm::vec2  vec2;
typedef glm::vec3  vec3;
typedef glm::vec4  vec4;
typedef glm::ivec2 ivec2;
typedef glm::ivec3 ivec3;
typedef glm::ivec4 ivec4;

struct Image
{
    void* pixels;
    ivec2 size;
    i32   nchannels;
    i32   stride;
};

struct Texture_2D
{
    GLuint      handle;
    std::string filepath;
};

enum Mode
{
    Mode_TexturedQuad,
    Mode_TexturedMesh,
    Mode_FBORender,
    Mode_Deferred,
    Mode_Count
};

enum DisplayMode
{
    Albedo,
    Normals,
    Positions,
    Depth
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
    LightType type;
    glm::vec3 color;
    glm::vec3 direction;
    glm::vec3 position;
    float range;
};

struct App
{
    // Loop
    f32  deltaTime;
    bool isRunning;

    // Input
    Input input;

    // Graphics
    char gpuName[64];
    char openGlVersion[64];
    OpenGLInfo oglInfo;

    bool enableDebugGroups = true;

    ivec2 displaySize;

    std::vector<Texture_2D>    textures_2D;
    
    std::vector<Model>      models;
    std::vector<Shader>     shaders;

    std::vector<Light> lights;

    // program indices
    u32 texturedGeometryShaderIdx;
    u32 texturedMeshShaderIdx;
    u32 deferredShaderIdx;
        
    // texture indices
    u32 diceTexIdx;
    u32 whiteTexIdx;
    u32 blackTexIdx;
    u32 normalTexIdx;
    u32 magentaTexIdx;

    // Mode
    Mode mode;

    Camera camera;

    //UBOs
    UniformBuffer transformsUBO;
    UniformBuffer globalParamsUBO;

    f32 time;

    // Framebuffer resources
    GLuint fboHandle;
    GLuint albedoTexture;
    GLuint normalTexture;
    GLuint positionTexture;
    GLuint depthTexture;

    u32 geometryPassShaderIdx;

    DisplayMode displayMode;

    // Embedded geometry (in-editor simple meshes such as
    // a screen filling quad, a cube, a sphere...)
    GLuint embeddedVertices;
    GLuint embeddedElements;

    // Location of the texture uniform in the textured quad shader
    GLuint programUniformTexture;

    // VAO object to link our screen filling quad with our textured quad shader
    GLuint vao;
};

void Init(App* app);

void Gui(App* app);

void Update(App* app);

void Render(App* app);

void ResizeFBO(App* app);