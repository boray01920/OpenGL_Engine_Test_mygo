#ifndef MODEL_CLASS_H
#define MODEL_CLASS_H

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <map>
#include <vector>

#include "Mesh.h"

struct BoneInfo {
	int id = -1;				//id is index in finalBoneMatrices
	glm::mat4 offset = glm::mat4(1.0f);	//offset matrix tranforms vertex from model space to bone space
};

class Model {
public:
	Model(const char* file);

	void Draw(Shader& shader, Camera& camera);

	std::map<std::string, BoneInfo> m_BoneInfoMap;
	auto& GetBoneInfoMap() { return m_BoneInfoMap; }

	int m_BoneCounter = 0;
	int& GetBoneCount() { return m_BoneCounter; }

	void SetVertexBoneDataToDefault(Vertex& vertex);
	void SetVertexBoneData(Vertex& vertex, int boneID, float weight);
	void ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene);

	static glm::mat4 aiMatToGlmStatic(const aiMatrix4x4& from);

	//glm::mat4 aiMatToGlm(const aiMatrix4x4& from);

	glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);

private:
	std::vector<Mesh> meshes;
	std::vector<glm::mat4> meshTransforms;   // identity for skinned meshes (bones already include node transforms)
	std::string directory;

	std::vector<Texture> loadedTextures;
	std::vector<std::string> loadedTexturePaths;

	void processNode(aiNode* node, const aiScene* scene, glm::mat4 parentTransform);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene);
	std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName);
	glm::mat4 aiMatToGlm(const aiMatrix4x4& from);
};

#endif