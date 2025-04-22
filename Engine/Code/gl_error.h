// gl_error.h
#pragma once

#include "platform.h"
#include <glad/glad.h>

struct App;


// Error checking macro
#define GL_CHECK(stmt) do { \
    stmt; \
    GLUtils::CheckGLError(#stmt, __FILE__, __LINE__); \
} while (0)

namespace GLUtils {
    // Basic error checking
    void CheckGLError(const char* stmt, const char* file, int line);

    class ErrorGuard {
    public:
        ErrorGuard(const char* context) : context(context) {
            CheckGLError("Pre-guard", __FILE__, __LINE__);
        }
        ~ErrorGuard() {
            CheckGLError(context, __FILE__, __LINE__);
        }
    private:
        const char* context;
    };

    // Debug callback functions
    void APIENTRY DebugCallback(GLenum source, GLenum type, GLuint id,
        GLenum severity, GLsizei length,
        const GLchar* message, const void* userParam);

    // Debug system initialization
    void InitDebugging(App* app);

    inline void CheckGLError(const char* stmt, const char* file, int line) {
        GLenum err;
        while ((err = glGetError()) != GL_NO_ERROR) {
            const char* errorStr;
            switch (err) {
            case GL_INVALID_ENUM:      errorStr = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE:     errorStr = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errorStr = "GL_INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY:     errorStr = "GL_OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                errorStr = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
            default: errorStr = "UNKNOWN_ERROR"; break;
            }
            ELOG("OpenGL error 0x%04X (%s) at %s:%i - for %s",
                err, errorStr, file, line, stmt);
        }
    }

    inline void APIENTRY DebugCallback(GLenum source, GLenum type, GLuint id,
        GLenum severity, GLsizei length,
        const GLchar* message, const void* userParam) {
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

    inline void InitDebugging(App* app) {
        if (GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 3)) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(DebugCallback, app);

            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                GL_DEBUG_SEVERITY_MEDIUM,
                0, nullptr, GL_TRUE);
        }
        else {
            ELOG("GL_KHR_debug not available (Requires OpenGL 4.3+). Using basic error checking.");
        }
    }

} // namespace GLUtils