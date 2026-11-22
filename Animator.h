#ifndef ANIMATOR_CLASS_H
#define ANIMATOR_CLASS_H

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Animation.h"
#include "Model.h"

class Animator {
public:
	Animator(Animation* animation);

	void UpdateAnimation(float dt);
	void PlayAnimation(Animation* pAnimation);
	void CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform);

	
	const std::vector<glm::mat4>& GetFinalBoneMatrices() const { return finalBoneMatrices; }

private:

	std::vector<glm::mat4> finalBoneMatrices;
	Animation* currentAnimation = nullptr;

	float currentTime;
};

#endif