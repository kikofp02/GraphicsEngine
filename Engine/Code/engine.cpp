//engine.cpp
#include "engine.h"
#include "gl_error.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#pragma region UBOs


#pragma region Buffer_Utilities

bool IsPowerOf2(u32 value)
{
	return value && !(value & (value - 1));
}

static u32 Align(u32 value, u32 alignment) {
	return (value + alignment - 1) & ~(alignment - 1);
}

Buffer CreateBuffer(u32 size, GLenum type, GLenum usage)
{
	Buffer buffer = {};
	buffer.size = size;
	buffer.type = type;

	GL_CHECK(glGenBuffers(1, &buffer.handle));
	GL_CHECK(glBindBuffer(type, buffer.handle));
	GL_CHECK(glBufferData(type, buffer.size, NULL, usage));
	GL_CHECK(glBindBuffer(type, 0));

	return buffer;
}

#define CreateConstantBuffer(size) CreateBuffer(size, GL_UNIFORM_BUFFER, GL_STREAM_DRAW)
#define CreateStaticVertexBuffer(size) CreateBuffer(size, GL_ARRAY_BUFFER, GL_STATIC_DRAW)
#define CreateStaticIndexBuffer(size) CreateBuffer(size, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW)

void BindBuffer(const Buffer& buffer)
{
	GL_CHECK(glBindBuffer(buffer.type, buffer.handle));
}

void MapBuffer(Buffer& buffer, GLenum access)
{
	GL_CHECK(glBindBuffer(buffer.type, buffer.handle));
	void* ptr = glMapBuffer(buffer.type, access);
	buffer.data = (u8*)ptr;

	buffer.head = 0;
}

void UnmapBuffer(Buffer& buffer)
{
	GL_CHECK(glBindBuffer(buffer.type, buffer.handle));
	GL_CHECK(glUnmapBuffer(buffer.type));
	GL_CHECK(glBindBuffer(buffer.type, 0));
}

void AlignHead(Buffer& buffer, u32 alignment)
{
	ASSERT(IsPowerOf2(alignment), "The alignment must be a power of 2");
	buffer.head = Align(buffer.head, alignment);
}

void PushAlignedData(Buffer& buffer, const void* data, u32 size, u32 alignment)
{
	ASSERT(buffer.data != NULL, "The buffer must be mapped first");
	AlignHead(buffer, alignment);
	memcpy((u8*)buffer.data + buffer.head, data, size);
	buffer.head += size;
}

#define PushData(buffer, data, size) PushAlignedData(buffer, data, size, 1)
#define PushUInt(buffer, value) { u32 v = value; PushAlignedData(buffer, &v, sizeof(v), 4); }
#define PushVec3(buffer, value) PushAlignedData(buffer, value_ptr(value), sizeof(value), sizeof(vec4))
#define PushVec4(buffer, value) PushAlignedData(buffer, value_ptr(value), sizeof(value), sizeof(vec4))
#define PushMat3(buffer, value) PushAlignedData(buffer, value_ptr(value), sizeof(value), sizeof(vec4))
#define PushMat4(buffer, value) PushAlignedData(buffer, value_ptr(value), sizeof(value), sizeof(vec4))

#pragma endregion

void InitUBOs(App* app) {
	// UBOs
	// Transform UBO
	GL_CHECK(glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
		reinterpret_cast<GLint*>(&app->transformsUBO.alignment)));

	app->transformsUBO.blockSize = (2 * sizeof(glm::mat4));
	app->transformsUBO.blockSize = Align(app->transformsUBO.blockSize, app->transformsUBO.alignment);

	app->transformsUBO.buffer = CreateBuffer(
		app->transformsUBO.blockSize * app->models.size(),
		GL_UNIFORM_BUFFER,
		GL_DYNAMIC_DRAW
	);

	// Global UBO
	GL_CHECK(glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
		reinterpret_cast<GLint*>(&app->globalParamsUBO.alignment)));

	size_t cameraPosSize = sizeof(glm::vec4);
	size_t lightCountSize = sizeof(glm::uvec4);
	size_t lightSize = 4 * sizeof(glm::vec4);
	app->globalParamsUBO.blockSize = cameraPosSize + lightCountSize + app->lights.size() * lightSize;
	app->globalParamsUBO.blockSize = Align(app->globalParamsUBO.blockSize, app->globalParamsUBO.alignment);

	app->globalParamsUBO.buffer = CreateBuffer(
		app->globalParamsUBO.blockSize,
		GL_UNIFORM_BUFFER,
		GL_STREAM_DRAW
	);
}

