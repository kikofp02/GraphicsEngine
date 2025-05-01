// model.h
#pragma once

#include "platform.h"
#include "shader.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <stb_image.h>
#include <stb_image_write.h>
#include <filesystem>

#include <vector>
#include <string>

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
};

struct Texture {
    GLuint id;
    std::string type;
    std::string path;
};

struct Material {
    std::vector<Texture> diffuse;
    std::vector<Texture> specular;
    std::vector<Texture> normal;
    std::vector<Texture> height;
    // TODO_K: Add more if needed (AO, Metalic...)
};

struct Submesh {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    Material material;
    u32 vertexOffset;
    u32 indexOffset;
};

class Mesh {
public:
    std::vector<Submesh> submeshes;
    GLuint VAO, VBO, EBO;

    Mesh() = default;

    void SetupMesh() {
        size_t totalVertices = 0;
        size_t totalIndices = 0;

        for (auto& submesh : submeshes) {
            totalVertices += submesh.vertices.size();
            totalIndices += submesh.indices.size();
        }

        // Create Buffers
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, totalVertices * sizeof(Vertex), nullptr, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, totalIndices * sizeof(u32), nullptr, GL_STATIC_DRAW);

        size_t vertexOffset = 0;
        size_t indexOffset = 0;

        for (auto& submesh : submeshes) {
            glBufferSubData(GL_ARRAY_BUFFER,
                vertexOffset * sizeof(Vertex),
                submesh.vertices.size() * sizeof(Vertex),
                submesh.vertices.data());

            std::vector<u32> adjustedIndices = submesh.indices;
            for (auto& index : adjustedIndices) {
                index += vertexOffset;
            }

            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                indexOffset * sizeof(u32),
                adjustedIndices.size() * sizeof(u32),
                adjustedIndices.data());

            submesh.vertexOffset = vertexOffset;
            submesh.indexOffset = indexOffset;

            vertexOffset += submesh.vertices.size();
            indexOffset += adjustedIndices.size();
        }

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));

        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Bitangent));

        glBindVertexArray(0);
    }

    void Draw(const Shader& shader) const {
        glBindVertexArray(VAO);

        shader.SetInt("material.diffuse", 0);
        shader.SetInt("material.specular", 1);
        shader.SetInt("material.normal", 2);
        shader.SetInt("material.height", 3);

        for (const auto& submesh : submeshes) {
            if (!submesh.material.diffuse.empty()) {
                const auto& tex = submesh.material.diffuse.front();
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex.id);
            }

            if (!submesh.material.specular.empty()) {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, submesh.material.specular.front().id);
            }

            if (!submesh.material.normal.empty()) {
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, submesh.material.normal.front().id);
            }

            if (!submesh.material.height.empty()) {
                glActiveTexture(GL_TEXTURE3);
                glBindTexture(GL_TEXTURE_2D, submesh.material.height.front().id);
            }

            // Draw elements
            glDrawElements(GL_TRIANGLES,
                submesh.indices.size(),
                GL_UNSIGNED_INT,
                (void*)(submesh.indexOffset * sizeof(u32)));
        }

        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }
};

class Model {
public:
    std::vector<Texture> textures_loaded;
    std::vector<Mesh> meshes;
    std::string directory;
    bool gammaCorrection;

    u32 bufferOffset;
    u32 bufferSize;

    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    Model() = default;

    Model(std::string const& path, bool gamma = false) : gammaCorrection(gamma) {
        LoadModel(path);
    }

    void Draw(Shader& shader) {
        for (auto& mesh : meshes) {
            mesh.Draw(shader);
        }
    }

private:
    void LoadModel(std::string const& path) {
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

        directory = path.substr(0, path.find_last_of('/'));

        Mesh mainMesh;
        ProcessNode(scene->mRootNode, scene, mainMesh);

        mainMesh.SetupMesh();
        meshes.push_back(mainMesh);
    }

