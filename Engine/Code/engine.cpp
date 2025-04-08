#include "engine.h"

#include <imgui.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <filesystem>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#pragma region ErrorHandleGL

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

#pragma endregion

#pragma region Loaders

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

    GLint attributeCount;
    glGetProgramiv(program.handle, GL_ACTIVE_ATTRIBUTES, &attributeCount);

    for (GLint i = 0; i < attributeCount; ++i) {
        GLchar attributeName[128];
        GLint attributeNameLength;
        GLint attributeSize;
        GLenum attributeType;

        glGetActiveAttrib(program.handle, i,
            ARRAY_COUNT(attributeName),
            &attributeNameLength,
            &attributeSize,
            &attributeType,
            attributeName);

        GLint attributeLocation = glGetAttribLocation(program.handle, attributeName);

        // Map OpenGL type to component count
        u8 componentCount = 1;
        switch (attributeType) {
        case GL_FLOAT_VEC2: componentCount = 2; break;
        case GL_FLOAT_VEC3: componentCount = 3; break;
        case GL_FLOAT_VEC4: componentCount = 4; break;
        default: break;
        }

        program.vertexInputLayout.attributes.push_back({ (u8)attributeLocation, componentCount });
    }

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

#pragma endregion

#pragma region Assimp

void ProcessAssimpMesh(const aiScene* scene, aiMesh* mesh, Mesh* myMesh, u32 baseMeshMaterialIndex, std::vector<u32>& submeshMaterialIndices)
{
    std::vector<float> vertices;
    std::vector<u32> indices;

    bool hasTexCoords = false;
    bool hasTangentSpace = false;

    // process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        vertices.push_back(mesh->mVertices[i].x);
        vertices.push_back(mesh->mVertices[i].y);
        vertices.push_back(mesh->mVertices[i].z);
        vertices.push_back(mesh->mNormals[i].x);
        vertices.push_back(mesh->mNormals[i].y);
        vertices.push_back(mesh->mNormals[i].z);

        if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
        {
            hasTexCoords = true;
            vertices.push_back(mesh->mTextureCoords[0][i].x);
            vertices.push_back(mesh->mTextureCoords[0][i].y);
        }

        if (mesh->mTangents != nullptr && mesh->mBitangents)
        {
            hasTangentSpace = true;
            vertices.push_back(mesh->mTangents[i].x);
            vertices.push_back(mesh->mTangents[i].y);
            vertices.push_back(mesh->mTangents[i].z);

            // For some reason ASSIMP gives me the bitangents flipped.
            // Maybe it's my fault, but when I generate my own geometry
            // in other files (see the generation of standard assets)
            // and all the bitangents have the orientation I expect,
            // everything works ok.
            // I think that (even if the documentation says the opposite)
            // it returns a left-handed tangent space matrix.
            // SOLUTION: I invert the components of the bitangent here.
            vertices.push_back(-mesh->mBitangents[i].x);
            vertices.push_back(-mesh->mBitangents[i].y);
            vertices.push_back(-mesh->mBitangents[i].z);
        }
    }

    // process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
        {
            indices.push_back(face.mIndices[j]);
        }
    }

    // store the proper (previously proceessed) material for this mesh
    submeshMaterialIndices.push_back(baseMeshMaterialIndex + mesh->mMaterialIndex);

    // create the vertex format
    VertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 0, 3, 0 });
    vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 1, 3, 3 * sizeof(float) });
    vertexBufferLayout.stride = 6 * sizeof(float);
    if (hasTexCoords)
    {
        vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 2, 2, vertexBufferLayout.stride });
        vertexBufferLayout.stride += 2 * sizeof(float);
    }
    if (hasTangentSpace)
    {
        vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 3, 3, vertexBufferLayout.stride });
        vertexBufferLayout.stride += 3 * sizeof(float);

        vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 4, 3, vertexBufferLayout.stride });
        vertexBufferLayout.stride += 3 * sizeof(float);
    }

    // add the submesh into the mesh
    Submesh submesh = {};
    submesh.vertexBufferLayout = vertexBufferLayout;
    submesh.vertices.swap(vertices);
    submesh.indices.swap(indices);
    myMesh->submeshes.push_back(submesh);
}

