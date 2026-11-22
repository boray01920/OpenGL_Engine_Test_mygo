#include "Model.h"
#include <algorithm>

Model::Model(const char* file) {
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(file, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_LimitBoneWeights | aiProcess_JoinIdenticalVertices);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cout << "ERROR: Assmp failed to load the model:" << importer.GetErrorString() << "---"<< std::endl;
			return;
	}

	std::string pathStr(file);
	std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
	size_t slash = pathStr.find_last_of('/');
	directory = (slash == std::string::npos) ? "." : pathStr.substr(0, slash);

	processNode(scene->mRootNode, scene, glm::mat4(1.0f));
}

void Model::SetVertexBoneDataToDefault(Vertex& vertex) {
	for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
		vertex.m_BoneIDs[i] = -1;
		vertex.m_Weights[i] = 0.0f;
	}
}

void Model::SetVertexBoneData(Vertex& vertex, int boneID, float weight) {
	for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
		if (vertex.m_BoneIDs[i] < 0) {
			vertex.m_BoneIDs[i] = boneID;
			vertex.m_Weights[i] = weight;
			return;
		}
	}
}

void Model::ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene) {
	for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++) {
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
		else {
			boneID = m_BoneInfoMap[boneName].id;
		}

		auto weights = mesh->mBones[boneIndex]->mWeights;
		unsigned int numWeights = mesh->mBones[boneIndex]->mNumWeights;

		for (unsigned int weightIndex = 0; weightIndex < numWeights; weightIndex++) {
			unsigned int vertexId = weights[weightIndex].mVertexId;
			float weight = weights[weightIndex].mWeight;
			if (vertexId >= vertices.size() || weight <= 0.0f) continue;
			SetVertexBoneData(vertices[vertexId], boneID, weight);
		}
	}
}


void Model::Draw(Shader& shader, Camera& camera) {

	glm::mat4 trans = glm::translate(glm::mat4(1.0f), position);
	glm::mat4 rot = glm::mat4_cast(rotation);
	glm::mat4 sca = glm::scale(glm::mat4(1.0f), scale);

	glm::mat4 modelTransform = trans * rot * sca;


	for (unsigned int i = 0; i < meshes.size(); i++) {
		glm::mat4 m = modelTransform * meshTransforms[i];
		glFrontFace(glm::determinant(glm::mat3(m)) < 0.0f ? GL_CW : GL_CCW);   // mirrored (negative-scale) nodes flip winding
		meshes[i].Draw(shader, camera, m);
	}
}

glm::mat4 Model::aiMatToGlm(const aiMatrix4x4& from) {
	glm::mat4 to;
	to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
	to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
	to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
	to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
	return to;
}

glm::mat4 Model::aiMatToGlmStatic(const aiMatrix4x4& from) {
	glm::mat4 to;
	to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
	to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
	to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
	to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
	return to;
}

void Model::processNode(aiNode* node, const aiScene* scene, glm::mat4 parentTransform) {
	glm::mat4 nodeTransform = parentTransform * aiMatToGlm(node->mTransformation);

	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshes.push_back(processMesh(mesh, scene));
		// Skinned meshes: bone matrices already contain the full node hierarchy, so the mesh node's own transform must not be applied again.
		meshTransforms.push_back(mesh->mNumBones > 0 ? glm::mat4(1.0f) : nodeTransform);
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		processNode(node->mChildren[i], scene, nodeTransform);
	}
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene) {
	std::vector<Vertex> vertices;
	std::vector<GLuint> indices;
	std::vector<Texture> textures;

	for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
		Vertex vertex{};
		SetVertexBoneDataToDefault(vertex);
		vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

		if (mesh->HasNormals()) {
			vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
		}
		else vertex.normal = glm::vec3(1.0f, 1.0f, 1.0f);

		vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);

		if (mesh->mTextureCoords[0]) {
			vertex.texUV = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
		}
		else vertex.texUV = glm::vec2(0.0f, 0.0f);

		vertices.push_back(vertex);
	}

	for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices;j++) {
			indices.push_back(face.mIndices[j]);
		}
	}

	aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
	
	std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "diffuse");
	if (diffuseMaps.empty()) diffuseMaps = loadMaterialTextures(material, aiTextureType_BASE_COLOR, "diffuse");
	textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

	std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "specular");
	textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

	std::vector<Texture> emissiveMaps = loadMaterialTextures(material, aiTextureType_EMISSIVE, "emissive");
	textures.insert(textures.end(), emissiveMaps.begin(), emissiveMaps.end());

	MaterialInfo info;
	aiColor4D c;
	if (material->Get(AI_MATKEY_COLOR_DIFFUSE, c) == AI_SUCCESS) info.baseColor = glm::vec4(c.r, c.g, c.b, c.a);
	aiColor3D e;
	if (material->Get(AI_MATKEY_COLOR_EMISSIVE, e) == AI_SUCCESS) info.emissive = glm::vec3(e.r, e.g, e.b); // no emissive texture => emissiveFactor would just add a flat glow, so only keep it when a texture exists
	if (emissiveMaps.empty()) info.emissive = glm::vec3(0.0f);
	int twoSided = 0;
	if (material->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS) info.doubleSided = twoSided != 0;

	ExtractBoneWeightForVertices(vertices, mesh, scene);

	return Mesh(vertices, indices, textures, info);
}



std::vector<Texture> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName) {
	std::vector<Texture> textures;

	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {  
		aiString str;
		mat->GetTexture(type, i, &str);

		std::string rel = str.C_Str();
		if (!rel.empty() && rel[0] == '*') { std::cout << "WARNING: embedded textures are not supported (" << rel << ")" << std::endl; continue; }

		// glTF uris are percentencoded
		std::string decoded;
		for (size_t k = 0; k < rel.size(); k++) {
			if (rel[k] == '%' && k + 2 < rel.size() + 0 && isxdigit((unsigned char)rel[k + 1]) && isxdigit((unsigned char)rel[k + 2])) {
				decoded += (char)std::stoi(rel.substr(k + 1, 2), nullptr, 16);
				k += 2;
			}
			else decoded += rel[k];
		}
		std::string texPath = directory + "/" + decoded;

		bool skip = false;

		for (unsigned int j = 0; j < loadedTexturePaths.size(); j++) {
			if (loadedTexturePaths[j] == texPath) {
				Texture cached = loadedTextures[j];
				cached.type = typeName;
				textures.push_back(cached);
				skip = true;
				break;  
			}
		}

		if (!skip) {
			Texture texture(texPath.c_str(), typeName.c_str(), (GLuint)loadedTextures.size());
			if (!texture.valid) continue;
			textures.push_back(texture);
			loadedTextures.push_back(texture);
			loadedTexturePaths.push_back(texPath);  
		}
	}

	return textures;
}