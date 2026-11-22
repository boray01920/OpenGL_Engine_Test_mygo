#include "Mesh.h"
#include <cstddef>

Mesh::Mesh(std::vector <Vertex>& vertices, std::vector <GLuint>& indices, std::vector <Texture>& textures, const MaterialInfo& material) {
	Mesh::material = material;
	Mesh::vertices = vertices;
	Mesh::indices = indices;
	Mesh::textures = textures;

	VAO.Bind();

	VBO VBO(vertices);
	EBO EBO(indices);

	VAO.LinkAttrib(VBO, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)0);
	VAO.LinkAttrib(VBO, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)(3 * sizeof(float)));
	VAO.LinkAttrib(VBO, 2, 3, GL_FLOAT, sizeof(Vertex), (void*)(6 * sizeof(float)));
	VAO.LinkAttrib(VBO, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)(9 * sizeof(float)));
	// bone attributes: must be set while the VBO is bound, and must be enabled
	VBO.Bind();
	glVertexAttribIPointer(5, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, m_BoneIDs));
	glEnableVertexAttribArray(5);
	glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_Weights));
	glEnableVertexAttribArray(6);
	VAO.Unbind();
	VBO.Unbind();
	EBO.Unbind();
}

void Mesh::Draw(Shader& shader, Camera& camera, glm::mat4 matrix, glm::vec3 translation, glm::quat rotation, glm::vec3 scale){
	shader.Activate();
	VAO.Bind();

	unsigned int numDiffuse  = 0;
	unsigned int numSpecular = 0;
	unsigned int numEmissive = 0;

	for (unsigned int i = 0; i < textures.size(); i++) {
		std::string num;
		std::string type = textures[i].type;

		if      (type == "diffuse" )  {num = std::to_string(numDiffuse++); }
		else if (type == "specular")  {num = std::to_string(numSpecular++);}
		else if (type == "emissive")  {num = std::to_string(numEmissive++);}
		
		textures[i].unit = i;   // Bind() uses 'unit', the sampler uniform uses i: keep them identical
		textures[i].texUnit(shader, (type + num).c_str(), i);
		textures[i].Bind();
	}

	glUniform1i(glGetUniformLocation(shader.ID, "hasDiffuse"), numDiffuse > 0);
	glUniform1i(glGetUniformLocation(shader.ID, "hasSpecular"), numSpecular > 0);
	glUniform1i(glGetUniformLocation(shader.ID, "hasEmissive"), numEmissive > 0);
	glUniform4fv(glGetUniformLocation(shader.ID, "baseColorFactor"), 1, glm::value_ptr(material.baseColor));
	glUniform3fv(glGetUniformLocation(shader.ID, "emissiveFactor"), 1, glm::value_ptr(material.emissive));

	// single-sided materials (inverted-hull outlines) rely on back-face culling
	if (material.doubleSided) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);
	glUniform3f(glGetUniformLocation(shader.ID, "camPos"), camera.Position.x, camera.Position.y, camera.Position.z);
	camera.Matrix(shader, "camMatrix");

	glm::mat4 trans = glm::mat4(1.0f);
	glm::mat4 rot = glm::mat4(1.0f);
	glm::mat4 sca = glm::mat4(1.0f);

	trans = glm::translate(trans, translation);
	rot = glm::mat4_cast(rotation);
	sca = glm::scale(sca, scale);
	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "translation"), 1, GL_FALSE, glm::value_ptr(trans));
	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "rotation"), 1, GL_FALSE, glm::value_ptr(rot));
	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "scale"), 1, GL_FALSE, glm::value_ptr(sca));
	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "model"), 1, GL_FALSE, glm::value_ptr(matrix));

	glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
}