void ProcessAssimpMaterial(App* app, aiMaterial* material, Material& myMaterial, String directory)
{
    aiString name;
    aiColor3D diffuseColor;
    aiColor3D emissiveColor;
    aiColor3D specularColor;
    ai_real shininess;
    material->Get(AI_MATKEY_NAME, name);
    material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
    material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);
    material->Get(AI_MATKEY_COLOR_SPECULAR, specularColor);
    material->Get(AI_MATKEY_SHININESS, shininess);

    myMaterial.name = name.C_Str();
    myMaterial.albedo = vec3(diffuseColor.r, diffuseColor.g, diffuseColor.b);
    myMaterial.emissive = vec3(emissiveColor.r, emissiveColor.g, emissiveColor.b);
    myMaterial.smoothness = shininess / 256.0f;

    aiString aiFilename;
    if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
    {
        material->GetTexture(aiTextureType_DIFFUSE, 0, &aiFilename);
        String filename = MakeString(aiFilename.C_Str());
        String filepath = MakePath(directory, filename);
        myMaterial.albedoTextureIdx = LoadTexture2D(app, filepath.str);
    }
    if (material->GetTextureCount(aiTextureType_EMISSIVE) > 0)
    {
        material->GetTexture(aiTextureType_EMISSIVE, 0, &aiFilename);
        String filename = MakeString(aiFilename.C_Str());
        String filepath = MakePath(directory, filename);
        myMaterial.emissiveTextureIdx = LoadTexture2D(app, filepath.str);
    }
    if (material->GetTextureCount(aiTextureType_SPECULAR) > 0)
    {
        material->GetTexture(aiTextureType_SPECULAR, 0, &aiFilename);
        String filename = MakeString(aiFilename.C_Str());
        String filepath = MakePath(directory, filename);
        myMaterial.specularTextureIdx = LoadTexture2D(app, filepath.str);
    }
    if (material->GetTextureCount(aiTextureType_NORMALS) > 0)
    {
        material->GetTexture(aiTextureType_NORMALS, 0, &aiFilename);
        String filename = MakeString(aiFilename.C_Str());
        String filepath = MakePath(directory, filename);
        myMaterial.normalsTextureIdx = LoadTexture2D(app, filepath.str);
    }
    if (material->GetTextureCount(aiTextureType_HEIGHT) > 0)
    {
        material->GetTexture(aiTextureType_HEIGHT, 0, &aiFilename);
        String filename = MakeString(aiFilename.C_Str());
        String filepath = MakePath(directory, filename);
        myMaterial.bumpTextureIdx = LoadTexture2D(app, filepath.str);
    }

    //myMaterial.createNormalFromBump();
}

void ProcessAssimpNode(const aiScene* scene, aiNode* node, Mesh* myMesh, u32 baseMeshMaterialIndex, std::vector<u32>& submeshMaterialIndices)
{
    // process all the node's meshes (if any)
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessAssimpMesh(scene, mesh, myMesh, baseMeshMaterialIndex, submeshMaterialIndices);
    }

    // then do the same for each of its children
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        ProcessAssimpNode(scene, node->mChildren[i], myMesh, baseMeshMaterialIndex, submeshMaterialIndices);
    }
}

u32 LoadModel(App* app, const char* filename)
{
    OpenGLErrorGuard guard("AssimpLoad");

    const aiScene* scene = aiImportFile(filename,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_PreTransformVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_OptimizeMeshes |
        aiProcess_SortByPType);

    if (!scene)
    {
        ELOG("Error loading mesh %s: %s", filename, aiGetErrorString());
        return UINT32_MAX;
    }

    app->meshes.push_back(Mesh{});
    Mesh& mesh = app->meshes.back();
    u32 meshIdx = (u32)app->meshes.size() - 1u;

    app->models.push_back(Model{});
    Model& model = app->models.back();
    model.meshIdx = meshIdx;
    u32 modelIdx = (u32)app->models.size() - 1u;

    String directory = GetDirectoryPart(MakeString(filename));

    // Create a list of materials
    u32 baseMeshMaterialIndex = (u32)app->materials.size();
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
    {
        app->materials.push_back(Material{});
        Material& material = app->materials.back();
        ProcessAssimpMaterial(app, scene->mMaterials[i], material, directory);
    }

    ProcessAssimpNode(scene, scene->mRootNode, &mesh, baseMeshMaterialIndex, model.materialIdx);

    aiReleaseImport(scene);

    u32 vertexBufferSize = 0;
    u32 indexBufferSize = 0;

    for (u32 i = 0; i < mesh.submeshes.size(); ++i)
    {
        vertexBufferSize += mesh.submeshes[i].vertices.size() * sizeof(float);
        indexBufferSize += mesh.submeshes[i].indices.size() * sizeof(u32);
    }

    GL_CHECK(glGenBuffers(1, &mesh.vertexBufferHandle));
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, vertexBufferSize, NULL, GL_STATIC_DRAW));

    glGenBuffers(1, &mesh.indexBufferHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferSize, NULL, GL_STATIC_DRAW);

    u32 indicesOffset = 0;
    u32 verticesOffset = 0;

    for (u32 i = 0; i < mesh.submeshes.size(); ++i)
    {
        const void* verticesData = mesh.submeshes[i].vertices.data();
        const u32   verticesSize = mesh.submeshes[i].vertices.size() * sizeof(float);
        glBufferSubData(GL_ARRAY_BUFFER, verticesOffset, verticesSize, verticesData);
        mesh.submeshes[i].vertexOffset = verticesOffset;
        verticesOffset += verticesSize;

        const void* indicesData = mesh.submeshes[i].indices.data();
        const u32   indicesSize = mesh.submeshes[i].indices.size() * sizeof(u32);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, indicesOffset, indicesSize, indicesData);
        mesh.submeshes[i].indexOffset = indicesOffset;
        indicesOffset += indicesSize;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return modelIdx;
}

