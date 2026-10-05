#ifndef ANIMATOR_CLASS_H
#define ANIMATOR_CLASS_H

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Animation.h"

class Animator {
public:
	Animator(Animation* animation);

	void UpdateAnimation(float dt);
	void PlayAnimation(Animation* pAnimation);
	void CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform);

	std::vector<glm::mat4> finalBoneMatrices;
	Animation* currentAnimation;

	float currentTime;
};

#endif