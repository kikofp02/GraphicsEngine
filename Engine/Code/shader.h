//shader.h
#pragma once

#include "platform.h"
#include "gl_error.h"
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <string>
#include <vector>

struct VertexShaderAttribute {
    u8 location;
    u8 componentCount;
};

struct VertexShaderLayout {
    std::vector<VertexShaderAttribute> attributes;
};

class Shader
{
public:
    GLuint handle;
    std::string filepath;
    std::string programName;
    u64 lastWriteTimestamp;
    VertexShaderLayout vertexInputLayout;

    // Constructor that creates shader from single source file with defines
    Shader(const char* filepath, const char* programName)
    {
        this->filepath = filepath;
        this->programName = programName;
        this->handle = CreateFromSource(filepath, programName);
        this->lastWriteTimestamp = GetFileLastWriteTimestamp(filepath);
        SetupVertexAttributes();
    }

    // Activate the shader
    void Use() const
    {
        GL_CHECK(glUseProgram(handle));
    }

    // Utility uniform functions
    void SetBool(const std::string& name, bool value) const
    {
        GL_CHECK(glUniform1i(glGetUniformLocation(handle, name.c_str()), (int)value));
    }

    void SetInt(const std::string& name, int value) const
    {
        GL_CHECK(glUniform1i(glGetUniformLocation(handle, name.c_str()), value));
    }

    void SetFloat(const std::string& name, float value) const
    {
        GL_CHECK(glUniform1f(glGetUniformLocation(handle, name.c_str()), value));
    }

    void SetVec2(const std::string& name, const glm::vec2& value) const
    {
        GL_CHECK(glUniform2fv(glGetUniformLocation(handle, name.c_str()), 1, &value[0]));
    }

    void SetVec2(const std::string& name, float x, float y) const
    {
        GL_CHECK(glUniform2f(glGetUniformLocation(handle, name.c_str()), x, y));
    }

    void SetVec3(const std::string& name, const glm::vec3& value) const
    {
        GL_CHECK(glUniform3fv(glGetUniformLocation(handle, name.c_str()), 1, &value[0]));
    }

    void SetVec3(const std::string& name, float x, float y, float z) const
    {
        GL_CHECK(glUniform3f(glGetUniformLocation(handle, name.c_str()), x, y, z));
    }

    void SetVec4(const std::string& name, const glm::vec4& value) const
    {
        GL_CHECK(glUniform4fv(glGetUniformLocation(handle, name.c_str()), 1, &value[0]));
    }

    void SetVec4(const std::string& name, float x, float y, float z, float w) const
    {
        GL_CHECK(glUniform4f(glGetUniformLocation(handle, name.c_str()), x, y, z, w));
    }

    void SetMat2(const std::string& name, const glm::mat2& mat) const
    {
        GL_CHECK(glUniformMatrix2fv(glGetUniformLocation(handle, name.c_str()), 1, GL_FALSE, &mat[0][0]));
    }

    void SetMat3(const std::string& name, const glm::mat3& mat) const
    {
        GL_CHECK(glUniformMatrix3fv(glGetUniformLocation(handle, name.c_str()), 1, GL_FALSE, &mat[0][0]));
    }

    void SetMat4(const std::string& name, const glm::mat4& mat) const
    {
        GL_CHECK(glUniformMatrix4fv(glGetUniformLocation(handle, name.c_str()), 1, GL_FALSE, &mat[0][0]));
    }