#pragma endregion

#pragma region Initialization

GLuint FindVAO(App* app, Mesh& mesh, u32 submeshIndex, const Program& program) {
    Submesh& submesh = mesh.submeshes[submeshIndex];

    for (VAO& vao : submesh.vaos) {
        if (vao.programHandle == program.handle) {
            return vao.handle;
        }
    }

    GLuint vaoHandle = 0;
    glGenVertexArrays(1, &vaoHandle);
    GL_CHECK(glBindVertexArray(vaoHandle));

    if (app->enableDebugGroups) {
        std::string label = "VAO_Prog" + program.programName + "_Submesh" + std::to_string(submeshIndex);
        glObjectLabel(GL_VERTEX_ARRAY, vaoHandle, -1, label.c_str());
    }

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);

    for (const auto& shaderAttr : program.vertexInputLayout.attributes) {
        bool linked = false;
        for (const auto& bufferAttr : submesh.vertexBufferLayout.attributes) {
            if (shaderAttr.location == bufferAttr.location) {
                GL_CHECK(glEnableVertexAttribArray(shaderAttr.location));
                glVertexAttribPointer(
                    shaderAttr.location,
                    bufferAttr.componentCount,
                    GL_FLOAT,
                    GL_FALSE,
                    submesh.vertexBufferLayout.stride,
                    (void*)(u64)(bufferAttr.offset + submesh.vertexOffset)
                );
                linked = true;
                break;
            }
        }
        assert(linked);
    }

    submesh.vaos.push_back({ vaoHandle, program.handle });
    return vaoHandle;
}

