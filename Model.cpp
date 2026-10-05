#include "Model.h"

Model::Model(const char* file) {
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(file, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cout << "ERROR: Assmp failed to load the model:" << importer.GetErrorString() << "---"<< std::endl;
			return;
	}

	std::string pathStr(file);
	directory = pathStr.substr(0, pathStr.find_last_of('/'));

	processNode(scene->mRootNode, scene, glm::mat4(1.0f));
}

void Model::Draw(Shader& shader, Camera& camera) {

	glm::mat4 trans = glm::translate(glm::mat4(1.0f), position);
	glm::mat4 rot = glm::mat4_cast(rotation);
	glm::mat4 sca = glm::scale(glm::mat4(1.0f), scale);

	glm::mat4 modelTransform = trans * rot * sca;


	for (unsigned int i = 0; i < meshes.size(); i++) {
		meshes[i].Draw(shader, camera, modelTransform * meshTransforms[i]);
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
		meshTransforms.push_back(nodeTransform);
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
	textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

	std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "specular");
	textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

	return Mesh(vertices, indices, textures);
}



std::vector<Texture> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName) {
	std::vector<Texture> textures;

	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {  
		aiString str;
		mat->GetTexture(type, i, &str);

		std::string texPath = directory + "/" + str.C_Str();

		bool skip = false;

		for (unsigned int j = 0; j < loadedTexturePaths.size(); j++) {
			if (loadedTexturePaths[j] == texPath) {
				textures.push_back(loadedTextures[j]);
				skip = true;
				break;  
			}
		}

		if (!skip) {
			Texture texture(texPath.c_str(), typeName.c_str(), (GLuint)loadedTextures.size());
			textures.push_back(texture);
			loadedTextures.push_back(texture);
			loadedTexturePaths.push_back(texPath);  
		}
	}

	return textures;
}