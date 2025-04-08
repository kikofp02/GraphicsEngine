//
// engine.cpp : Put all your graphics stuff in this file. This is kind of the graphics module.
// In here, you should type all your OpenGL commands, and you can also type code to handle
// input platform events (e.g to move the camera or react to certain shortcuts), writing some
// graphics related GUI options, and so on.
//

#include "engine.h"
#include <imgui.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <filesystem>

void CheckGLError(const char* stmt, const char* file, int line)
{
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
    {
        const char* errorStr;
        switch (err) {
        case GL_INVALID_ENUM:      errorStr = "GL_INVALID_ENUM"; break;
        case GL_INVALID_VALUE:     errorStr = "GL_INVALID_VALUE"; break;
        case GL_INVALID_OPERATION: errorStr = "GL_INVALID_OPERATION"; break;
        case GL_OUT_OF_MEMORY:     errorStr = "GL_OUT_OF_MEMORY"; break;
        case GL_INVALID_FRAMEBUFFER_OPERATION: errorStr = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
        default:                  errorStr = "UNKNOWN_ERROR"; break;
        }
        ELOG("OpenGL error 0x%04X (%s) at %s:%i - for %s",
            err, errorStr, file, line, stmt);
    }
}

// Kronos extension
void APIENTRY OnGLError(GLenum source, GLenum type, GLuint id,
    GLenum severity, GLsizei length,
    const GLchar* message, const void* userParam)
{
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
        return;

    App* app = (App*)userParam;

    ELOG("OpenGL Debug Message [%d]: %s", id, message);

    const char* sourceStr = "";
    switch (source) {
    case GL_DEBUG_SOURCE_API:             sourceStr = "API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   sourceStr = "Window System"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "Shader Compiler"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:     sourceStr = "Third Party"; break;
    case GL_DEBUG_SOURCE_APPLICATION:     sourceStr = "Application"; break;
    case GL_DEBUG_SOURCE_OTHER:           sourceStr = "Other"; break;
    }

    const char* typeStr = "";
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:               typeStr = "Error"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "Deprecated"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typeStr = "Undefined Behavior"; break;
    case GL_DEBUG_TYPE_PORTABILITY:         typeStr = "Portability"; break;
    case GL_DEBUG_TYPE_PERFORMANCE:         typeStr = "Performance"; break;
    case GL_DEBUG_TYPE_MARKER:              typeStr = "Marker"; break;
    case GL_DEBUG_TYPE_PUSH_GROUP:          typeStr = "Push Group"; break;
    case GL_DEBUG_TYPE_POP_GROUP:           typeStr = "Pop Group"; break;
    case GL_DEBUG_TYPE_OTHER:               typeStr = "Other"; break;
    }

    const char* severityStr = "";
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:         severityStr = "High"; break;
    case GL_DEBUG_SEVERITY_MEDIUM:       severityStr = "Medium"; break;
    case GL_DEBUG_SEVERITY_LOW:          severityStr = "Low"; break;
    case GL_DEBUG_SEVERITY_NOTIFICATION: severityStr = "Notification"; break;
    }

    ELOG("  Source: %s, Type: %s, Severity: %s", sourceStr, typeStr, severityStr);

#ifdef _DEBUG
    if (severity == GL_DEBUG_SEVERITY_HIGH)
        __debugbreak();
#endif
}

void InitDebugCallback(App* app)
{
    // Check for OpenGL 4.3+ (where KHR_debug became core)
    if (GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 3))
    {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(OnGLError, app);

        // Enable only medium/high severity messages
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
            GL_DEBUG_SEVERITY_MEDIUM,
            0, nullptr, GL_TRUE);
    }
    else
    {
        ELOG("GL_KHR_debug not available (Requires OpenGL 4.3+). Using basic error checking.");
    }
}

GLuint CreateProgramFromSource(String programSource, const char* shaderName)
{
    GLchar  infoLogBuffer[1024] = {};
    GLsizei infoLogBufferSize = sizeof(infoLogBuffer);
    GLsizei infoLogSize;
    GLint   success;

    char versionString[] = "#version 430\n";
    char shaderNameDefine[128];
    sprintf(shaderNameDefine, "#define %s\n", shaderName);
    char vertexShaderDefine[] = "#define VERTEX\n";
    char fragmentShaderDefine[] = "#define FRAGMENT\n";

    const GLchar* vertexShaderSource[] = {
        versionString,
        shaderNameDefine,
        vertexShaderDefine,
        programSource.str
    };
    const GLint vertexShaderLengths[] = {
        (GLint) strlen(versionString),
        (GLint) strlen(shaderNameDefine),
        (GLint) strlen(vertexShaderDefine),
        (GLint) programSource.len
    };
    const GLchar* fragmentShaderSource[] = {
        versionString,
        shaderNameDefine,
        fragmentShaderDefine,
        programSource.str
    };
    const GLint fragmentShaderLengths[] = {
        (GLint) strlen(versionString),
        (GLint) strlen(shaderNameDefine),
        (GLint) strlen(fragmentShaderDefine),
        (GLint) programSource.len
    };

    GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vshader, ARRAY_COUNT(vertexShaderSource), vertexShaderSource, vertexShaderLengths);
    glCompileShader(vshader);
    glGetShaderiv(vshader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vshader, infoLogBufferSize, &infoLogSize, infoLogBuffer);
        ELOG("glCompileShader() failed with vertex shader %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
    }

    GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fshader, ARRAY_COUNT(fragmentShaderSource), fragmentShaderSource, fragmentShaderLengths);
    glCompileShader(fshader);
    glGetShaderiv(fshader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fshader, infoLogBufferSize, &infoLogSize, infoLogBuffer);
        ELOG("glCompileShader() failed with fragment shader %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
    }

    GLuint programHandle;
    GL_CHECK(programHandle = glCreateProgram());
    GL_CHECK(glAttachShader(programHandle, vshader));
    glAttachShader(programHandle, fshader);
    GL_CHECK(glLinkProgram(programHandle));
    glGetProgramiv(programHandle, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(programHandle, infoLogBufferSize, &infoLogSize, infoLogBuffer);
        ELOG("glLinkProgram() failed with program %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
    }

    glUseProgram(0);

    glDetachShader(programHandle, vshader);
    glDetachShader(programHandle, fshader);
    glDeleteShader(vshader);
    glDeleteShader(fshader);

    return programHandle;
}