void CreateSphere(App* app) {
    const int H = 32; // Horizontal slices
    const int V = 16; // Vertical slices

    std::vector<float> vertices;
    std::vector<u32> indices;

    // Generate vertices (position + normal + texCoords)
    for (int v = 0; v <= V; ++v) {
        for (int h = 0; h <= H; ++h) {
            // Spherical coordinates
            float theta = h * 2.0f * glm::pi<float>() / H;
            float phi = v * glm::pi<float>() / V;

            // Position
            float x = sin(phi) * cos(theta);
            float y = cos(phi);
            float z = sin(phi) * sin(theta);

            // Normal (same as position for unit sphere)
            // TexCoords
            float u = (float)h / H;
            float vCoord = (float)v / V;

            // Add vertex attributes
            vertices.push_back(x);  // Position X
            vertices.push_back(y);  // Position Y
            vertices.push_back(z);  // Position Z
            vertices.push_back(x);  // Normal X
            vertices.push_back(y);  // Normal Y
            vertices.push_back(z);  // Normal Z
            vertices.push_back(u);  // TexCoord U
            vertices.push_back(vCoord); // TexCoord V
        }
    }

    // Generate indices
    for (int v = 0; v < V; ++v) {
        for (int h = 0; h < H; ++h) {
            int i0 = v * (H + 1) + h;
            int i1 = v * (H + 1) + (h + 1);
            int i2 = (v + 1) * (H + 1) + h;
            int i3 = (v + 1) * (H + 1) + (h + 1);

            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);

            indices.push_back(i1);
            indices.push_back(i3);
            indices.push_back(i2);
        }
    }

    // Create vertex buffer layout
    VertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.attributes.push_back({ 0, 3, 0 }); // Position
    vertexBufferLayout.attributes.push_back({ 1, 3, 3 * sizeof(float) }); // Normal
    vertexBufferLayout.attributes.push_back({ 2, 2, 6 * sizeof(float) }); // TexCoord
    vertexBufferLayout.stride = 8 * sizeof(float);

    // Create submesh
    Submesh submesh = {};
    submesh.vertexBufferLayout = vertexBufferLayout;
    submesh.vertices = vertices;
    submesh.indices = indices;

    // Create mesh
    Mesh mesh = {};
    mesh.submeshes.push_back(submesh);

    // Create GPU buffers
    glGenBuffers(1, &mesh.vertexBufferHandle);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &mesh.indexBufferHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(u32), indices.data(), GL_STATIC_DRAW);

    // Add to app
    app->meshes.push_back(mesh);

    // Create default material
    Material material = {};
    material.name = "DefaultMaterial";
    material.albedo = glm::vec3(1.0f);
    material.albedoTextureIdx = app->whiteTexIdx;
    app->materials.push_back(material);

    // Create model
    Model model = {};
    model.meshIdx = app->meshes.size() - 1;
    model.materialIdx.push_back(app->materials.size() - 1);
    app->models.push_back(model);

    // Debug labels
    if (app->enableDebugGroups) {
        glObjectLabel(GL_BUFFER, mesh.vertexBufferHandle, -1, "Sphere_VBO");
        glObjectLabel(GL_BUFFER, mesh.indexBufferHandle, -1, "Sphere_IBO");
    }
}

void CreateCube(App* app) {

    std::vector<float> vertices;
    std::vector<u32> indices;

    // Cube vertices (position + normal + texCoords)
    const float cubeVertices[] = {
        // Front face
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f, // 0
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f, // 1
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f, // 2
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f, // 3

        // Back face
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f, // 4
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f, // 5
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f, // 6
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, // 7

        // Top face
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f, // 8
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f, // 9
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f, // 10
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f, // 11

        // Bottom face
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f, // 12
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f, // 13
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f, // 14
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f, // 15

        // Right face
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f, // 16
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, // 17
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, // 18
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f, // 19

         // Left face
         -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f, // 20
         -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f, // 21
         -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f, // 22
         -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f  // 23
    };

    const u32 cubeIndices[] = {
        // Front
        0, 1, 2, 0, 2, 3,
        // Back
        5, 4, 7, 5, 7, 6,
        // Top
        8, 9, 10, 8, 10, 11,
        // Bottom
        13, 12, 15, 13, 15, 14,
        // Right
        16, 17, 18, 16, 18, 19,
        // Left
        21, 20, 23, 21, 23, 22
    };

    // Copy data to vectors
    vertices.assign(cubeVertices, cubeVertices + sizeof(cubeVertices) / sizeof(float));
    indices.assign(cubeIndices, cubeIndices + sizeof(cubeIndices) / sizeof(u32));

    // Create vertex buffer layout
    VertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.attributes.push_back({ 0, 3, 0 }); // Position
    vertexBufferLayout.attributes.push_back({ 1, 3, 3 * sizeof(float) }); // Normal
    vertexBufferLayout.attributes.push_back({ 2, 2, 6 * sizeof(float) }); // TexCoord
    vertexBufferLayout.stride = 8 * sizeof(float);

    // Create submesh
    Submesh submesh = {};
    submesh.vertexBufferLayout = vertexBufferLayout;
    submesh.vertices = vertices;
    submesh.indices = indices;

    // Create mesh
    Mesh mesh = {};
    mesh.submeshes.push_back(submesh);

    // Create GPU buffers
    glGenBuffers(1, &mesh.vertexBufferHandle);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &mesh.indexBufferHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(u32), indices.data(), GL_STATIC_DRAW);

    // Add to app
    app->meshes.push_back(mesh);

    // Create default material
    Material material = {};
    material.name = "DefaultCubeMaterial";
    material.albedo = glm::vec3(1.0f, 1.0f, 1.0f);
    material.albedoTextureIdx = app->whiteTexIdx;
    app->materials.push_back(material);

    // Create model
    Model model = {};
    model.meshIdx = app->meshes.size() - 1;
    model.materialIdx.push_back(app->materials.size() - 1);
    app->models.push_back(model);

    // Debug labels
    if (app->enableDebugGroups) {
        glObjectLabel(GL_BUFFER, mesh.vertexBufferHandle, -1, "Cube_VBO");
        glObjectLabel(GL_BUFFER, mesh.indexBufferHandle, -1, "Cube_IBO");
    }
}

