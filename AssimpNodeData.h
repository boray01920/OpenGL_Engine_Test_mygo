#ifndef ASSIMP_NODE_DATA_H
#define	ASSIMP_NODE_DATA_H

#include <vector>
#include <string>
#include <glm/glm.hpp>

struct AssimpNodeData {
	glm::mat4 transformation;
	std::string name;
	int childrenCount;
	std::vector<AssimpNodeData> children;
};

#endif