    // Reload shader if source file has changed
    bool ReloadIfNeeded()
    {
        u64 currentTimestamp = GetFileLastWriteTimestamp(filepath.c_str());
        if (currentTimestamp > lastWriteTimestamp)
        {
            GLuint newHandle = CreateFromSource(filepath.c_str(), programName.c_str());
            if (newHandle)
            {
                glDeleteProgram(handle);
                handle = newHandle;
                lastWriteTimestamp = currentTimestamp;
                SetupVertexAttributes();
                return true;

                ELOG("Reload shader: %s", this->filepath.c_str());

                //TODO_K: taria guapo poner que si no funca el program cargue uno default
                /*if (!program.handle) {
                    program.handle = CreateProgramFromSource(defaultShaderSource, "DEFAULT");
                }*/
            }
        }
        return false;
    }

private:
    GLuint CreateFromSource(const char* filepath, const char* programName)
    {
        String programSource = ReadTextFile(filepath);
        if (programSource.str == nullptr)
        {
            ELOG("Failed to load shader file: %s", filepath);
            return 0;
        }

        GLchar infoLogBuffer[1024] = {};
        GLsizei infoLogBufferSize = sizeof(infoLogBuffer);
        GLsizei infoLogSize;
        GLint success;

        char versionString[] = "#version 430\n";
        char shaderNameDefine[128];
        sprintf(shaderNameDefine, "#define %s\n", programName);
        char vertexShaderDefine[] = "#define VERTEX\n";
        char fragmentShaderDefine[] = "#define FRAGMENT\n";

        const GLchar* vertexShaderSource[] = {
            versionString,
            shaderNameDefine,
            vertexShaderDefine,
            programSource.str
        };
        const GLint vertexShaderLengths[] = {
            (GLint)strlen(versionString),
            (GLint)strlen(shaderNameDefine),
            (GLint)strlen(vertexShaderDefine),
            (GLint)programSource.len
        };

        const GLchar* fragmentShaderSource[] = {
            versionString,
            shaderNameDefine,
            fragmentShaderDefine,
            programSource.str
        };
        const GLint fragmentShaderLengths[] = {
            (GLint)strlen(versionString),
            (GLint)strlen(shaderNameDefine),
            (GLint)strlen(fragmentShaderDefine),
            (GLint)programSource.len
        };

        GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
        GL_CHECK(glShaderSource(vshader, ARRAY_COUNT(vertexShaderSource), vertexShaderSource, vertexShaderLengths));
        GL_CHECK(glCompileShader(vshader));
        GL_CHECK(glGetShaderiv(vshader, GL_COMPILE_STATUS, &success));
        if (!success)
        {
            glGetShaderInfoLog(vshader, infoLogBufferSize, &infoLogSize, infoLogBuffer);
            ELOG("glCompileShader() failed with vertex shader %s\nReported message:\n%s\n", programName, infoLogBuffer);
            glDeleteShader(vshader);
            return 0;
        }

        GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
        GL_CHECK(glShaderSource(fshader, ARRAY_COUNT(fragmentShaderSource), fragmentShaderSource, fragmentShaderLengths));
        GL_CHECK(glCompileShader(fshader));
        GL_CHECK(glGetShaderiv(fshader, GL_COMPILE_STATUS, &success));
        if (!success)
        {
            glGetShaderInfoLog(fshader, infoLogBufferSize, &infoLogSize, infoLogBuffer);
            ELOG("glCompileShader() failed with fragment shader %s\nReported message:\n%s\n", programName, infoLogBuffer);
            glDeleteShader(vshader);
            glDeleteShader(fshader);
            return 0;
        }

        GLuint programHandle = glCreateProgram();
        if (!programHandle)
        {
            ELOG("glCreateProgram() failed");
            glDeleteShader(vshader);
            glDeleteShader(fshader);
            return 0;
        }

        GL_CHECK(glAttachShader(programHandle, vshader));
        GL_CHECK(glAttachShader(programHandle, fshader));
        GL_CHECK(glLinkProgram(programHandle));
        GL_CHECK(glGetProgramiv(programHandle, GL_LINK_STATUS, &success));
        if (!success)
        {
            glGetProgramInfoLog(programHandle, infoLogBufferSize, &infoLogSize, infoLogBuffer);
            ELOG("glLinkProgram() failed with program %s\nReported message:\n%s\n", programName, infoLogBuffer);
            glDeleteProgram(programHandle);
            programHandle = 0;
        }

        glDetachShader(programHandle, vshader);
        glDetachShader(programHandle, fshader);
        glDeleteShader(vshader);
        glDeleteShader(fshader);

        return programHandle;
    }

    void SetupVertexAttributes()
    {
        vertexInputLayout.attributes.clear();

        GLint attributeCount;
        glGetProgramiv(handle, GL_ACTIVE_ATTRIBUTES, &attributeCount);

        for (GLint i = 0; i < attributeCount; ++i) {
            GLchar attributeName[128];
            GLint attributeNameLength;
            GLint attributeSize;
            GLenum attributeType;

            glGetActiveAttrib(handle, i,
                ARRAY_COUNT(attributeName),
                &attributeNameLength,
                &attributeSize,
                &attributeType,
                attributeName);

            GLint attributeLocation = glGetAttribLocation(handle, attributeName);

            u8 componentCount = 1;
            switch (attributeType) {
            case GL_FLOAT_VEC2: componentCount = 2; break;
            case GL_FLOAT_VEC3: componentCount = 3; break;
            case GL_FLOAT_VEC4: componentCount = 4; break;
            default: break;
            }

            vertexInputLayout.attributes.push_back({ (u8)attributeLocation, componentCount });
        }
    }
};