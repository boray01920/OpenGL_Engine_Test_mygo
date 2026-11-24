#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include "Mesh.h"

// Builds a unit cube (-0.5..0.5) with per-face normals, flat colored via MaterialInfo.
// Scale/translate it with the model matrix passed to Mesh::Draw. Needs a live GL context.
inline Mesh MakeBoxMesh(const glm::vec4& color) {
	struct Face { glm::vec3 n, u, v; };
	const Face faces[6] = {
		{ { 1, 0, 0}, {0, 1, 0}, {0, 0, 1} },
		{ {-1, 0, 0}, {0, 0, 1}, {0, 1, 0} },
		{ { 0, 1, 0}, {0, 0, 1}, {1, 0, 0} },
		{ { 0,-1, 0}, {1, 0, 0}, {0, 0, 1} },
		{ { 0, 0, 1}, {1, 0, 0}, {0, 1, 0} },
		{ { 0, 0,-1}, {0, 1, 0}, {1, 0, 0} },
	};
	const glm::vec2 uvs[4] = { {0,0}, {1,0}, {1,1}, {0,1} };
	const glm::vec2 signs[4] = { {-1,-1}, {1,-1}, {1,1}, {-1,1} };

	std::vector<Vertex> vertices;
	std::vector<GLuint> indices;
	for (const Face& f : faces) {
		GLuint base = (GLuint)vertices.size();
		for (int i = 0; i < 4; i++) {
			Vertex vert{};
			vert.position = f.n * 0.5f + f.u * (signs[i].x * 0.5f) + f.v * (signs[i].y * 0.5f);
			vert.normal = f.n;
			vert.color = glm::vec3(1.0f);
			vert.texUV = uvs[i];
			for (int b = 0; b < MAX_BONE_INFLUENCE; b++) { vert.m_BoneIDs[b] = -1; vert.m_Weights[b] = 0.0f; }
			vertices.push_back(vert);
		}
		const GLuint idx[6] = { 0, 1, 2, 0, 2, 3 };
		for (GLuint k : idx) indices.push_back(base + k);
	}

	std::vector<Texture> textures;
	MaterialInfo mat;
	mat.baseColor = color;
	return Mesh(vertices, indices, textures, mat);
}

#endif
