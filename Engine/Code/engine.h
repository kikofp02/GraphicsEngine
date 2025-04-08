//
// engine.h: This file contains the types and functions relative to the engine.
//

#pragma once

#include "platform.h"
#include <glad/glad.h>

typedef glm::vec2  vec2;
typedef glm::vec3  vec3;
typedef glm::vec4  vec4;
typedef glm::ivec2 ivec2;
typedef glm::ivec3 ivec3;
typedef glm::ivec4 ivec4;

// BUFFER structs
struct VertexBufferAttribute
{
    u8 location;
    u8 componentCount;
    u8 offset;
};

struct VertexBufferLayout
{
    std::vector<VertexBufferAttribute> attributes;
    u8 stride;
};

struct VertexShaderAttribute
{
    u8 location;
    u8 componentCount;
};

struct VertexShaderLayout
{
    std::vector<VertexShaderAttribute> attributes;
};

struct Submesh
{
    VertexBufferLayout vertexBufferLayout;
    std::vector<float> vertices;
    std::vector<u32> indices;
    u32 vertexOffset;
    u32 indexOffset;
};

struct Mesh
{
    std::vector<Submesh> submeshes;
    GLuint vertexBufferHandle;
    GLuint indexBufferHandle;
};

struct Image
{
    void* pixels;
    ivec2 size;
    i32   nchannels;
    i32   stride;
};

struct Texture
{
    GLuint      handle;
    std::string filepath;
};

struct Program
{
    GLuint             handle;
    std::string        filepath;
    std::string        programName;
    u64                lastWriteTimestamp;
    VertexShaderLayout vertexInputLayout;
};

enum Mode
{
    Mode_TexturedQuad,
    Mode_Count
};

struct OpenGLInfo {
    std::string glVersion;
    std::string glRenderer;
    std::string glVendor;
    std::string glslVersion;
    std::vector<std::string> glExtensions;
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

    std::vector<Texture>  textures;
    std::vector<Program>  programs;

    // program indices
    u32 texturedGeometryProgramIdx;
    u32 texturedMeshProgramIdx;
    
    // texture indices
    u32 diceTexIdx;
    u32 whiteTexIdx;
    u32 blackTexIdx;
    u32 normalTexIdx;
    u32 magentaTexIdx;

    // Mode
    Mode mode;

    // Embedded geometry (in-editor simple meshes such as
    // a screen filling quad, a cube, a sphere...)
    GLuint embeddedVertices;
    GLuint embeddedElements;

    // Location of the texture uniform in the textured quad shader
    GLuint programUniformTexture;

    // VAO object to link our screen filling quad with our textured quad shader
    GLuint vao;
};

// Error checking
#define GL_CHECK(stmt) do { \
    stmt; \
    CheckGLError(#stmt, __FILE__, __LINE__); \
} while (0)

void CheckGLError(const char* stmt, const char* file, int line);

class OpenGLErrorGuard
{
public:
    OpenGLErrorGuard(const char* context) : context(context) {
        CheckGLError("Pre-guard", __FILE__, __LINE__);
    }
    ~OpenGLErrorGuard() {
        CheckGLError(context, __FILE__, __LINE__);
    }
private:
    const char* context;
};

// Kronos extension
void APIENTRY OnGLError(GLenum source, GLenum type, GLuint id,
    GLenum severity, GLsizei length,
    const GLchar* message, const void* userParam);
void InitDebugCallback(App* app);

void Init(App* app);

void Gui(App* app);

void Update(App* app);

void Render(App* app);

