#include "Animator.h"

Animator::Animator(Animation* animation) {
	currentTime = 0.0f;
	currentAnimation = animation;

	finalBoneMatrices.reserve(100);
	for (int i = 0; i < 100; i++) { finalBoneMatrices.push_back(glm::mat4(1.0f)); } 
}

void Animator::UpdateAnimation(float dt) {
	if (!currentAnimation) { return; }

	currentTime += currentAnimation->GetTicksPerSecond() * dt;
	if (currentAnimation->GetDuration() > 0.0f) currentTime = fmod(currentTime, currentAnimation->GetDuration());

	CalculateBoneTransform(&currentAnimation->GetRootNode(), glm::mat4(1.0f));
}

void Animator::PlayAnimation(Animation* pAnimation) {
	currentAnimation = pAnimation;
	currentTime = 0.0f;
	for (auto& m : finalBoneMatrices) m = glm::mat4(1.0f);
}

void Animator::CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform) {
	std::string nodeName = node->name;
	glm::mat4 nodeTransform = node->transformation;

	Bone* bone = currentAnimation->FindBone(nodeName);

	if (bone) {
		bone->Update(currentTime);
		nodeTransform = bone->GetLocalTransform();
	}

	glm::mat4 globalTransformation = parentTransform * nodeTransform;

	auto& boneInfoMap = currentAnimation->GetBoneIDMap();   //was copied every node, every frame
	auto it = boneInfoMap.find(nodeName);
	if (it != boneInfoMap.end()) {
		int index = it->second.id;
		if (index >= 0 && index < (int)finalBoneMatrices.size())
			finalBoneMatrices[index] = globalTransformation * it->second.offset;
	}

	for (size_t i = 0; i < node->children.size(); i++) {CalculateBoneTransform(&node->children[i], globalTransformation);}
}