void CreatePlane(App* app) {

    std::vector<float> vertices;
    std::vector<u32> indices;

    // Define vertices (position + normal + texCoords)
    const float planeSize = 1.0f;
    const float positions[] = {
        -planeSize, 0.0f, -planeSize,  // 0
         planeSize, 0.0f, -planeSize,  // 1
         planeSize, 0.0f,  planeSize,  // 2
        -planeSize, 0.0f,  planeSize   // 3
    };

    const float normals[] = {
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };

    const float texCoords[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };

    // Combine into interleaved vertex data
    for (int i = 0; i < 4; ++i) {
        // Position
        vertices.push_back(positions[i * 3]);
        vertices.push_back(positions[i * 3 + 1]);
        vertices.push_back(positions[i * 3 + 2]);

        // Normal
        vertices.push_back(normals[i * 3]);
        vertices.push_back(normals[i * 3 + 1]);
        vertices.push_back(normals[i * 3 + 2]);

        // TexCoord
        vertices.push_back(texCoords[i * 2]);
        vertices.push_back(texCoords[i * 2 + 1]);
    }

    // Define indices
    const u32 planeIndices[] = {
        0, 1, 2,
        0, 2, 3
    };
    indices.assign(planeIndices, planeIndices + 6);

    // Create vertex buffer layout
    VertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.attributes.push_back({ 0, 3, 0 }); // Position
    vertexBufferLayout.attributes.push_back({ 1, 3, 3 * sizeof(float) }); // Normal
    vertexBufferLayout.attributes.push_back({ 2, 2, 6 * sizeof(float) }); // TexCoord
    vertexBufferLayout.stride = 8 * sizeof(float);

    // Create submesh
    Submesh submesh = {};
    submesh.vertexBufferLayout = vertexBufferLayout;
    submesh.vertices = vertices;
    submesh.indices = indices;

    // Create mesh
    Mesh mesh = {};
    mesh.submeshes.push_back(submesh);

    // Create GPU buffers
    glGenBuffers(1, &mesh.vertexBufferHandle);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &mesh.indexBufferHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(u32), indices.data(), GL_STATIC_DRAW);

    // Add to app
    app->meshes.push_back(mesh);

    // Create default material
    Material material = {};
    material.name = "DefaultPlaneMaterial";
    material.albedo = glm::vec3(0.8f, 0.8f, 0.8f);
    material.albedoTextureIdx = app->whiteTexIdx;
    app->materials.push_back(material);

    // Create model
    Model model = {};
    model.meshIdx = app->meshes.size() - 1;
    model.materialIdx.push_back(app->materials.size() - 1);
    app->models.push_back(model);

    // Debug labels
    if (app->enableDebugGroups) {
        glObjectLabel(GL_BUFFER, mesh.vertexBufferHandle, -1, "Plane_VBO");
        glObjectLabel(GL_BUFFER, mesh.indexBufferHandle, -1, "Plane_IBO");
    }
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
    app->texturedGeometryProgramIdx = LoadProgram(app, "texturedGeo.glsl", "TEXTURED_GEOMETRY");
    Program& texturedGeometryProgram = app->programs[app->texturedGeometryProgramIdx];
    app->programUniformTexture = glGetUniformLocation(texturedGeometryProgram.handle, "uTexture");

    // - textures
    app->diceTexIdx = LoadTexture2D(app, "dice.png");

    if (app->enableDebugGroups) {
        glObjectLabel(GL_VERTEX_ARRAY, app->vao, -1, "MainVAO");
        glObjectLabel(GL_BUFFER, app->embeddedVertices, -1, "QuadVertices");
        glObjectLabel(GL_BUFFER, app->embeddedElements, -1, "QuadIndices");
    }
}

