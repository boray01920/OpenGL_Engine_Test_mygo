#ifndef MESH_CLASS_H
#define MESH_CLASS_H

#include <string>

#include "VAO.h"
#include "EBO.h"
#include "Camera.h"
#include "Texture.h"

struct MaterialInfo {
	glm::vec4 baseColor = glm::vec4(1.0f);      //glTF baseColorFactor
	glm::vec3 emissive  = glm::vec3(0.0f);      //glTF emissiveFactor
	bool doubleSided = false;
	std::string name;                           //material name from the file (used by Level rules)
};

class Mesh {
public:
	std::vector <Vertex> vertices;
	std::vector <GLuint> indices;
	std::vector <Texture> textures;

	MaterialInfo material;

	VAO VAO;

	Mesh(std::vector <Vertex>& vertices, std::vector <GLuint>& indices, std::vector <Texture>& textures, const MaterialInfo& material = MaterialInfo());

	void Draw(
		Shader& shader,
		Camera& camera,
		glm::mat4 matrix = glm::mat4(1.0f),
		glm::vec3 translation = glm::vec3(0.0f, 0.0f, 0.0f),
		glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f));
};

#endif