void UpdateUBOs(App* app) {

	// Transform UBO
	// Calculate matrices
	glm::mat4 view = app->camera.GetViewMatrix();
	glm::mat4 projection = glm::perspective(glm::radians(app->camera.Zoom),
		(float)app->displaySize.x / (float)app->displaySize.y,
		app->camera.z_near, app->camera.z_far
	);

	MapBuffer(app->transformsUBO.buffer, GL_WRITE_ONLY);
	app->transformsUBO.currentOffset = 0;

	for (auto& model : app->models) {
		size_t blockStart = app->transformsUBO.currentOffset;

		glm::mat4 modelMat = glm::mat4(1.0f);
		modelMat = glm::translate(modelMat, model.position);
		modelMat = glm::rotate(modelMat, glm::radians(model.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		modelMat = glm::rotate(modelMat, glm::radians(model.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		modelMat = glm::rotate(modelMat, glm::radians(model.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		modelMat = glm::scale(modelMat, model.scale);

		glm::mat4 vp = projection * view;

		PushMat4(app->transformsUBO.buffer, modelMat);
		PushMat4(app->transformsUBO.buffer, vp);

		AlignHead(app->transformsUBO.buffer, app->transformsUBO.blockSize);

		model.bufferOffset = blockStart;
		model.bufferSize = app->transformsUBO.blockSize;

		app->transformsUBO.currentOffset += app->transformsUBO.blockSize;
	}

	UnmapBuffer(app->transformsUBO.buffer);

	// Global UBO
	MapBuffer(app->globalParamsUBO.buffer, GL_WRITE_ONLY);

	PushVec3(app->globalParamsUBO.buffer, app->camera.Position);
	PushUInt(app->globalParamsUBO.buffer, static_cast<u32>(app->lights.size()));

	// Lights array
	for (auto& light : app->lights) {
		PushUInt(app->globalParamsUBO.buffer, light.enabled ? 1u : 0u);
		PushUInt(app->globalParamsUBO.buffer, static_cast<u32>(light.type));
		AlignHead(app->globalParamsUBO.buffer, 16);

		PushVec3(app->globalParamsUBO.buffer, light.direction);
		PushVec4(app->globalParamsUBO.buffer, glm::vec4(light.color, light.intensity));
		PushVec4(app->globalParamsUBO.buffer, glm::vec4(light.position, light.range));
	}

	UnmapBuffer(app->globalParamsUBO.buffer);
}

#pragma endregion

#pragma region Initialization

#pragma region ModelLoaders

std::shared_ptr<Texture> GetTexture(App* app, std::string name) {
	for (const auto& texture : app->textures_loaded) {
		if (texture && texture->name == name) {
			return texture;
		}
	}
	return nullptr;
}

void LoadRifleModel(App* app) {
	Model model("Rifle/Rifle.fbx", app);
	model.scale = glm::vec3(0.1f);

	// Textures
	Model::LoadTexture(app, "Rifle/");

#pragma region Mat1

	model.materials[0]->diffuse.texture = GetTexture(app, "low_Upper_BaseColor");
	model.materials[0]->diffuse.prop_enabled = true;
	model.materials[0]->diffuse.tex_enabled = true;

	model.materials[0]->metallic.texture = GetTexture(app, "low_Upper_Metallic");
	model.materials[0]->metallic.prop_enabled = true;
	model.materials[0]->metallic.tex_enabled = true;

	model.materials[0]->roughness.texture = GetTexture(app, "low_Upper_Roughness");
	model.materials[0]->roughness.prop_enabled = true;
	model.materials[0]->roughness.tex_enabled = true;

	model.materials[0]->normal.texture = GetTexture(app, "low_Upper_Normal");
	model.materials[0]->normal.prop_enabled = true;
	model.materials[0]->normal.tex_enabled = true;

#pragma endregion

#pragma region Mat2

	model.materials[1]->diffuse.texture = GetTexture(app, "low_Lower_BaseColor");
	model.materials[1]->diffuse.prop_enabled = true;
	model.materials[1]->diffuse.tex_enabled = true;

	model.materials[1]->metallic.texture = GetTexture(app, "low_Lower_Metallic");
	model.materials[1]->metallic.prop_enabled = true;
	model.materials[1]->metallic.tex_enabled = true;

	model.materials[1]->roughness.texture = GetTexture(app, "low_Lower_Roughness");
	model.materials[1]->roughness.prop_enabled = true;
	model.materials[1]->roughness.tex_enabled = true;

	model.materials[1]->normal.texture = GetTexture(app, "low_Lower_Normal");
	model.materials[1]->normal.prop_enabled = true;
	model.materials[1]->normal.tex_enabled = true;

#pragma endregion

#pragma region Mat3

	model.materials[2]->diffuse.texture = GetTexture(app, "low_Bcg_BaseColor");
	model.materials[2]->diffuse.prop_enabled = true;
	model.materials[2]->diffuse.tex_enabled = true;

	model.materials[2]->metallic.texture = GetTexture(app, "low_Bcg_Metallic");
	model.materials[2]->metallic.prop_enabled = true;
	model.materials[2]->metallic.tex_enabled = true;

	model.materials[2]->roughness.texture = GetTexture(app, "low_Bcg_Roughness");
	model.materials[2]->roughness.prop_enabled = true;
	model.materials[2]->roughness.tex_enabled = true;

	model.materials[2]->normal.texture = GetTexture(app, "low_Bcg_Normal");
	model.materials[2]->normal.prop_enabled = true;
	model.materials[2]->normal.tex_enabled = true;

#pragma endregion

#pragma region Mat4

	model.materials[3]->diffuse.texture = GetTexture(app, "low_Mag_BaseColor");
	model.materials[3]->diffuse.prop_enabled = true;
	model.materials[3]->diffuse.tex_enabled = true;

	model.materials[3]->metallic.texture = GetTexture(app, "low_Mag_Metallic");
	model.materials[3]->metallic.prop_enabled = true;
	model.materials[3]->metallic.tex_enabled = true;

	model.materials[3]->roughness.texture = GetTexture(app, "low_Mag_Roughness");
	model.materials[3]->roughness.prop_enabled = true;
	model.materials[3]->roughness.tex_enabled = true;

	model.materials[3]->normal.texture = GetTexture(app, "low_Mag_Normal");
	model.materials[3]->normal.prop_enabled = true;
	model.materials[3]->normal.tex_enabled = true;

#pragma endregion

#pragma region Mat5

	model.materials[4]->diffuse.texture = GetTexture(app, "low_Silencer_BaseColor");
	model.materials[4]->diffuse.prop_enabled = true;
	model.materials[4]->diffuse.tex_enabled = true;

	model.materials[4]->metallic.texture = GetTexture(app, "low_Silencer_Metallic");
	model.materials[4]->metallic.prop_enabled = true;
	model.materials[4]->metallic.tex_enabled = true;

	model.materials[4]->roughness.texture = GetTexture(app, "low_Silencer_Roughness");
	model.materials[4]->roughness.prop_enabled = true;
	model.materials[4]->roughness.tex_enabled = true;

	model.materials[4]->normal.texture = GetTexture(app, "low_Silencer_Normal");
	model.materials[4]->normal.prop_enabled = true;
	model.materials[4]->normal.tex_enabled = true;

#pragma endregion

#pragma region Mat6

	model.materials[5]->diffuse.texture = GetTexture(app, "low_Scope_BaseColor");
	model.materials[5]->diffuse.prop_enabled = true;
	model.materials[5]->diffuse.tex_enabled = true;

	model.materials[5]->metallic.texture = GetTexture(app, "low_Scope_Metallic");
	model.materials[5]->metallic.prop_enabled = true;
	model.materials[5]->metallic.tex_enabled = true;

	model.materials[5]->roughness.texture = GetTexture(app, "low_Scope_Roughness");
	model.materials[5]->roughness.prop_enabled = true;
	model.materials[5]->roughness.tex_enabled = true;

	model.materials[5]->normal.texture = GetTexture(app, "low_Scope_Normal");
	model.materials[5]->normal.prop_enabled = true;
	model.materials[5]->normal.tex_enabled = true;

#pragma endregion

	app->models.push_back(model);
}

void LoadBackPackModel(App* app) {
	Model model("Backpack/Survival_BackPack_2.fbx", app);
	model.scale = glm::vec3(0.01f);

	// Textures
	Model::LoadTexture(app, "Backpack/");
	model.materials[0]->diffuse.texture = GetTexture(app, "1001_albedo");
	model.materials[0]->diffuse.prop_enabled = true;
	model.materials[0]->diffuse.tex_enabled = true;

	model.materials[0]->metallic.texture = GetTexture(app, "1001_metallic");
	model.materials[0]->metallic.prop_enabled = true;
	model.materials[0]->metallic.tex_enabled = true;

	model.materials[0]->roughness.texture = GetTexture(app, "1001_roughness");
	model.materials[0]->roughness.prop_enabled = true;
	model.materials[0]->roughness.tex_enabled = true;

	model.materials[0]->normal.texture = GetTexture(app, "1001_normal");
	model.materials[0]->normal.prop_enabled = true;
	model.materials[0]->normal.tex_enabled = true;

	app->models.push_back(model);
}

void LoadPatrickModel(App* app) {
	Model model("Patrick/Patrick.obj", app);

	// Textures
	Model::LoadTexture(app, "Backpack/");

	app->models.push_back(model);
}

void GeneratePlaneModel(App* app, float size, int subdivisions) {
	Model model;
	model.name = "Plane";

	Mesh planeMesh;

	// Generate vertices
	float step = size / subdivisions;
	float halfSize = size * 0.5f;

	for (int z = 0; z <= subdivisions; ++z) {
		for (int x = 0; x <= subdivisions; ++x) {
			Vertex vertex;
			vertex.Position = glm::vec3(
				-halfSize + x * step,
				0.0f,
				-halfSize + z * step
			);
			vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
			vertex.TexCoords = glm::vec2(
				static_cast<float>(x) / subdivisions,
				static_cast<float>(z) / subdivisions
			);
			vertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
			vertex.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);

			planeMesh.vertices.push_back(vertex);
		}
	}

	// Generate indices
	for (int z = 0; z < subdivisions; ++z) {
		for (int x = 0; x < subdivisions; ++x) {
			int topLeft = z * (subdivisions + 1) + x;
			int topRight = topLeft + 1;
			int bottomLeft = (z + 1) * (subdivisions + 1) + x;
			int bottomRight = bottomLeft + 1;

			planeMesh.indices.push_back(topLeft);
			planeMesh.indices.push_back(bottomLeft);
			planeMesh.indices.push_back(topRight);

			planeMesh.indices.push_back(topRight);
			planeMesh.indices.push_back(bottomLeft);
			planeMesh.indices.push_back(bottomRight);
		}
	}

	model.materials.push_back(std::make_shared<Material>());
	model.materials.back()->name = "Plane_Material";

	planeMesh.material = model.materials.back();

	planeMesh.SetupMesh();

	model.meshes.push_back(planeMesh);
	app->models.push_back(model);
}

void LoadBrickPlane(App* app) {
	GeneratePlaneModel(app, 5, 1);

	// Textures
	Model::LoadTexture(app, "Bricks/");
	Model& last = app->models.back();

	last.materials[0]->diffuse.texture = GetTexture(app, "bricks2");
	last.materials[0]->diffuse.prop_enabled = true;
	last.materials[0]->diffuse.tex_enabled = true;

	last.materials[0]->height.texture = GetTexture(app, "bricks2_disp");
	last.materials[0]->height.prop_enabled = true;
	last.materials[0]->height.tex_enabled = true;

	last.materials[0]->normal.texture = GetTexture(app, "bricks2_normal");
	last.materials[0]->normal.prop_enabled = true;
	last.materials[0]->normal.tex_enabled = true;
}

#pragma endregion

void InitTexturedQuad(App* app) {
	// - vertex buffers
	struct VertexV3V2 {
		glm::vec3 pos;
		glm::vec2 uv;
	};

	const VertexV3V2 vertices[] = {
	{glm::vec3(-1.0, -1.0, 0.0), glm::vec2(0.0, 0.0)},  // Bottom-left
	{glm::vec3(1.0, -1.0, 0.0), glm::vec2(1.0, 0.0)},   // Bottom-right
	{glm::vec3(1.0, 1.0, 0.0), glm::vec2(1.0, 1.0)},    // Top-right
	{glm::vec3(-1.0, 1.0, 0.0), glm::vec2(0.0, 1.0)}    // Top-left
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

	if (app->enableDebugGroups) {
		glObjectLabel(GL_VERTEX_ARRAY, app->vao, -1, "MainVAO");
		glObjectLabel(GL_BUFFER, app->embeddedVertices, -1, "QuadVertices");
		glObjectLabel(GL_BUFFER, app->embeddedElements, -1, "QuadIndices");
	}
}

void InitFBOs(App* app) {

#pragma region GeometryFBO

	// Create FBO
	glGenFramebuffers(1, &app->geometryFboHandle);
	glBindFramebuffer(GL_FRAMEBUFFER, app->geometryFboHandle);

	// Albedo
	glGenTextures(1, &app->albedoTexture);
	glBindTexture(GL_TEXTURE_2D, app->albedoTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, app->displaySize.x, app->displaySize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, app->albedoTexture, 0);

	// Normal
	glGenTextures(1, &app->normalTexture);
	glBindTexture(GL_TEXTURE_2D, app->normalTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, app->displaySize.x, app->displaySize.y, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, app->normalTexture, 0);

	// Position
	glGenTextures(1, &app->positionTexture);
	glBindTexture(GL_TEXTURE_2D, app->positionTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, app->displaySize.x, app->displaySize.y, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, app->positionTexture, 0);

	// Depth
	glGenTextures(1, &app->depthTexture);
	glBindTexture(GL_TEXTURE_2D, app->depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, app->displaySize.x, app->displaySize.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, app->depthTexture, 0);

	// MaterialProps (Metallic, Roughness, Height, Ambient Oclusion?)
	glGenTextures(1, &app->materialPropsTexture);
	glBindTexture(GL_TEXTURE_2D, app->materialPropsTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, app->displaySize.x, app->displaySize.y, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, app->materialPropsTexture, 0);


	GLenum drawGeoBuffers[] = {
		GL_COLOR_ATTACHMENT0,       // Albedo
		GL_COLOR_ATTACHMENT1,       // Normal
		GL_COLOR_ATTACHMENT2,       // Position
		GL_COLOR_ATTACHMENT3,       // MaterialProps
	};
	glDrawBuffers(4, drawGeoBuffers);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		ELOG("Geometry FBO initialization failed!");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

#pragma endregion

#pragma region SceneFBO
	// Create FBO
	glGenFramebuffers(1, &app->sceneFboHandle);
	glBindFramebuffer(GL_FRAMEBUFFER, app->sceneFboHandle);

	// Scene
	glGenTextures(1, &app->sceneTexture);
	glBindTexture(GL_TEXTURE_2D, app->sceneTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, app->displaySize.x, app->displaySize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, app->sceneTexture, 0);

	// Brightness
	glGenTextures(1, &app->brightnessTexture);
	glBindTexture(GL_TEXTURE_2D, app->brightnessTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, app->displaySize.x, app->displaySize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, app->brightnessTexture, 0);

	GLenum drawSceneBuffers[] = {
		GL_COLOR_ATTACHMENT0,       // Scene
		GL_COLOR_ATTACHMENT1,       // Brightness
	};
	glDrawBuffers(2, drawSceneBuffers);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		ELOG("Scene FBO initialization failed!");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
#pragma endregion

}

void InitPingPongBlurFBO(App* app) {

	// Create Ping Pong Framebuffers for repetitive blurring
	glGenFramebuffers(2, app->pingPongFboHandle);
	glGenTextures(2, app->pingPongTextures);
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, app->pingPongFboHandle[i]);
		glBindTexture(GL_TEXTURE_2D, app->pingPongTextures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, app->displaySize.x, app->displaySize.y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, app->pingPongTextures[i], 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			ELOG("PingPongBlur FBO initialization failed!");
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Init(App* app)
{
	GLUtils::InitDebugging(app);

	InitGUI(app);

	app->mode = Mode_Deferred;
	app->displayMode = Display_Albedo;

	// Camera inizialization
	app->camera = Camera(glm::vec3(0.0f, 20.0f, 30.0f));

	GL_CHECK(glEnable(GL_DEPTH_TEST));
	GL_CHECK(glClearColor(0.1f, 0.1f, 0.1f, 1.0f));

	InitFBOs(app);
	InitPingPongBlurFBO(app);
	InitTexturedQuad(app);

#pragma region Shaders

	app->shaders.emplace_back("Shaders/debug_textures.glsl", "DEBUG_TEXTURES");
	app->debugTexturesShaderIdx = app->shaders.size() - 1;

	app->shaders.emplace_back("Shaders/forward.glsl", "FORWARD");
	app->forwardShaderIdx = app->shaders.size() - 1;

	app->shaders.emplace_back("Shaders/geometry_pass.glsl", "GEOMETRY_PASS");
	app->geometryPassShaderIdx = app->shaders.size() - 1;

	app->shaders.emplace_back("Shaders/deferred_lighting.glsl", "DEFERRED_LIGHTING");
	app->deferredLightingShaderIdx = app->shaders.size() - 1;

	app->shaders.emplace_back("Shaders/bloom_pass.glsl", "BLOOM_PASS");
	app->bloomPassShaderIdx = app->shaders.size() - 1;

	app->shaders.emplace_back("Shaders/composition.glsl", "COMPOSITION");
	app->compositionShaderIdx = app->shaders.size() - 1;

#pragma endregion

#pragma region Models

	
	LoadBackPackModel(app);
	LoadBrickPlane(app);
	LoadPatrickModel(app);
	LoadRifleModel(app);

	app->selectedModel = &app->models[0];
	app->selectedMaterial = app->selectedModel->materials[0];

	app->camera.SetMode(CAMERA_ORBIT);

#pragma endregion

#pragma region Lights

	// Lights
	Light dir1;
	dir1.name = "directional_light_1";
	dir1.type = LightType_Directional;
	dir1.color = glm::vec3(1.0f, 0.95f, 0.8f);
	dir1.position = glm::vec3(0.0f);
	dir1.direction = glm::normalize(glm::vec3(-1.0f, -1.0f, -0.5f));
	dir1.intensity = 8.0;
	app->lights.push_back(dir1);

	Light point1;
	point1.enabled = false;
	point1.name = "point_light_1";
	point1.type = LightType_Point;
	point1.color = glm::vec3(0.3f, 0.6f, 1.0f);
	point1.position = glm::vec3(0.0f, 1.5f, 0.0f);
	point1.direction = glm::vec3(0.0f);
	point1.range = 40.0f;
	point1.intensity = 15.0;
	app->lights.push_back(point1);

#pragma endregion

	InitUBOs(app);

	// Enable debug options
	if (app->enableDebugGroups) {
		for (Model model : app->models) {
			for (Mesh mesh : model.meshes) {
				glObjectLabel(GL_VERTEX_ARRAY, mesh.VAO, -1, "ModelVAO");
				glObjectLabel(GL_BUFFER, mesh.VBO, -1, "ModelVBO");
				glObjectLabel(GL_BUFFER, mesh.EBO, -1, "ModelEBO");
			}

			for (const auto& texture : app->textures_loaded) {
				glObjectLabel(GL_TEXTURE, texture->id, -1, texture->path.c_str());
			}
		}
	}
}

#pragma endregion

void ResizeFBO(App* app) {
	// Albedo
	glBindTexture(GL_TEXTURE_2D, app->albedoTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
		app->displaySize.x, app->displaySize.y,
		0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	// Normal
	glBindTexture(GL_TEXTURE_2D, app->normalTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F,
		app->displaySize.x, app->displaySize.y,
		0, GL_RGB, GL_FLOAT, NULL);

	// Position
	glBindTexture(GL_TEXTURE_2D, app->positionTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F,
		app->displaySize.x, app->displaySize.y,
		0, GL_RGB, GL_FLOAT, NULL);

	// Depth
	glBindTexture(GL_TEXTURE_2D, app->depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
		app->displaySize.x, app->displaySize.y,
		0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	// Material Properties
	glBindTexture(GL_TEXTURE_2D, app->materialPropsTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
		app->displaySize.x, app->displaySize.y,
		0, GL_RGBA, GL_FLOAT, NULL);

	// Scene
	glBindTexture(GL_TEXTURE_2D, app->sceneTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
		app->displaySize.x, app->displaySize.y,
		0, GL_RGBA, GL_FLOAT, NULL);
	
	// Brightness
	glBindTexture(GL_TEXTURE_2D, app->brightnessTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
		app->displaySize.x, app->displaySize.y,
		0, GL_RGBA, GL_FLOAT, NULL);

	// Blur Ping Pong
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindTexture(GL_TEXTURE_2D, app->pingPongTextures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
			app->displaySize.x, app->displaySize.y,
			0, GL_RGBA, GL_FLOAT, NULL);
	}

	// Composite
	glBindTexture(GL_TEXTURE_2D, app->compositeTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
		app->displaySize.x, app->displaySize.y,
		0, GL_RGBA, GL_FLOAT, NULL);

	glBindTexture(GL_TEXTURE_2D, 0);

}

void Gui(App* app)
{
	ImGuiDockNodeFlags dock_flags = 0;
	dock_flags |= ImGuiDockNodeFlags_PassthruCentralNode;
	ImGui::DockSpaceOverViewport(0, dock_flags);

	UpdateMainMenu(app);
	app->panelManager.UpdatePanels(app);

	ImGui::End();
}

void Update(App* app)
{
	//TODO_K: optimize this to not run every frame?
	for (Shader& shader : app->shaders)
	{
		shader.ReloadIfNeeded();
	}

	UpdateUBOs(app);

	if (!app->models.empty() && app->rotate_models) {
		float speed = 1.0f;
		float radius = 2.0f;

		// Circular motion
		//app->models[0].position.x = sin(app->time * speed) * radius;
		//app->models[0].position.z = cos(app->time * speed) * radius;

		for (size_t i = 0; i < app->models.size(); ++i)
		{
			app->models[i].rotation.y = fmod(app->models[i].rotation.y + (app->rotate_speed), 360.0f);
		}
	}

	// You can handle app->input keyboard/mouse here
#pragma region InputHandle

	// Change mode
	if (app->input.keys[Key::K_2] == BUTTON_RELEASE) {
		switch (app->mode)
		{
		case Mode_Forward:      app->mode = Mode_DebugFBO; break;
		case Mode_DebugFBO:     app->mode = Mode_Deferred; break;
		case Mode_Deferred:     app->mode = Mode_Forward; break;
		default: break;
		}
	}

	// Change display mode
	if (app->input.keys[Key::K_3] == BUTTON_RELEASE) {
		switch (app->displayMode)
		{
		case Display_Albedo:        app->displayMode = Display_Normals; break;
		case Display_Normals:       app->displayMode = Display_Positions; break;
		case Display_Positions:     app->displayMode = Display_Depth; break;
		case Display_Depth:         app->displayMode = Display_MatProps; break;
		case Display_MatProps:      app->displayMode = Display_LightPass; break;
		case Display_LightPass:     app->displayMode = Display_Brightness; break;
		case Display_Brightness:    app->displayMode = Display_Blurr; break;
		case Display_Blurr:			app->displayMode = Display_Albedo; break;
		default: break;
		}
	}

	app->camera.SetOrbitTarget(app->selectedModel->position);

	if (app->input.keys[Key::K_F] == BUTTON_RELEASE) {
		if (app->camera.Mode == CAMERA_FREE) {
			app->camera.SetMode(CAMERA_ORBIT);
		}
		else {
			app->camera.SetMode(CAMERA_FREE);
		}
	}

	// Camera Update
	if (app->input.keys[Key::K_W] == BUTTON_PRESSED)
		app->camera.ProcessKeyboard(M_FORWARD, app->deltaTime);
	if (app->input.keys[Key::K_S] == BUTTON_PRESSED)
		app->camera.ProcessKeyboard(M_BACKWARD, app->deltaTime);
	if (app->input.keys[Key::K_A] == BUTTON_PRESSED)
		app->camera.ProcessKeyboard(M_LEFT, app->deltaTime);
	if (app->input.keys[Key::K_D] == BUTTON_PRESSED)
		app->camera.ProcessKeyboard(M_RIGHT, app->deltaTime);
	if (app->input.keys[Key::K_SPACE] == BUTTON_PRESSED)
		app->camera.ProcessKeyboard(M_UP, app->deltaTime);
	if (app->input.keys[Key::K_CTRL] == BUTTON_PRESSED)
		app->camera.ProcessKeyboard(M_DOWN, app->deltaTime);

	if (app->input.mouseButtons[MouseButton::RIGHT] == BUTTON_PRESSED)
	{
		float xoffset = app->input.mouseDelta.x;
		float yoffset = -app->input.mouseDelta.y;
		app->camera.ProcessMouseMovement(xoffset, yoffset);
	}

	if (app->input.scrollDelta.y != 0)
	{
		app->camera.ProcessMouseScroll(app->input.scrollDelta.y);
	}

#pragma endregion

	app->time += app->deltaTime;
}

void ForwardRendering(App* app) {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

	if (app->enableDebugGroups) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 2, -1, "Forward");
	Shader& currentShader = app->shaders[app->forwardShaderIdx];
	currentShader.Use();

	GL_CHECK(glBindBufferRange(GL_UNIFORM_BUFFER, 0, app->globalParamsUBO.buffer.handle, 0, app->globalParamsUBO.blockSize));

	if (app->renderAll) {
		for (Model& model : app->models) {
			glBindBufferRange(GL_UNIFORM_BUFFER, 1,
				app->transformsUBO.buffer.handle,
				model.bufferOffset,
				app->transformsUBO.blockSize);
			model.Draw(currentShader);
		}
	}
	else {
		glBindBufferRange(GL_UNIFORM_BUFFER, 1,
			app->transformsUBO.buffer.handle,
			app->selectedModel->bufferOffset,
			app->transformsUBO.blockSize);
		app->selectedModel->Draw(currentShader);
	}

	glDisable(GL_BLEND);

	if (app->enableDebugGroups) glPopDebugGroup();
}

void DeferredRendering(App* app) {
	if (app->enableDebugGroups) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 4, -1, "Deferred");

	// --- Geometry Pass ---
	glBindFramebuffer(GL_FRAMEBUFFER, app->geometryFboHandle);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	Shader& geoShader = app->shaders[app->geometryPassShaderIdx];
	geoShader.Use();
	geoShader.SetFloat("parallaxScale", app->parallax_scale);
	geoShader.SetFloat("numLayers", app->parallax_layers);

	glBindBufferRange(GL_UNIFORM_BUFFER, 0, app->globalParamsUBO.buffer.handle, 0, app->globalParamsUBO.blockSize);
	if (app->renderAll) {
		for (Model& model : app->models) {
			glBindBufferRange(GL_UNIFORM_BUFFER, 1,
				app->transformsUBO.buffer.handle,
				model.bufferOffset,
				app->transformsUBO.blockSize);
			model.Draw(geoShader);
		}
	}
	else {
		glBindBufferRange(GL_UNIFORM_BUFFER, 1,
			app->transformsUBO.buffer.handle,
			app->selectedModel->bufferOffset,
			app->transformsUBO.blockSize);
		app->selectedModel->Draw(geoShader);
	}

	// --- Lighting Pass ---
	glBindFramebuffer(GL_FRAMEBUFFER, app->sceneFboHandle);
	glDisable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT);

	Shader& lightShader = app->shaders[app->deferredLightingShaderIdx];
	lightShader.Use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, app->albedoTexture);
	lightShader.SetInt("gAlbedo", 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, app->normalTexture);
	lightShader.SetInt("gNormal", 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, app->positionTexture);
	lightShader.SetInt("gPosition", 2);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, app->materialPropsTexture);
	lightShader.SetInt("gMatProps", 3);

	glBindVertexArray(app->vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

	// --- Bloom Pass ---
	Shader& bloomShader = app->shaders[app->bloomPassShaderIdx];
	bloomShader.Use();

	bool horizontal = true, first_iteration = true;
	for (unsigned int i = 0; i < app->bloomAmount; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, app->pingPongFboHandle[horizontal]);
		bloomShader.SetBool("horizontal", horizontal);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, first_iteration ? app->brightnessTexture : app->pingPongTextures[!horizontal]);

		glBindVertexArray(app->vao);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

		horizontal = !horizontal;
		if (first_iteration)
			first_iteration = false;
	}
	app->bloomTexture = app->pingPongTextures[!horizontal];

	// --- Final Composition ---
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	Shader& compositionShader = app->shaders[app->compositionShaderIdx];
	compositionShader.Use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, app->sceneTexture);
	compositionShader.SetInt("tScene", 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, app->bloomTexture);
	compositionShader.SetInt("tBloom", 1);

	compositionShader.SetBool("bloom_enable", app->bloomEnable);
	compositionShader.SetFloat("bloom_exposure", app->bloomExposure);
	compositionShader.SetFloat("bloom_gamma", app->bloomGamma);

	glBindVertexArray(app->vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

	glEnable(GL_DEPTH_TEST);

	if (app->enableDebugGroups) glPopDebugGroup();
}

void DebugRendering(App* app) {
	if (app->enableDebugGroups) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 3, -1, "DebugFBO");

	// --- Display Pass ---
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT);

	Shader& displayShader = app->shaders[app->debugTexturesShaderIdx];
	displayShader.Use();
	displayShader.SetInt("uDisplayMode", app->displayMode);

	glActiveTexture(GL_TEXTURE0);
	switch (app->displayMode) {
	case Display_Albedo:
		glBindTexture(GL_TEXTURE_2D, app->albedoTexture);
		break;
	case Display_Normals:
		glBindTexture(GL_TEXTURE_2D, app->normalTexture);
		break;
	case Display_Positions:
		glBindTexture(GL_TEXTURE_2D, app->positionTexture);
		break;
	case Display_Depth:
		glBindTexture(GL_TEXTURE_2D, app->depthTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
		break;
	case Display_MatProps:
		glBindTexture(GL_TEXTURE_2D, app->materialPropsTexture);
		break;
	case Display_LightPass:
		glBindTexture(GL_TEXTURE_2D, app->sceneTexture);
		break;
	case Display_Brightness:
		glBindTexture(GL_TEXTURE_2D, app->brightnessTexture);
		break;
	case Display_Blurr:
		glBindTexture(GL_TEXTURE_2D, app->bloomTexture);
		break;
	}

	glBindVertexArray(app->vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
	glEnable(GL_DEPTH_TEST);

	if (app->enableDebugGroups) glPopDebugGroup();
}

void Render(App* app)
{
	GLUtils::ErrorGuard renderGuard("MainRender");

	if (app->enableDebugGroups) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "MainRenderPass");

	GL_CHECK(glViewport(0, 0, app->displaySize.x, app->displaySize.y));
	GL_CHECK(glClearColor(app->bg_color.r, app->bg_color.g, app->bg_color.b, app->bg_color.a));

	switch (app->mode)
	{
	case Mode_Forward:
		ForwardRendering(app);
		break;

	case Mode_Deferred:
		DeferredRendering(app);
		break;

	case Mode_DebugFBO:
		DeferredRendering(app);
		DebugRendering(app);
		break;

	default:
		break;
	}

	if (app->enableDebugGroups) glPopDebugGroup();
}