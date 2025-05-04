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

#include <vector>
#include <string>
#include <filesystem>

struct App;

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
};

struct Texture {
    GLuint id;
    std::string name;
    std::string path;

    ~Texture() {
        if (id != 0) {
            glDeleteTextures(1, &id);
        }
    }

    bool operator==(const Texture& other) const {
        return path == other.path;
    }
};

struct Mat_Property
{
    std::shared_ptr<Texture> texture;
    glm::vec4 color;
    bool tex_enabled = false;
    bool prop_enabled = false;
};

class Material {
public:
    Material();

    std::string name;

    Mat_Property diffuse;
    Mat_Property specular;
    Mat_Property normal;
    Mat_Property height;
    Mat_Property roughness;     //roughness vs glossiness = glossines es el inverso del otro
};

class Mesh {
public:
    // Submesh data now directly in Mesh
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    std::shared_ptr<Material> material;
    u32 vertexOffset;
    u32 indexOffset;

    GLuint VAO, VBO, EBO;

    void SetupMesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(u32), indices.data(), GL_STATIC_DRAW);

        // Vertex attributes
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

    void Draw(const Shader& shader) const;
};

class Model {
public:
    std::string name;
    std::vector<Mesh> meshes;
    std::vector<std::shared_ptr<Material>> materials;
    std::string directory;

    u32 bufferOffset;
    u32 bufferSize;

    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    Model() = default;

    Model(std::string const& path, App* app) {
        LoadModel(path, app);
    }

    void Draw(Shader& shader) {
        for (auto& mesh : meshes) {
            mesh.Draw(shader);
        }
    }

private:
    void LoadModel(std::string const& path, App* app);

    void ProcessNode(aiNode* node, const aiScene* scene, App* app);

    void ProcessMesh(App* app, aiMesh* mesh, const aiScene* scene, Mesh& new_mesh);

    void LoadMaterialTextures(App* app, aiMaterial* mat, Material& material);

    static GLuint TextureFromFile(const char* path);


public:

    bool LoadTextureToMat(App* app, std::shared_ptr<Texture>& texture, std::string path);
    static bool LoadTexture(App* app, std::string path);

    static float RandomColorRGB() {
        return static_cast<float>(rand() % 101) / 100.0f; // 0 to 1.00, two decimal precision
    }

    static GLuint CreateSolidColorTexture(float r, float g, float b, float a = 1.0f);

    static Texture TextureFromColor(std::string textureName, glm::vec4 color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
};