void InitTexturedMesh(App* app)
{
    /*CreatePlane(app);
    CreateSphere(app);
    CreateCube(app);*/

    Mesh mesh = {};
    Submesh submesh = {};

    VertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.attributes.push_back({ 0, 3, 0 }); // position at location 0
    vertexBufferLayout.attributes.push_back({ 2, 2, 3 * sizeof(float) }); // texCoords at location 2
    vertexBufferLayout.stride = 5 * sizeof(float); // 3 floats pos + 2 floats uv

    // Define vertices and indices (matches the PDF shader)
    const float vertices[] = {
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, // pos + uv
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 0.0f, 1.0f
    };

    const u32 indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    // Copy data to submesh
    submesh.vertices.assign(vertices, vertices + sizeof(vertices) / sizeof(float));
    submesh.indices.assign(indices, indices + sizeof(indices) / sizeof(u32));
    submesh.vertexBufferLayout = vertexBufferLayout;

    // Add submesh to mesh
    mesh.submeshes.push_back(submesh);

    // Create GPU buffers
    glGenBuffers(1, &mesh.vertexBufferHandle);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
    glBufferData(GL_ARRAY_BUFFER, submesh.vertices.size() * sizeof(float), submesh.vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &mesh.indexBufferHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh.indices.size() * sizeof(u32), submesh.indices.data(), GL_STATIC_DRAW);

    // Store mesh in app
    app->meshes.push_back(mesh);

    // Load shader program (matches PDF page 5)
    app->texturedMeshProgramIdx = LoadProgram(app, "texturedMesh.glsl", "TEXTURED_MESH");
    Program& texturedMeshProgram = app->programs[app->texturedMeshProgramIdx];
    app->texturedMeshProgram_uTexture = glGetUniformLocation(texturedMeshProgram.handle, "uTexture");

    // Load model
    u32 patrickModelIdx = LoadModel(app, "Patrick/Patrick.obj");
    if (patrickModelIdx == UINT32_MAX) {
        ELOG("Failed to load Patrick model");
        // TODO: Create Default quad mesh
        CreateSphere(app);
    }

    // The model has also an "\Patrick\Patrick.mtl", should we see what to do with this???

    if (app->enableDebugGroups) {
        glObjectLabel(GL_BUFFER, mesh.vertexBufferHandle, -1, "MeshVBO");
        glObjectLabel(GL_BUFFER, mesh.indexBufferHandle, -1, "MeshIBO");
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
    app->mode = Mode_TexturedMesh;
    
    switch (app->mode)
    {
    case Mode_TexturedQuad:
        InitTexturedQuad(app);
        break;
    case Mode_TexturedMesh:
        InitTexturedMesh(app);
        break;
    case Mode_Count:
        break;
    default:
        break;
    }

    app->whiteTexIdx = LoadTexture2D(app, "color_white.png");
    app->blackTexIdx = LoadTexture2D(app, "color_black.png");
    app->normalTexIdx = LoadTexture2D(app, "color_normal.png");
    app->magentaTexIdx = LoadTexture2D(app, "color_magenta.png");

    if (app->enableDebugGroups) {
        for (size_t i = 0; i < app->textures.size(); ++i) {
            glObjectLabel(GL_TEXTURE, app->textures[i].handle, -1,
                app->textures[i].filepath.c_str());
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

        case Mode_TexturedMesh:
        {
            if (app->enableDebugGroups) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 2, -1, "TexturedMesh");

            Program& texturedMeshProgram = app->programs[app->texturedMeshProgramIdx];
            glUseProgram(texturedMeshProgram.handle);

            for (Model& model : app->models) {
                Mesh& mesh = app->meshes[model.meshIdx];

                for (u32 i = 0; i < mesh.submeshes.size(); ++i) {
                    Submesh& submesh = mesh.submeshes[i];
                    GLuint vao = FindVAO(app, mesh, i, texturedMeshProgram);
                    glBindVertexArray(vao);

                    u32 materialIdx = (i < model.materialIdx.size()) ? model.materialIdx[i] : 0;
                    Material& material = app->materials[materialIdx];
                    u32 textureIdx = material.albedoTextureIdx != UINT32_MAX
                        ? material.albedoTextureIdx
                        : app->whiteTexIdx;

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, app->textures[textureIdx].handle);
                    GL_CHECK(glUniform1i(app->texturedMeshProgram_uTexture, 0));

                    glDrawElements(GL_TRIANGLES, submesh.indices.size(),
                        GL_UNSIGNED_INT, (void*)(u64)submesh.indexOffset);
                }
            }

            if (app->enableDebugGroups) glPopDebugGroup();
        }
        break;        

        default:;
    }

    if (app->enableDebugGroups) glPopDebugGroup();
}