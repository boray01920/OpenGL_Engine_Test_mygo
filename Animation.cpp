#include "Animation.h"
#include "Model.h"

Animation::Animation(const std::string& animationPath, Model* model) {
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(animationPath, aiProcess_Triangulate);

	if (!scene || !scene->mRootNode || scene->mNumAnimations == 0) {
		std::cout << "ERROR: Failed to load animation from => " << animationPath << "!!!!" << std::endl;
		return;
	}

	auto animation = scene->mAnimations[0];
	duration = (float)animation->mDuration;
	ticksPerSecond = (int)animation->mTicksPerSecond;

	ReadHierarchyData(rootNode, scene->mRootNode);
	ReadMissingBones(animation, *model);
}

Bone* Animation::FindBone(const std::string& name) {
	for (auto& bone : bones) {
		if (bone.GetBoneName() == name) {
			return &bone;
		}
	}
	return nullptr;
}

void Animation::ReadMissingBones(const aiAnimation* animation, Model& model) {
	int size = animation->mNumChannels;

	auto& boneInfoMapFromModel = model.GetBoneInfoMap();
	int& boneCount = model.GetBoneCount();

	for (int i = 0; i < size; i++) {
		auto channel = animation->mChannels[i];
		std::string boneName = channel->mNodeName.data;

		if (boneInfoMapFromModel.find(boneName) == boneInfoMapFromModel.end()) {
			boneInfoMapFromModel[boneName].id = boneCount;
			boneCount++;
		}
		bones.push_back(Bone(boneName, boneInfoMapFromModel[boneName].id, channel));
	}
	boneInfoMap = boneInfoMapFromModel;
}

void Animation::ReadHierarchyData(AssimpNodeData& destination, const aiNode* src) {
	destination.name = src->mName.data;
	destination.transformation = Model::aiMatToGlmStatic(src->mTransformation);
	destination.childrenCount = src->mNumChildren;

	for (unsigned int i = 0; i < src->mNumChildren; i++) {
		AssimpNodeData newData;
		ReadHierarchyData(newData, src->mChildren[i]);
		destination.children.push_back(newData);
	}
}

