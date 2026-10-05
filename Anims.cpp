#include "Anims.h"

void UpdateLightAnim(glm::vec3& lightPos, glm::mat4& lightModel, const glm::vec3& pyramidPos, float radius){
	float t = (float)glfwGetTime();

	glm::vec3 orbitOffset = glm::vec3(radius, 0.0f, 0.0f);
	orbitOffset = glm::rotate(orbitOffset, t, glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f)));

	lightPos = pyramidPos + orbitOffset;

	lightModel = glm::mat4(1.0f);
	lightModel = glm::translate(lightModel, lightPos);
	lightModel = glm::rotate(lightModel, t, glm::vec3(0.5f, 1.0f, 0.0f));

}