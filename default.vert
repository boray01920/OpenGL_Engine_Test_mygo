#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec2 aTex;
layout (location = 5) in ivec4 boneIds;
layout (location = 6) in vec4 weights;


uniform mat4 camMatrix;
uniform mat4 model;
uniform mat4 translation;
uniform mat4 rotation;
uniform mat4 scale;

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];

out vec3 color;
out vec2 texCoord;
out vec3 Normal;
out vec3 crntPos;

void main()
{

	vec4 totalPosition = vec4(0.0f);
	vec3 totalNormal = vec3(0.0f);
	for(int i = 0; i < MAX_BONE_INFLUENCE; i++){
	if(boneIds[i] == -1) 
            continue;
        if(boneIds[i] >=MAX_BONES) 
        {
            totalPosition = vec4(aPos, 1.0f);
			totalNormal = aNormal;
            break;
        }

		vec4 localPosition = finalBonesMatrices[boneIds[i]] * vec4(aPos, 1.0f);
		totalPosition += localPosition * weights[i];

		vec3 localNormal = mat3(finalBonesMatrices[boneIds[i]]) * aNormal;
		totalNormal += localNormal * weights[i];
	}

	if(totalPosition == vec4(0.0f)){
		totalPosition = vec4(aPos,1.0f);
		totalNormal = aNormal;
	}

	crntPos = vec3(model * translation * rotation * scale * totalPosition); 
	//i dont remember why its -rotation, i think it was a bug with OpenGL !!CORRECTION: IT WAS A BUG!
	//wait now it changes nothing... or does it?
	Normal = normalize(totalNormal);
	color = aColor;
	texCoord = mat2(1.0, 0.0, 0.0, -1.0) * aTex;


	gl_Position = camMatrix * vec4(crntPos, 1.0);
}