u32 LoadProgram(App* app, const char* filepath, const char* programName)
{
    String programSource = ReadTextFile(filepath);

    Program program = {};
    program.handle = CreateProgramFromSource(programSource, programName);
    program.filepath = filepath;
    program.programName = programName;
    program.lastWriteTimestamp = GetFileLastWriteTimestamp(filepath);

    app->programs.push_back(program);
    return app->programs.size() - 1;
}

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
    OpenGLErrorGuard guard("CreateTexture");

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
    for (u32 texIdx = 0; texIdx < app->textures.size(); ++texIdx)
        if (app->textures[texIdx].filepath == filepath)
            return texIdx;

    Image image = LoadImage(filepath);

    if (image.pixels)
    {
        Texture tex = {};
        tex.handle = CreateTexture2DFromImage(image);
        tex.filepath = filepath;

        u32 texIdx = app->textures.size();
        app->textures.push_back(tex);

        FreeImage(image);
        return texIdx;
    }
    else
    {
        return UINT32_MAX;
    }
}

void Init(App* app)
{
    InitDebugCallback(app);

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
    app->mode = Mode_TexturedQuad;
    
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
    app->texturedGeometryProgramIdx = LoadProgram(app, "texturedGeo.glsl", "TEXTURED_GEOMETRY");

    Program& texturedGeometryProgram = app->programs[app->texturedGeometryProgramIdx];

    app->programUniformTexture = glGetUniformLocation(texturedGeometryProgram.handle, "uTexture");

    // - textures
    app->diceTexIdx = LoadTexture2D(app, "dice.png");
    app->whiteTexIdx = LoadTexture2D(app, "color_white.png");
    app->blackTexIdx = LoadTexture2D(app, "color_black.png");
    app->normalTexIdx = LoadTexture2D(app, "color_normal.png");
    app->magentaTexIdx = LoadTexture2D(app, "color_magenta.png");

    if (app->enableDebugGroups) {
        glObjectLabel(GL_VERTEX_ARRAY, app->vao, -1, "MainVAO");
        glObjectLabel(GL_BUFFER, app->embeddedVertices, -1, "QuadVertices");
        glObjectLabel(GL_BUFFER, app->embeddedElements, -1, "QuadIndices");

        for (size_t i = 0; i < app->textures.size(); ++i) {
            glObjectLabel(GL_TEXTURE, app->textures[i].handle, -1,
                app->textures[i].filepath.c_str());
        }
    }
}

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
    for (Program& program : app->programs)
    {
        u64 currentTimestamp = GetFileLastWriteTimestamp(program.filepath.c_str());
        if (currentTimestamp > program.lastWriteTimestamp)
        {
            glDeleteProgram(program.handle);
            String programSource = ReadTextFile(program.filepath.c_str());
            program.handle = CreateProgramFromSource(programSource, program.programName.c_str());
            program.lastWriteTimestamp = currentTimestamp;

            ELOG("Reload shader: %s", program.filepath.c_str());

            //TODO_K: taria guapo poner que si no funca el program cargue uno default
            /*if (!program.handle) {
                program.handle = CreateProgramFromSource(defaultShaderSource, "DEFAULT");
            }*/
        }
    }

    // You can handle app->input keyboard/mouse here
}

void Render(App* app)
{
    OpenGLErrorGuard renderGuard("MainRender");

    if (app->enableDebugGroups) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "MainRenderPass");

    switch (app->mode)
    {
        case Mode_TexturedQuad:
        {
            if (app->enableDebugGroups) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 1, -1, "TexturedQuad");
            // TODO: Draw your textured quad here!
            
            // - clear the framebuffer
            GL_CHECK(glClearColor(0.1f, 0.1f, 0.1f, 1.0f));
            GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

            // - set the viewport
            GL_CHECK(glViewport(0, 0, app->displaySize.x, app->displaySize.y));

            // - set the blending state
            // - bind the texture into unit 0
            // - bind the program 
            //   (...and make its texture sample from unit 0)
            // - bind the vao
            Program& programTexturedGeometry = app->programs[app->texturedGeometryProgramIdx];
            GL_CHECK(glUseProgram(programTexturedGeometry.handle));
            GL_CHECK(glBindVertexArray(app->vao));

            GL_CHECK(glEnable(GL_BLEND));
            GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

            GL_CHECK(glUniform1i(app->programUniformTexture, 0));
            GL_CHECK(glActiveTexture(GL_TEXTURE0));
            GLuint textureHandle = app->textures[app->diceTexIdx].handle;
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

        if (app->enableDebugGroups) glPopDebugGroup();

        default:;
    }
}