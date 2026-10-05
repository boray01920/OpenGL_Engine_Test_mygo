#ifndef ANIMATION_CLASS_H
#define ANIMATION_CLASS_H

#include <vector>
#include <map>
#include <string>
#include<assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include "Bone.h"
#include "AssimpNodeData.h"
#include "Model.h"

class Animation {
public:
	Animation() = default;
	Animation(const std::string& animationPath, Model* model);

	Bone* FindBone(const std::string& name);

	float GetTicksPerSecond()	{ return ticksPerSecond;}
	float GetDuration()			{ return duration;}
	const AssimpNodeData& GetRootNode() { return rootNode; }
	std::map<std::string, BoneInfo>& GetBoneIDMap() { return boneInfoMap; }

private:
	float duration;
	float ticksPerSecond;
	std::vector<Bone> bones;
	AssimpNodeData rootNode;
	std::map<std::string, BoneInfo> boneInfoMap;

	void ReadMissingBones(const aiAnimation* animation, Model& model);
	void ReadHierarchyData(AssimpNodeData& destination, const aiNode* src);
};

#endif