    void ProcessNode(aiNode* node, const aiScene* scene, Mesh& mesh) {
        for (u32 i = 0; i < node->mNumMeshes; i++) {
            aiMesh* aiMesh = scene->mMeshes[node->mMeshes[i]];
            mesh.submeshes.push_back(ProcessSubmesh(aiMesh, scene));
        }

        for (u32 i = 0; i < node->mNumChildren; i++) {
            ProcessNode(node->mChildren[i], scene, mesh);
        }
    }

    Submesh ProcessSubmesh(aiMesh* mesh, const aiScene* scene) {
        Submesh submesh;

        for (u32 i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
            vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

            if (mesh->HasNormals()) {
                vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            }

            if (mesh->mTextureCoords[0]) {
                vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
                vertex.Tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
                vertex.Bitangent = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
            }

            submesh.vertices.push_back(vertex);
        }

        for (u32 i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (u32 j = 0; j < face.mNumIndices; j++) {
                submesh.indices.push_back(face.mIndices[j]);
            }
        }

        if (mesh->mMaterialIndex >= 0) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

            if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                std::vector<Texture> diffuseMaps = LoadMaterialTextures(
                    material, aiTextureType_DIFFUSE, "texture_diffuse");
                submesh.material.diffuse.insert(submesh.material.diffuse.end(), diffuseMaps.begin(), diffuseMaps.end());

                std::vector<Texture> specularMaps = LoadMaterialTextures(
                    material, aiTextureType_SPECULAR, "texture_specular");
                submesh.material.specular.insert(submesh.material.specular.end(), specularMaps.begin(), specularMaps.end());

                std::vector<Texture> normalMaps = LoadMaterialTextures(
                    material, aiTextureType_NORMALS, "texture_normal");
                submesh.material.normal.insert(submesh.material.normal.end(), normalMaps.begin(), normalMaps.end());

                std::vector<Texture> heightMaps = LoadMaterialTextures(
                    material, aiTextureType_HEIGHT, "texture_height");
                submesh.material.height.insert(submesh.material.height.end(), heightMaps.begin(), heightMaps.end());
            }
            else {
                submesh.material.diffuse.push_back(DefaultTexture("texture_diffuse"));
            }
        }

        return submesh;
    }

    std::vector<Texture> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName) {
        std::vector<Texture> textures;
        for (u32 i = 0; i < mat->GetTextureCount(type); i++) {
            aiString str;
            mat->GetTexture(type, i, &str);

            bool skip = false;
            for (u32 j = 0; j < textures_loaded.size(); j++) {
                if (std::strcmp(textures_loaded[j].path.c_str(), str.C_Str()) == 0) {
                    textures.push_back(textures_loaded[j]);
                    skip = true;
                    break;
                }
            }

            if (!skip) {
                Texture texture;
                texture.id = TextureFromFile(str.C_Str(), directory);
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture);
            }
        }
        return textures;
    }

    static GLuint TextureFromFile(const char* path, const std::string& directory) {
        std::string filename = directory + '/' + std::string(path);

        GLuint textureID;
        glGenTextures(1, &textureID);

        int width, height, nrComponents;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);

        if (data) {
            GLenum format = GL_RGB;
            if (nrComponents == 1) format = GL_RED;
            else if (nrComponents == 3) format = GL_RGB;
            else if (nrComponents == 4) format = GL_RGBA;

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
            ELOG("Texture failed to load at path: %s", path);
            stbi_image_free(data);
        }

        return textureID;
    }

    GLuint CreateSolidColorTexture(float r, float g, float b, float a = 1.0f) {
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

    public:

    Texture DefaultTexture(std::string textureName, glm::vec3 color = glm::vec3(0.8f, 0.8f, 0.8f)) {
        aiColor3D diffuseColor(color.x, color.y, color.z); // Default gray

        Texture fallbackDiffuse;
        fallbackDiffuse.id = CreateSolidColorTexture(diffuseColor.r, diffuseColor.g, diffuseColor.b);
        fallbackDiffuse.type = textureName;
        fallbackDiffuse.path = "fallback_texture";
        return fallbackDiffuse;
    }
};