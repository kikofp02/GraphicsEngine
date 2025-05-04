// model.cpp
#include "engine.h"
#include "model.h"

Material::Material() {
    diffuse.color       = glm::vec4(glm::vec3(Model::RandomColorRGB(), Model::RandomColorRGB(), Model::RandomColorRGB()), 1.0f);
    specular.color      = glm::vec4(glm::vec3(0.5f), 1.0f);
    normal.color        = glm::vec4(glm::vec3(1.f), 1.0f);
    height.color        = glm::vec4(glm::vec3(0.f), 1.0f);
    roughness.color     = glm::vec4(glm::vec3(0.f), 1.0f);
}

void Mesh::Draw(const Shader& shader) const {
    glBindVertexArray(VAO);

    shader.SetInt("mat_textures.diffuse", 0);
    shader.SetVec4("material.diffuse.color", material->diffuse.color);
    /*shader.SetInt("material.specular.text", 1);
    shader.SetVec3("material.diffuse.color", material->diffuse.color);
    shader.SetInt("material.normal.text", 2);
    shader.SetVec3("material.diffuse.color", material->diffuse.color);
    shader.SetInt("material.height.text", 3);
    shader.SetVec3("material.diffuse.color", material->diffuse.color);*/

    if (material->diffuse.tex_enabled) {
        shader.SetBool("material.diffuse.use_text", true);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, material->diffuse.texture->id);
    }
    else {
        shader.SetBool("material.diffuse.use_text", false);
    }

    /*if (material->specular.tex_enabled) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, material->specular.texture->id);
    }
    else {
        Texture color_tex = Model::TextureFromColor("color_specular", material->specular.color);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, color_tex.id);
    }

    if (material->normal.tex_enabled) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, material->normal.texture->id);
    }
    else {
        Texture color_tex = Model::TextureFromColor("color_normal", material->normal.color);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, color_tex.id);
    }

    if (material->height.tex_enabled) {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, material->height.texture->id);
    }
    else {
        Texture color_tex = Model::TextureFromColor("color_height", material->height.color);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, color_tex.id);
    }*/

    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
}

void Model::LoadModel(std::string const& path, App* app) {
    GLUtils::ErrorGuard guard("AssimpLoad");

    const aiScene* scene = aiImportFile(path.c_str(),
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_PreTransformVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_OptimizeMeshes |
        aiProcess_SortByPType);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        ELOG("Error loading mesh %s: %s", path.c_str(), aiGetErrorString());
        return;
    }

    this->name = std::filesystem::path(path).stem().string();

    directory = path.substr(0, path.find_last_of('/'));
    ProcessNode(scene->mRootNode, scene, app);
}

void Model::ProcessNode(aiNode* node, const aiScene* scene, App* app) {
    for (u32 i = 0; i < node->mNumMeshes; i++) {
        aiMesh* ai_mesh = scene->mMeshes[node->mMeshes[i]];
        Mesh mesh;
        ProcessMesh(app, ai_mesh, scene, mesh);
        mesh.SetupMesh();
        meshes.push_back(mesh);
    }

    for (u32 i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene, app);
    }
}

void Model::ProcessMesh(App* app, aiMesh* mesh, const aiScene* scene, Mesh& new_mesh) {
    // Vertices
    for (u32 i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

        if (mesh->HasNormals()) {
            vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
        }

        if (mesh->mTextureCoords[0]) {
            vertex.TexCoords = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
            vertex.Tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
            vertex.Bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
        }

        new_mesh.vertices.push_back(vertex);
    }

    // Indices
    for (u32 i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (u32 j = 0; j < face.mNumIndices; j++) {
            new_mesh.indices.push_back(face.mIndices[j]);
        }
    }

    // Material
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
        materials.push_back(std::make_shared<Material>());
        new_mesh.material = materials.back();

        aiString matName;
        if (mat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) {
            new_mesh.material->name = matName.C_Str();
        }
        else {
            new_mesh.material->name = "unnamed_material";
        }

        LoadMaterialTextures(app, mat, *new_mesh.material);
    }
}

void Model::LoadMaterialTextures(App* app, aiMaterial* mat, Material& material) {

    if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
        aiString path;
        mat->GetTexture(aiTextureType_DIFFUSE, 0, &path);
        material.diffuse.tex_enabled = LoadTextureToMat(app, material.diffuse.texture, path.C_Str());
    }
}

bool Model::LoadTextureToMat(App* app, std::shared_ptr<Texture>& texture, std::string path) {

    std::filesystem::path fsPath(path);
    std::string textureName = fsPath.stem().string();
    std::string fullPath = (std::filesystem::path(directory) / fsPath).lexically_normal().string();

    auto it = std::find_if(app->textures_loaded.begin(), app->textures_loaded.end(),
        [&](const std::shared_ptr<Texture>& t) { return t->path == fullPath; });

    if (it != app->textures_loaded.end()) {
        texture = *it;
        return true;
    }

    GLuint textureID = TextureFromFile(fullPath.c_str());
    if (textureID == 0) {
        return false;
    }

    std::shared_ptr<Texture> new_texture = std::make_shared<Texture>();
    new_texture->id = textureID;
    new_texture->name = textureName;
    new_texture->path = fullPath;
    app->textures_loaded.push_back(new_texture);
    texture = new_texture;
}

bool Model::LoadTexture(App* app, std::string path) {

    std::filesystem::path fsPath(path);
    std::string textureName = fsPath.stem().string();
    std::string fullPath = (fsPath).lexically_normal().string();

    auto it = std::find_if(app->textures_loaded.begin(), app->textures_loaded.end(),
        [&](const std::shared_ptr<Texture>& t) { return t->path == fullPath; });

    if (it != app->textures_loaded.end()) {
        return true;
    }

    GLuint textureID = TextureFromFile(fullPath.c_str());
    if (textureID == 0) {
        return false;
    }

    std::shared_ptr<Texture> new_texture = std::make_shared<Texture>();
    new_texture->id = textureID;
    new_texture->name = textureName;
    new_texture->path = fullPath;
    app->textures_loaded.push_back(new_texture);
}

GLuint Model::TextureFromFile(const char* path) {
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);

    if (data) {
        GLenum format = GL_RGBA;
        if (nrComponents == 1) format = GL_RED;
        else if (nrComponents == 3) format = GL_RGB;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else {
        ELOG("Failed to load texture at path: %s", path);
        stbi_image_free(data);
        glDeleteTextures(1, &textureID);
        return 0;
    }

    return textureID;
}

GLuint Model::CreateSolidColorTexture(float r, float g, float b, float a) {
    GLuint textureID;
    glGenTextures(1, &textureID);

    unsigned char pixel[] = {
        static_cast<unsigned char>(r * 255),
        static_cast<unsigned char>(g * 255),
        static_cast<unsigned char>(b * 255),
        static_cast<unsigned char>(a * 255)
    };

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return textureID;
}

Texture Model::TextureFromColor(std::string textureName, glm::vec4 color) {
    aiColor3D diffuseColor(color.x, color.y, color.z); // Default gray

    Texture fallbackDiffuse;
    fallbackDiffuse.id = CreateSolidColorTexture(diffuseColor.r, diffuseColor.g, diffuseColor.b, color.w);
    fallbackDiffuse.name = textureName;
    fallbackDiffuse.path = "color_texture";
    return fallbackDiffuse;
}