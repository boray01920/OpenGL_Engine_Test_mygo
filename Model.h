#ifndef MODEL_CLASS_H
#define MODEL_CLASS_H

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <map>
#include <vector>

#include "Mesh.h"

struct BoneInfo {
	int id;				//id is index in finalBoneMatrices
	glm::mat4 offset;	//offset matrix tranforms vertex from model space to bone space
};

class Model {
public:
	Model(const char* file);

	void Draw(Shader& shader, Camera& camera);

	std::map<std::string, BoneInfo> m_BoneInfoMap;
	auto& GetBoneInfoMap() { return m_BoneInfoMap; }

	int m_BoneCounter = 0;
	int& GetBoneCount() { return m_BoneCounter; }
	static glm::mat4 aiMatToGlmStatic(const aiMatrix4x4& from);

	//glm::mat4 aiMatToGlm(const aiMatrix4x4& from);

	glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);

private:
	std::vector<Mesh> meshes;
	std::vector<glm::mat4> meshTransforms;
	std::string directory;

	std::vector<Texture> loadedTextures;
	std::vector<std::string> loadedTexturePaths;

	void processNode(aiNode* node, const aiScene* scene, glm::mat4 parentTransform);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene);
	std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName);
	glm::mat4 aiMatToGlm(const aiMatrix4x4& from);

	//std::map<std::string, BoneInfo> m_BoneInfoMap;
	//int m_BoneCounter = 0;

	//auto& GetBoneInfoMap()	{ return m_BoneInfoMap; }
	//int&	GetBoneCount()	{ return m_BoneCounter; }

	void SetVertexBoneDataToDefault(Vertex& vertex) {
		for (int i = 0; i < AI_MAX_BONE_WEIGHTS;i++) {
			vertex.m_BoneIDs[i] = -1;
			vertex.m_Weights[i] = 0.0f;
		}
	}

	void SetVertexBoneData(Vertex& vertex, int boneID, float weight) {
		for (int i = 0; i < MAX_BONE_INFLUENCE;i++) {
			if (vertex.m_BoneIDs[i] < 0) {
				vertex.m_BoneIDs[i] = boneID;
				vertex.m_Weights[i] = weight;
				return;
			}
		}
	}

	void ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene) {
		for (unsigned boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++) {
			int boneID = -1;
			std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();

			if (m_BoneInfoMap.find(boneName) == m_BoneInfoMap.end()) {
				BoneInfo newBoneInfo;
				newBoneInfo.id = m_BoneCounter;
				newBoneInfo.offset = aiMatToGlm(mesh->mBones[boneIndex]->mOffsetMatrix);
				m_BoneInfoMap[boneName] = newBoneInfo;
				boneID = m_BoneCounter;
				m_BoneCounter++;
			}

			else { boneID = m_BoneInfoMap[boneName].id; }

			auto m_Weights = mesh->mBones[boneIndex]->mWeights;
			unsigned int numWeights = mesh->mBones[boneIndex]->mNumWeights;

			for (unsigned int weightIndex = 0; weightIndex < numWeights; weightIndex++) {
				unsigned int vertexId = m_Weights[weightIndex].mVertexId;
				float weight = m_Weights[weightIndex].mWeight;
				SetVertexBoneData(vertices[vertexId], boneID, weight);
			}
		}
	}
};

#endif